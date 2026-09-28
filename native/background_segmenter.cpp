#include "background_segmenter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <dlfcn.h>
#include <iostream>

namespace {
struct TfLiteModel;
struct TfLiteInterpreterOptions;
struct TfLiteInterpreter;
struct TfLiteTensor;

enum TfLiteStatus { kTfLiteOk = 0 };
enum TfLiteType { kTfLiteNoType = 0, kTfLiteFloat32 = 1 };

template <typename T>
bool loadSymbol(void* library, const char* name, T& target) {
    target = reinterpret_cast<T>(dlsym(library, name));
    if (!target) {
        std::cerr << "TensorFlow Lite symbol unavailable: " << name << std::endl;
        return false;
    }
    return true;
}
}  // namespace

struct BackgroundSegmenter::Api {
    TfLiteModel* (*modelCreateFromFile)(const char*);
    void (*modelDelete)(TfLiteModel*);
    TfLiteInterpreterOptions* (*optionsCreate)();
    void (*optionsDelete)(TfLiteInterpreterOptions*);
    void (*optionsSetNumThreads)(TfLiteInterpreterOptions*, int32_t);
    TfLiteInterpreter* (*interpreterCreate)(
        const TfLiteModel*,
        const TfLiteInterpreterOptions*
    );
    void (*interpreterDelete)(TfLiteInterpreter*);
    TfLiteStatus (*allocateTensors)(TfLiteInterpreter*);
    TfLiteStatus (*invoke)(TfLiteInterpreter*);
    TfLiteTensor* (*getInputTensor)(TfLiteInterpreter*, int32_t);
    const TfLiteTensor* (*getOutputTensor)(const TfLiteInterpreter*, int32_t);
    TfLiteType (*tensorType)(const TfLiteTensor*);
    int32_t (*tensorNumDims)(const TfLiteTensor*);
    int32_t (*tensorDim)(const TfLiteTensor*, int32_t);
    size_t (*tensorByteSize)(const TfLiteTensor*);
    TfLiteStatus (*tensorCopyFromBuffer)(TfLiteTensor*, const void*, size_t);
    TfLiteStatus (*tensorCopyToBuffer)(const TfLiteTensor*, void*, size_t);
};

BackgroundSegmenter::BackgroundSegmenter() = default;

BackgroundSegmenter::~BackgroundSegmenter() {
    reset();
}

void BackgroundSegmenter::reset() {
    if (api_) {
        if (interpreter_) {
            api_->interpreterDelete(
                reinterpret_cast<TfLiteInterpreter*>(interpreter_)
            );
        }
        if (options_) {
            api_->optionsDelete(
                reinterpret_cast<TfLiteInterpreterOptions*>(options_)
            );
        }
        if (model_) {
            api_->modelDelete(reinterpret_cast<TfLiteModel*>(model_));
        }
    }
    delete api_;
    api_ = nullptr;
    interpreter_ = nullptr;
    options_ = nullptr;
    model_ = nullptr;
    if (library_) {
        dlclose(library_);
        library_ = nullptr;
    }
    inputBuffer_.clear();
    inputWidth_ = inputHeight_ = outputWidth_ = outputHeight_ = 0;
    outputChannels_ = 0;
}

bool BackgroundSegmenter::initialize(const std::string& modelPath) {
    reset();
    library_ = dlopen("libtensorflow-lite.so.2", RTLD_NOW | RTLD_LOCAL);
    if (!library_) {
        library_ = dlopen("libtensorflow-lite.so.2.14.1", RTLD_NOW | RTLD_LOCAL);
    }
    if (!library_) {
        library_ = dlopen("libtensorflow-lite.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (!library_) {
        std::cerr << "TensorFlow Lite runtime unavailable: " << dlerror()
                  << std::endl;
        return false;
    }

    api_ = new Api{};
    bool loaded = true;
#define LOAD(field, symbol) loaded = loadSymbol(library_, symbol, api_->field) && loaded
    LOAD(modelCreateFromFile, "TfLiteModelCreateFromFile");
    LOAD(modelDelete, "TfLiteModelDelete");
    LOAD(optionsCreate, "TfLiteInterpreterOptionsCreate");
    LOAD(optionsDelete, "TfLiteInterpreterOptionsDelete");
    LOAD(optionsSetNumThreads, "TfLiteInterpreterOptionsSetNumThreads");
    LOAD(interpreterCreate, "TfLiteInterpreterCreate");
    LOAD(interpreterDelete, "TfLiteInterpreterDelete");
    LOAD(allocateTensors, "TfLiteInterpreterAllocateTensors");
    LOAD(invoke, "TfLiteInterpreterInvoke");
    LOAD(getInputTensor, "TfLiteInterpreterGetInputTensor");
    LOAD(getOutputTensor, "TfLiteInterpreterGetOutputTensor");
    LOAD(tensorType, "TfLiteTensorType");
    LOAD(tensorNumDims, "TfLiteTensorNumDims");
    LOAD(tensorDim, "TfLiteTensorDim");
    LOAD(tensorByteSize, "TfLiteTensorByteSize");
    LOAD(tensorCopyFromBuffer, "TfLiteTensorCopyFromBuffer");
    LOAD(tensorCopyToBuffer, "TfLiteTensorCopyToBuffer");
#undef LOAD
    if (!loaded) {
        reset();
        return false;
    }

    model_ = api_->modelCreateFromFile(modelPath.c_str());
    options_ = api_->optionsCreate();
    if (!model_ || !options_) {
        reset();
        return false;
    }
    api_->optionsSetNumThreads(
        reinterpret_cast<TfLiteInterpreterOptions*>(options_), 2
    );
    interpreter_ = api_->interpreterCreate(
        reinterpret_cast<const TfLiteModel*>(model_),
        reinterpret_cast<const TfLiteInterpreterOptions*>(options_)
    );
    if (!interpreter_ || api_->allocateTensors(
            reinterpret_cast<TfLiteInterpreter*>(interpreter_)
        ) != kTfLiteOk) {
        reset();
        return false;
    }

    TfLiteTensor* input = api_->getInputTensor(
        reinterpret_cast<TfLiteInterpreter*>(interpreter_), 0
    );
    const TfLiteTensor* output = api_->getOutputTensor(
        reinterpret_cast<const TfLiteInterpreter*>(interpreter_), 0
    );
    if (!input || !output || api_->tensorType(input) != kTfLiteFloat32 ||
        api_->tensorType(output) != kTfLiteFloat32 ||
        api_->tensorNumDims(input) != 4 || api_->tensorNumDims(output) != 4) {
        std::cerr << "Unsupported selfie segmentation model tensors" << std::endl;
        reset();
        return false;
    }

    inputHeight_ = api_->tensorDim(input, 1);
    inputWidth_ = api_->tensorDim(input, 2);
    outputHeight_ = api_->tensorDim(output, 1);
    outputWidth_ = api_->tensorDim(output, 2);
    outputChannels_ = api_->tensorDim(output, 3);
    if (inputWidth_ <= 0 || inputHeight_ <= 0 ||
        outputWidth_ <= 0 || outputHeight_ <= 0 || outputChannels_ <= 0) {
        reset();
        return false;
    }
    inputBuffer_.resize(inputWidth_ * inputHeight_ * 3);
    return true;
}

bool BackgroundSegmenter::segment(
    const uint8_t* rgba,
    int width,
    int height,
    std::vector<float>& mask
) {
    if (!available() || !rgba || width <= 0 || height <= 0) return false;

    for (int y = 0; y < inputHeight_; ++y) {
        const int sourceY = std::min(height - 1, y * height / inputHeight_);
        for (int x = 0; x < inputWidth_; ++x) {
            const int sourceX = std::min(width - 1, x * width / inputWidth_);
            const uint8_t* pixel = rgba + (sourceY * width + sourceX) * 4;
            const size_t index = static_cast<size_t>(y * inputWidth_ + x) * 3;
            inputBuffer_[index] = pixel[0] / 255.0f;
            inputBuffer_[index + 1] = pixel[1] / 255.0f;
            inputBuffer_[index + 2] = pixel[2] / 255.0f;
        }
    }

    TfLiteTensor* input = api_->getInputTensor(
        reinterpret_cast<TfLiteInterpreter*>(interpreter_), 0
    );
    if (api_->tensorCopyFromBuffer(
            input, inputBuffer_.data(), inputBuffer_.size() * sizeof(float)
        ) != kTfLiteOk ||
        api_->invoke(reinterpret_cast<TfLiteInterpreter*>(interpreter_)) !=
            kTfLiteOk) {
        return false;
    }

    const TfLiteTensor* output = api_->getOutputTensor(
        reinterpret_cast<const TfLiteInterpreter*>(interpreter_), 0
    );
    const size_t count = static_cast<size_t>(outputWidth_ * outputHeight_);
    const size_t outputCount = count * static_cast<size_t>(outputChannels_);
    std::vector<float> outputValues(outputCount);
    if (api_->tensorByteSize(output) != outputCount * sizeof(float) ||
        api_->tensorCopyToBuffer(
            output, outputValues.data(), outputCount * sizeof(float)
        ) != kTfLiteOk) {
        return false;
    }

    std::vector<float> freshMask(count);
    if (outputChannels_ == 1) {
        freshMask = std::move(outputValues);
    } else {
        constexpr int personClass = 15;
        if (outputChannels_ <= personClass) return false;
        for (size_t pixel = 0; pixel < count; ++pixel) {
            const float* classes = outputValues.data() + pixel * outputChannels_;
            const float maximum = *std::max_element(
                classes, classes + outputChannels_);
            float denominator = 0.0f;
            for (int channel = 0; channel < outputChannels_; ++channel) {
                denominator += std::exp(classes[channel] - maximum);
            }
            freshMask[pixel] = std::exp(classes[personClass] - maximum) /
                std::max(denominator, 0.000001f);
        }
    }

    if (mask.size() != count) {
        mask = std::move(freshMask);
    } else {
        for (size_t i = 0; i < count; ++i) {
            // Keep just enough history to calm model shimmer without leaving a
            // visible silhouette behind a moving subject.
            mask[i] = 0.88f * freshMask[i] + 0.12f * mask[i];
        }
    }
    return true;
}
