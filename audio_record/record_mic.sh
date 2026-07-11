#!/usr/bin/env bash
# 控制录音时长，调用 record_mic 从麦克风采集并保存 WAV
# 用法: ./record_mic.sh [时长秒] [输出文件] [设备名]
# 示例: ./record_mic.sh 10 test.wav plughw:2,0

set -euo pipefail

DIR=/home/orangepi/program/audio/awk/audio_record
DURATION=10
OUTPUT=test.wav
DEVICE=plughw:2,0

if [ $# -ge 1 ]; then
  DURATION=$1
fi
if [ $# -ge 2 ]; then
  OUTPUT=$2
fi
if [ $# -ge 3 ]; then
  DEVICE=$3
fi

EXE=$DIR/build/record_mic

if [ ! -x "$EXE" ]; then
  echo "找不到: $EXE" >&2
  echo "请先编译: cd $DIR/build && cmake .. && cmake --build ." >&2
  exit 1
fi

cd "$DIR"
exec "$EXE" "$DURATION" "$OUTPUT" "$DEVICE"
