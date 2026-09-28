# Optional NVIDIA runtime artifact

Camora's Debian package is CPU-ready and intentionally contains no CUDA,
cuDNN, or ONNX Runtime CUDA provider libraries. Settings reads
`assets/runtime/linux-gpu-runtime.json` to offer an optional per-user install.

A release owner must build one vetted `tar.gz`. Its metadata can be recorded
before publication by leaving `url` null; replace it with an authoritative HTTPS
URL only after the exact archive has been published:

```json
{
  "schemaVersion": 1,
  "artifact": {
    "version": "RELEASED_RUNTIME_VERSION",
    "url": null,
    "sha256": "64_LOWERCASE_HEX_DIGITS",
    "minDriver": "VETTED_MINIMUM_NVIDIA_DRIVER"
  }
}
```

Do not set `url` until the exact artifact has passed the CUDA RVM warm-up and
shutdown tests and has been published. The archive must contain a `lib/` directory with
`libonnxruntime_providers_shared.so`, `libonnxruntime_providers_cuda.so`, and
all vetted CUDA/cuDNN redistributable dependencies, including the sonames
checked by `GpuRuntimeManager`.

At runtime Camora detects the driver with `nvidia-smi`, requires HTTPS,
streams download progress, verifies the complete archive with SHA-256, rejects
absolute or parent-traversal archive paths, verifies required libraries, then
atomically activates the version under:

`$XDG_DATA_HOME/camora/runtime/current` (or
`~/.local/share/camora/runtime/current`).

The installed launcher adds that private `lib/` directory automatically on the
next launch. Users never set `LD_LIBRARY_PATH`. If CUDA registration, session
creation, or inference warm-up fails, the existing native code destroys the
failed CUDA session and creates a fresh CPU session.
