# NVIDIA runtime payload

Place the x86_64 redistributable CUDA 13/cuDNN 9 shared libraries used by the
ONNX Runtime 1.29.1 GPU build in the lib directory, or set the
CAMORA_NVIDIA_LIB_DIR environment variable when running the Debian packaging
script.

Required runtime families are validated by the packaging script. Development
headers, Python, pip, nvcc, and the CUDA Toolkit are not packaged.
