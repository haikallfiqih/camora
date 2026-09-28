#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

rm -rf "$ROOT/build/native-build"
mkdir -p "$ROOT/build/native-build"
mkdir -p "$ROOT/build/native"

cmake \
  -S "$ROOT/native" \
  -B "$ROOT/build/native-build" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release

cmake --build "$ROOT/build/native-build"

cp \
  "$ROOT/build/native-build/libcamora_v4l2.so" \
  "$ROOT/build/native/libcamora_v4l2.so"

echo
echo "Native library built:"
echo "$ROOT/build/native/libcamora_v4l2.so"
