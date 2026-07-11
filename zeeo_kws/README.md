# zeeo_kws — 唤醒词库（给同事接入）

只暴露 **头文件 + 动态库 + 示例**，内部基于 sherpa-onnx。

**集成文档（喂数据、事件字段、业务处理）见 [INTEGRATION.md](./INTEGRATION.md)。**

## 目录结构

```text
zeeo_kws/
  include/zeeo_kws/kws_engine.h   # 对外 API
  lib/libzeeo_kws.dylib           # 编译后生成（Mac；Linux 为 .so，.gitignore）
  core/kws_engine.cc              # 库实现（同事不必关心）
  src/feed_pcm_demo.cc            # 调用示例：读 wav → ProcessInt16
  src/feed_pcm_demo               # 编译后的示例程序
```

## 编译脚本（仓库根目录）

| 脚本 | 平台 | 用途 |
|------|------|------|
| `run_build_mac.sh` | Mac | sherpa demo：wav + PortAudio 麦克风 |
| `run_build_orangepi.sh` | Orange Pi | sherpa demo：wav + ALSA 麦克风 |
| `run_build_lib.sh` | **Mac / Orange Pi 通用** | zeeo_kws 库 + `feed_pcm_demo` |

## 两种测试方式对比

| 方式 | Mac | Orange Pi | 说明 |
|------|-----|-----------|------|
| **直接调 sherpa 程序** | `run-kws-test-mac.sh 1/2` | `run-kws-wav.sh` / `run-kws-mic-alsa.sh` | 不经过 zeeo_kws |
| **库接口（推荐给同事）** | `run-kws-zeeo-mac.sh` | `run-kws-feed-demo.sh` | `KwsEngine::ProcessInt16` |

## 编译库 + 示例（Mac 或 Orange Pi 同一命令）

在仓库根目录：

```bash
bash run_build_lib.sh
```

Mac 测试：

```bash
bash run-kws-zeeo-mac.sh
# 或
bash run-kws-zeeo-mac.sh audio_record/test.wav
```

Orange Pi 测试：

```bash
./run-kws-feed-demo.sh
```

## 同事只拿 include + lib + src 时

1. 拷贝这三处：

   - `zeeo_kws/include/`
   - `zeeo_kws/lib/libzeeo_kws.dylib`（Linux 为 `.so`）
   - `zeeo_kws/src/feed_pcm_demo.cc`

2. 模型目录（onnx + `keywords_zeeo.txt`）单独部署

3. 编译示例：

```bash
cd zeeo_kws
bash build_example.sh
./src/feed_pcm_demo /path/to/model_dir /path/to/test.wav
```

4. 自有程序只需：

```cpp
#include "zeeo_kws/kws_engine.h"

zeeo_kws::KwsConfig cfg;
cfg.model_dir = "/path/to/model";
zeeo_kws::KwsEngine kws(cfg);

// 在自己的采麦循环里：
kws.ProcessInt16(pcm_int16, num_samples);
```

## 音频约定

- int16、单声道、16 kHz
- 建议每块 1600～3200 样本
