#pragma once

#include <string>

#include "third_party/onnxruntime/include/onnxruntime_cxx_api.h"

enum class InferenceBackend {
    Cpu,
    Cuda,
};

struct InferenceInfo {
    InferenceBackend backend = InferenceBackend::Cpu;
    std::string provider = "CPUExecutionProvider";
    std::string device = "CPU";
    bool accelerated = false;
};

class InferenceBackendSelector {
public:
    static InferenceInfo configure(
        Ort::SessionOptions& options,
        InferenceBackend backend
    );

    static const char* name(InferenceBackend backend);
};
