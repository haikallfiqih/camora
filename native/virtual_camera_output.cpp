#include "virtual_camera_output.h"

#include "capture_engine.h"

#include <gst/app/gstappsrc.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

VirtualCameraOutput::VirtualCameraOutput(CaptureEngine& capture)
    : capture_(capture) {}

VirtualCameraOutput::~VirtualCameraOutput() {
    stop();
}

bool VirtualCameraOutput::start(
    int width,
    int height,
    int fps,
    std::string& error
) {
    stop();

    if (!capture_.running() || width <= 0 || height <= 0 || fps <= 0) {
        error = "Start the camera before starting Camora Virtual Camera.";
        return false;
    }

    g_autoptr(GError) pipelineError = nullptr;
    pipeline_ = gst_parse_launch(
        "appsrc name=camora-source is-live=true format=time do-timestamp=true "
        "block=false max-buffers=1 leaky-type=downstream ! "
        "videoconvert ! pipewiresink name=camora-sink "
        "mode=provide client-name=Camora",
        &pipelineError);

    if (!pipeline_) {
        error = pipelineError ? pipelineError->message
                              : "PipeWire output could not be created.";
        return false;
    }

    GstElement* sink =
        gst_bin_get_by_name(GST_BIN(pipeline_), "camora-sink");
    if (!sink) {
        error = "Camora PipeWire output could not be created.";
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        return false;
    }
    GstStructure* properties = gst_structure_new(
        "properties",
        "media.class", G_TYPE_STRING, "Video/Source",
        "media.role", G_TYPE_STRING, "Camera",
        "node.name", G_TYPE_STRING, "camora-virtual-camera",
        "node.description", G_TYPE_STRING, "Camora Virtual Camera",
        "media.name", G_TYPE_STRING, "Camora Virtual Camera",
        nullptr);
    g_object_set(sink, "stream-properties", properties, nullptr);
    gst_structure_free(properties);
    gst_object_unref(sink);

    source_ = gst_bin_get_by_name(GST_BIN(pipeline_), "camora-source");
    if (!source_) {
        error = "Camora output source could not be created.";
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        return false;
    }

    width_ = width;
    height_ = height;
    fps_ = fps;
    const guint frameSize =
        static_cast<guint>(width_) * static_cast<guint>(height_) * 4;

    g_autoptr(GstCaps) caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGBA",
        "width", G_TYPE_INT, width_,
        "height", G_TYPE_INT, height_,
        "framerate", GST_TYPE_FRACTION, fps_, 1,
        nullptr);
    gst_app_src_set_caps(GST_APP_SRC(source_), caps);

    bufferPool_ = gst_buffer_pool_new();
    GstStructure* poolConfig =
        gst_buffer_pool_get_config(bufferPool_);
    gst_buffer_pool_config_set_params(
        poolConfig, caps, frameSize, 2, 4);
    if (!gst_buffer_pool_set_config(bufferPool_, poolConfig) ||
        !gst_buffer_pool_set_active(bufferPool_, TRUE)) {
        error = "Camora output buffers could not be initialized.";
        stop();
        return false;
    }

    const GstStateChangeReturn state =
        gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (state == GST_STATE_CHANGE_FAILURE) {
        error = "PipeWire rejected the Camora camera source.";
        stop();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        statusMessage_ = "Available to PipeWire camera applications";
    }
    running_ = true;
    worker_ = std::thread(&VirtualCameraOutput::outputLoop, this);
    return true;
}

void VirtualCameraOutput::stop() {
    running_ = false;
    capture_.wakeFrameWaiters();
    if (worker_.joinable()) worker_.join();

    if (source_) {
        gst_app_src_end_of_stream(GST_APP_SRC(source_));
        gst_object_unref(source_);
        source_ = nullptr;
    }
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    if (bufferPool_) {
        gst_buffer_pool_set_active(bufferPool_, FALSE);
        gst_object_unref(bufferPool_);
        bufferPool_ = nullptr;
    }

    std::lock_guard<std::mutex> lock(statusMutex_);
    statusMessage_.clear();
}

std::string VirtualCameraOutput::statusMessage() const {
    std::lock_guard<std::mutex> lock(statusMutex_);
    return statusMessage_;
}

void VirtualCameraOutput::setError(const std::string& message) {
    std::lock_guard<std::mutex> lock(statusMutex_);
    statusMessage_ = message;
}

void VirtualCameraOutput::outputLoop() {
    using Clock = std::chrono::steady_clock;

    const GstClockTime frameDuration =
        GST_SECOND / static_cast<GstClockTime>(std::max(1, fps_));
    const size_t frameSize =
        static_cast<size_t>(width_) * height_ * 4;
    uint64_t lastSequence = 0;
    uint64_t intervalFirstSequence = 0;
    uint64_t pushed = 0;
    uint64_t dropped = 0;
    uint64_t pushCount = 0;
    std::chrono::nanoseconds totalPushTime{0};
    std::chrono::nanoseconds maxPushTime{0};
    auto metricsStart = Clock::now();
    std::chrono::nanoseconds totalCopyTime{0};
    std::chrono::nanoseconds maxCopyTime{0};

    while (running_) {
        GstBufferPoolAcquireParams acquireParams{};
        acquireParams.flags = GST_BUFFER_POOL_ACQUIRE_FLAG_DONTWAIT;

        GstBuffer* buffer = nullptr;
        const GstFlowReturn acquireResult =
            gst_buffer_pool_acquire_buffer(
                bufferPool_, &buffer, &acquireParams);
        if (acquireResult != GST_FLOW_OK || !buffer) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
            gst_buffer_unref(buffer);
            continue;
        }

        const auto frame = capture_.waitForLatestFrame(
            lastSequence, std::chrono::milliseconds(100));
        const bool copied = frame &&
            frame->rgba.size() == frameSize && map.size >= frameSize;
        const uint64_t sequence = copied ? frame->sequence : lastSequence;
        if (copied) {
            const auto copyStart = Clock::now();
            std::memcpy(map.data, frame->rgba.data(), frameSize);
            const auto copyTime = std::chrono::duration_cast<
                std::chrono::nanoseconds>(Clock::now() - copyStart);
            totalCopyTime += copyTime;
            maxCopyTime = std::max(maxCopyTime, copyTime);
        }
        gst_buffer_unmap(buffer, &map);

        if (!running_) {
            gst_buffer_unref(buffer);
            break;
        }
        if (!copied) {
            gst_buffer_unref(buffer);
            continue;
        }

        if (lastSequence != 0 && sequence > lastSequence + 1) {
            dropped += sequence - lastSequence - 1;
        }
        if (intervalFirstSequence == 0) intervalFirstSequence = sequence;
        lastSequence = sequence;

        GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
        GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
        GST_BUFFER_DURATION(buffer) = frameDuration;

        const auto pushStart = Clock::now();
        const GstFlowReturn pushResult =
            gst_app_src_push_buffer(GST_APP_SRC(source_), buffer);
        const auto pushTime = Clock::now() - pushStart;
        totalPushTime +=
            std::chrono::duration_cast<std::chrono::nanoseconds>(pushTime);
        maxPushTime = std::max(
            maxPushTime,
            std::chrono::duration_cast<std::chrono::nanoseconds>(pushTime));
        ++pushCount;

        if (pushResult != GST_FLOW_OK &&
            pushResult != GST_FLOW_FLUSHING) {
            setError(
                "The PipeWire camera stream stopped accepting frames.");
            running_ = false;
            break;
        }
        ++pushed;

        const auto now = Clock::now();
        const auto elapsed = now - metricsStart;
        if (elapsed >= std::chrono::seconds(1)) {
            const double seconds =
                std::chrono::duration<double>(elapsed).count();
            const uint64_t submitted = intervalFirstSequence == 0
                ? 0
                : lastSequence - intervalFirstSequence + 1;
            const double averagePushMs = pushCount == 0
                ? 0.0
                : std::chrono::duration<double, std::milli>(
                      totalPushTime).count() / pushCount;
            const double maxPushMs =
                std::chrono::duration<double, std::milli>(
                    maxPushTime).count();
            const double averageCopyMs = pushCount == 0
                ? 0.0
                : std::chrono::duration<double, std::milli>(
                      totalCopyTime).count() / pushCount;
            const double maxCopyMs =
                std::chrono::duration<double, std::milli>(
                    maxCopyTime).count();

            std::clog
                << "[Camora VC] submitted=" << submitted
                << " pushed=" << pushed
                << " dropped=" << dropped
                << " fps=" << (pushed / seconds)
                << " copy_avg=" << averageCopyMs << "ms"
                << " copy_max=" << maxCopyMs << "ms"
                << " push_avg=" << averagePushMs << "ms"
                << " push_max=" << maxPushMs << "ms"
                << std::endl;

            metricsStart = now;
            intervalFirstSequence = lastSequence + 1;
            pushed = 0;
            dropped = 0;
            pushCount = 0;
            totalPushTime = std::chrono::nanoseconds::zero();
            maxPushTime = std::chrono::nanoseconds::zero();
            totalCopyTime = std::chrono::nanoseconds::zero();
            maxCopyTime = std::chrono::nanoseconds::zero();
        }
    }
}
