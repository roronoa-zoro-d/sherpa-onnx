// sherpa-onnx/csrc/sherpa-onnx-keyword-spotter-microphone.cc
//
// Copyright (c)  2024  Xiaomi Corporation

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <vector>

#include "sherpa-onnx/csrc/display.h"
#include "sherpa-onnx/csrc/keyword-spotter.h"
#include "sherpa-onnx/csrc/macros.h"
#include "sherpa-onnx/csrc/microphone.h"

bool stop = false;
float mic_sample_rate = 16000;

static void Handler(int32_t /*sig*/) {
  stop = true;
  fprintf(stderr, "\nCaught Ctrl + C. Exiting...\n");
}

int32_t main(int32_t argc, char *argv[]) {
  signal(SIGINT, Handler);

  const char *kUsageMessage = R"usage(
This program uses streaming models with microphone for keyword spotting.
Usage:

  ./bin/sherpa-onnx-keyword-spotter-microphone \
    --tokens=/path/to/tokens.txt \
    --encoder=/path/to/encoder.onnx \
    --decoder=/path/to/decoder.onnx \
    --joiner=/path/to/joiner.onnx \
    --provider=cpu \
    --num-threads=1 \
    --keywords-file=keywords.txt

Please refer to
https://k2-fsa.github.io/sherpa/onnx/kws/pretrained_models/index.html
for a list of pre-trained models to download.
)usage";

  sherpa_onnx::ParseOptions po(kUsageMessage);
  sherpa_onnx::KeywordSpotterConfig config;

  config.Register(&po);
  po.Read(argc, argv);
  if (po.NumArgs() != 0) {
    po.PrintUsage();
    SHERPA_ONNX_EXIT(EXIT_FAILURE);
  }

  fprintf(stderr, "%s\n", config.ToString().c_str());

  if (!config.Validate()) {
    fprintf(stderr, "Errors in config!\n");
    return -1;
  }

  sherpa_onnx::KeywordSpotter spotter(config);
  auto s = spotter.CreateStream();

  sherpa_onnx::Microphone mic;

  int32_t device_index = mic.GetDefaultInputDevice();
  if (device_index < 0) {
    fprintf(stderr, "No default input device found\n");
    fprintf(stderr, "If you are using Linux, please switch to \n");
    fprintf(stderr, " ./bin/sherpa-onnx-keyword-spotter-alsa \n");
    SHERPA_ONNX_EXIT(EXIT_FAILURE);
  }

  const char *pDeviceIndex = std::getenv("SHERPA_ONNX_MIC_DEVICE");
  if (pDeviceIndex) {
    fprintf(stderr, "Use specified device: %s\n", pDeviceIndex);
    device_index = atoi(pDeviceIndex);
  }

  mic.PrintDevices(device_index);

  const char *pSampleRateStr = std::getenv("SHERPA_ONNX_MIC_SAMPLE_RATE");
  if (pSampleRateStr) {
    mic_sample_rate = atof(pSampleRateStr);
    fprintf(stderr, "Use sample rate %f for mic\n", mic_sample_rate);
  }

  int32_t chunk = static_cast<int32_t>(0.1 * mic_sample_rate);
  if (!mic.OpenBlockingDevice(device_index, static_cast<int32_t>(mic_sample_rate),
                            1, chunk)) {
    fprintf(stderr, "portaudio error: %d\n", device_index);
    SHERPA_ONNX_EXIT(EXIT_FAILURE);
  }

  int32_t keyword_index = 0;
  sherpa_onnx::Display display;
  std::vector<float> samples(chunk);

  while (!stop) {
    int32_t n = mic.Read(samples.data(), chunk);
    if (n <= 0) {
      continue;
    }

    s->AcceptWaveform(static_cast<int32_t>(mic_sample_rate), samples.data(), n);

    while (spotter.IsReady(s.get())) {
      spotter.DecodeStream(s.get());

      const auto r = spotter.GetResult(s.get());
      if (!r.keyword.empty()) {
        display.Print(keyword_index, r.AsJsonString());
        fflush(stderr);
        keyword_index++;

        spotter.Reset(s.get());
      }
    }
  }

  return 0;
}
