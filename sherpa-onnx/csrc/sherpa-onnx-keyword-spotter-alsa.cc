// sherpa-onnx/csrc/sherpa-onnx-keyword-spotter-alsa.cc
//
// Copyright (c)  2024  Xiaomi Corporation
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "sherpa-onnx/csrc/alsa.h"
#include "sherpa-onnx/csrc/display.h"
#include "sherpa-onnx/csrc/keyword-spotter.h"
#include "sherpa-onnx/csrc/macros.h"
#include "sherpa-onnx/csrc/parse-options.h"
#include "zeeo_kws/kws_engine.h"

bool stop = false;

static void Handler(int sig) {
  stop = true;
  fprintf(stderr, "\nCaught Ctrl + C. Exiting...\n");
}

int main(int32_t argc, char *argv[]) {
  signal(SIGINT, Handler);

  const char *kUsageMessage = R"usage(
Usage:
  ./bin/sherpa-onnx-keyword-spotter-alsa \
    --tokens=/path/to/tokens.txt \
    --encoder=/path/to/encoder.onnx \
    --decoder=/path/to/decoder.onnx \
    --joiner=/path/to/joiner.onnx \
    --provider=cpu \
    --num-threads=2 \
    --keywords-file=keywords.txt \
    device_name

Please refer to
https://k2-fsa.github.io/sherpa/onnx/kws/pretrained_models/index.html
for a list of pre-trained models to download.

The device name specifies which microphone to use in case there are several
on your system. You can use

  arecord -l

to find all available microphones on your computer. For instance, if it outputs

**** List of CAPTURE Hardware Devices ****
card 3: UACDemoV10 [UACDemoV1.0], device 0: USB Audio [USB Audio]
  Subdevices: 1/1
  Subdevice #0: subdevice #0

and if you want to select card 3 and device 0 on that card, please use:

  plughw:3,0

as the device_name.
)usage";
  sherpa_onnx::ParseOptions po(kUsageMessage);
  sherpa_onnx::KeywordSpotterConfig config;

  config.Register(&po);

  po.Read(argc, argv);
  if (po.NumArgs() != 1) {
    fprintf(stderr, "Please provide only 1 argument: the device name\n");
    po.PrintUsage();
    SHERPA_ONNX_EXIT(EXIT_FAILURE);
  }

  fprintf(stderr, "%s\n", config.ToString().c_str());

  if (!config.Validate()) {
    fprintf(stderr, "Errors in config!\n");
    return -1;
  }

  zeeo_kws::KwsConfig kws_cfg;
  kws_cfg.tokens = config.model_config.tokens;
  kws_cfg.encoder = config.model_config.transducer.encoder;
  kws_cfg.decoder = config.model_config.transducer.decoder;
  kws_cfg.joiner = config.model_config.transducer.joiner;
  kws_cfg.keywords_file = config.keywords_file;
  kws_cfg.sample_rate = config.feat_config.sampling_rate;
  kws_cfg.num_threads = config.model_config.num_threads;
  kws_cfg.keywords_threshold = config.keywords_threshold;
  kws_cfg.keywords_score = config.keywords_score;

  std::unique_ptr<zeeo_kws::KwsEngine> kws_engine;
  try {
    kws_engine = std::make_unique<zeeo_kws::KwsEngine>(kws_cfg);
  } catch (const std::exception &e) {
    fprintf(stderr, "Failed to create KwsEngine: %s\n", e.what());
    return -1;
  }

  int32_t expected_sample_rate = kws_engine->sample_rate();

  std::string device_name = po.GetArg(1);
  sherpa_onnx::Alsa alsa(device_name.c_str());
  fprintf(stderr, "Use recording device: %s\n", device_name.c_str());

  if (alsa.GetExpectedSampleRate() != expected_sample_rate) {
    fprintf(stderr, "sample rate: %d != %d\n", alsa.GetExpectedSampleRate(),
            expected_sample_rate);
    SHERPA_ONNX_EXIT(-1);
  }

  int32_t chunk = 0.1 * alsa.GetActualSampleRate();

  sherpa_onnx::Display display;

  int32_t keyword_index = 0;
  std::vector<int16_t> pcm_chunk(chunk);

  while (!stop) {
    const std::vector<float> &samples = alsa.Read(chunk);

    for (size_t i = 0; i < samples.size(); ++i) {
      float v = samples[i];
      if (v > 1.f) {
        v = 1.f;
      } else if (v < -1.f) {
        v = -1.f;
      }
      pcm_chunk[i] = static_cast<int16_t>(v * 32767.f);
    }

    auto events = kws_engine->ProcessInt16(pcm_chunk.data(),
                                          static_cast<int32_t>(samples.size()));
    for (const auto &ev : events) {
      display.Print(keyword_index, ev.json);
      fflush(stderr);
      ++keyword_index;
    }
  }

  return 0;
}
