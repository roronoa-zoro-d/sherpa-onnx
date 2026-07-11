#!/usr/bin/env bash
# Orange Pi (RK3588) 编译 KWS demo（wav + ALSA 麦克风）
# 用法: bash run_build_orangepi.sh
#
# 测试: ./run-kws-wav.sh
#       ./run-kws-mic-alsa.sh
#
# 库接口请用: bash run_build_lib.sh
#
# 若 cmake 报错或改了 CMakeLists，先: rm -rf build

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

mkdir -p build && cd build

# ZEEO_KWS=ON：CMake 要求开启后才编译 sherpa-onnx-keyword-spotter-alsa
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DSHERPA_ONNX_ENABLE_RKNN=OFF \
  -DSHERPA_ONNX_ENABLE_PORTAUDIO=OFF \
  -DSHERPA_ONNX_ENABLE_BINARY=ON \
  -DSHERPA_ONNX_ENABLE_ZEEO_KWS=ON

cmake --build . --target sherpa-onnx-keyword-spotter
cmake --build . --target sherpa-onnx-keyword-spotter-alsa

echo
echo "完成:"
echo "  build/bin/sherpa-onnx-keyword-spotter       (wav,  ./run-kws-wav.sh)"
echo "  build/bin/sherpa-onnx-keyword-spotter-alsa  (mic,  ./run-kws-mic-alsa.sh)"
echo
echo "库接口: bash run_build_lib.sh"
