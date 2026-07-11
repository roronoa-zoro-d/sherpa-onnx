// zeeo_kws/src/feed_pcm_demo.cc
//
// 同事接入示例：从 wav 读出 int16 PCM，分块调用 KwsEngine::ProcessInt16。
// 不依赖 sherpa-onnx 头文件，只需 zeeo_kws/include + zeeo_kws/lib。

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "zeeo_kws/kws_engine.h"

namespace {

struct WavPcm16 {
  int32_t sample_rate = 0;
  std::vector<int16_t> samples;
};

bool ReadWavMonoPcm16(const std::string &path, WavPcm16 *out) {
  std::ifstream is(path, std::ios::binary);
  if (!is) {
    return false;
  }

  char riff[4];
  is.read(riff, 4);
  if (std::strncmp(riff, "RIFF", 4) != 0) {
    return false;
  }

  is.seekg(4, std::ios::cur);  // chunk size

  char wave[4];
  is.read(wave, 4);
  if (std::strncmp(wave, "WAVE", 4) != 0) {
    return false;
  }

  uint16_t audio_format = 0;
  uint16_t num_channels = 0;
  uint32_t sample_rate = 0;
  uint16_t bits_per_sample = 0;
  std::vector<int16_t> pcm;
  bool got_fmt = false;
  bool got_data = false;

  while (is && !(got_fmt && got_data)) {
    char chunk_id[4];
    if (!is.read(chunk_id, 4)) {
      break;
    }
    uint32_t chunk_size = 0;
    is.read(reinterpret_cast<char *>(&chunk_size), 4);

    if (std::strncmp(chunk_id, "fmt ", 4) == 0) {
      is.read(reinterpret_cast<char *>(&audio_format), 2);
      is.read(reinterpret_cast<char *>(&num_channels), 2);
      is.read(reinterpret_cast<char *>(&sample_rate), 4);
      is.seekg(6, std::ios::cur);
      is.read(reinterpret_cast<char *>(&bits_per_sample), 2);
      if (chunk_size > 16) {
        is.seekg(chunk_size - 16, std::ios::cur);
      }
      got_fmt = true;
    } else if (std::strncmp(chunk_id, "data", 4) == 0) {
      if (chunk_size % 2 != 0) {
        return false;
      }
      pcm.resize(chunk_size / 2);
      is.read(reinterpret_cast<char *>(pcm.data()), chunk_size);
      got_data = true;
    } else {
      is.seekg(chunk_size, std::ios::cur);
    }
  }

  if (!got_fmt || !got_data || audio_format != 1 || bits_per_sample != 16) {
    return false;
  }

  if (num_channels == 1) {
    out->sample_rate = static_cast<int32_t>(sample_rate);
    out->samples = std::move(pcm);
    return true;
  }

  if (num_channels != 2) {
    return false;
  }

  out->sample_rate = static_cast<int32_t>(sample_rate);
  out->samples.resize(pcm.size() / 2);
  for (size_t i = 0; i < out->samples.size(); ++i) {
    out->samples[i] = pcm[i * 2];
  }
  return true;
}

void PrintUsage(const char *prog) {
  fprintf(stderr,
          "用法:\n"
          "  %s <model_dir> <wav_file> [keywords_file]\n"
          "\n"
          "示例:\n"
          "  %s models/sherpa-onnx-kws-zipformer-wenetspeech-3.3M-2024-01-01-mobile "
          "audio_record/test.wav\n",
          prog, prog);
}

}  // namespace

int main(int argc, char *argv[]) {
  if (argc < 3) {
    PrintUsage(argv[0]);
    return EXIT_FAILURE;
  }

  const std::string model_dir = argv[1];
  const std::string wav_file = argv[2];

  WavPcm16 wav;
  if (!ReadWavMonoPcm16(wav_file, &wav)) {
    fprintf(stderr, "无法读取 wav（需 PCM 16bit mono/stereo）: %s\n",
            wav_file.c_str());
    return EXIT_FAILURE;
  }
  if (wav.sample_rate != 16000) {
    fprintf(stderr, "需要 16 kHz wav，当前 %d Hz\n", wav.sample_rate);
    return EXIT_FAILURE;
  }

  zeeo_kws::KwsConfig cfg;
  cfg.model_dir = model_dir;
  if (argc >= 4) {
    cfg.keywords_file = argv[3];
  }

  try {
    zeeo_kws::KwsEngine engine(cfg);
    constexpr int32_t kChunk = 3200;  // 0.2 s
    int32_t index = 0;

    for (size_t offset = 0; offset < wav.samples.size(); offset += kChunk) {
      int32_t n = static_cast<int32_t>(std::min(
          static_cast<size_t>(kChunk), wav.samples.size() - offset));
      auto events = engine.ProcessInt16(wav.samples.data() + offset, n);
      for (const auto &ev : events) {
        ++index;
        fprintf(stderr, "%d:%s\n", index, ev.json.c_str());
      }
    }

    fprintf(stderr, "完成，共检测到 %d 次唤醒\n", index);
  } catch (const std::exception &e) {
    fprintf(stderr, "错误: %s\n", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
