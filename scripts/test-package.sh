#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
VERSION_RAW="$(awk '/^version:/ {print $2; exit}' "$PROJECT_ROOT/pubspec.yaml")"
VERSION="${VERSION_RAW%%+*}"
PACKAGE_PATH="${1:-$PROJECT_ROOT/dist/camora_${VERSION}_amd64.deb}"
[[ -f "$PACKAGE_PATH" ]] || { echo "Package not found: $PACKAGE_PATH" >&2; exit 1; }
for command in dpkg-deb readelf ldd grep find; do
  command -v "$command" >/dev/null || { echo "Missing validation command: $command" >&2; exit 1; }
done
mkdir -p "$PROJECT_ROOT/build"
EXTRACT_DIR="$(mktemp -d "$PROJECT_ROOT/build/camora-package-test.XXXXXX")"
cleanup() { rm -rf -- "$EXTRACT_DIR"; }
trap cleanup EXIT
dpkg-deb --info "$PACKAGE_PATH" >/dev/null
dpkg-deb --extract "$PACKAGE_PATH" "$EXTRACT_DIR"
required_files=(opt/camora/camora opt/camora/camora-bin opt/camora/data/flutter_assets/assets/models/rvm_mobilenetv3_fp32.onnx opt/camora/data/flutter_assets/assets/runtime/linux-gpu-runtime.json opt/camora/lib/libonnxruntime.so.1.29.1 opt/camora/licenses/ONNXRUNTIME-LICENSE opt/camora/licenses/RVM-LICENSE usr/share/applications/camora.desktop usr/share/pixmaps/camora.png)
for relative_path in "${required_files[@]}"; do
  [[ -e "$EXTRACT_DIR/$relative_path" ]] || { echo "Missing expected package file: /$relative_path" >&2; exit 1; }
done
APP="$EXTRACT_DIR/opt/camora/camora-bin"
LIB_DIR="$EXTRACT_DIR/opt/camora/lib"
RPATH_OUTPUT="$(readelf -d "$APP" | grep -E 'RPATH|RUNPATH' || true)"
[[ "$RPATH_OUTPUT" == *'$ORIGIN/lib'* ]] || { echo "Invalid Camora private-library RPATH:" >&2; echo "$RPATH_OUTPUT" >&2; exit 1; }
APP_LDD="$(ldd "$APP")"
if grep -q 'not found' <<<"$APP_LDD"; then echo "Unresolved Camora executable dependency:" >&2; echo "$APP_LDD" >&2; exit 1; fi
if find "$LIB_DIR" -maxdepth 1 -type f \( -name 'libonnxruntime_providers_cuda.so*' -o -name 'libcuda*.so*' -o -name 'libcud*.so*' -o -name 'libcublas*.so*' -o -name 'libcurand*.so*' -o -name 'libnvrtc*.so*' \) | grep -q .; then
  echo "GPU-only library found in CPU package:" >&2
  find "$LIB_DIR" -maxdepth 1 -type f -name '*.so*' >&2
  exit 1
fi
if ! grep -q 'camora/runtime/current/lib' "$EXTRACT_DIR/opt/camora/camora"; then
  echo "Camora launcher does not activate the private per-user runtime." >&2
  exit 1
fi
if grep -aRIlE '/home/holy|/tmp/camora-cuda|~/Code/camora' "$EXTRACT_DIR" >/dev/null; then
  echo "Development path found in package payload:" >&2
  grep -aRIlE '/home/holy|/tmp/camora-cuda|~/Code/camora' "$EXTRACT_DIR" >&2
  exit 1
fi
echo "Package validation passed: $PACKAGE_PATH"
echo "RPATH: $RPATH_OUTPUT"
echo "Executable dependencies: resolved"
echo "GPU runtime: excluded from CPU package; private runtime launcher present"
echo "Note: install and launch testing in clean Ubuntu/Debian VMs is still required."
