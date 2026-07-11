// 从 ALSA 麦克风录音并保存为 WAV（16kHz / mono / S16_LE）
// 用法: record_mic [时长秒] [输出文件] [设备名]
//
// 录音核心流程:
//   1. snd_pcm_open      打开 ALSA 设备（如 plughw:2,0）
//   2. snd_pcm_hw_params 配置采样率/声道/格式
//   3. snd_pcm_readi     循环从麦克风读 PCM 数据  ← 真正“采音”在这里
//   4. fwrite            把 PCM 写入 wav 文件

#include <alsa/asoundlib.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

// KWS 常用格式，与 arecord -f S16_LE -r 16000 -c 1 一致
constexpr uint32_t kSampleRate = 16000;
constexpr uint16_t kChannels = 1;
constexpr uint16_t kBitsPerSample = 16;

// 标准 PCM WAV 文件头（44 字节）
struct WavHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t chunk_size = 0;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  uint32_t fmt_size = 16;
  uint16_t audio_format = 1;  // 1 = PCM
  uint16_t num_channels = kChannels;
  uint32_t sample_rate = kSampleRate;
  uint32_t byte_rate = kSampleRate * kChannels * kBitsPerSample / 8;
  uint16_t block_align = kChannels * kBitsPerSample / 8;
  uint16_t bits_per_sample = kBitsPerSample;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t data_size = 0;  // 录音结束后回填实际 PCM 字节数
};

void Die(const char *msg) {
  std::fprintf(stderr, "错误: %s\n", msg);
  std::exit(1);
}

void DieAlsa(const char *msg, int err) {
  std::fprintf(stderr, "错误: %s: %s\n", msg, snd_strerror(err));
  std::exit(1);
}

// 录音结束后回到文件开头，写入正确的 WAV 头
void WriteHeader(FILE *fp, uint32_t data_bytes) {
  WavHeader h;
  h.data_size = data_bytes;
  h.chunk_size = 36 + data_bytes;

  if (std::fseek(fp, 0, SEEK_SET) != 0) {
    Die("无法回写 WAV 头");
  }
  if (std::fwrite(&h, sizeof(h), 1, fp) != 1) {
    Die("无法写入 WAV 头");
  }
}

}  // namespace

int main(int argc, char *argv[]) {
  // ---------- 命令行参数 ----------
  int duration_sec = 10;
  std::string output = "test.wav";
  std::string device = "plughw:2,0";  // Orange Pi 5 板载 ES8388，card 2

  if (argc >= 2) {
    duration_sec = std::atoi(argv[1]);
  }
  if (argc >= 3) {
    output = argv[2];
  }
  if (argc >= 4) {
    device = argv[3];
  }

  if (duration_sec <= 0) {
    Die("时长必须 > 0");
  }

  std::printf("设备: %s\n", device.c_str());
  std::printf("时长: %d 秒\n", duration_sec);
  std::printf("输出: %s\n", output.c_str());
  std::printf("格式: %u Hz, %u ch, S16_LE\n", kSampleRate, kChannels);

  // ---------- 1. 打开 ALSA 录音设备 ----------
  snd_pcm_t *handle = nullptr;
  int err = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_CAPTURE, 0);
  if (err < 0) {
    DieAlsa("无法打开录音设备", err);
  }

  // ---------- 2. 配置硬件参数 ----------
  snd_pcm_hw_params_t *hw_params;
  snd_pcm_hw_params_alloca(&hw_params);
  snd_pcm_hw_params_any(handle, hw_params);
  snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(handle, hw_params, kChannels);

  unsigned int rate = kSampleRate;
  int dir = 0;
  snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, &dir);
  snd_pcm_hw_params(handle, hw_params);  // 参数生效

  // 每次从 ALSA 读 1024 帧；每帧 = 1 个采样点（mono）
  snd_pcm_uframes_t frames_per_read = 1024;
  const size_t bytes_per_frame = kChannels * kBitsPerSample / 8;
  std::vector<int16_t> buffer(frames_per_read * kChannels);

  // ---------- 3. 创建 WAV 文件，先写占位头 ----------
  FILE *fp = std::fopen(output.c_str(), "wb");
  if (!fp) {
    Die("无法创建输出文件");
  }

  WavHeader placeholder;
  if (std::fwrite(&placeholder, sizeof(placeholder), 1, fp) != 1) {
    Die("无法写入 WAV 头占位");
  }

  const int total_frames = duration_sec * static_cast<int>(rate);
  int captured_frames = 0;
  uint32_t data_bytes = 0;

  std::printf("开始录音...\n");

  // ========== 4. 录音主循环（核心） ==========
  // snd_pcm_readi: 从麦克风驱动读取一帧帧 PCM 到 buffer
  // fwrite:        把 buffer 追加写入 wav 的 data 区
  while (captured_frames < total_frames) {
    int frames_to_read = static_cast<int>(frames_per_read);
    if (captured_frames + frames_to_read > total_frames) {
      frames_to_read = total_frames - captured_frames;  // 最后一包可能不足 1024 帧
    }

    // ★ 真正从麦克风读音频数据 ★
    int rc = snd_pcm_readi(handle, buffer.data(), frames_to_read);
    if (rc == -EPIPE) {
      // 缓冲区溢出，重新 prepare 后继续
      snd_pcm_prepare(handle);
      continue;
    }
    if (rc < 0) {
      DieAlsa("读取音频失败", rc);
    }

    // 把读到的 PCM 写入文件
    size_t n = static_cast<size_t>(rc) * bytes_per_frame;
    if (std::fwrite(buffer.data(), 1, n, fp) != n) {
      Die("写入文件失败");
    }

    captured_frames += rc;
    data_bytes += static_cast<uint32_t>(n);
  }

  // ---------- 5. 回填 WAV 头，关闭资源 ----------
  WriteHeader(fp, data_bytes);
  std::fclose(fp);
  snd_pcm_close(handle);

  std::printf("完成: %s (%u 字节 PCM, 约 %d 秒)\n", output.c_str(), data_bytes,
              duration_sec);
  return 0;
}
