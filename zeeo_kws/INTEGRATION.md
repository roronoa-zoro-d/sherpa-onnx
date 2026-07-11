# zeeo_kws 接口接入指南

面向集成方：如何用 `KwsEngine` 做**流式唤醒词检测**，并在业务里处理检测结果。

---

## 1. 交付物

| 路径 | 说明 |
|------|------|
| `include/zeeo_kws/kws_engine.h` | **唯一需要 include 的头文件** |
| `lib/libzeeo_kws.dylib` / `libzeeo_kws.so` | 动态库（Mac / Linux 各平台分别编译） |
| `src/feed_pcm_demo.cc` | 参考示例（读 wav 分块喂数据） |

模型目录（与库分开部署）需包含：

```text
model_dir/
  tokens.txt
  encoder-epoch-12-avg-2-chunk-16-left-64.int8.onnx
  decoder-epoch-12-avg-2-chunk-16-left-64.onnx
  joiner-epoch-12-avg-2-chunk-16-left-64.int8.onnx
  keywords_zeeo.txt          # 唤醒词列表（可自定义路径）
```

---

## 2. 最小调用流程

```cpp
#include "zeeo_kws/kws_engine.h"

// 1. 配置
zeeo_kws::KwsConfig cfg;
cfg.model_dir = "/path/to/model_dir";
// cfg.keywords_file = "/path/to/keywords_zeeo.txt";  // 可选，默认 model_dir/keywords_zeeo.txt

// 2. 创建引擎（加载模型，可能抛 std::exception）
zeeo_kws::KwsEngine engine(cfg);

// 3. 在音频循环里喂 PCM（见下一节）
int16_t pcm[] = { /* 来自麦克风或文件 */ };
int32_t n = 3200;  // 样本数，非字节数
auto events = engine.ProcessInt16(pcm, n);

// 4. 处理本次返回的事件
for (const auto &ev : events) {
  // ev.keyword / ev.json / ev.start_time ...
}
```

链接时：

```bash
g++ -std=c++17 your_app.cc \
  -I/path/to/zeeo_kws/include \
  -L/path/to/zeeo_kws/lib -Wl,-rpath,/path/to/zeeo_kws/lib \
  -lzeeo_kws -o your_app
```

Mac 运行前：`export DYLD_LIBRARY_PATH=/path/to/zeeo_kws/lib`  
Linux：`export LD_LIBRARY_PATH=/path/to/zeeo_kws/lib`

---

## 3. 音频输入约定（必须满足）

| 项目 | 要求 |
|------|------|
| 格式 | **int16 PCM** |
| 声道 | **mono 单声道** |
| 采样率 | **16000 Hz**（与 `cfg.sample_rate` 一致，当前固定 16000） |
| 字节序 | 小端（常规平台默认） |

若麦克风是 48kHz / float / 双声道，需在**业务侧**先重采样、转 mono、转 int16，再调用 `ProcessInt16`。

---

## 4. 喂数据：`ProcessInt16`（核心）

```cpp
std::vector<KwsEvent> ProcessInt16(const int16_t *samples, int32_t num_samples);
```

### 4.1 参数

| 参数 | 含义 |
|------|------|
| `samples` | 指向 **一块连续** int16 采样点 |
| `num_samples` | 采样点个数（**不是字节数**）。16000 点 = 1 秒 |

`nullptr` 或 `num_samples <= 0` 时返回空 vector，不报错。

### 4.2 返回值

返回 **本次调用中新检测到的唤醒事件**（0 个、1 个，极少数情况多个）。

- 流式接口：每来一块音频就调一次，**不要**等整段 wav 攒齐再一次性传入（除非测试）。
- 块大小任意；推荐 **1600（0.1s）或 3200（0.2s）** 样本，延迟与 CPU 开销较均衡。

### 4.3 参考循环（与 `feed_pcm_demo.cc` 一致）

```cpp
constexpr int32_t kChunk = 3200;  // 0.2 秒 @ 16kHz

for (size_t offset = 0; offset < total_samples; offset += kChunk) {
  int32_t n = static_cast<int32_t>(
      std::min(static_cast<size_t>(kChunk), total_samples - offset));

  auto events = engine.ProcessInt16(all_samples.data() + offset, n);

  for (const auto &ev : events) {
    OnWakeWordDetected(ev);  // 业务回调
  }
}
```

### 4.4 麦克风流式伪代码

```cpp
while (recording) {
  int32_t n = ReadMicInt16(mic_buf, kChunk);  // 自行实现：ALSA / PortAudio 等
  auto events = engine.ProcessInt16(mic_buf, n);
  for (const auto &ev : events) {
    HandleWake(ev);
  }
}
```

### 4.5 检测后状态

默认 `cfg.auto_reset_on_detect = true`：每次检测到唤醒词后，**内部解码状态自动 Reset**，避免同一句重复触发。  
若需连续检测同一句话的多次命中，可设为 `false`，或手动 `engine.Reset()`。

---

## 5. 事件结构：`KwsEvent`

每次命中唤醒词，得到一个 `KwsEvent`：

```cpp
struct KwsEvent {
  std::string keyword;              // 命中的唤醒词（与 keywords 文件 @ 后缀一致）
  std::vector<std::string> tokens;  // 子词/token 序列
  std::vector<float> timestamps;    // 每个 token 的时间戳（秒）
  float start_time;                 // 本段起始时间（秒）
  std::string json;                 // 上述字段的 JSON 字符串
};
```

### 5.1 字段说明

| 字段 | 类型 | 含义 | 业务怎么用 |
|------|------|------|------------|
| `keyword` | string | **唤醒词名称**。对应 `keywords_zeeo.txt` 里 `@` 后面的名字 | **主逻辑用这个**：`if (ev.keyword == "Zeeo")` 启动对话 |
| `tokens` | string[] | 模型解码出的 token 序列（含拼音/子词） | 调试、日志；一般不必参与分支 |
| `timestamps` | float[] | 与 `tokens` 一一对应，单位 **秒** | 分析触发时刻、UI 标注 |
| `start_time` | float | 该次检测片段的起始时间（秒） | 与 timestamps 配合做时间线 |
| `json` | string | 完整结果的 JSON（见下） | 直接落日志、上报云端、透传 UI |

### 5.2 `json` 示例

```json
{
  "start_time": 1.24,
  "keyword": "Zeeo",
  "timestamps": [1.24, 1.36, 1.48],
  "tokens": ["z", "ǐ", "ōu"]
}
```

实际 `tokens` 内容随模型与关键词而定，可能与拼音片段或 BPE 单元对应。

### 5.3 当前配置的唤醒词（`keywords_zeeo.txt`）

文件格式：`<拼音/token 序列> @<对外名称>`

```text
z ǐ ōu @Zeeo
x iǎo ōu @XiaoOu
x iǎo z ǐ ōu @XiaoZiOu
l ín m ěi l ì @林美丽
```

命中后 **`ev.keyword` 为 `@` 右侧名称**，例如 `"Zeeo"`、`"XiaoOu"`，**不是**拼音字符串。

### 5.4 业务处理示例

```cpp
void OnWakeWordDetected(const zeeo_kws::KwsEvent &ev) {
  // 推荐：按 keyword 分支
  if (ev.keyword == "Zeeo") {
    StartVoiceAssistant();
  } else if (ev.keyword == "XiaoOu") {
    StartXiaoOuMode();
  }

  // 可选：打日志
  fprintf(stderr, "wake: %s at %.2fs\n", ev.keyword.c_str(), ev.start_time);
  // 或整段 JSON：ev.json
}
```

**注意**：一次 `ProcessInt16` 可能返回 **多个** `KwsEvent`（极少见）；循环处理即可。

---

## 6. 配置项：`KwsConfig`

| 字段 | 默认 | 说明 |
|------|------|------|
| `model_dir` | （必填） | 模型目录 |
| `keywords_file` | `{model_dir}/keywords_zeeo.txt` | 唤醒词文件 |
| `tokens` / `encoder` / `decoder` / `joiner` | 由 model_dir 推导 | 一般不用改 |
| `sample_rate` | 16000 | 必须 16000 |
| `num_threads` | 2 | ONNX 推理线程数 |
| `keywords_threshold` | 0.25 | 阈值越低越敏感、误唤醒越多 |
| `keywords_score` | 1.0 | 关键词 bonus 分数 |
| `auto_reset_on_detect` | true | 命中后自动 Reset |

调参建议：

- 误唤醒多 → 略**提高** `keywords_threshold`（如 0.3～0.4）
- 唤不醒 → 略**降低** `keywords_threshold`（如 0.15～0.2）

---

## 7. 其它 API

```cpp
void Reset();                    // 清空流式解码状态（切场景、切用户时调用）
int32_t sample_rate() const;     // 固定 16000
const KwsConfig &config() const; // 当前配置
```

---

## 8. 线程与异常

| 项 | 说明 |
|----|------|
| 线程安全 | **非线程安全**。同一 `KwsEngine` 只在一个线程里调用 `ProcessInt16` |
| 多路音频 | 每路一个 `KwsEngine` 实例 |
| 构造失败 | 模型/关键词文件缺失 → 抛 `std::runtime_error`，需 try/catch |
| 运行中 | `ProcessInt16` 正常不抛异常 |

---

## 9. 与 `feed_pcm_demo` 的对应关系

示例程序等价于：

```text
读 wav → 校验 16kHz → 创建 KwsEngine
  → 每 3200 样本调用 ProcessInt16
  → 打印 ev.json
```

本地测试：

```bash
bash run_build_lib.sh
bash run-kws-zeeo-mac.sh                              # Mac
bash run-kws-zeeo-mac.sh /path/to/your/test.wav
# Orange Pi: ./run-kws-feed-demo.sh
```

---

## 10. 集成检查清单

- [ ] 音频已转为 **int16 / mono / 16kHz**
- [ ] 模型与 `keywords_zeeo.txt` 路径正确
- [ ] 动态库路径已设置（`-rpath` 或 `LD_LIBRARY_PATH` / `DYLD_LIBRARY_PATH`）
- [ ] 采麦循环中**持续**调用 `ProcessInt16`，块大小 1600～3200
- [ ] 业务逻辑读 **`ev.keyword`** 或解析 **`ev.json`**
- [ ] 根据场景设置 `auto_reset_on_detect` / 手动 `Reset()`

---

## 11. 常见问题

**Q: 为什么 wav 测试显示「共检测到 0 次」？**  
A: 音频内容未包含配置的唤醒词，或阈值过高。换含「子鸥 / Zeeo」的测试音频，或调低 `keywords_threshold`。

**Q: `ProcessInt16` 可以一次传入整段 10 秒音频吗？**  
A: 可以，但**不推荐**用于实时场景；流式应分块喂，才能低延迟触发业务。

**Q: 检测到一次会回调几次？**  
A: 通常每次命中 1 个 `KwsEvent`；`auto_reset_on_detect=true` 时同一句不会连续刷屏。

**Q: Mac 编的 `.dylib` 能在 Orange Pi 上用吗？**  
A: 不能。需在对应平台分别运行 `run_build_lib.sh` 生成 `.dylib` / `.so`。
