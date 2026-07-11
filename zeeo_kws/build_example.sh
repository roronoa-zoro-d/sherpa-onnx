#!/usr/bin/env bash
# 仅使用 zeeo_kws/include + zeeo_kws/lib 编译示例（模仿同事接入）
# 前提: 已 bash run_build_lib.sh

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

LIB=$(ls "$ROOT/lib"/libzeeo_kws.* 2>/dev/null | head -1)
if [[ -z "$LIB" ]]; then
  echo "找不到 lib/libzeeo_kws.* ，请先 bash run_build_lib.sh" >&2
  exit 1
fi

g++ -std=c++17 -O2 \
  -I"$ROOT/include" \
  "$ROOT/src/feed_pcm_demo.cc" \
  -L"$ROOT/lib" \
  -Wl,-rpath,"$ROOT/lib" \
  -lzeeo_kws \
  -o "$ROOT/src/feed_pcm_demo"

echo "完成: $ROOT/src/feed_pcm_demo"
