#!/usr/bin/env bash
# Mac / Orange Pi 通用：编译 zeeo_kws 库 + feed_pcm_demo 示例（不涉及麦克风采音）
# 用法: bash run_build_lib.sh
#
# Mac 测试:   bash run-kws-zeeo-mac.sh
# 板子测试:   ./run-kws-feed-demo.sh
#
# 产物:
#   zeeo_kws/include/zeeo_kws/kws_engine.h
#   zeeo_kws/lib/libzeeo_kws.{dylib,so}
#   zeeo_kws/src/feed_pcm_demo
#
# 若 cmake 报错或改了 CMakeLists，先: rm -rf build

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

case "$(uname -s)" in
  Darwin)
    LIB_EXT=dylib
    TEST_HINT="bash run-kws-zeeo-mac.sh"
    ;;
  Linux)
    LIB_EXT=so
    TEST_HINT="./run-kws-feed-demo.sh"
    ;;
  *)
    echo "不支持的平台: $(uname -s)" >&2
    exit 1
    ;;
esac

# 第二层嵌套 cmake 只查项目根 / build / ~/Downloads，不查 depends/；链到 depends 里已有包
ensure_nested_dep_links() {
  local deps=(
    eigen-3.4.0.tar.gz
    kaldifst-1.8.0.tar.gz
    kissfft-febd4caeed32e33ad8b2e0bb5ea77542c40f18ec.zip
    openfst-1.8.5-2026-04-10.tar.gz
  )
  for name in "${deps[@]}"; do
    if [[ -f "$SCRIPT_DIR/depends/$name" && ! -e "$SCRIPT_DIR/$name" ]]; then
      ln -sf "depends/$name" "$SCRIPT_DIR/$name"
      echo "链接嵌套依赖: $name -> depends/$name"
    fi
  done
  # kaldifst 嵌套 openfst 也认 openfst-kaldifst 命名
  local kof=openfst-kaldifst-1.8.5-2026-04-10.tar.gz
  if [[ -f "$SCRIPT_DIR/depends/$kof" && ! -e "$SCRIPT_DIR/openfst-1.8.5-2026-04-10.tar.gz" ]]; then
    ln -sf "depends/$kof" "$SCRIPT_DIR/openfst-1.8.5-2026-04-10.tar.gz"
    echo "链接嵌套依赖: openfst-1.8.5-2026-04-10.tar.gz -> depends/$kof"
  fi
}
ensure_nested_dep_links

mkdir -p build && cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DSHERPA_ONNX_ENABLE_RKNN=OFF \
  -DSHERPA_ONNX_ENABLE_PORTAUDIO=OFF \
  -DSHERPA_ONNX_ENABLE_BINARY=ON \
  -DSHERPA_ONNX_ENABLE_ZEEO_KWS=ON

cmake --build . --target zeeo_kws feed_pcm_demo

echo
echo "完成 ($(uname -s)):"
echo "  zeeo_kws/include/zeeo_kws/kws_engine.h"
echo "  zeeo_kws/lib/libzeeo_kws.${LIB_EXT}"
echo "  zeeo_kws/src/feed_pcm_demo"
echo
echo "测试: ${TEST_HINT}"
