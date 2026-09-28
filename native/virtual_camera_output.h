#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include <gst/gst.h>

class CaptureEngine;

class VirtualCameraOutput {
public:
    explicit VirtualCameraOutput(CaptureEngine& capture);
    ~VirtualCameraOutput();

    bool start(int width, int height, int fps, std::string& error);
    void stop();

    bool running() const { return running_; }
    std::string statusMessage() const;

private:
    void outputLoop();
    void setError(const std::string& message);

    CaptureEngine& capture_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    GstElement* pipeline_ = nullptr;
    GstElement* source_ = nullptr;
    GstBufferPool* bufferPool_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int fps_ = 0;
    mutable std::mutex statusMutex_;
    std::string statusMessage_;
};
