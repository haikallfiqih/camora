#include "capture_engine.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <algorithm>

#include <cstring>
#include <iostream>

CaptureEngine::CaptureEngine() {
    gst_init(nullptr, nullptr);
}

CaptureEngine::~CaptureEngine() {
    stop();
}

bool CaptureEngine::start(
    const std::string& device,
    int width,
    int height,
    int fps
) {
    stop();

    device_ = device;
    width_ = width;
    height_ = height;
    fps_ = fps;

    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        latestFrame_.reset();
        latestFrameSequence_ = 0;
    }
    {
        std::lock_guard<std::mutex> lock(framePoolMutex_);
        framePool_.clear();
    }

    subjectMask_.clear();
    previousSubjectMask_.clear();
    segmentationFrame_.reset();

    {
        std::lock_guard<std::mutex> lock(processingMutex_);
        processingPending_ = false;
        processingStop_ = false;
        processingInput_.reset();
    }

    {
        std::lock_guard<std::mutex> lock(segmentationMutex_);
        segmentationPending_ = false;
        segmentationStop_ = false;
    }

    running_ = true;

    segmentationThread_ = std::thread(
        &CaptureEngine::segmentationLoop,
        this
    );

    processingThread_ = std::thread(
        &CaptureEngine::processingLoop,
        this
    );

    thread_ = std::thread(
        &CaptureEngine::captureLoop,
        this
    );

    return true;
}

void CaptureEngine::stop() {
    running_ = false;
    frameCondition_.notify_all();

    if (thread_.joinable()) {
        thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(processingMutex_);
        processingStop_ = true;
        processingPending_ = false;
    }
    processingCondition_.notify_one();

    if (processingThread_.joinable()) {
        processingThread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(segmentationMutex_);
        segmentationStop_ = true;
        segmentationPending_ = false;
    }
    segmentationCondition_.notify_one();
    if (segmentationThread_.joinable()) {
        segmentationThread_.join();
    }

    // All workers are stopped, so nothing can access the RVM session.
    // Release ONNX Runtime / CUDA resources explicitly while the CUDA
    // driver and process runtime are still fully alive.
    segmenter_.shutdown();
}

void CaptureEngine::setLowLightEnhancement(
    bool enabled,
    int strength
) {
    const int safeStrength =
        std::clamp(strength, 0, 100);

    lowLightEnabled_ = enabled;
    lowLightStrength_ = safeStrength;

    lowLightProcessor_.setEnabled(enabled);
    lowLightProcessor_.setStrength(safeStrength);
}

bool CaptureEngine::configureSegmentationModel(
    const std::string& modelPath
) {
    return segmenter_.initialize(modelPath, width_, height_);
}

void CaptureEngine::setBackgroundReplacement(
    bool enabled,
    std::vector<uint8_t> pixels,
    int width,
    int height
) {
    std::lock_guard<std::mutex> lock(backgroundMutex_);
    backgroundPixels_ = std::move(pixels);
    backgroundWidth_ = width;
    backgroundHeight_ = height;
    backgroundEnabled_ = enabled && !backgroundPixels_.empty();

    if (!backgroundEnabled_) {
        segmenter_.resetTemporalState();
        previousSubjectMask_.clear();
    }
}

void CaptureEngine::setBackgroundBlur(
    bool enabled,
    int strength
) {
    backgroundBlurStrength_ = std::clamp(strength, 0, 100);
    backgroundBlurEnabled_ = enabled;

    if (!enabled) {
        segmenter_.resetTemporalState();
        previousSubjectMask_.clear();
    }
}


void CaptureEngine::setBackgroundRemoval(
    bool enabled
) {
    backgroundRemovalEnabled_ = enabled;

    if (!enabled) {
        segmenter_.resetTemporalState();
        previousSubjectMask_.clear();
    }
}


void CaptureEngine::setAutoFraming(
    bool enabled,
    int sensitivity
) {
    autoFramingSensitivity_ =
        std::clamp(sensitivity, 0, 100);

    const bool wasEnabled =
        autoFramingEnabled_.exchange(enabled);

    if (wasEnabled && !enabled) {
        resetAutoFraming();
    }
}

void CaptureEngine::resetAutoFraming() {
    autoFramingInitialized_ = false;
    autoFrameCenterX_ = 0.5f;
    autoFrameCenterY_ = 0.5f;
    autoFrameZoom_ = 1.0f;
}

std::shared_ptr<VideoFrame> CaptureEngine::acquireFrame() {
    const size_t required = static_cast<size_t>(width_) * height_ * 4;
    std::lock_guard<std::mutex> lock(framePoolMutex_);
    for (const auto& frame : framePool_) {
        if (frame.use_count() == 1) {
            frame->rgba.resize(required);
            frame->width = width_;
            frame->height = height_;
            frame->sequence = 0;
            return frame;
        }
    }

    auto frame = std::make_shared<VideoFrame>();
    frame->rgba.resize(required);
    frame->width = width_;
    frame->height = height_;
    framePool_.push_back(frame);
    return frame;
}

void CaptureEngine::submitProcessingFrame(
    const uint8_t* rgba,
    size_t size
) {
    if (!rgba || size == 0) return;

    auto frame = acquireFrame();
    if (frame->rgba.size() != size) return;
    std::memcpy(frame->rgba.data(), rgba, size);

    std::lock_guard<std::mutex> lock(processingMutex_);
    if (processingStop_) return;

    // One owning pending slot. Replacing it drops stale work without latency.
    processingInput_ = std::move(frame);
    processingPending_ = true;
    processingCondition_.notify_one();
}

void CaptureEngine::processingLoop() {
    std::shared_ptr<VideoFrame> frame;

    while (true) {
        {
            std::unique_lock<std::mutex> lock(processingMutex_);

            processingCondition_.wait(
                lock,
                [this] {
                    return processingPending_ ||
                           processingStop_;
                }
            );

            if (processingStop_) {
                return;
            }

            frame = std::move(processingInput_);
            processingPending_ = false;
        }

        if (!frame || frame->rgba.empty()) {
            continue;
        }

        // Heavy processing happens WITHOUT frameMutex_.
        lowLightProcessor_.process(
            frame->rgba.data(),
            width_,
            height_
        );

        const bool segmentationEffectEnabled =
            backgroundEnabled_ ||
            backgroundBlurEnabled_ ||
            backgroundRemovalEnabled_ ||
            autoFramingEnabled_;

        if (segmentationEffectEnabled && segmenter_.available()) {
            submitSegmentationFrame(std::move(frame));
        }

        // When background replacement is enabled, the segmentation
        // worker owns publishing because it has the mask matched to
        // this frame. Publishing here as well would alternate between
        // raw and composited frames, causing background flicker.
        if (!backgroundEnabled_ &&
            !backgroundBlurEnabled_ &&
            !backgroundRemovalEnabled_ &&
            !autoFramingEnabled_) {
            publishFrame(std::move(frame));
        }
    }
}

void CaptureEngine::submitSegmentationFrame(
    std::shared_ptr<VideoFrame> frame
) {
    if (!frame || frame->rgba.empty()) return;

    std::unique_lock<std::mutex> lock(segmentationMutex_, std::try_to_lock);
    if (!lock.owns_lock() || segmentationStop_) return;

    const int sampleWidth = segmenter_.maskWidth();
    const int sampleHeight = segmenter_.maskHeight();
    if (sampleWidth <= 0 || sampleHeight <= 0) return;
    segmentationInput_.resize(
        static_cast<size_t>(sampleWidth * sampleHeight * 4));
    for (int y = 0; y < sampleHeight; ++y) {
        const int sourceY = std::min(
            height_ - 1, y * height_ / sampleHeight);
        for (int x = 0; x < sampleWidth; ++x) {
            const int sourceX = std::min(
                width_ - 1, x * width_ / sampleWidth);
            const uint8_t* source = frame->rgba.data() +
                (sourceY * width_ + sourceX) * 4;
            uint8_t* destination = segmentationInput_.data() +
                (y * sampleWidth + x) * 4;
            destination[0] = source[0];
            destination[1] = source[1];
            destination[2] = source[2];
            destination[3] = 255;
        }
    }

    // The downsample and full-resolution image share this exact owner.
    segmentationFrame_ = std::move(frame);
    segmentationPending_ = true;
    lock.unlock();
    segmentationCondition_.notify_one();
}

void CaptureEngine::segmentationLoop() {
    std::vector<uint8_t> input;
    std::shared_ptr<VideoFrame> sourceFrame;
#ifndef NDEBUG
    using MetricsClock = std::chrono::steady_clock;
    auto metricsStart = MetricsClock::now();
    std::chrono::nanoseconds processingTime{0};
    std::chrono::nanoseconds publicationTime{0};
    uint64_t processedFrames = 0;
#endif

    while (true) {
        {
            std::unique_lock<std::mutex> lock(segmentationMutex_);
            segmentationCondition_.wait(lock, [this] {
                return segmentationPending_ || segmentationStop_;
            });
            if (segmentationStop_) return;
            input.swap(segmentationInput_);
            sourceFrame = std::move(segmentationFrame_);
            segmentationPending_ = false;
        }


#ifndef NDEBUG
        const auto processingStart = MetricsClock::now();
#endif

        if ((!backgroundEnabled_ &&
             !backgroundBlurEnabled_ &&
             !backgroundRemovalEnabled_ &&
             !autoFramingEnabled_) ||
            input.empty() ||
            !segmenter_.segment(
                input.data(),
                segmenter_.maskWidth(),
                segmenter_.maskHeight(),
                subjectMask_
            )) {
            continue;
        }

        const int maskWidth = segmenter_.maskWidth();
        const int maskHeight = segmenter_.maskHeight();

        const bool hasPreviousMask =
            previousSubjectMask_.size() == subjectMask_.size();

        refinedMask_.resize(subjectMask_.size());

        for (size_t i = 0; i < subjectMask_.size(); ++i) {
            const float current = std::clamp(
                subjectMask_[i], 0.0f, 1.0f);

            // Motion is measured only to decide whether refinement is safe.
            // The previous alpha is never blended into the current alpha.
            const float motion = hasPreviousMask
                ? std::abs(current - previousSubjectMask_[i])
                : 1.0f;

            float refined = current;

            // Only touch genuinely uncertain matte pixels.
            if (current > 0.08f &&
                current < 0.92f &&
                motion < 0.08f) {

                // Mild stable-edge refinement.
                //
                // One smoothstep only. Unlike v1, this deliberately avoids
                // aggressively collapsing uncertain foreground pixels.
                const float smooth =
                    current * current * (3.0f - 2.0f * current);

                // Blend only 25% toward the refined value.
                // 75% remains the original RVM matte.
                refined =
                    current * 0.75f +
                    smooth * 0.25f;
            }

            refinedMask_[i] = refined;
        }

        // Save CURRENT raw RVM mask only for motion detection next frame.
        // Do not save refinedMask here.
        previousSubjectMask_ = subjectMask_;

        alpha_.resize(static_cast<size_t>(width_ * height_));
        for (int y = 0; y < height_; ++y) {
            const float maskY = height_ > 1
                ? y * (maskHeight - 1.0f) / (height_ - 1.0f)
                : 0.0f;
            const int y0 = static_cast<int>(maskY);
            const int y1 = std::min(maskHeight - 1, y0 + 1);
            const float fy = maskY - y0;
            for (int x = 0; x < width_; ++x) {
                const float maskX = width_ > 1
                    ? x * (maskWidth - 1.0f) / (width_ - 1.0f)
                    : 0.0f;
                const int x0 = static_cast<int>(maskX);
                const int x1 = std::min(maskWidth - 1, x0 + 1);
                const float fx = maskX - x0;
                const float top = refinedMask_[y0 * maskWidth + x0] *
                        (1.0f - fx) +
                    refinedMask_[y0 * maskWidth + x1] * fx;
                const float bottom = refinedMask_[y1 * maskWidth + x0] *
                        (1.0f - fx) +
                    refinedMask_[y1 * maskWidth + x1] * fx;
                const float probability = top * (1.0f - fy) + bottom * fy;
                const float confidence = std::clamp(
                    (probability - 0.03f) / 0.94f, 0.0f, 1.0f);
                const float feathered = confidence * confidence *
                    (3.0f - 2.0f * confidence);
                alpha_[y * width_ + x] = static_cast<uint8_t>(
                    feathered * 255.0f);
            }
        }

        if (!sourceFrame || sourceFrame->rgba.size() !=
            static_cast<size_t>(width_ * height_ * 4)) {
            continue;
        }

        if (backgroundEnabled_) {
            compositeBackground(
                sourceFrame->rgba.data(),
                alpha_
            );
        } else if (backgroundBlurEnabled_) {
            compositeBackgroundBlur(
                sourceFrame->rgba.data(),
                alpha_
            );
        } else if (backgroundRemovalEnabled_) {
            compositeBackgroundRemoval(
                sourceFrame->rgba.data(),
                alpha_
            );
        }

        // Auto Framing is independent from the background mode.
        // It runs after background compositing so Blur/Image/Removal
        // can be combined with subject tracking.
        if (autoFramingEnabled_) {
            applyAutoFraming(
                sourceFrame->rgba.data(),
                refinedMask_,
                maskWidth,
                maskHeight
            );
        }


#ifndef NDEBUG
        const auto publicationStart = MetricsClock::now();
#endif
        publishFrame(std::move(sourceFrame));
#ifndef NDEBUG
        const auto now = MetricsClock::now();
        processingTime += std::chrono::duration_cast<std::chrono::nanoseconds>(
            publicationStart - processingStart);
        publicationTime += std::chrono::duration_cast<std::chrono::nanoseconds>(
            now - publicationStart);
        ++processedFrames;
        const auto elapsed = now - metricsStart;
        if (elapsed >= std::chrono::seconds(1)) {
            const double processAverageMs = processedFrames == 0 ? 0.0 :
                std::chrono::duration<double, std::milli>(processingTime).count() /
                    processedFrames;
            const double publishAverageMs = processedFrames == 0 ? 0.0 :
                std::chrono::duration<double, std::milli>(publicationTime).count() /
                    processedFrames;
            std::clog << "[Camora processing] frames=" << processedFrames
                      << " effect_avg=" << processAverageMs << "ms"
                      << " publish_avg=" << publishAverageMs << "ms"
                      << std::endl;
            metricsStart = now;
            processingTime = std::chrono::nanoseconds::zero();
            publicationTime = std::chrono::nanoseconds::zero();
            processedFrames = 0;
        }
#endif
    }
}

void CaptureEngine::applyAutoFraming(
    uint8_t* rgba,
    const std::vector<float>& mask,
    int maskWidth,
    int maskHeight
) {
    if (!rgba ||
        !autoFramingEnabled_ ||
        maskWidth <= 0 ||
        maskHeight <= 0 ||
        mask.size() !=
            static_cast<size_t>(maskWidth * maskHeight)) {
        return;
    }

    // --------------------------------------------------------
    // Find subject bounds directly in the small RVM mask.
    // This avoids another full-resolution scan.
    // --------------------------------------------------------

    int minX = maskWidth;
    int minY = maskHeight;
    int maxX = -1;
    int maxY = -1;
    int subjectPixels = 0;

    constexpr float subjectThreshold = 0.35f;

    for (int y = 0; y < maskHeight; ++y) {
        for (int x = 0; x < maskWidth; ++x) {
            const float confidence =
                mask[static_cast<size_t>(
                    y * maskWidth + x)];

            if (confidence < subjectThreshold) {
                continue;
            }

            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
            ++subjectPixels;
        }
    }

    // Ignore tiny segmentation noise.
    const int minimumSubjectPixels =
        std::max(12, (maskWidth * maskHeight) / 500);

    if (maxX < minX ||
        maxY < minY ||
        subjectPixels < minimumSubjectPixels) {
        return;
    }

    const float subjectCenterX =
        (static_cast<float>(minX + maxX) * 0.5f) /
        std::max(1, maskWidth - 1);

    const float subjectCenterY =
        (static_cast<float>(minY + maxY) * 0.5f) /
        std::max(1, maskHeight - 1);

    const float subjectWidth =
        static_cast<float>(maxX - minX + 1) /
        static_cast<float>(maskWidth);

    const float subjectHeight =
        static_cast<float>(maxY - minY + 1) /
        static_cast<float>(maskHeight);

    // --------------------------------------------------------
    // Framing target.
    //
    // Keep generous head/body room. Auto Framing should feel like
    // a camera operator, not an aggressive face crop.
    // --------------------------------------------------------

    // Digital auto-framing needs crop headroom.
    //
    // At 1.0x the crop is the entire sensor frame, so there is
    // physically nowhere for a digital pan to move. Keep a mild
    // base crop while Auto Framing is active.
    constexpr float baseZoom = 1.20f;
    constexpr float maximumZoom = 1.45f;

    // If the subject is unusually small we may zoom in further,
    // but ordinary webcam framing stays around the base zoom.
    constexpr float targetSubjectWidth = 0.52f;
    constexpr float targetSubjectHeight = 0.72f;

    const float sizeDrivenZoom = std::min(
        targetSubjectWidth /
            std::max(subjectWidth, 0.01f),
        targetSubjectHeight /
            std::max(subjectHeight, 0.01f)
    );

    float targetZoom = std::clamp(
        std::max(baseZoom, sizeDrivenZoom),
        baseZoom,
        maximumZoom
    );

    // --------------------------------------------------------
    // Dead zone.
    //
    // Small body/head movement must not make the camera shake.
    // --------------------------------------------------------

    const int sensitivity =
        std::clamp(
            autoFramingSensitivity_.load(),
            0,
            100
        );

    const float normalizedSensitivity =
        static_cast<float>(sensitivity) / 100.0f;

    // High sensitivity -> smaller dead zone.
    const float deadZone =
        0.085f -
        normalizedSensitivity * 0.045f;

    float targetCenterX =
        autoFramingInitialized_
            ? autoFrameCenterX_
            : subjectCenterX;

    float targetCenterY =
        autoFramingInitialized_
            ? autoFrameCenterY_
            : subjectCenterY;

    if (!autoFramingInitialized_ ||
        std::abs(subjectCenterX - autoFrameCenterX_) >
            deadZone) {
        targetCenterX = subjectCenterX;
    }

    // Vertical framing intentionally moves less eagerly.
    // Webcam users move horizontally much more often.
    if (!autoFramingInitialized_ ||
        std::abs(subjectCenterY - autoFrameCenterY_) >
            deadZone * 1.35f) {
        targetCenterY = subjectCenterY;
    }

    // --------------------------------------------------------
    // Temporal smoothing.
    // Sensitivity controls response speed, not just dead-zone.
    // --------------------------------------------------------

    const float centerSmoothing =
        0.055f +
        normalizedSensitivity * 0.115f;

    const float zoomSmoothing =
        0.035f +
        normalizedSensitivity * 0.065f;

    if (!autoFramingInitialized_) {
        autoFrameCenterX_ = targetCenterX;
        autoFrameCenterY_ = targetCenterY;

        // Auto Framing requires crop headroom immediately so
        // horizontal/vertical tracking can actually move.
        autoFrameZoom_ = baseZoom;

        autoFramingInitialized_ = true;
    } else {
        autoFrameCenterX_ +=
            (targetCenterX - autoFrameCenterX_) *
            centerSmoothing;

        autoFrameCenterY_ +=
            (targetCenterY - autoFrameCenterY_) *
            centerSmoothing;

        autoFrameZoom_ +=
            (targetZoom - autoFrameZoom_) *
            zoomSmoothing;
    }

    // --------------------------------------------------------
    // Convert smoothed framing into a crop rectangle.
    // --------------------------------------------------------

    const float zoom =
        std::clamp(autoFrameZoom_, 1.0f, 1.45f);

    int cropWidth =
        static_cast<int>(
            static_cast<float>(width_) / zoom);

    int cropHeight =
        static_cast<int>(
            static_cast<float>(height_) / zoom);

    cropWidth = std::clamp(cropWidth, 2, width_);
    cropHeight = std::clamp(cropHeight, 2, height_);

    int cropX =
        static_cast<int>(
            autoFrameCenterX_ * width_ -
            cropWidth * 0.5f);

    int cropY =
        static_cast<int>(
            autoFrameCenterY_ * height_ -
            cropHeight * 0.5f);

    cropX = std::clamp(
        cropX,
        0,
        std::max(0, width_ - cropWidth)
    );

    cropY = std::clamp(
        cropY,
        0,
        std::max(0, height_ - cropHeight)
    );

    // No visible crop yet.
    if (cropWidth == width_ &&
        cropHeight == height_) {
        return;
    }

    // --------------------------------------------------------
    // Bilinear crop + scale back to original output dimensions.
    //
    // Output resolution never changes, so Flutter and the future
    // virtual-camera sink remain stable.
    // --------------------------------------------------------

    autoFramingOutput_.resize(
        static_cast<size_t>(width_ * height_ * 4));

    const float xScale =
        width_ > 1
            ? static_cast<float>(cropWidth - 1) /
                static_cast<float>(width_ - 1)
            : 0.0f;

    const float yScale =
        height_ > 1
            ? static_cast<float>(cropHeight - 1) /
                static_cast<float>(height_ - 1)
            : 0.0f;

    for (int y = 0; y < height_; ++y) {
        const float sourceY =
            static_cast<float>(cropY) +
            static_cast<float>(y) * yScale;

        const int y0 =
            std::clamp(
                static_cast<int>(sourceY),
                0,
                height_ - 1);

        const int y1 =
            std::min(height_ - 1, y0 + 1);

        const float fy =
            sourceY - static_cast<float>(y0);

        for (int x = 0; x < width_; ++x) {
            const float sourceX =
                static_cast<float>(cropX) +
                static_cast<float>(x) * xScale;

            const int x0 =
                std::clamp(
                    static_cast<int>(sourceX),
                    0,
                    width_ - 1);

            const int x1 =
                std::min(width_ - 1, x0 + 1);

            const float fx =
                sourceX - static_cast<float>(x0);

            const uint8_t* p00 =
                rgba +
                static_cast<size_t>(
                    y0 * width_ + x0) * 4;

            const uint8_t* p10 =
                rgba +
                static_cast<size_t>(
                    y0 * width_ + x1) * 4;

            const uint8_t* p01 =
                rgba +
                static_cast<size_t>(
                    y1 * width_ + x0) * 4;

            const uint8_t* p11 =
                rgba +
                static_cast<size_t>(
                    y1 * width_ + x1) * 4;

            uint8_t* out =
                autoFramingOutput_.data() +
                static_cast<size_t>(
                    y * width_ + x) * 4;

            for (int channel = 0;
                 channel < 4;
                 ++channel) {
                const float top =
                    p00[channel] * (1.0f - fx) +
                    p10[channel] * fx;

                const float bottom =
                    p01[channel] * (1.0f - fx) +
                    p11[channel] * fx;

                out[channel] =
                    static_cast<uint8_t>(
                        std::clamp(
                            top * (1.0f - fy) +
                                bottom * fy,
                            0.0f,
                            255.0f
                        )
                    );
            }
        }
    }

    std::memcpy(
        rgba,
        autoFramingOutput_.data(),
        autoFramingOutput_.size()
    );
}

void CaptureEngine::compositeBackgroundRemoval(
    uint8_t* rgba,
    const std::vector<uint8_t>& alpha
) {
    if (!rgba ||
        alpha.size() != static_cast<size_t>(width_ * height_) ||
        !backgroundRemovalEnabled_) {
        return;
    }

    const size_t pixelCount =
        static_cast<size_t>(width_ * height_);

    // RGB remains untouched.
    //
    // The matched RVM matte becomes the frame's alpha channel so
    // downstream consumers that support RGBA receive real transparency.
    for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
        rgba[pixel * 4 + 3] = alpha[pixel];
    }
}

void CaptureEngine::compositeBackgroundBlur(
    uint8_t* rgba,
    const std::vector<uint8_t>& alpha
) {
    if (!rgba ||
        alpha.size() != static_cast<size_t>(width_ * height_) ||
        !backgroundBlurEnabled_) {
        return;
    }

    // Blur at quarter resolution for realtime performance.
    constexpr int scale = 4;

    const int blurWidth =
        std::max(1, (width_ + scale - 1) / scale);
    const int blurHeight =
        std::max(1, (height_ + scale - 1) / scale);

    const size_t blurPixels =
        static_cast<size_t>(blurWidth * blurHeight);

    // RGB is premultiplied by BACKGROUND confidence.
    //
    // This is important:
    // foreground colours never become part of the blur kernel.
    // We blur RGB * weight and weight independently, then normalize.
    blurWeightedRgb_.resize(blurPixels * 3);
    blurWeight_.resize(blurPixels);

    for (int y = 0; y < blurHeight; ++y) {
        const int sourceY = std::min(
            height_ - 1,
            y * scale + scale / 2
        );

        for (int x = 0; x < blurWidth; ++x) {
            const int sourceX = std::min(
                width_ - 1,
                x * scale + scale / 2
            );

            const size_t sourcePixel =
                static_cast<size_t>(
                    sourceY * width_ + sourceX);

            const size_t smallPixel =
                static_cast<size_t>(
                    y * blurWidth + x);

            const uint8_t* source =
                rgba + sourcePixel * 4;

            const float subject =
                static_cast<float>(alpha[sourcePixel]) / 255.0f;

            // Make uncertain subject-edge pixels contribute even less
            // to the background estimate.
            float backgroundWeight = 1.0f - subject;
            backgroundWeight *= backgroundWeight;

            blurWeight_[smallPixel] = backgroundWeight;

            blurWeightedRgb_[smallPixel * 3 + 0] =
                source[0] * backgroundWeight;
            blurWeightedRgb_[smallPixel * 3 + 1] =
                source[1] * backgroundWeight;
            blurWeightedRgb_[smallPixel * 3 + 2] =
                source[2] * backgroundWeight;
        }
    }

    const int strength = std::clamp(
        backgroundBlurStrength_.load(), 0, 100);

    const int radius = std::max(
        1,
        2 + strength * 10 / 100
    );

    // Blur weighted RGB and background weight together.
    blurHorizontalRgb_.resize(blurPixels * 3);
    blurHorizontalWeight_.resize(blurPixels);
    blurredRgb_.resize(blurPixels * 3);
    blurredWeight_.resize(blurPixels);

    // Horizontal pass.
    for (int y = 0; y < blurHeight; ++y) {
        float rgbSum[3] = {0.0f, 0.0f, 0.0f};
        float weightSum = 0.0f;

        for (int x = -radius; x <= radius; ++x) {
            const int sx =
                std::clamp(x, 0, blurWidth - 1);

            const size_t p =
                static_cast<size_t>(
                    y * blurWidth + sx);

            weightSum += blurWeight_[p];

            rgbSum[0] += blurWeightedRgb_[p * 3 + 0];
            rgbSum[1] += blurWeightedRgb_[p * 3 + 1];
            rgbSum[2] += blurWeightedRgb_[p * 3 + 2];
        }

        for (int x = 0; x < blurWidth; ++x) {
            const size_t p =
                static_cast<size_t>(
                    y * blurWidth + x);

            blurHorizontalWeight_[p] = weightSum;

            blurHorizontalRgb_[p * 3 + 0] = rgbSum[0];
            blurHorizontalRgb_[p * 3 + 1] = rgbSum[1];
            blurHorizontalRgb_[p * 3 + 2] = rgbSum[2];

            const int removeX =
                std::clamp(
                    x - radius,
                    0,
                    blurWidth - 1
                );

            const int addX =
                std::clamp(
                    x + radius + 1,
                    0,
                    blurWidth - 1
                );

            const size_t remove =
                static_cast<size_t>(
                    y * blurWidth + removeX);

            const size_t add =
                static_cast<size_t>(
                    y * blurWidth + addX);

            weightSum +=
                blurWeight_[add] - blurWeight_[remove];

            for (int channel = 0;
                 channel < 3;
                 ++channel) {
                rgbSum[channel] +=
                    blurWeightedRgb_[add * 3 + channel] -
                    blurWeightedRgb_[remove * 3 + channel];
            }
        }
    }

    // Vertical pass.
    for (int x = 0; x < blurWidth; ++x) {
        float rgbSum[3] = {0.0f, 0.0f, 0.0f};
        float weightSum = 0.0f;

        for (int y = -radius; y <= radius; ++y) {
            const int sy =
                std::clamp(y, 0, blurHeight - 1);

            const size_t p =
                static_cast<size_t>(
                    sy * blurWidth + x);

            weightSum += blurHorizontalWeight_[p];

            rgbSum[0] += blurHorizontalRgb_[p * 3 + 0];
            rgbSum[1] += blurHorizontalRgb_[p * 3 + 1];
            rgbSum[2] += blurHorizontalRgb_[p * 3 + 2];
        }

        for (int y = 0; y < blurHeight; ++y) {
            const size_t p =
                static_cast<size_t>(
                    y * blurWidth + x);

            blurredWeight_[p] = weightSum;

            blurredRgb_[p * 3 + 0] = rgbSum[0];
            blurredRgb_[p * 3 + 1] = rgbSum[1];
            blurredRgb_[p * 3 + 2] = rgbSum[2];

            const int removeY =
                std::clamp(
                    y - radius,
                    0,
                    blurHeight - 1
                );

            const int addY =
                std::clamp(
                    y + radius + 1,
                    0,
                    blurHeight - 1
                );

            const size_t remove =
                static_cast<size_t>(
                    removeY * blurWidth + x);

            const size_t add =
                static_cast<size_t>(
                    addY * blurWidth + x);

            weightSum +=
                blurHorizontalWeight_[add] -
                blurHorizontalWeight_[remove];

            for (int channel = 0;
                 channel < 3;
                 ++channel) {
                rgbSum[channel] +=
                    blurHorizontalRgb_[add * 3 + channel] -
                    blurHorizontalRgb_[remove * 3 + channel];
            }
        }
    }

    // Reconstruct normalized background colour.
    blurBackground_.resize(blurPixels * 3);

    for (size_t pixel = 0; pixel < blurPixels; ++pixel) {
        const float w = blurredWeight_[pixel];

        if (w > 0.001f) {
            for (int channel = 0; channel < 3; ++channel) {
                blurBackground_[pixel * 3 + channel] =
                    static_cast<uint8_t>(
                        std::clamp(
                            blurredRgb_[pixel * 3 + channel] / w,
                            0.0f,
                            255.0f
                        )
                    );
            }
        } else {
            // This normally only happens deep inside a large subject.
            // It is hidden by subject alpha anyway.
            blurBackground_[pixel * 3 + 0] = 0;
            blurBackground_[pixel * 3 + 1] = 0;
            blurBackground_[pixel * 3 + 2] = 0;
        }
    }

    // Composite with the ORIGINAL matched RVM alpha.
    //
    // No sqrt(), threshold or extra matte manipulation here.
    // Background reconstruction and subject matte are separate jobs.
    for (int y = 0; y < height_; ++y) {
        const int by =
            std::min(blurHeight - 1, y / scale);

        for (int x = 0; x < width_; ++x) {
            const int bx =
                std::min(blurWidth - 1, x / scale);

            const size_t pixel =
                static_cast<size_t>(y * width_ + x);

            const size_t backgroundPixel =
                static_cast<size_t>(
                    by * blurWidth + bx);

            uint8_t* foreground =
                rgba + pixel * 4;

            const uint8_t* blurredBackground =
                blurBackground_.data() +
                backgroundPixel * 3;

            const int subjectAlpha = alpha[pixel];
            const int backgroundAlpha =
                255 - subjectAlpha;

            for (int channel = 0;
                 channel < 3;
                 ++channel) {
                foreground[channel] =
                    static_cast<uint8_t>(
                        (
                            foreground[channel] *
                                subjectAlpha +
                            blurredBackground[channel] *
                                backgroundAlpha +
                            127
                        ) / 255
                    );
            }
        }
    }
}

void CaptureEngine::compositeBackground(
    uint8_t* rgba,
    const std::vector<uint8_t>& alpha
) {
    if (!rgba ||
        alpha.size() != static_cast<size_t>(width_ * height_)) {
        return;
    }

    std::lock_guard<std::mutex> backgroundLock(backgroundMutex_);
    if (!backgroundEnabled_ || backgroundPixels_.empty() ||
        backgroundWidth_ <= 0 || backgroundHeight_ <= 0) return;

    const size_t pixelCount = static_cast<size_t>(width_ * height_);
    if (backgroundWidth_ == width_ && backgroundHeight_ == height_) {
        for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
            uint8_t* foreground = rgba + pixel * 4;
            const uint8_t* background = backgroundPixels_.data() + pixel * 4;
            const int alphaValue = alpha[pixel];
            const int inverseAlpha = 255 - alphaValue;
            foreground[0] = static_cast<uint8_t>(
                (foreground[0] * alphaValue +
                 background[0] * inverseAlpha + 127) / 255);
            foreground[1] = static_cast<uint8_t>(
                (foreground[1] * alphaValue +
                 background[1] * inverseAlpha + 127) / 255);
            foreground[2] = static_cast<uint8_t>(
                (foreground[2] * alphaValue +
                 background[2] * inverseAlpha + 127) / 255);
        }
        return;
    }

    for (int y = 0; y < height_; ++y) {
        const int backgroundY = std::min(
            backgroundHeight_ - 1, y * backgroundHeight_ / height_);
        for (int x = 0; x < width_; ++x) {
            const int backgroundX = std::min(
                backgroundWidth_ - 1, x * backgroundWidth_ / width_);
            uint8_t* foreground = rgba + (y * width_ + x) * 4;
            const uint8_t* background = backgroundPixels_.data() +
                (backgroundY * backgroundWidth_ + backgroundX) * 4;
            const int alphaValue = alpha[y * width_ + x];
            const int inverseAlpha = 255 - alphaValue;
            for (int channel = 0; channel < 3; ++channel) {
                foreground[channel] = static_cast<uint8_t>(
                    (foreground[channel] * alphaValue +
                     background[channel] * inverseAlpha + 127) / 255);
            }
        }
    }
}
void CaptureEngine::publishFrame(
    std::shared_ptr<VideoFrame> frame
) {
    if (!frame || frame->rgba.empty()) return;
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        frame->sequence = ++latestFrameSequence_;
        latestFrame_ = std::move(frame);
    }
    frameCondition_.notify_all();
}

std::shared_ptr<const VideoFrame> CaptureEngine::latestFrame() const {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return latestFrame_;
}

std::shared_ptr<const VideoFrame> CaptureEngine::waitForLatestFrame(
    uint64_t afterSequence,
    std::chrono::milliseconds timeout
) {
    std::unique_lock<std::mutex> lock(frameMutex_);
    frameCondition_.wait_for(lock, timeout, [this, afterSequence] {
        return !running_ || latestFrameSequence_ > afterSequence;
    });
    if (!running_ || !latestFrame_ ||
        latestFrame_->sequence <= afterSequence) {
        return {};
    }
    return latestFrame_;
}

bool CaptureEngine::copyLatestFrame(
    uint8_t* destination,
    int destinationSize
) {
    if (!destination) return false;
    auto frame = latestFrame();
    if (!frame || destinationSize < static_cast<int>(frame->rgba.size())) {
        return false;
    }
    std::memcpy(destination, frame->rgba.data(), frame->rgba.size());
    return true;
}

bool CaptureEngine::waitAndCopyLatestFrame(
    uint8_t* destination,
    int destinationSize,
    uint64_t& sequence,
    uint64_t afterSequence,
    std::chrono::milliseconds timeout
) {
    if (!destination) return false;
    auto frame = waitForLatestFrame(afterSequence, timeout);
    if (!frame || destinationSize < static_cast<int>(frame->rgba.size())) {
        return false;
    }
    std::memcpy(destination, frame->rgba.data(), frame->rgba.size());
    sequence = frame->sequence;
    return true;
}

void CaptureEngine::wakeFrameWaiters() {
    frameCondition_.notify_all();
}

void CaptureEngine::captureLoop() {
    std::string pipelineString =
        "v4l2src device=" + device_ +
        " ! image/jpeg,width=" +
        std::to_string(width_) +
        ",height=" +
        std::to_string(height_) +
        ",framerate=" +
        std::to_string(fps_) +
        "/1"
        " ! jpegdec"
        " ! videoconvert"
        " ! video/x-raw,format=RGBA"
        " ! appsink name=sink"
        " max-buffers=1"
        " drop=true"
        " sync=false";

    GError* error = nullptr;

    GstElement* pipeline =
        gst_parse_launch(
            pipelineString.c_str(),
            &error
        );

    if (!pipeline) {
        if (error) {
            std::cerr
                << "GStreamer pipeline error: "
                << error->message
                << std::endl;

            g_error_free(error);
        }

        running_ = false;
        return;
    }

    GstElement* sink =
        gst_bin_get_by_name(
            GST_BIN(pipeline),
            "sink"
        );

    if (!sink) {
        gst_object_unref(pipeline);
        running_ = false;
        return;
    }

    gst_element_set_state(
        pipeline,
        GST_STATE_PLAYING
    );

    while (running_) {
        GstSample* sample =
            gst_app_sink_try_pull_sample(
                GST_APP_SINK(sink),
                100 * GST_MSECOND
            );

        if (!sample) {
            continue;
        }

        GstBuffer* buffer =
            gst_sample_get_buffer(sample);

        GstMapInfo map{};

        if (
            buffer &&
            gst_buffer_map(
                buffer,
                &map,
                GST_MAP_READ
            )
        ) {
            const size_t required =
                static_cast<size_t>(
                    width_ * height_ * 4
                );

            if (map.size >= required) {
                submitProcessingFrame(
                    map.data,
                    required
                );
            }

            gst_buffer_unmap(
                buffer,
                &map
            );
        }

        gst_sample_unref(sample);
    }

    gst_element_set_state(
        pipeline,
        GST_STATE_NULL
    );

    gst_object_unref(sink);
    gst_object_unref(pipeline);
}
