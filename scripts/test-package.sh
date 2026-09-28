#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
VERSION_RAW="$(awk '/^version:/ {print $2; exit}' "$PROJECT_ROOT/pubspec.yaml")"
VERSION="${VERSION_RAW%%+*}"
PACKAGE_PATH="${1:-$PROJECT_ROOT/dist/camora_${VERSION}_amd64.deb}"
[[ -f "$PACKAGE_PATH" ]] || { echo "Package not found: $PACKAGE_PATH" >&2; exit 1; }
for command in dpkg-deb readelf ldd grep find; do command -v "$command" >/dev/null || { echo "Missing validation command: $command" >&2; exit 1; }; done
mkdir -p "$PROJECT_ROOT/build"
EXTRACT_DIR="$(mktemp -d "$PROJECT_ROOT/build/camora-package-test.XXXXXX")"
cleanup() { rm -rf -- "$EXTRACT_DIR"; }
trap cleanup EXIT
dpkg-deb --info "$PACKAGE_PATH" >/dev/null
dpkg-deb --extract "$PACKAGE_PATH" "$EXTRACT_DIR"
required_files=(opt/camora/camora opt/camora/data/flutter_assets/assets/models/rvm_mobilenetv3_fp32.onnx opt/camora/lib/libonnxruntime.so.1.29.1 opt/camora/lib/libonnxruntime_providers_cuda.so opt/camora/lib/libcublas.so.13 opt/camora/lib/libcudnn.so.9 opt/camora/licenses/ONNXRUNTIME-LICENSE opt/camora/licenses/RVM-LICENSE opt/camora/licenses/NVIDIA-CUDA-LICENSE usr/share/applications/camora.desktop usr/share/pixmaps/camora.png)
for relative_path in "${required_files[@]}"; do [[ -e "$EXTRACT_DIR/$relative_path" ]] || { echo "Missing expected package file: /$relative_path" >&2; exit 1; }; done
APP="$EXTRACT_DIR/opt/camora/camora"
LIB_DIR="$EXTRACT_DIR/opt/camora/lib"
RPATH_OUTPUT="$(readelf -d "$APP" | grep -E 'RPATH|RUNPATH' || true)"
[[ "$RPATH_OUTPUT" == *'$ORIGIN/lib'* ]] || { echo "Invalid Camora private-library RPATH:" >&2; echo "$RPATH_OUTPUT" >&2; exit 1; }
APP_LDD="$(ldd "$APP")"
if grep -q 'not found' <<<"$APP_LDD"; then echo "Unresolved Camora executable dependency:" >&2; echo "$APP_LDD" >&2; exit 1; fi
CUDA_LDD="$(env LD_LIBRARY_PATH="$LIB_DIR" ldd "$LIB_DIR/libonnxruntime_providers_cuda.so")"
if grep -q 'not found' <<<"$CUDA_LDD"; then echo "Unresolved private CUDA provider dependency:" >&2; echo "$CUDA_LDD" >&2; exit 1; fi
if grep -aRIlE '/home/holy|/tmp/camora-cuda|~/Code/camora' "$EXTRACT_DIR" >/dev/null; then echo "Development path found in package payload:" >&2; grep -aRIlE '/home/holy|/tmp/camora-cuda|~/Code/camora' "$EXTRACT_DIR" >&2; exit 1; fi
echo "Package validation passed: $PACKAGE_PATH"
echo "RPATH: $RPATH_OUTPUT"
echo "Executable dependencies: resolved"
echo "CUDA provider dependencies: resolved from private lib directory"
echo "Note: install and launch testing in a clean Ubuntu/Debian VM is still required."
