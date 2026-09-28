#include "inference_backend.h"

#include <iostream>
#include <string>
#include <unordered_map>

InferenceInfo InferenceBackendSelector::configure(
    Ort::SessionOptions& options
) {
    InferenceInfo info;

    options.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_ALL
    );

    // CPU remains the guaranteed fallback.
    options.SetIntraOpNumThreads(4);
    options.SetInterOpNumThreads(1);

    try {
        Ort::CUDAProviderOptions cudaOptions;

        cudaOptions.Update({
            {"device_id", "0"},
            {"do_copy_in_default_stream", "1"},
        });

        options.AppendExecutionProvider_CUDA_V2(
            *cudaOptions
        );

        info.backend = InferenceBackend::Cuda;
        info.provider = "CUDAExecutionProvider";
        info.device = "CUDA:0";
        info.accelerated = true;

        std::cerr
            << "[Camora] inference backend: "
            << info.provider
            << " (" << info.device << ")"
            << std::endl;

        return info;
    } catch (const Ort::Exception& error) {
        std::cerr
            << "[Camora] CUDA unavailable, falling back to CPU: "
            << error.what()
            << std::endl;
    } catch (const std::exception& error) {
        std::cerr
            << "[Camora] CUDA setup failed, falling back to CPU: "
            << error.what()
            << std::endl;
    }

    info.backend = InferenceBackend::Cpu;
    info.provider = "CPUExecutionProvider";
    info.device = "CPU";
    info.accelerated = false;

    std::cerr
        << "[Camora] inference backend: "
        << info.provider
        << std::endl;

    return info;
}

const char* InferenceBackendSelector::name(
    InferenceBackend backend
) {
    switch (backend) {
        case InferenceBackend::Cuda:
            return "CUDA";
        case InferenceBackend::Cpu:
        default:
            return "CPU";
    }
}
