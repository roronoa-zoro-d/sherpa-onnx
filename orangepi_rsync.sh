#!/usr/bin/env bash
# Mac 同步 sherpa-onnx 到 Orange Pi 5
# 用法: ./orangepi_rsync.sh

set -euo pipefail

# === 板子连接（IP 变了只改这里）===
BOARD_IP=192.168.1.50
BOARD_USER=orangepi
REMOTE_DIR=/home/orangepi/program/audio/awk
LOCAL_DIR=/Users/zoro/Program/audio/asr/k2_zoro/sherpa-onnx

echo "同步: $LOCAL_DIR/"
echo "  →  $BOARD_USER@$BOARD_IP:$REMOTE_DIR/"
echo

rsync -avz --progress \
  --exclude 'build/' \
  --exclude 'depends/' \
  --exclude 'depends_bak/' \
  --exclude '.git/' \
  "$LOCAL_DIR/" "$BOARD_USER@$BOARD_IP:$REMOTE_DIR/"

echo
echo "完成."
