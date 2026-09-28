#pragma once

#include <cstdint>
#include <string>
#include <vector>

class BackgroundSegmenter {
public:
    BackgroundSegmenter();
    ~BackgroundSegmenter();

    BackgroundSegmenter(const BackgroundSegmenter&) = delete;
    BackgroundSegmenter& operator=(const BackgroundSegmenter&) = delete;

    bool initialize(const std::string& modelPath);
    bool segment(
        const uint8_t* rgba,
        int width,
        int height,
        std::vector<float>& mask
    );
    bool available() const { return interpreter_ != nullptr; }
    int maskWidth() const { return outputWidth_; }
    int maskHeight() const { return outputHeight_; }

private:
    void reset();

    void* library_ = nullptr;
    void* model_ = nullptr;
    void* options_ = nullptr;
    void* interpreter_ = nullptr;
    int inputWidth_ = 0;
    int inputHeight_ = 0;
    int outputWidth_ = 0;
    int outputHeight_ = 0;
    int outputChannels_ = 0;

    struct Api;
    Api* api_ = nullptr;
    std::vector<float> inputBuffer_;
};
