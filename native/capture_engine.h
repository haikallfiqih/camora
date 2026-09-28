#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "background_segmenter.h"

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

    bool configureSegmentationModel(const std::string& modelPath);

    void setBackgroundReplacement(
        bool enabled,
        std::vector<uint8_t> pixels,
        int width,
        int height
    );

    bool backgroundReplacementAvailable() const {
        return segmenter_.configuredForFrame(width_, height_);
    }

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
    void segmentationLoop();
    void submitSegmentationFrame(const uint8_t* rgba);
    void compositeBackground(uint8_t* rgba);

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

    BackgroundSegmenter segmenter_;
    std::atomic<bool> backgroundEnabled_{false};
    std::mutex backgroundMutex_;
    std::vector<uint8_t> backgroundPixels_;
    int backgroundWidth_ = 0;
    int backgroundHeight_ = 0;
    std::vector<float> subjectMask_;
    std::mutex segmentationMutex_;
    std::condition_variable segmentationCondition_;
    std::thread segmentationThread_;
    std::vector<uint8_t> segmentationInput_;
    bool segmentationPending_ = false;
    bool segmentationStop_ = false;
    std::mutex alphaMutex_;
    std::vector<uint8_t> subjectAlpha_;
    int segmentationFrame_ = 0;

    std::mutex frameMutex_;
    std::vector<uint8_t> latestFrame_;
};
