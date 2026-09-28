#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class BackgroundSegmenter {
public:
    BackgroundSegmenter();
    ~BackgroundSegmenter();

    BackgroundSegmenter(const BackgroundSegmenter&) = delete;
    BackgroundSegmenter& operator=(const BackgroundSegmenter&) = delete;

    bool initialize(
        const std::string& modelPath,
        int frameWidth,
        int frameHeight
    );
    bool segment(
        const uint8_t* rgba,
        int width,
        int height,
        std::vector<float>& mask
    );
    bool available() const;
    const char* backendName() const;
    bool configuredForFrame(int width, int height) const;
    void resetTemporalState();
    int maskWidth() const { return inputWidth_; }
    int maskHeight() const { return inputHeight_; }

    // Explicitly release the ONNX Runtime session and GPU resources.
    // Must only be called when no segmentation worker is using this object.
    void shutdown();

private:
    void reset();

    struct Impl;
    std::unique_ptr<Impl> impl_;
    int inputWidth_ = 0;
    int inputHeight_ = 0;
    int frameWidth_ = 0;
    int frameHeight_ = 0;
    std::vector<float> inputBuffer_;
};
