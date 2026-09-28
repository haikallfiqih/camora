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
    subjectAlpha_.clear();
    segmentationFrame_ = 0;

    running_ = true;

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
}

void CaptureEngine::setLowLightEnhancement(
    bool enabled,
    int strength
) {
    const int safeStrength = std::clamp(strength, 0, 100);
    lowLightEnabled_ = enabled;
    lowLightStrength_ = safeStrength;

    const double gamma = enabled
        ? 1.0 + (1.5 * safeStrength / 100.0)
        : 1.0;
    std::lock_guard<std::mutex> lock(effectMutex_);
    if (lowLightFilter_) {
        g_object_set(
            G_OBJECT(lowLightFilter_),
            "gamma",
            gamma,
            nullptr
        );
    }
}

bool CaptureEngine::configureSegmentationModel(
    const std::string& modelPath
) {
    return segmenter_.initialize(modelPath);
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
}

void CaptureEngine::compositeBackground(uint8_t* rgba) {
    if (!backgroundEnabled_ || !segmenter_.available()) return;

    const bool refreshMask = segmentationFrame_++ % 2 == 0 ||
        subjectAlpha_.size() != static_cast<size_t>(width_ * height_);
    if (refreshMask) {
        if (!segmenter_.segment(rgba, width_, height_, subjectMask_)) return;

        const int maskWidth = segmenter_.maskWidth();
        const int maskHeight = segmenter_.maskHeight();
        subjectAlpha_.resize(static_cast<size_t>(width_ * height_));
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
                const float top = subjectMask_[y0 * maskWidth + x0] *
                        (1.0f - fx) +
                    subjectMask_[y0 * maskWidth + x1] * fx;
                const float bottom = subjectMask_[y1 * maskWidth + x0] *
                        (1.0f - fx) +
                    subjectMask_[y1 * maskWidth + x1] * fx;
                const float probability = top * (1.0f - fy) + bottom * fy;
                const float confidence = std::clamp(
                    (probability - 0.20f) / 0.55f, 0.0f, 1.0f);
                const float feathered = confidence * confidence *
                    (3.0f - 2.0f * confidence);
                subjectAlpha_[y * width_ + x] = static_cast<uint8_t>(
                    feathered * 255.0f);
            }
        }
    }
    if (subjectAlpha_.empty()) return;

    std::lock_guard<std::mutex> lock(backgroundMutex_);
    if (!backgroundEnabled_ || backgroundPixels_.empty() ||
        backgroundWidth_ <= 0 || backgroundHeight_ <= 0) return;

    for (int y = 0; y < height_; ++y) {
        const int backgroundY = std::min(
            backgroundHeight_ - 1, y * backgroundHeight_ / height_);
        for (int x = 0; x < width_; ++x) {
            const int backgroundX = std::min(
                backgroundWidth_ - 1, x * backgroundWidth_ / width_);
            uint8_t* foreground = rgba + (y * width_ + x) * 4;
            const uint8_t* background = backgroundPixels_.data() +
                (backgroundY * backgroundWidth_ + backgroundX) * 4;
            const int alpha = subjectAlpha_[y * width_ + x];
            const int inverseAlpha = 255 - alpha;
            for (int channel = 0; channel < 3; ++channel) {
                foreground[channel] = static_cast<uint8_t>(
                    (foreground[channel] * alpha +
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
        " ! gamma name=lowlight"
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

    GstElement* lowLightFilter =
        gst_bin_get_by_name(
            GST_BIN(pipeline),
            "lowlight"
        );

    if (lowLightFilter) {
        const int strength = lowLightStrength_.load();
        const double gamma = lowLightEnabled_
            ? 1.0 + (1.5 * strength / 100.0)
            : 1.0;
        g_object_set(
            G_OBJECT(lowLightFilter),
            "gamma",
            gamma,
            nullptr
        );
        std::lock_guard<std::mutex> lock(effectMutex_);
        lowLightFilter_ = lowLightFilter;
    }

    GstElement* sink =
        gst_bin_get_by_name(
            GST_BIN(pipeline),
            "sink"
        );

    if (!sink) {
        {
            std::lock_guard<std::mutex> lock(effectMutex_);
            lowLightFilter_ = nullptr;
        }
        if (lowLightFilter) {
            gst_object_unref(lowLightFilter);
        }
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
                std::lock_guard<std::mutex>
                    lock(frameMutex_);

                std::memcpy(
                    latestFrame_.data(),
                    map.data,
                    required
                );

                compositeBackground(latestFrame_.data());
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

    {
        std::lock_guard<std::mutex> lock(effectMutex_);
        lowLightFilter_ = nullptr;
    }
    if (lowLightFilter) {
        gst_object_unref(lowLightFilter);
    }

    gst_object_unref(sink);
    gst_object_unref(pipeline);
}
