#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "background_segmenter.h"
#include "low_light_processor.h"

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

    void setBackgroundBlur(
        bool enabled,
        int strength
    );

    void setBackgroundRemoval(
        bool enabled
    );

    void setAutoFraming(
        bool enabled,
        int sensitivity
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
    void processingLoop();
    void submitProcessingFrame(const uint8_t* rgba, size_t size);
    void segmentationLoop();
    void submitSegmentationFrame(const uint8_t* rgba);
    void compositeBackground(
        uint8_t* rgba,
        const std::vector<uint8_t>& alpha
    );
    void compositeBackgroundBlur(
        uint8_t* rgba,
        const std::vector<uint8_t>& alpha
    );

    void compositeBackgroundRemoval(
        uint8_t* rgba,
        const std::vector<uint8_t>& alpha
    );

    void applyAutoFraming(
        uint8_t* rgba,
        const std::vector<float>& mask,
        int maskWidth,
        int maskHeight
    );

    void resetAutoFraming();

    std::string device_;

    int width_ = 0;
    int height_ = 0;
    int fps_ = 0;

    std::atomic<bool> running_{false};
    std::atomic<bool> lowLightEnabled_{false};
    std::atomic<int> lowLightStrength_{50};

    LowLightProcessor lowLightProcessor_;

    std::thread thread_;

    // Latest-frame processing worker.
    //
    // Capture never waits for effects. If processing is slower
    // than capture, the pending frame is replaced with the newest
    // frame instead of building latency.
    std::thread processingThread_;
    std::mutex processingMutex_;
    std::condition_variable processingCondition_;
    std::vector<uint8_t> processingInput_;
    bool processingPending_ = false;
    bool processingStop_ = false;

    BackgroundSegmenter segmenter_;
    std::atomic<bool> backgroundEnabled_{false};
    std::atomic<bool> backgroundBlurEnabled_{false};
    std::atomic<int> backgroundBlurStrength_{70};
    std::atomic<bool> backgroundRemovalEnabled_{false};
    std::atomic<bool> autoFramingEnabled_{false};
    std::atomic<int> autoFramingSensitivity_{50};

    // Smoothed framing state. Owned by the segmentation worker.
    bool autoFramingInitialized_ = false;
    float autoFrameCenterX_ = 0.5f;
    float autoFrameCenterY_ = 0.5f;
    float autoFrameZoom_ = 1.0f;
    std::mutex backgroundMutex_;
    std::vector<uint8_t> backgroundPixels_;
    int backgroundWidth_ = 0;
    int backgroundHeight_ = 0;
    std::vector<float> subjectMask_;

    // Used only for motion detection during edge refinement.
    // Never blended into the current matte, avoiding temporal ghosting.
    std::vector<float> previousSubjectMask_;

    std::mutex segmentationMutex_;
    std::condition_variable segmentationCondition_;
    std::thread segmentationThread_;
    std::vector<uint8_t> segmentationInput_;
    std::vector<uint8_t> segmentationFrame_;
    bool segmentationPending_ = false;
    bool segmentationStop_ = false;

    std::mutex frameMutex_;
    std::vector<uint8_t> latestFrame_;
};
