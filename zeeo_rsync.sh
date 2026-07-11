#!/usr/bin/env bash
# 个人 git 仓库 → 公司 zeeo-speech 目录（本地 rsync，不覆盖公司 .git）
# 用法: ./zeeo_rsync.sh
#
# 源: k2_zoro/sherpa-onnx（dev-kws-orangepi）
# 目标: zeeo-speech（公司仓库，保留其 .git / README 等）

set -euo pipefail

# === 路径（一般不用改）===
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOCAL_DIR="$SCRIPT_DIR"
DEST_DIR=/Users/zoro/Program/audio/asr/zeeo-speech
GIT_BRANCH=dev-kws-orangepi

cd "$LOCAL_DIR"

current=$(git branch --show-current 2>/dev/null || true)
if [[ "$current" != "$GIT_BRANCH" ]]; then
  echo "错误: 当前分支是 ${current:-未知}，请先: git checkout $GIT_BRANCH" >&2
  exit 1
fi

if [[ ! -d "$DEST_DIR" ]]; then
  echo "错误: 目标目录不存在: $DEST_DIR" >&2
  exit 1
fi

mkdir -p "$LOCAL_DIR/depends" "$DEST_DIR"

echo "同步 → $DEST_DIR/"
echo "  源分支: $GIT_BRANCH"
echo "  同步: 源码 + depends/(Mac+aarch64) + models/ + zeeo_kws/ + run_build_*.sh + run-kws-*.sh"
echo "  保留: 公司目录 .git/、README.md、.aliyun/ 等（不覆盖、不删除）"
echo "  排除: build/、编译产物、个人专用脚本"
echo

rsync -av --progress \
  --exclude '.git/' \
  --exclude '.aliyun/' \
  --exclude 'build/' \
  --exclude 'build_mac/' \
  --exclude 'build_lib/' \
  --exclude 'depends_bak/' \
  --exclude 'depends/.DS_Store' \
  --exclude 'zeeo_kws/lib/*.dylib' \
  --exclude 'zeeo_kws/lib/*.so' \
  --exclude 'zeeo_kws/src/feed_pcm_demo' \
  --exclude 'zeeo_rsync.sh' \
  --exclude 'orangepi_rsync.sh' \
  --exclude 'OrangePi5-Mac远程使用教程.md' \
  --exclude 'README.md' \
  --exclude 'CHANGELOG.md' \
  --exclude 'LICENSE' \
  --exclude 'CPPLINT.cfg' \
  --exclude 'scripts/' \
  --exclude 'c-api-examples/' \
  --exclude 'toolchains/' \
  "$LOCAL_DIR/" "$DEST_DIR/"

# 嵌套 cmake 依赖 symlink（与 run_build_lib.sh 一致）
link_nested_dep() {
  local name=$1
  if [[ -f "$DEST_DIR/depends/$name" && ! -e "$DEST_DIR/$name" ]]; then
    ln -sf "depends/$name" "$DEST_DIR/$name"
    echo "  链接: $name -> depends/$name"
  fi
}

echo
echo "嵌套依赖 symlink（目标目录）:"
link_nested_dep eigen-3.4.0.tar.gz
link_nested_dep kaldifst-1.8.0.tar.gz
link_nested_dep kissfft-febd4caeed32e33ad8b2e0bb5ea77542c40f18ec.zip
link_nested_dep openfst-1.8.5-2026-04-10.tar.gz
if [[ -f "$DEST_DIR/depends/openfst-kaldifst-1.8.5-2026-04-10.tar.gz" \
      && ! -e "$DEST_DIR/openfst-1.8.5-2026-04-10.tar.gz" ]]; then
  ln -sf "depends/openfst-kaldifst-1.8.5-2026-04-10.tar.gz" \
    "$DEST_DIR/openfst-1.8.5-2026-04-10.tar.gz"
  echo "  链接: openfst-1.8.5-2026-04-10.tar.gz -> depends/openfst-kaldifst-..."
fi

echo
echo "完成. 公司目录编译库:"
echo "  cd $DEST_DIR && bash run_build_lib.sh"
echo "  bash run-kws-zeeo-mac.sh"
echo
echo "提交到公司 git 请自行:"
echo "  cd $DEST_DIR && git status"
