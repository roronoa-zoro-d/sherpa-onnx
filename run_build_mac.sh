#!/usr/bin/env bash
# Mac 本地编译 KWS demo（wav + PortAudio 麦克风）
# 用法: bash run_build_mac.sh
#
# 测试: ./run-kws-test-mac.sh 1   # wav
#       ./run-kws-test-mac.sh 2   # 麦克风
#
# 库接口请用: bash run_build_lib.sh
#
# 若 cmake 报错或改了 CMakeLists，先: rm -rf build

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

mkdir -p build && cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DSHERPA_ONNX_ENABLE_RKNN=OFF \
  -DSHERPA_ONNX_ENABLE_PORTAUDIO=ON \
  -DSHERPA_ONNX_ENABLE_BINARY=ON \
  -DSHERPA_ONNX_ENABLE_ZEEO_KWS=OFF

cmake --build . --target sherpa-onnx-keyword-spotter
cmake --build . --target sherpa-onnx-keyword-spotter-microphone

echo
echo "完成:"
echo "  build/bin/sherpa-onnx-keyword-spotter             (wav,  run-kws-test-mac.sh 1)"
echo "  build/bin/sherpa-onnx-keyword-spotter-microphone  (mic,  run-kws-test-mac.sh 2)"
echo
echo "库接口: bash run_build_lib.sh"
