#include "capture_engine.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <algorithm>
#include <cmath>
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

    running_ = true;

    thread_ = std::thread(
        &CaptureEngine::captureLoop,
        this
    );

    return true;
}

void CaptureEngine::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (thread_.joinable()) {
        thread_.join();
    }
}

void CaptureEngine::setLowLightEnhancement(
    bool enabled,
    int strength
) {
    lowLightEnabled_ = enabled;
    lowLightStrength_ = std::clamp(strength, 0, 100);
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
                std::lock_guard<std::mutex>
                    lock(frameMutex_);

                std::memcpy(
                    latestFrame_.data(),
                    map.data,
                    required
                );

                if (lowLightEnabled_) {
                    const int strength = lowLightStrength_.load();
                    const double gamma = 1.0 - (0.55 * strength / 100.0);
                    uint8_t lookup[256];
                    for (int value = 0; value < 256; ++value) {
                        lookup[value] = static_cast<uint8_t>(std::round(
                            std::pow(value / 255.0, gamma) * 255.0));
                    }
                    for (size_t offset = 0; offset < required; offset += 4) {
                        latestFrame_[offset] = lookup[latestFrame_[offset]];
                        latestFrame_[offset + 1] = lookup[latestFrame_[offset + 1]];
                        latestFrame_[offset + 2] = lookup[latestFrame_[offset + 2]];
                    }
                }
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
