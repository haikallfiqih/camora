#include "background_segmenter.h"
#include "inference_backend.h"

#include "third_party/onnxruntime/include/onnxruntime_cxx_api.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <numeric>

namespace {
constexpr int kMaximumMattingDimension = 448;

int alignDimension(float dimension) {
    return std::max(32, static_cast<int>(std::round(dimension / 32.0f)) * 32);
}

size_t elementCount(const std::vector<int64_t>& shape) {
    return std::accumulate(
        shape.begin(), shape.end(), size_t{1},
        [](size_t count, int64_t dimension) {
            return count * static_cast<size_t>(dimension);
        });
}
}  // namespace

struct BackgroundSegmenter::Impl {
    Impl()
        : environment(ORT_LOGGING_LEVEL_FATAL, "CamoraRVM"),
          memory(Ort::MemoryInfo::CreateCpu(
              OrtArenaAllocator, OrtMemTypeDefault)) {}

    Ort::Env environment;
    Ort::SessionOptions options;
    InferenceInfo inferenceInfo;
    std::unique_ptr<Ort::Session> session;
    Ort::MemoryInfo memory;
    std::array<std::vector<float>, 4> recurrentData;
    std::array<std::vector<int64_t>, 4> recurrentShapes;
};

BackgroundSegmenter::BackgroundSegmenter() = default;
BackgroundSegmenter::~BackgroundSegmenter() = default;

void BackgroundSegmenter::shutdown() {
    reset();
}

void BackgroundSegmenter::reset() {
    impl_.reset();
    inputBuffer_.clear();
    inputWidth_ = 0;
    inputHeight_ = 0;
    frameWidth_ = 0;
    frameHeight_ = 0;
}

bool BackgroundSegmenter::available() const {
    return impl_ && impl_->session;
}

bool BackgroundSegmenter::configuredForFrame(int width, int height) const {
    return available() && frameWidth_ == width && frameHeight_ == height;
}

void BackgroundSegmenter::resetTemporalState() {
    if (!impl_) return;
    for (size_t index = 0; index < impl_->recurrentData.size(); ++index) {
        impl_->recurrentData[index] = {0.0f};
        impl_->recurrentShapes[index] = {1, 1, 1, 1};
    }
}

bool BackgroundSegmenter::initialize(
    const std::string& modelPath,
    int frameWidth,
    int frameHeight
) {
    reset();
    if (frameWidth <= 0 || frameHeight <= 0) return false;
    frameWidth_ = frameWidth;
    frameHeight_ = frameHeight;

    if (frameWidth >= frameHeight) {
        inputWidth_ = kMaximumMattingDimension;
        inputHeight_ = alignDimension(
            kMaximumMattingDimension * frameHeight /
            static_cast<float>(frameWidth));
    } else {
        inputHeight_ = kMaximumMattingDimension;
        inputWidth_ = alignDimension(
            kMaximumMattingDimension * frameWidth /
            static_cast<float>(frameHeight));
    }

    try {
        impl_ = std::make_unique<Impl>();

        impl_->inferenceInfo =
            InferenceBackendSelector::configure(
                impl_->options);

        impl_->session = std::make_unique<Ort::Session>(
            impl_->environment,
            modelPath.c_str(),
            impl_->options);

        std::cerr
            << "[Camora] RVM initialized with "
            << impl_->inferenceInfo.provider
            << std::endl;
        for (size_t index = 0; index < impl_->recurrentData.size(); ++index) {
            impl_->recurrentData[index] = {0.0f};
            impl_->recurrentShapes[index] = {1, 1, 1, 1};
        }
        inputBuffer_.resize(
            static_cast<size_t>(3 * inputWidth_ * inputHeight_));
        return true;
    } catch (const Ort::Exception& error) {
        std::cerr << "Could not initialize RVM: " << error.what() << std::endl;
        reset();
        return false;
    }
}

bool BackgroundSegmenter::segment(
    const uint8_t* rgba,
    int width,
    int height,
    std::vector<float>& mask
) {
    if (!available() || !rgba || width != inputWidth_ ||
        height != inputHeight_) {
        return false;
    }

    const size_t planeSize = static_cast<size_t>(width * height);
    for (size_t pixel = 0; pixel < planeSize; ++pixel) {
        inputBuffer_[pixel] = rgba[pixel * 4] / 255.0f;
        inputBuffer_[planeSize + pixel] = rgba[pixel * 4 + 1] / 255.0f;
        inputBuffer_[planeSize * 2 + pixel] = rgba[pixel * 4 + 2] / 255.0f;
    }

    try {
        const std::array<int64_t, 4> sourceShape = {1, 3, height, width};
        std::vector<Ort::Value> inputs;
        inputs.reserve(6);
        inputs.emplace_back(Ort::Value::CreateTensor<float>(
            impl_->memory, inputBuffer_.data(), inputBuffer_.size(),
            sourceShape.data(), sourceShape.size()));
        for (size_t index = 0; index < impl_->recurrentData.size(); ++index) {
            inputs.emplace_back(Ort::Value::CreateTensor<float>(
                impl_->memory,
                impl_->recurrentData[index].data(),
                impl_->recurrentData[index].size(),
                impl_->recurrentShapes[index].data(),
                impl_->recurrentShapes[index].size()));
        }
        float downsampleRatio = 1.0f;
        const std::array<int64_t, 1> ratioShape = {1};
        inputs.emplace_back(Ort::Value::CreateTensor<float>(
            impl_->memory, &downsampleRatio, 1,
            ratioShape.data(), ratioShape.size()));

        constexpr std::array<const char*, 6> inputNames = {
            "src", "r1i", "r2i", "r3i", "r4i", "downsample_ratio"};
        constexpr std::array<const char*, 5> outputNames = {
            "pha", "r1o", "r2o", "r3o", "r4o"};
        std::vector<Ort::Value> outputs = impl_->session->Run(
            Ort::RunOptions{nullptr}, inputNames.data(), inputs.data(),
            inputs.size(), outputNames.data(), outputNames.size());

        const auto alphaShape = outputs[0]
            .GetTensorTypeAndShapeInfo().GetShape();
        const size_t alphaCount = elementCount(alphaShape);
        if (alphaCount != planeSize) return false;
        const float* alpha = outputs[0].GetTensorData<float>();
        mask.assign(alpha, alpha + alphaCount);

        for (size_t index = 0; index < impl_->recurrentData.size(); ++index) {
            impl_->recurrentShapes[index] = outputs[index + 1]
                .GetTensorTypeAndShapeInfo().GetShape();
            const size_t count = elementCount(impl_->recurrentShapes[index]);
            const float* values = outputs[index + 1].GetTensorData<float>();
            impl_->recurrentData[index].assign(values, values + count);
        }
        return true;
    } catch (const Ort::Exception& error) {
        std::cerr << "RVM inference failed: " << error.what() << std::endl;
        return false;
    }
}
