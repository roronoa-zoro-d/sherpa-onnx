# C API examples（dev-kws 分支）

本目录仅保留 **KWS（关键词唤醒）** 相关示例：

- `kws-c-api.c` — 从 **WAV 文件** 读入 PCM，调用 C API 做流式关键词检测。
- `keywords-spotter-buffered-tokens-keywords-c-api.c` — 使用 **缓冲 token / 关键词串** 的变体示例。

构建：在工程根目录配置 CMake 时打开 `SHERPA_ONNX_BUILD_C_API_EXAMPLES`（默认在顶层工程为 ON），编译后可在 `build/bin` 找到对应可执行文件。

与麦克风 / ALSA 相关的官方可执行程序在 `sherpa-onnx/csrc/` 中：`sherpa-onnx-keyword-spotter-microphone`、`sherpa-onnx-keyword-spotter-alsa`（需相应依赖）。
