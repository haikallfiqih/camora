#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class CaptureEngine {
public:
    CaptureEngine();
    ~CaptureEngine();

    bool start(
        const std::string& device,
        int width,
        int height,
        int fps
    );

    void stop();

    void setLowLightEnhancement(
        bool enabled,
        int strength
    );

    bool copyLatestFrame(
        uint8_t* destination,
        int destinationSize
    );

    int width() const { return width_; }
    int height() const { return height_; }
    int frameSize() const {
        return width_ * height_ * 4;
    }

    bool running() const {
        return running_;
    }

private:
    void captureLoop();

    std::string device_;

    int width_ = 0;
    int height_ = 0;
    int fps_ = 0;

    std::atomic<bool> running_{false};
    std::atomic<bool> lowLightEnabled_{false};
    std::atomic<int> lowLightStrength_{50};

    std::thread thread_;

    std::mutex effectMutex_;
    void* lowLightFilter_ = nullptr;

    std::mutex frameMutex_;
    std::vector<uint8_t> latestFrame_;
};
