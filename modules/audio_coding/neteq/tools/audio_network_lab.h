/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_AUDIO_NETWORK_LAB_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_AUDIO_NETWORK_LAB_H_

#include <stdint.h>
#include <stddef.h>

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace webrtc {
namespace test {

enum class AudioNetworkLabRedundancyMode {
  kNone,
  kOpusFec,
  kRed,
  kDred,
};

absl::optional<AudioNetworkLabRedundancyMode> ParseAudioNetworkLabRedundancyMode(
    absl::string_view mode);
absl::string_view AudioNetworkLabRedundancyModeToString(
    AudioNetworkLabRedundancyMode mode);

struct FixedLossEvent {
  int start_ms = 0;
  int duration_ms = 0;
};

struct NetworkImpairmentConfig {
  int bandwidth_kbps = 0;
  int delay_ms = 0;
  int jitter_stddev_ms = 0;
  double loss_pct = 0.0;
  bool allow_reorder = false;
  int queue_len_packets = 0;
  int avg_burst_loss_length = 0;
  uint64_t random_seed = 1;
  std::vector<FixedLossEvent> fixed_loss_events;
};

struct AudioEncoderExperimentConfig {
  std::string codec = "opus";
  int bitrate_bps = 32000;
  int frame_ms = 20;
  int channels = 1;
  bool enable_dtx = false;
  AudioNetworkLabRedundancyMode redundancy_mode =
      AudioNetworkLabRedundancyMode::kNone;
  int expected_loss_pct = 0;
  // DRED recovery depth in 10 ms frames. Only used when redundancy_mode is
  // kDred. -1 means "derive from `expected_loss_pct` (or the simulated network
  // loss, whichever is larger) at runtime".
  int dred_duration_frames = -1;
  // When true, maps to WebRtcOpus_EnableCbr (SDP `cbr=1`). Default is VBR.
  bool cbr_enabled = false;
  // When true (and cbr_enabled is false), maps to Opus constrained VBR (CVBR).
  bool cvbr_enabled = false;
};

struct NetEqExperimentConfig {
  size_t max_packets = 200;
  bool fast_accelerate = false;
  int min_delay_ms = 0;
};

struct AudioNetworkLabConfig {
  std::string input_pcm;
  std::string output_audio;
  std::string summary_json;
  std::string textlog;
  AudioEncoderExperimentConfig encoder;
  NetworkImpairmentConfig network;
  NetEqExperimentConfig neteq;
};

struct AudioNetworkLabSummary {
  struct InputConfig {
    std::string input_pcm;
    int sample_rate_hz = 48000;
    int channels = 1;
    int input_duration_ms = 0;
  } input;

  struct EncoderConfig {
    std::string codec = "opus";
    int bitrate_bps = 32000;
    int frame_ms = 20;
    bool enable_dtx = false;
    std::string redundancy_mode = "none";
    int expected_loss_pct = 0;
    int dred_duration_frames = 0;
    std::string dred_duration_source = "none";
    bool cbr_enabled = false;
    bool cvbr_enabled = false;
  } encoder;

  struct NetworkConfig {
    int bandwidth_kbps = 0;
    int delay_ms = 0;
    int jitter_stddev_ms = 0;
    double loss_pct = 0.0;
    bool allow_reorder = false;
    int queue_len_packets = 0;
    int avg_burst_loss_length = 0;
    uint64_t random_seed = 1;
    std::vector<FixedLossEvent> fixed_loss_events;
  } network;

  struct NetEqConfig {
    size_t max_packets = 200;
    bool fast_accelerate = false;
    int min_delay_ms = 0;
  } neteq;

  struct EncodedStats {
    int packets = 0;
    size_t bytes = 0;
  } encoded;

  struct NetworkStats {
    int enqueued_packets = 0;
    int delivered_packets = 0;
    int lost_packets = 0;
    int queue_dropped_packets = 0;
    int fixed_loss_dropped_packets = 0;
    int reordered_packets = 0;
  } network_stats;

  struct OutputStats {
    std::string output_audio;
    int sample_rate_hz = 48000;
    int channels = 1;
    int decoded_duration_ms = 0;
    size_t decoded_samples = 0;
  } output;

  struct NetEqStats {
    int current_buffer_size_ms = 0;
    int preferred_buffer_size_ms = 0;
    int mean_waiting_time_ms = 0;
    int median_waiting_time_ms = 0;
    int min_waiting_time_ms = 0;
    int max_waiting_time_ms = 0;

    uint64_t total_samples_received = 0;
    uint64_t concealed_samples = 0;
    uint64_t silent_concealed_samples = 0;
    uint64_t concealment_events = 0;
    uint64_t fec_packets_received = 0;
    uint64_t fec_packets_discarded = 0;
    uint64_t packets_discarded = 0;
    uint64_t jitter_buffer_delay_ms = 0;
    uint64_t jitter_buffer_emitted_count = 0;
    uint64_t jitter_buffer_target_delay_ms = 0;
    uint64_t jitter_buffer_minimum_delay_ms = 0;
    uint64_t inserted_samples_for_deceleration = 0;
    uint64_t removed_samples_for_acceleration = 0;
    uint64_t delayed_packet_outage_samples = 0;
    uint64_t relative_packet_arrival_delay_ms = 0;
    uint64_t packet_buffer_flushes = 0;
    int32_t interruption_count = 0;
    int32_t total_interruption_duration_ms = 0;

    double expand_rate = 0.0;
    double speech_expand_rate = 0.0;
    double accelerate_rate = 0.0;
    double preemptive_expand_rate = 0.0;
    double secondary_decoded_rate = 0.0;
    double secondary_discarded_rate = 0.0;

    double max_expand_rate = 0.0;
    double max_secondary_decoded_rate = 0.0;
    double max_secondary_discarded_rate = 0.0;
  } neteq_stats;

  std::string ToJson() const;
};

struct AudioNetworkLabRunResult {
  bool ok = false;
  std::string error;
  AudioNetworkLabSummary summary;
};

class AudioNetworkLabRunner {
 public:
  AudioNetworkLabRunResult Run(const AudioNetworkLabConfig& config);
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_AUDIO_NETWORK_LAB_H_
