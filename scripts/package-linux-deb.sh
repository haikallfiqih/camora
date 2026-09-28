#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
VERSION_RAW="$(awk '/^version:/ {print $2; exit}' "$PROJECT_ROOT/pubspec.yaml")"
VERSION="${VERSION_RAW%%+*}"
ARCH="amd64"
BUNDLE="$PROJECT_ROOT/build/linux/x64/release/bundle"
DIST_DIR="$PROJECT_ROOT/dist"
PACKAGE_PATH="$DIST_DIR/camora_${VERSION}_${ARCH}.deb"
for command in flutter dpkg-deb readelf ldd awk sed install find grep; do
  command -v "$command" >/dev/null || { echo "Missing required packaging command: $command" >&2; exit 1; }
done
[[ -n "$VERSION" ]] || { echo "Could not read version from pubspec.yaml" >&2; exit 1; }
echo "[Camora package] Building Flutter Linux release"
(cd "$PROJECT_ROOT" && flutter build linux --release)
MODEL="$BUNDLE/data/flutter_assets/assets/models/rvm_mobilenetv3_fp32.onnx"
ONNXRUNTIME="$BUNDLE/lib/libonnxruntime.so.1.29.1"
[[ -f "$MODEL" ]] || { echo "Bundled RVM model is missing: $MODEL" >&2; exit 1; }
[[ -f "$ONNXRUNTIME" ]] || { echo "Bundled ONNX Runtime is missing: $ONNXRUNTIME" >&2; exit 1; }
RPATH="$(readelf -d "$BUNDLE/camora" | sed -n 's/.*Library rpath: \[\(.*\)\].*/\1/p')"
[[ "$RPATH" == *'$ORIGIN/lib'* ]] || { echo "Camora executable does not contain the required private-library RPATH." >&2; exit 1; }
mkdir -p "$PROJECT_ROOT/build"
STAGING_DIR="$(mktemp -d "$PROJECT_ROOT/build/camora-deb.XXXXXX")"
cleanup() { rm -rf -- "$STAGING_DIR"; }
trap cleanup EXIT
APP_DIR="$STAGING_DIR/opt/camora"
LICENSE_DIR="$APP_DIR/licenses"
install -d "$APP_DIR" "$LICENSE_DIR" "$STAGING_DIR/DEBIAN" "$STAGING_DIR/usr/share/applications" "$STAGING_DIR/usr/share/pixmaps"
cp -a "$BUNDLE/." "$APP_DIR/"
mv "$APP_DIR/camora" "$APP_DIR/camora-bin"
install -m 0755 "$PROJECT_ROOT/packaging/linux/camora-launcher" "$APP_DIR/camora"
rm -f "$APP_DIR"/lib/libonnxruntime_providers_*.so
if find "$APP_DIR/lib" -maxdepth 1 -type f \( -name 'libcuda*.so*' -o -name 'libcud*.so*' -o -name 'libcublas*.so*' -o -name 'libcurand*.so*' -o -name 'libnvrtc*.so*' \) | grep -q .; then
  echo "GPU runtime library leaked into CPU package." >&2
  exit 1
fi
rm -rf "$APP_DIR/lib/cmake" "$APP_DIR/lib/pkgconfig"
install -m 0644 "$PROJECT_ROOT/packaging/linux/licenses/CAMORA-NOTICE" "$LICENSE_DIR/CAMORA-NOTICE"
install -m 0644 "$PROJECT_ROOT/native/third_party/onnxruntime/LICENSE" "$LICENSE_DIR/ONNXRUNTIME-LICENSE"
install -m 0644 "$PROJECT_ROOT/assets/models/RVM_LICENSE" "$LICENSE_DIR/RVM-LICENSE"
install -m 0644 "$PROJECT_ROOT/assets/models/README.md" "$LICENSE_DIR/RVM-MODEL-NOTICE"
[[ ! -f "$APP_DIR/data/flutter_assets/NOTICES.Z" ]] || cp "$APP_DIR/data/flutter_assets/NOTICES.Z" "$LICENSE_DIR/FLUTTER-NOTICES.Z"
install -m 0644 "$PROJECT_ROOT/packaging/linux/deb/camora.desktop" "$STAGING_DIR/usr/share/applications/camora.desktop"
install -m 0644 "$PROJECT_ROOT/assets/images/camora_logo.png" "$STAGING_DIR/usr/share/pixmaps/camora.png"
INSTALLED_SIZE="$(du -sk "$STAGING_DIR/opt" "$STAGING_DIR/usr" | awk '{total += $1} END {print total}')"
sed -e "s/@VERSION@/$VERSION/g" -e "s/@INSTALLED_SIZE@/$INSTALLED_SIZE/g" "$PROJECT_ROOT/packaging/linux/deb/control.in" > "$STAGING_DIR/DEBIAN/control"
find "$STAGING_DIR" -type d -exec chmod 0755 '{}' +
find "$STAGING_DIR" -type f -exec chmod 0644 '{}' +
chmod 0755 "$APP_DIR/camora" "$APP_DIR/camora-bin"
if grep -aRIlE '/home/holy|/tmp/camora-cuda|~/Code/camora' "$STAGING_DIR" >/dev/null; then
  echo "A development-only path leaked into the package payload." >&2
  grep -aRIlE '/home/holy|/tmp/camora-cuda|~/Code/camora' "$STAGING_DIR" >&2
  exit 1
fi
mkdir -p "$DIST_DIR"
rm -f "$PACKAGE_PATH"
echo "[Camora package] Building $PACKAGE_PATH"
dpkg-deb --root-owner-group -Zxz -z6 --build "$STAGING_DIR" "$PACKAGE_PATH"
echo "[Camora package] Validating package"
"$PROJECT_ROOT/scripts/test-package.sh" "$PACKAGE_PATH"
echo "[Camora package] Created $PACKAGE_PATH"
