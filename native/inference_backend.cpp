#include "inference_backend.h"

InferenceInfo InferenceBackendSelector::configure(
    Ort::SessionOptions& options,
    InferenceBackend backend
) {
    options.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_ALL
    );
    options.SetIntraOpNumThreads(4);
    options.SetInterOpNumThreads(1);

    if (backend == InferenceBackend::Cuda) {
        Ort::CUDAProviderOptions cudaOptions;
        cudaOptions.Update({
            {"device_id", "0"},
            {"do_copy_in_default_stream", "1"},
        });
        options.AppendExecutionProvider_CUDA_V2(*cudaOptions);
        return {
            InferenceBackend::Cuda,
            "CUDAExecutionProvider",
            "CUDA:0",
            true,
        };
    }

    return {
        InferenceBackend::Cpu,
        "CPUExecutionProvider",
        "CPU",
        false,
    };
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
