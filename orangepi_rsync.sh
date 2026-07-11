#!/usr/bin/env bash
# Mac 开发 → rsync 源码 + 依赖 + 模型到 Orange Pi 5（板子不含 git）
# 用法: ./orangepi_rsync.sh

set -euo pipefail

# === 板子连接（IP 变了只改这里）===
BOARD_IP=192.168.1.50
BOARD_USER=orangepi
REMOTE_DIR=/home/orangepi/program/audio/awk

# === Mac 本地仓库 ===
LOCAL_DIR=/Users/zoro/Program/audio/asr/k2_zoro/sherpa-onnx
GIT_BRANCH=dev-kws-orangepi

# 板子编译用的 onnxruntime（aarch64 静态库，与 Mac 的 osx 包不同）
ONNXRT_AARCH64=onnxruntime-linux-aarch64-static_lib-1.24.4-glibc2_17.zip
ONNXRT_URL=https://github.com/csukuangfj/onnxruntime-libs/releases/download/v1.24.4/onnxruntime-linux-aarch64-static_lib-1.24.4-glibc2_17.zip

cd "$LOCAL_DIR"

current=$(git branch --show-current)
if [ "$current" != "$GIT_BRANCH" ]; then
  echo "错误: 当前分支是 $current，请先切换: git checkout $GIT_BRANCH" >&2
  exit 1
fi

mkdir -p "$LOCAL_DIR/depends"

if [ ! -f "$LOCAL_DIR/depends/$ONNXRT_AARCH64" ]; then
  echo "depends/ 缺少板子用 onnxruntime，正在下载 ..."
  curl -L -o "$LOCAL_DIR/depends/$ONNXRT_AARCH64" "$ONNXRT_URL"
  echo
fi

echo "同步 → $BOARD_USER@$BOARD_IP:$REMOTE_DIR/"
echo "  Mac 分支: $GIT_BRANCH"
echo "  同步: 源码 + depends/(aarch64) + models/ + run_build_*.sh + 嵌套依赖 symlink"
echo "  排除: Mac 专用 onnxruntime、build/、Mac 编出的 zeeo_kws 产物"
echo

# 同步整个仓库根目录，用 exclude 过滤；避免多源 rsync 把子目录内容摊平到同一层
rsync -avz --progress \
  --exclude '.git/' \
  --exclude 'build/' \
  --exclude 'build_mac/' \
  --exclude 'build_lib/' \
  --exclude 'depends_bak/' \
  --exclude 'depends/onnxruntime-osx-*' \
  --exclude 'depends/.DS_Store' \
  --exclude 'zeeo_kws/lib/*.dylib' \
  --exclude 'zeeo_kws/lib/*.so' \
  --exclude 'zeeo_kws/src/feed_pcm_demo' \
  --exclude 'zeeo_rsync.sh' \
  --exclude 'orangepi_rsync.sh' \
  --exclude 'README.md' \
  --exclude 'CHANGELOG.md' \
  --exclude 'LICENSE' \
  --exclude 'CPPLINT.cfg' \
  --exclude 'run-kws-microphone.sh' \
  --exclude 'scripts/' \
  --exclude 'c-api-examples/' \
  --exclude 'toolchains/' \
  "$LOCAL_DIR/" "$BOARD_USER@$BOARD_IP:$REMOTE_DIR/"

echo
echo "完成. 板子上编译库:"
echo "  ssh $BOARD_USER@$BOARD_IP"
echo "  cd $REMOTE_DIR && bash run_build_lib.sh"
echo "  ./run-kws-feed-demo.sh"
