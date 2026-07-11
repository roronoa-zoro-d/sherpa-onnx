// zeeo_kws/core/kws_engine.cc

#include "zeeo_kws/kws_engine.h"

#include <sys/stat.h>

#include <stdexcept>
#include <utility>

#include "sherpa-onnx/csrc/keyword-spotter.h"

namespace zeeo_kws {
namespace {

constexpr const char *kDefaultEncoder =
    "encoder-epoch-12-avg-2-chunk-16-left-64.int8.onnx";
constexpr const char *kDefaultDecoder =
    "decoder-epoch-12-avg-2-chunk-16-left-64.onnx";
constexpr const char *kDefaultJoiner =
    "joiner-epoch-12-avg-2-chunk-16-left-64.int8.onnx";
constexpr const char *kDefaultTokens = "tokens.txt";
constexpr const char *kDefaultKeywords = "keywords_zeeo.txt";

bool FileExists(const std::string &path) {
  if (path.empty()) {
    return false;
  }
  struct stat st {};
  return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string JoinPath(const std::string &dir, const std::string &name) {
  if (dir.empty()) {
    return name;
  }
  if (dir.back() == '/') {
    return dir + name;
  }
  return dir + "/" + name;
}

KwsConfig NormalizeConfig(KwsConfig cfg) {
  if (cfg.model_dir.empty() && cfg.encoder.empty()) {
    throw std::runtime_error(
        "zeeo_kws: model_dir or explicit encoder path is required");
  }

  if (cfg.tokens.empty()) {
    cfg.tokens = JoinPath(cfg.model_dir, kDefaultTokens);
  }
  if (cfg.encoder.empty()) {
    cfg.encoder = JoinPath(cfg.model_dir, kDefaultEncoder);
  }
  if (cfg.decoder.empty()) {
    cfg.decoder = JoinPath(cfg.model_dir, kDefaultDecoder);
  }
  if (cfg.joiner.empty()) {
    cfg.joiner = JoinPath(cfg.model_dir, kDefaultJoiner);
  }
  if (cfg.keywords_file.empty()) {
    cfg.keywords_file = JoinPath(cfg.model_dir, kDefaultKeywords);
  }

  return cfg;
}

void ValidatePaths(const KwsConfig &cfg) {
  const char *names[] = {"tokens", "encoder", "decoder", "joiner",
                         "keywords_file"};
  const std::string *paths[] = {&cfg.tokens, &cfg.encoder, &cfg.decoder,
                                &cfg.joiner, &cfg.keywords_file};

  for (int32_t i = 0; i < 5; ++i) {
    if (!FileExists(*paths[i])) {
      throw std::runtime_error(std::string("zeeo_kws: missing ") + names[i] +
                               ": " + *paths[i]);
    }
  }
}

sherpa_onnx::KeywordSpotterConfig BuildSpotterConfig(const KwsConfig &cfg) {
  sherpa_onnx::KeywordSpotterConfig spotter_cfg;
  spotter_cfg.model_config.transducer.encoder = cfg.encoder;
  spotter_cfg.model_config.transducer.decoder = cfg.decoder;
  spotter_cfg.model_config.transducer.joiner = cfg.joiner;
  spotter_cfg.model_config.tokens = cfg.tokens;
  spotter_cfg.model_config.provider_config.provider = "cpu";
  spotter_cfg.model_config.num_threads = cfg.num_threads;
  spotter_cfg.keywords_file = cfg.keywords_file;
  spotter_cfg.keywords_threshold = cfg.keywords_threshold;
  spotter_cfg.keywords_score = cfg.keywords_score;
  spotter_cfg.feat_config.sampling_rate = cfg.sample_rate;
  return spotter_cfg;
}

KwsEvent ToEvent(const sherpa_onnx::KeywordResult &r) {
  KwsEvent ev;
  ev.keyword = r.keyword;
  ev.tokens = r.tokens;
  ev.timestamps = r.timestamps;
  ev.start_time = r.start_time;
  ev.json = r.AsJsonString();
  return ev;
}

void Int16ToFloat(const int16_t *in, int32_t n, std::vector<float> *out) {
  out->resize(n);
  for (int32_t i = 0; i < n; ++i) {
    (*out)[i] = in[i] / 32768.f;
  }
}

}  // namespace

struct KwsEngine::Impl {
  KwsConfig config;
  std::unique_ptr<sherpa_onnx::KeywordSpotter> spotter;
  std::unique_ptr<sherpa_onnx::OnlineStream> stream;
  std::vector<float> float_buf;

  explicit Impl(KwsConfig cfg) : config(NormalizeConfig(std::move(cfg))) {
    if (config.sample_rate != 16000) {
      throw std::runtime_error("zeeo_kws: sample_rate must be 16000");
    }
    ValidatePaths(config);
    auto spotter_cfg = BuildSpotterConfig(config);
    if (!spotter_cfg.Validate()) {
      throw std::runtime_error("zeeo_kws: invalid spotter config");
    }
    spotter = std::make_unique<sherpa_onnx::KeywordSpotter>(spotter_cfg);
    stream = spotter->CreateStream();
  }
};

KwsEngine::KwsEngine(const KwsConfig &config)
    : impl_(std::make_unique<Impl>(config)) {}

KwsEngine::~KwsEngine() = default;

std::vector<KwsEvent> KwsEngine::ProcessInt16(const int16_t *samples,
                                              int32_t num_samples) {
  if (samples == nullptr || num_samples <= 0) {
    return {};
  }

  Int16ToFloat(samples, num_samples, &impl_->float_buf);
  impl_->stream->AcceptWaveform(impl_->config.sample_rate, impl_->float_buf.data(),
                                impl_->float_buf.size());

  std::vector<KwsEvent> events;
  while (impl_->spotter->IsReady(impl_->stream.get())) {
    impl_->spotter->DecodeStream(impl_->stream.get());

    const auto r = impl_->spotter->GetResult(impl_->stream.get());
    if (!r.keyword.empty()) {
      events.push_back(ToEvent(r));
      if (impl_->config.auto_reset_on_detect) {
        impl_->spotter->Reset(impl_->stream.get());
      }
    }
  }
  return events;
}

void KwsEngine::Reset() { impl_->spotter->Reset(impl_->stream.get()); }

int32_t KwsEngine::sample_rate() const { return impl_->config.sample_rate; }

const KwsConfig &KwsEngine::config() const { return impl_->config; }

}  // namespace zeeo_kws
