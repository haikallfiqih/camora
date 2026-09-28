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

    latestFrame_.resize(
        width_ * height_ * 4
    );

    subjectMask_.clear();
    previousSubjectMask_.clear();
    segmentationFrame_.clear();

    {
        std::lock_guard<std::mutex> lock(processingMutex_);
        processingPending_ = false;
        processingStop_ = false;
        processingInput_.clear();
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

void CaptureEngine::submitProcessingFrame(
    const uint8_t* rgba,
    size_t size
) {
    if (!rgba || size == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(processingMutex_);

    if (processingStop_) {
        return;
    }

    // One pending slot only.
    //
    // If processing is behind, overwrite the old pending frame
    // with the newest camera frame. This intentionally drops
    // processing frames instead of accumulating latency.
    if (processingInput_.size() != size) {
        processingInput_.resize(size);
    }

    std::memcpy(
        processingInput_.data(),
        rgba,
        size
    );

    processingPending_ = true;
    processingCondition_.notify_one();
}

void CaptureEngine::processingLoop() {
    std::vector<uint8_t> frame;

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

            frame.swap(processingInput_);
            processingPending_ = false;
        }

        if (frame.empty()) {
            continue;
        }

        // Heavy processing happens WITHOUT frameMutex_.
        lowLightProcessor_.process(
            frame.data(),
            width_,
            height_
        );

        const bool segmentationEffectEnabled =
            backgroundEnabled_ ||
            backgroundBlurEnabled_ ||
            backgroundRemovalEnabled_;

        if (segmentationEffectEnabled && segmenter_.available()) {
            submitSegmentationFrame(frame.data());
        }

        // When background replacement is enabled, the segmentation
        // worker owns publishing because it has the mask matched to
        // this frame. Publishing here as well would alternate between
        // raw and composited frames, causing background flicker.
        if (!backgroundEnabled_ &&
            !backgroundBlurEnabled_ &&
            !backgroundRemovalEnabled_) {
            std::lock_guard<std::mutex> lock(frameMutex_);

            if (latestFrame_.size() == frame.size()) {
                std::memcpy(
                    latestFrame_.data(),
                    frame.data(),
                    frame.size()
                );
            }
        }
    }
}

void CaptureEngine::submitSegmentationFrame(const uint8_t* rgba) {
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
            const uint8_t* source = rgba + (sourceY * width_ + sourceX) * 4;
            uint8_t* destination = segmentationInput_.data() +
                (y * sampleWidth + x) * 4;
            destination[0] = source[0];
            destination[1] = source[1];
            destination[2] = source[2];
            destination[3] = 255;
        }
    }
    const size_t fullFrameSize =
        static_cast<size_t>(width_ * height_ * 4);

    segmentationFrame_.assign(
        rgba,
        rgba + fullFrameSize
    );

    segmentationPending_ = true;
    lock.unlock();
    segmentationCondition_.notify_one();
}

void CaptureEngine::segmentationLoop() {
    std::vector<uint8_t> input;
    std::vector<uint8_t> sourceFrame;

    while (true) {
        {
            std::unique_lock<std::mutex> lock(segmentationMutex_);
            segmentationCondition_.wait(lock, [this] {
                return segmentationPending_ || segmentationStop_;
            });
            if (segmentationStop_) return;
            input.swap(segmentationInput_);
            sourceFrame.swap(segmentationFrame_);
            segmentationPending_ = false;
        }

        if ((!backgroundEnabled_ &&
             !backgroundBlurEnabled_ &&
             !backgroundRemovalEnabled_) ||
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

        std::vector<float> refinedMask(subjectMask_.size());

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

            refinedMask[i] = refined;
        }

        // Save CURRENT raw RVM mask only for motion detection next frame.
        // Do not save refinedMask here.
        previousSubjectMask_ = subjectMask_;

        std::vector<uint8_t> alpha(static_cast<size_t>(width_ * height_));
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
                const float top = refinedMask[y0 * maskWidth + x0] *
                        (1.0f - fx) +
                    refinedMask[y0 * maskWidth + x1] * fx;
                const float bottom = refinedMask[y1 * maskWidth + x0] *
                        (1.0f - fx) +
                    refinedMask[y1 * maskWidth + x1] * fx;
                const float probability = top * (1.0f - fy) + bottom * fy;
                const float confidence = std::clamp(
                    (probability - 0.03f) / 0.94f, 0.0f, 1.0f);
                const float feathered = confidence * confidence *
                    (3.0f - 2.0f * confidence);
                alpha[y * width_ + x] = static_cast<uint8_t>(
                    feathered * 255.0f);
            }
        }

        if (sourceFrame.size() !=
            static_cast<size_t>(width_ * height_ * 4)) {
            continue;
        }

        if (backgroundEnabled_) {
            compositeBackground(
                sourceFrame.data(),
                alpha
            );
        } else if (backgroundBlurEnabled_) {
            compositeBackgroundBlur(
                sourceFrame.data(),
                alpha
            );
        } else if (backgroundRemovalEnabled_) {
            compositeBackgroundRemoval(
                sourceFrame.data(),
                alpha
            );
        }

        {
            std::lock_guard<std::mutex> lock(frameMutex_);

            if (latestFrame_.size() == sourceFrame.size()) {
                std::memcpy(
                    latestFrame_.data(),
                    sourceFrame.data(),
                    sourceFrame.size()
                );
            }
        }
    }
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
    std::vector<float> weightedRgb(blurPixels * 3, 0.0f);
    std::vector<float> weight(blurPixels, 0.0f);

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

            weight[smallPixel] = backgroundWeight;

            weightedRgb[smallPixel * 3 + 0] =
                source[0] * backgroundWeight;
            weightedRgb[smallPixel * 3 + 1] =
                source[1] * backgroundWeight;
            weightedRgb[smallPixel * 3 + 2] =
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
    std::vector<float> horizontalRgb(blurPixels * 3, 0.0f);
    std::vector<float> horizontalWeight(blurPixels, 0.0f);

    std::vector<float> blurredRgb(blurPixels * 3, 0.0f);
    std::vector<float> blurredWeight(blurPixels, 0.0f);

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

            weightSum += weight[p];

            rgbSum[0] += weightedRgb[p * 3 + 0];
            rgbSum[1] += weightedRgb[p * 3 + 1];
            rgbSum[2] += weightedRgb[p * 3 + 2];
        }

        for (int x = 0; x < blurWidth; ++x) {
            const size_t p =
                static_cast<size_t>(
                    y * blurWidth + x);

            horizontalWeight[p] = weightSum;

            horizontalRgb[p * 3 + 0] = rgbSum[0];
            horizontalRgb[p * 3 + 1] = rgbSum[1];
            horizontalRgb[p * 3 + 2] = rgbSum[2];

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
                weight[add] - weight[remove];

            for (int channel = 0;
                 channel < 3;
                 ++channel) {
                rgbSum[channel] +=
                    weightedRgb[add * 3 + channel] -
                    weightedRgb[remove * 3 + channel];
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

            weightSum += horizontalWeight[p];

            rgbSum[0] += horizontalRgb[p * 3 + 0];
            rgbSum[1] += horizontalRgb[p * 3 + 1];
            rgbSum[2] += horizontalRgb[p * 3 + 2];
        }

        for (int y = 0; y < blurHeight; ++y) {
            const size_t p =
                static_cast<size_t>(
                    y * blurWidth + x);

            blurredWeight[p] = weightSum;

            blurredRgb[p * 3 + 0] = rgbSum[0];
            blurredRgb[p * 3 + 1] = rgbSum[1];
            blurredRgb[p * 3 + 2] = rgbSum[2];

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
                horizontalWeight[add] -
                horizontalWeight[remove];

            for (int channel = 0;
                 channel < 3;
                 ++channel) {
                rgbSum[channel] +=
                    horizontalRgb[add * 3 + channel] -
                    horizontalRgb[remove * 3 + channel];
            }
        }
    }

    // Reconstruct normalized background colour.
    std::vector<uint8_t> background(blurPixels * 3);

    for (size_t pixel = 0; pixel < blurPixels; ++pixel) {
        const float w = blurredWeight[pixel];

        if (w > 0.001f) {
            for (int channel = 0; channel < 3; ++channel) {
                background[pixel * 3 + channel] =
                    static_cast<uint8_t>(
                        std::clamp(
                            blurredRgb[pixel * 3 + channel] / w,
                            0.0f,
                            255.0f
                        )
                    );
            }
        } else {
            // This normally only happens deep inside a large subject.
            // It is hidden by subject alpha anyway.
            background[pixel * 3 + 0] = 0;
            background[pixel * 3 + 1] = 0;
            background[pixel * 3 + 2] = 0;
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
                background.data() +
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
bool CaptureEngine::copyLatestFrame(
    uint8_t* destination,
    int destinationSize
) {
    if (!destination) {
        return false;
    }

    const int required =
        width_ * height_ * 4;

    if (destinationSize < required) {
        return false;
    }

    std::lock_guard<std::mutex> lock(
        frameMutex_
    );

    if (latestFrame_.empty()) {
        return false;
    }

    std::memcpy(
        destination,
        latestFrame_.data(),
        required
    );

    return true;
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
