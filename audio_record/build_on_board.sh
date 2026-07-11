#!/usr/bin/env bash
# 在 Orange Pi 上编译 record_mic
# 用法: ./build_on_board.sh

set -euo pipefail

DIR=/home/orangepi/program/audio/audio_record/

cd "$DIR"
mkdir -p build
cd build

cmake ..
cmake --build . 

echo
echo "编译完成: $DIR/build/record_mic"
echo "试录: cd $DIR && ./record_mic.sh 10 test.wav"
