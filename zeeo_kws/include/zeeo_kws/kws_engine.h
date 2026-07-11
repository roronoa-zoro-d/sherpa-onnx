// zeeo_kws/include/zeeo_kws/kws_engine.h
//
// Team-facing keyword spotting wrapper (int16 mono 16 kHz PCM in).

#ifndef ZEEO_KWS_KWS_ENGINE_H_
#define ZEEO_KWS_KWS_ENGINE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace zeeo_kws {

/// Initialization options for KwsEngine.
struct KwsConfig {
  /// Directory containing encoder/decoder/joiner/tokens (required unless all
  /// model paths are set explicitly).
  std::string model_dir;

  /// Wake-word token file. Default: {model_dir}/keywords_zeeo.txt
  std::string keywords_file;

  /// Optional explicit model paths (override {model_dir}/<default_name>).
  std::string tokens;
  std::string encoder;
  std::string decoder;
  std::string joiner;

  /// Expected input sample rate. Must be 16000.
  int32_t sample_rate = 16000;

  int32_t num_threads = 2;
  float keywords_threshold = 0.25f;
  float keywords_score = 1.0f;

  /// Reset decoder state automatically after each detection.
  bool auto_reset_on_detect = true;
};

/// One keyword-spotting hit.
struct KwsEvent {
  std::string keyword;
  std::vector<std::string> tokens;
  std::vector<float> timestamps;
  float start_time = 0.f;
  std::string json;
};

/// Streaming KWS engine. Not thread-safe: call ProcessInt16 from one thread only.
class KwsEngine {
 public:
  explicit KwsEngine(const KwsConfig &config);
  ~KwsEngine();

  KwsEngine(const KwsEngine &) = delete;
  KwsEngine &operator=(const KwsEngine &) = delete;

  /// Feed int16 mono PCM at sample_rate() Hz.
  /// Chunk size is arbitrary; 1600 (0.1 s) or 3200 (0.2 s) is recommended.
  /// Returns newly detected events from this call (usually 0 or 1).
  std::vector<KwsEvent> ProcessInt16(const int16_t *samples,
                                     int32_t num_samples);

  /// Clear decoder state manually.
  void Reset();

  int32_t sample_rate() const;
  const KwsConfig &config() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace zeeo_kws

#endif  // ZEEO_KWS_KWS_ENGINE_H_
