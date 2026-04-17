/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/audio_network_lab.h"

#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/audio/audio_frame.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "api/field_trials_view.h"
#include "api/neteq/neteq.h"
#include "api/rtp_headers.h"
#include "call/simulated_network.h"
#include "common_audio/wav_file.h"
#include "modules/audio_coding/codecs/red/audio_encoder_copy_red.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/neteq/default_neteq_factory.h"
#include "modules/audio_coding/neteq/tools/input_audio_file.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace test {
namespace {

constexpr int kInputSampleRateHz = 48000;
constexpr int kInputFrameMs = 10;
constexpr int kInputSamplesPerChannel10ms = kInputSampleRateHz / 100;
constexpr int kOpusPayloadType = 111;
constexpr int kRedPayloadType = 117;
constexpr uint32_t kSsrc = 0x13572468;

class EmptyFieldTrialsView final : public FieldTrialsView {
 public:
  std::string Lookup(absl::string_view key) const override { return ""; }
};

class OptionalTextLog {
 public:
  explicit OptionalTextLog(absl::string_view path) {
    if (!path.empty()) {
      stream_.open(std::string(path));
    }
  }

  bool valid() const { return !stream_.is_open() || stream_.good(); }

  void WriteLine(absl::string_view line) {
    if (stream_.is_open()) {
      stream_ << line << '\n';
    }
  }

 private:
  std::ofstream stream_;
};

class AudioOutputSink {
 public:
  virtual ~AudioOutputSink() = default;
  virtual bool Write(const int16_t* audio, size_t num_samples) = 0;
};

class NullAudioOutputSink final : public AudioOutputSink {
 public:
  bool Write(const int16_t* audio, size_t num_samples) override { return true; }
};

class PcmAudioOutputSink final : public AudioOutputSink {
 public:
  explicit PcmAudioOutputSink(absl::string_view path)
      : file_(fopen(std::string(path).c_str(), "wb")) {}

  ~PcmAudioOutputSink() override {
    if (file_ != nullptr) {
      fclose(file_);
    }
  }

  bool valid() const { return file_ != nullptr; }

  bool Write(const int16_t* audio, size_t num_samples) override {
    RTC_DCHECK(file_ != nullptr);
    return fwrite(audio, sizeof(*audio), num_samples, file_) == num_samples;
  }

 private:
  FILE* file_ = nullptr;
};

class WavAudioOutputSink final : public AudioOutputSink {
 public:
  WavAudioOutputSink(absl::string_view path, int sample_rate_hz, int channels)
      : writer_(std::string(path), sample_rate_hz, channels) {}

  bool Write(const int16_t* audio, size_t num_samples) override {
    writer_.WriteSamples(audio, num_samples);
    return true;
  }

 private:
  WavWriter writer_;
};

struct EncodedPacket {
  uint64_t packet_id = 0;
  int64_t send_time_us = 0;
  RTPHeader header;
  rtc::Buffer payload;
};

class MemoryPacketizationCallback final : public AudioPacketizationCallback {
 public:
  void set_send_time_us(int64_t send_time_us) { send_time_us_ = send_time_us; }

  std::vector<EncodedPacket> TakePackets() { return std::move(packets_); }

  int32_t SendData(AudioFrameType frame_type,
                   uint8_t payload_type,
                   uint32_t timestamp,
                   const uint8_t* payload_data,
                   size_t payload_len_bytes,
                   int64_t absolute_capture_timestamp_ms) override {
    EncodedPacket packet;
    packet.packet_id = next_packet_id_++;
    packet.send_time_us = send_time_us_;
    packet.header.payloadType = payload_type;
    packet.header.sequenceNumber = next_sequence_number_++;
    packet.header.timestamp = timestamp;
    packet.header.ssrc = kSsrc;
    packet.payload.AppendData(payload_data, payload_len_bytes);
    packets_.push_back(std::move(packet));
    return 0;
  }

 private:
  int64_t send_time_us_ = 0;
  uint64_t next_packet_id_ = 1;
  uint16_t next_sequence_number_ = 0;
  std::vector<EncodedPacket> packets_;
};

double Q14ToDouble(uint16_t value) {
  return static_cast<double>(value) / static_cast<double>(1 << 14);
}

bool EndsWith(absl::string_view value, absl::string_view suffix) {
  return value.size() >= suffix.size() &&
         value.substr(value.size() - suffix.size()) == suffix;
}

std::string EscapeJsonString(absl::string_view input) {
  std::string result;
  result.reserve(input.size());
  for (char c : input) {
    switch (c) {
      case '\\':
        result += "\\\\";
        break;
      case '"':
        result += "\\\"";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        result += c;
        break;
    }
  }
  return result;
}

bool ValidateNetworkImpairmentConfig(const NetworkImpairmentConfig& config,
                                     std::string* error) {
  if (config.avg_burst_loss_length > 1) {
    if (config.loss_pct <= 0.0 || config.loss_pct >= 100.0) {
      *error =
          "Burst loss model (--avg_burst_loss_length > 1) requires "
          "0 < loss_pct < 100.";
      return false;
    }
    const double prob_loss = config.loss_pct / 100.0;
    const int min_avg_burst = static_cast<int>(
        std::ceil(prob_loss / (1.0 - prob_loss)));
    if (config.avg_burst_loss_length <= min_avg_burst) {
      std::ostringstream oss;
      oss << "For loss_pct=" << config.loss_pct
          << ", --avg_burst_loss_length must be greater than " << min_avg_burst
          << " (SimulatedNetwork Gilbert-Elliot constraint).";
      *error = oss.str();
      return false;
    }
  }
  return true;
}

bool IsPacketDroppedByFixedLoss(const std::vector<FixedLossEvent>& events,
                                int64_t send_time_us) {
  const int send_time_ms = static_cast<int>(send_time_us / 1000);
  for (const FixedLossEvent& event : events) {
    if (send_time_ms >= event.start_ms &&
        send_time_ms < event.start_ms + event.duration_ms) {
      return true;
    }
  }
  return false;
}

std::string BuildFixedLossEventsJson(const std::vector<FixedLossEvent>& events) {
  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < events.size(); ++i) {
    if (i > 0) {
      oss << ", ";
    }
    oss << "{\"start_ms\": " << events[i].start_ms << ", \"duration_ms\": "
        << events[i].duration_ms << "}";
  }
  oss << "]";
  return oss.str();
}

absl::optional<int64_t> MinOptionalUs(absl::optional<int64_t> a,
                                      absl::optional<int64_t> b) {
  if (!a.has_value()) {
    return b;
  }
  if (!b.has_value()) {
    return a;
  }
  return std::min(*a, *b);
}

// Maps a packet-loss hint (percent) to a DRED recovery depth in 10 ms frames.
// Mirrors the spirit of the xiph/webrtc-opus-ng adaptive control: keep a
// short safety net at low loss, extend it monotonically as loss grows, and
// cap at libopus' documented 1 s upper bound. Guaranteed non-zero whenever
// DRED is enabled so the recovery path remains active even at ~0% loss.
int AutoDredDurationFramesFromLoss(int loss_pct) {
  if (loss_pct <= 2) {
    return 20;  // 200 ms safety net.
  }
  if (loss_pct <= 5) {
    return 30;  // 300 ms.
  }
  if (loss_pct <= 10) {
    return 50;  // 500 ms.
  }
  if (loss_pct <= 20) {
    return 80;  // 800 ms.
  }
  return 100;  // 1 s (libopus maximum).
}

struct DredSelection {
  int duration_frames = 0;
  std::string source = "disabled";
};

DredSelection SelectDredDuration(const AudioEncoderExperimentConfig& encoder,
                                 double network_loss_pct) {
  DredSelection selection;
  if (encoder.redundancy_mode != AudioNetworkLabRedundancyMode::kDred) {
    return selection;
  }
  if (encoder.dred_duration_frames > 0) {
    selection.duration_frames = std::min(encoder.dred_duration_frames, 100);
    selection.source = "flag";
    return selection;
  }
  const int loss_hint = std::max(
      encoder.expected_loss_pct,
      static_cast<int>(std::ceil(network_loss_pct)));
  selection.duration_frames = AutoDredDurationFramesFromLoss(loss_hint);
  selection.source = "auto";
  return selection;
}

AudioEncoderOpus::Config CreateOpusConfig(
    const AudioEncoderExperimentConfig& config,
    const DredSelection& dred_selection) {
  AudioEncoderOpus::Config opus_config;
  opus_config.sample_rate_hz = kInputSampleRateHz;
  opus_config.num_channels = config.channels;
  opus_config.frame_size_ms = config.frame_ms;
  opus_config.bitrate_bps = config.bitrate_bps;
  opus_config.application = config.channels == 1
                                ? AudioEncoderOpus::Config::ApplicationMode::
                                      kVoip
                                : AudioEncoderOpus::Config::ApplicationMode::
                                      kAudio;
  opus_config.fec_enabled =
      config.redundancy_mode == AudioNetworkLabRedundancyMode::kOpusFec;
  if (config.redundancy_mode == AudioNetworkLabRedundancyMode::kDred) {
    opus_config.dred = dred_selection.duration_frames;
  }
  opus_config.dtx_enabled = config.enable_dtx;
  return opus_config;
}

std::unique_ptr<AudioEncoder> CreateEncoderStack(
    const AudioEncoderExperimentConfig& config,
    const DredSelection& dred_selection,
    std::string* error) {
  if (!absl::EqualsIgnoreCase(config.codec, "opus")) {
    *error = "Only codec=opus is supported in audio_network_lab.";
    return nullptr;
  }

  AudioEncoderOpus::Config opus_config = CreateOpusConfig(config, dred_selection);
  if (!opus_config.IsOk()) {
    *error = "Invalid Opus encoder configuration.";
    return nullptr;
  }

  std::unique_ptr<AudioEncoder> encoder =
      AudioEncoderOpus::MakeAudioEncoder(opus_config, kOpusPayloadType);
  if (!encoder) {
    *error = "Failed to create Opus encoder.";
    return nullptr;
  }

  if (config.redundancy_mode == AudioNetworkLabRedundancyMode::kRed) {
    AudioEncoderCopyRed::Config red_config;
    red_config.payload_type = kRedPayloadType;
    red_config.speech_encoder = std::move(encoder);
    static const EmptyFieldTrialsView kEmptyFieldTrials;
    encoder =
        std::make_unique<AudioEncoderCopyRed>(std::move(red_config),
                                              kEmptyFieldTrials);
  }
  return encoder;
}

std::unique_ptr<AudioOutputSink> CreateOutputSink(const AudioNetworkLabConfig& config,
                                                  int sample_rate_hz,
                                                  int channels,
                                                  std::string* error) {
  if (config.output_audio.empty()) {
    return std::make_unique<NullAudioOutputSink>();
  }

  FILE* probe = fopen(config.output_audio.c_str(), "wb");
  if (probe == nullptr) {
    *error = "Failed to open output audio file for writing.";
    return nullptr;
  }
  fclose(probe);

  if (EndsWith(config.output_audio, ".wav")) {
    return std::make_unique<WavAudioOutputSink>(config.output_audio,
                                                sample_rate_hz, channels);
  }

  auto sink = std::make_unique<PcmAudioOutputSink>(config.output_audio);
  if (!sink->valid()) {
    *error = "Failed to open output audio file for writing.";
    return nullptr;
  }
  return sink;
}

void UpdateMaxNetEqRates(const NetEqNetworkStatistics& stats,
                         AudioNetworkLabSummary* summary) {
  summary->neteq_stats.max_expand_rate =
      std::max(summary->neteq_stats.max_expand_rate,
               Q14ToDouble(stats.expand_rate));
  summary->neteq_stats.max_secondary_decoded_rate =
      std::max(summary->neteq_stats.max_secondary_decoded_rate,
               Q14ToDouble(stats.secondary_decoded_rate));
  summary->neteq_stats.max_secondary_discarded_rate =
      std::max(summary->neteq_stats.max_secondary_discarded_rate,
               Q14ToDouble(stats.secondary_discarded_rate));
}

void PopulateFinalNetEqStats(NetEq* neteq, AudioNetworkLabSummary* summary) {
  const NetEqNetworkStatistics current = neteq->CurrentNetworkStatistics();
  const NetEqLifetimeStatistics lifetime = neteq->GetLifetimeStatistics();
  const NetEqOperationsAndState operations = neteq->GetOperationsAndState();

  summary->neteq_stats.current_buffer_size_ms =
      current.current_buffer_size_ms;
  summary->neteq_stats.preferred_buffer_size_ms =
      current.preferred_buffer_size_ms;
  summary->neteq_stats.mean_waiting_time_ms = current.mean_waiting_time_ms;
  summary->neteq_stats.median_waiting_time_ms = current.median_waiting_time_ms;
  summary->neteq_stats.min_waiting_time_ms = current.min_waiting_time_ms;
  summary->neteq_stats.max_waiting_time_ms = current.max_waiting_time_ms;
  summary->neteq_stats.expand_rate = Q14ToDouble(current.expand_rate);
  summary->neteq_stats.speech_expand_rate =
      Q14ToDouble(current.speech_expand_rate);
  summary->neteq_stats.accelerate_rate = Q14ToDouble(current.accelerate_rate);
  summary->neteq_stats.preemptive_expand_rate =
      Q14ToDouble(current.preemptive_rate);
  summary->neteq_stats.secondary_decoded_rate =
      Q14ToDouble(current.secondary_decoded_rate);
  summary->neteq_stats.secondary_discarded_rate =
      Q14ToDouble(current.secondary_discarded_rate);

  summary->neteq_stats.total_samples_received =
      lifetime.total_samples_received;
  summary->neteq_stats.concealed_samples = lifetime.concealed_samples;
  summary->neteq_stats.silent_concealed_samples =
      lifetime.silent_concealed_samples;
  summary->neteq_stats.concealment_events = lifetime.concealment_events;
  summary->neteq_stats.fec_packets_received = lifetime.fec_packets_received;
  summary->neteq_stats.fec_packets_discarded = lifetime.fec_packets_discarded;
  summary->neteq_stats.packets_discarded = lifetime.packets_discarded;
  summary->neteq_stats.jitter_buffer_delay_ms =
      lifetime.jitter_buffer_delay_ms;
  summary->neteq_stats.jitter_buffer_emitted_count =
      lifetime.jitter_buffer_emitted_count;
  summary->neteq_stats.jitter_buffer_target_delay_ms =
      lifetime.jitter_buffer_target_delay_ms;
  summary->neteq_stats.jitter_buffer_minimum_delay_ms =
      lifetime.jitter_buffer_minimum_delay_ms;
  summary->neteq_stats.inserted_samples_for_deceleration =
      lifetime.inserted_samples_for_deceleration;
  summary->neteq_stats.removed_samples_for_acceleration =
      lifetime.removed_samples_for_acceleration;
  summary->neteq_stats.delayed_packet_outage_samples =
      lifetime.delayed_packet_outage_samples;
  summary->neteq_stats.relative_packet_arrival_delay_ms =
      lifetime.relative_packet_arrival_delay_ms;
  summary->neteq_stats.interruption_count = lifetime.interruption_count;
  summary->neteq_stats.total_interruption_duration_ms =
      lifetime.total_interruption_duration_ms;
  summary->neteq_stats.packet_buffer_flushes =
      operations.packet_buffer_flushes;
}

bool WriteSummaryFile(const AudioNetworkLabSummary& summary,
                      absl::string_view path) {
  std::ofstream out{std::string(path)};
  if (!out.is_open()) {
    return false;
  }
  out << summary.ToJson() << '\n';
  return out.good();
}

}  // namespace

absl::optional<AudioNetworkLabRedundancyMode>
ParseAudioNetworkLabRedundancyMode(absl::string_view mode) {
  std::string lowered = std::string(mode);
  absl::AsciiStrToLower(&lowered);
  if (lowered == "none") {
    return AudioNetworkLabRedundancyMode::kNone;
  }
  if (lowered == "opus_fec") {
    return AudioNetworkLabRedundancyMode::kOpusFec;
  }
  if (lowered == "red") {
    return AudioNetworkLabRedundancyMode::kRed;
  }
  if (lowered == "dred") {
    return AudioNetworkLabRedundancyMode::kDred;
  }
  return absl::nullopt;
}

absl::string_view AudioNetworkLabRedundancyModeToString(
    AudioNetworkLabRedundancyMode mode) {
  switch (mode) {
    case AudioNetworkLabRedundancyMode::kNone:
      return "none";
    case AudioNetworkLabRedundancyMode::kOpusFec:
      return "opus_fec";
    case AudioNetworkLabRedundancyMode::kRed:
      return "red";
    case AudioNetworkLabRedundancyMode::kDred:
      return "dred";
  }
  return "unknown";
}

std::string AudioNetworkLabSummary::ToJson() const {
  std::ostringstream oss;
  oss << "{\n"
      << "  \"input\": {\n"
      << "    \"input_pcm\": \"" << EscapeJsonString(input.input_pcm) << "\",\n"
      << "    \"sample_rate_hz\": " << input.sample_rate_hz << ",\n"
      << "    \"channels\": " << input.channels << ",\n"
      << "    \"input_duration_ms\": " << input.input_duration_ms << "\n"
      << "  },\n"
      << "  \"encoder\": {\n"
      << "    \"codec\": \"" << EscapeJsonString(encoder.codec) << "\",\n"
      << "    \"bitrate_bps\": " << encoder.bitrate_bps << ",\n"
      << "    \"frame_ms\": " << encoder.frame_ms << ",\n"
      << "    \"enable_dtx\": " << (encoder.enable_dtx ? "true" : "false")
      << ",\n"
      << "    \"redundancy_mode\": \""
      << EscapeJsonString(encoder.redundancy_mode) << "\",\n"
      << "    \"expected_loss_pct\": " << encoder.expected_loss_pct << ",\n"
      << "    \"dred_duration_frames\": " << encoder.dred_duration_frames
      << ",\n"
      << "    \"dred_duration_source\": \""
      << EscapeJsonString(encoder.dred_duration_source) << "\"\n"
      << "  },\n"
      << "  \"network\": {\n"
      << "    \"bandwidth_kbps\": " << network.bandwidth_kbps << ",\n"
      << "    \"delay_ms\": " << network.delay_ms << ",\n"
      << "    \"jitter_stddev_ms\": " << network.jitter_stddev_ms << ",\n"
      << "    \"loss_pct\": " << network.loss_pct << ",\n"
      << "    \"allow_reorder\": "
      << (network.allow_reorder ? "true" : "false") << ",\n"
      << "    \"queue_len_packets\": " << network.queue_len_packets << ",\n"
      << "    \"avg_burst_loss_length\": " << network.avg_burst_loss_length
      << ",\n"
      << "    \"random_seed\": " << network.random_seed << ",\n"
      << "    \"fixed_loss_events_ms\": "
      << BuildFixedLossEventsJson(network.fixed_loss_events) << "\n"
      << "  },\n"
      << "  \"neteq\": {\n"
      << "    \"max_packets\": " << neteq.max_packets << ",\n"
      << "    \"fast_accelerate\": "
      << (neteq.fast_accelerate ? "true" : "false") << ",\n"
      << "    \"min_delay_ms\": " << neteq.min_delay_ms << "\n"
      << "  },\n"
      << "  \"encoded\": {\n"
      << "    \"packets\": " << encoded.packets << ",\n"
      << "    \"bytes\": " << encoded.bytes << "\n"
      << "  },\n"
      << "  \"network_stats\": {\n"
      << "    \"enqueued_packets\": " << network_stats.enqueued_packets
      << ",\n"
      << "    \"delivered_packets\": " << network_stats.delivered_packets
      << ",\n"
      << "    \"lost_packets\": " << network_stats.lost_packets << ",\n"
      << "    \"queue_dropped_packets\": "
      << network_stats.queue_dropped_packets << ",\n"
      << "    \"fixed_loss_dropped_packets\": "
      << network_stats.fixed_loss_dropped_packets << ",\n"
      << "    \"reordered_packets\": " << network_stats.reordered_packets
      << "\n"
      << "  },\n"
      << "  \"output\": {\n"
      << "    \"output_audio\": \"" << EscapeJsonString(output.output_audio)
      << "\",\n"
      << "    \"sample_rate_hz\": " << output.sample_rate_hz << ",\n"
      << "    \"channels\": " << output.channels << ",\n"
      << "    \"decoded_duration_ms\": " << output.decoded_duration_ms
      << ",\n"
      << "    \"decoded_samples\": " << output.decoded_samples << "\n"
      << "  },\n"
      << "  \"neteq_stats\": {\n"
      << "    \"current_buffer_size_ms\": "
      << neteq_stats.current_buffer_size_ms << ",\n"
      << "    \"preferred_buffer_size_ms\": "
      << neteq_stats.preferred_buffer_size_ms << ",\n"
      << "    \"mean_waiting_time_ms\": " << neteq_stats.mean_waiting_time_ms
      << ",\n"
      << "    \"median_waiting_time_ms\": "
      << neteq_stats.median_waiting_time_ms << ",\n"
      << "    \"min_waiting_time_ms\": " << neteq_stats.min_waiting_time_ms
      << ",\n"
      << "    \"max_waiting_time_ms\": " << neteq_stats.max_waiting_time_ms
      << ",\n"
      << "    \"total_samples_received\": "
      << neteq_stats.total_samples_received << ",\n"
      << "    \"concealed_samples\": " << neteq_stats.concealed_samples
      << ",\n"
      << "    \"silent_concealed_samples\": "
      << neteq_stats.silent_concealed_samples << ",\n"
      << "    \"concealment_events\": " << neteq_stats.concealment_events
      << ",\n"
      << "    \"fec_packets_received\": "
      << neteq_stats.fec_packets_received << ",\n"
      << "    \"fec_packets_discarded\": "
      << neteq_stats.fec_packets_discarded << ",\n"
      << "    \"packets_discarded\": " << neteq_stats.packets_discarded
      << ",\n"
      << "    \"jitter_buffer_delay_ms\": "
      << neteq_stats.jitter_buffer_delay_ms << ",\n"
      << "    \"jitter_buffer_emitted_count\": "
      << neteq_stats.jitter_buffer_emitted_count << ",\n"
      << "    \"jitter_buffer_target_delay_ms\": "
      << neteq_stats.jitter_buffer_target_delay_ms << ",\n"
      << "    \"jitter_buffer_minimum_delay_ms\": "
      << neteq_stats.jitter_buffer_minimum_delay_ms << ",\n"
      << "    \"inserted_samples_for_deceleration\": "
      << neteq_stats.inserted_samples_for_deceleration << ",\n"
      << "    \"removed_samples_for_acceleration\": "
      << neteq_stats.removed_samples_for_acceleration << ",\n"
      << "    \"delayed_packet_outage_samples\": "
      << neteq_stats.delayed_packet_outage_samples << ",\n"
      << "    \"relative_packet_arrival_delay_ms\": "
      << neteq_stats.relative_packet_arrival_delay_ms << ",\n"
      << "    \"packet_buffer_flushes\": "
      << neteq_stats.packet_buffer_flushes << ",\n"
      << "    \"interruption_count\": " << neteq_stats.interruption_count
      << ",\n"
      << "    \"total_interruption_duration_ms\": "
      << neteq_stats.total_interruption_duration_ms << ",\n"
      << "    \"expand_rate\": " << neteq_stats.expand_rate << ",\n"
      << "    \"speech_expand_rate\": " << neteq_stats.speech_expand_rate
      << ",\n"
      << "    \"accelerate_rate\": " << neteq_stats.accelerate_rate << ",\n"
      << "    \"preemptive_expand_rate\": "
      << neteq_stats.preemptive_expand_rate << ",\n"
      << "    \"secondary_decoded_rate\": "
      << neteq_stats.secondary_decoded_rate << ",\n"
      << "    \"secondary_discarded_rate\": "
      << neteq_stats.secondary_discarded_rate << ",\n"
      << "    \"max_expand_rate\": " << neteq_stats.max_expand_rate << ",\n"
      << "    \"max_secondary_decoded_rate\": "
      << neteq_stats.max_secondary_decoded_rate << ",\n"
      << "    \"max_secondary_discarded_rate\": "
      << neteq_stats.max_secondary_discarded_rate << "\n"
      << "  }\n"
      << "}";
  return oss.str();
}

AudioNetworkLabRunResult AudioNetworkLabRunner::Run(
    const AudioNetworkLabConfig& config) {
  AudioNetworkLabRunResult result;
  result.summary.input.input_pcm = config.input_pcm;
  result.summary.input.channels = config.encoder.channels;
  result.summary.encoder.codec = config.encoder.codec;
  result.summary.encoder.bitrate_bps = config.encoder.bitrate_bps;
  result.summary.encoder.frame_ms = config.encoder.frame_ms;
  result.summary.encoder.enable_dtx = config.encoder.enable_dtx;
  result.summary.encoder.redundancy_mode = std::string(
      AudioNetworkLabRedundancyModeToString(config.encoder.redundancy_mode));
  result.summary.encoder.expected_loss_pct = config.encoder.expected_loss_pct;
  result.summary.network.bandwidth_kbps = config.network.bandwidth_kbps;
  result.summary.network.delay_ms = config.network.delay_ms;
  result.summary.network.jitter_stddev_ms = config.network.jitter_stddev_ms;
  result.summary.network.loss_pct = config.network.loss_pct;
  result.summary.network.allow_reorder = config.network.allow_reorder;
  result.summary.network.queue_len_packets = config.network.queue_len_packets;
  result.summary.network.avg_burst_loss_length =
      config.network.avg_burst_loss_length;
  result.summary.network.random_seed = config.network.random_seed;
  result.summary.network.fixed_loss_events = config.network.fixed_loss_events;
  result.summary.neteq.max_packets = config.neteq.max_packets;
  result.summary.neteq.fast_accelerate = config.neteq.fast_accelerate;
  result.summary.neteq.min_delay_ms = config.neteq.min_delay_ms;
  result.summary.output.output_audio = config.output_audio;
  result.summary.output.channels = config.encoder.channels;

  if (config.input_pcm.empty()) {
    result.error = "--input_pcm is required.";
    return result;
  }
  if (config.summary_json.empty()) {
    result.error = "--summary_json is required.";
    return result;
  }
  if (config.encoder.channels != 1 && config.encoder.channels != 2) {
    result.error = "Only channels=1 or channels=2 are supported.";
    return result;
  }
  if (config.encoder.bitrate_bps <= 0) {
    result.error = "--bitrate_bps must be > 0.";
    return result;
  }
  if (config.encoder.frame_ms <= 0) {
    result.error = "--frame_ms must be > 0.";
    return result;
  }
  if (config.encoder.expected_loss_pct < 0 ||
      config.encoder.expected_loss_pct > 100) {
    result.error = "--expected_loss_pct must be between 0 and 100.";
    return result;
  }
  if (config.network.bandwidth_kbps < 0) {
    result.error = "--bandwidth_kbps must be >= 0.";
    return result;
  }
  if (config.network.delay_ms < 0) {
    result.error = "--delay_ms must be >= 0.";
    return result;
  }
  if (config.network.jitter_stddev_ms < 0) {
    result.error = "--jitter_stddev_ms must be >= 0.";
    return result;
  }
  if (config.network.loss_pct < 0.0 || config.network.loss_pct > 100.0) {
    result.error = "--loss_pct must be between 0 and 100.";
    return result;
  }
  if (config.network.queue_len_packets < 0) {
    result.error = "--queue_len_packets must be >= 0.";
    return result;
  }
  if (config.network.avg_burst_loss_length < 0) {
    result.error = "--avg_burst_loss_length must be >= 0.";
    return result;
  }
  if (config.neteq.max_packets == 0) {
    result.error = "--neteq_max_packets must be > 0.";
    return result;
  }
  if (config.neteq.min_delay_ms < 0) {
    result.error = "--neteq_min_delay_ms must be >= 0.";
    return result;
  }

  {
    std::string network_error;
    if (!ValidateNetworkImpairmentConfig(config.network, &network_error)) {
      result.error = network_error;
      return result;
    }
  }

  {
    FILE* input = fopen(config.input_pcm.c_str(), "rb");
    if (input == nullptr) {
      result.error = "Failed to open input PCM file.";
      return result;
    }
    fclose(input);
  }

  OptionalTextLog textlog(config.textlog);
  if (!textlog.valid()) {
    result.error = "Failed to open text log file.";
    return result;
  }

  const DredSelection dred_selection =
      SelectDredDuration(config.encoder, config.network.loss_pct);
  result.summary.encoder.dred_duration_frames = dred_selection.duration_frames;
  result.summary.encoder.dred_duration_source = dred_selection.source;

  std::string encoder_error;
  std::unique_ptr<AudioEncoder> encoder =
      CreateEncoderStack(config.encoder, dred_selection, &encoder_error);
  if (!encoder) {
    result.error = encoder_error;
    return result;
  }

  std::string output_error;
  std::unique_ptr<AudioOutputSink> output_sink =
      CreateOutputSink(config, kInputSampleRateHz, config.encoder.channels,
                       &output_error);
  if (!output_sink) {
    result.error = output_error;
    return result;
  }

  auto acm = AudioCodingModule::Create();
  acm->SetEncoder(std::move(encoder));
  acm->SetPacketLossRate(config.encoder.expected_loss_pct);

  MemoryPacketizationCallback packetization_callback;
  RTC_CHECK_EQ(acm->RegisterTransportCallback(&packetization_callback), 0);

  NetEq::Config neteq_config;
  neteq_config.sample_rate_hz = kInputSampleRateHz;
  neteq_config.max_packets_in_buffer = config.neteq.max_packets;
  neteq_config.enable_fast_accelerate = config.neteq.fast_accelerate;
  neteq_config.min_delay_ms = config.neteq.min_delay_ms;

  SimulatedClock clock(Timestamp::Zero());
  DefaultNetEqFactory neteq_factory;
  std::unique_ptr<NetEq> neteq = neteq_factory.CreateNetEq(
      neteq_config, CreateBuiltinAudioDecoderFactory(), &clock);

  SdpAudioFormat::Parameters opus_params;
  opus_params["minptime"] = "10";
  opus_params["stereo"] = config.encoder.channels == 2 ? "1" : "0";
  if (config.encoder.redundancy_mode ==
      AudioNetworkLabRedundancyMode::kOpusFec) {
    opus_params["useinbandfec"] = "1";
  }
  RTC_CHECK(neteq->RegisterPayloadType(
      kOpusPayloadType, SdpAudioFormat("opus", kInputSampleRateHz, 2,
                                       opus_params)));
  if (config.encoder.redundancy_mode == AudioNetworkLabRedundancyMode::kRed) {
    RTC_CHECK(
        neteq->RegisterPayloadType(kRedPayloadType, SdpAudioFormat("red", 8000,
                                                                   1)));
  }

  SimulatedNetwork::Config network_config;
  network_config.link_capacity_kbps = config.network.bandwidth_kbps;
  network_config.queue_delay_ms = config.network.delay_ms;
  network_config.delay_standard_deviation_ms = config.network.jitter_stddev_ms;
  network_config.loss_percent =
      static_cast<int>(std::round(config.network.loss_pct));
  network_config.allow_reordering = config.network.allow_reorder;
  network_config.queue_length_packets =
      config.network.queue_len_packets > 0
          ? static_cast<size_t>(config.network.queue_len_packets)
          : 0;
  network_config.avg_burst_loss_length =
      config.network.avg_burst_loss_length > 1
          ? config.network.avg_burst_loss_length
          : -1;
  SimulatedNetwork network(network_config, config.network.random_seed);

  InputAudioFile input_file(config.input_pcm, /*loop_at_end=*/false);

  AudioFrame input_frame;
  input_frame.sample_rate_hz_ = kInputSampleRateHz;
  input_frame.num_channels_ = config.encoder.channels;
  input_frame.samples_per_channel_ = kInputSamplesPerChannel10ms;

  std::map<uint64_t, EncodedPacket> in_flight_packets;
  int64_t next_capture_us = 0;
  absl::optional<int64_t> next_decode_us;
  int input_duration_ms = 0;
  int decoded_duration_ms = 0;
  uint64_t last_delivered_packet_id = 0;
  bool input_done = false;

  auto process_network = [&](int64_t now_us) -> bool {
    while (true) {
      absl::optional<int64_t> next_delivery = network.NextDeliveryTimeUs();
      if (!next_delivery.has_value() || *next_delivery > now_us) {
        break;
      }
      for (const PacketDeliveryInfo& delivery :
           network.DequeueDeliverablePackets(now_us)) {
        auto packet_it = in_flight_packets.find(delivery.packet_id);
        if (packet_it == in_flight_packets.end()) {
          result.error = "Internal error: missing in-flight packet.";
          return false;
        }

        if (delivery.receive_time_us == PacketDeliveryInfo::kNotReceived) {
          ++result.summary.network_stats.lost_packets;
          textlog.WriteLine("NETWORK_LOSS packet_id=" +
                            std::to_string(delivery.packet_id));
          in_flight_packets.erase(packet_it);
          continue;
        }

        if (delivery.packet_id < last_delivered_packet_id) {
          ++result.summary.network_stats.reordered_packets;
        }
        last_delivered_packet_id =
            std::max(last_delivered_packet_id, delivery.packet_id);
        ++result.summary.network_stats.delivered_packets;

        const rtc::ArrayView<const uint8_t> payload_view(
            packet_it->second.payload.data(), packet_it->second.payload.size());
        int insert_status =
            neteq->InsertPacket(packet_it->second.header, payload_view);
        if (insert_status != NetEq::kOK) {
          result.error = "NetEq::InsertPacket failed.";
          return false;
        }
        UpdateMaxNetEqRates(neteq->CurrentNetworkStatistics(), &result.summary);
        if (!next_decode_us.has_value()) {
          next_decode_us = now_us;
        }
        textlog.WriteLine("DELIVER packet_id=" +
                          std::to_string(delivery.packet_id) + " time_ms=" +
                          std::to_string(now_us / 1000));
        in_flight_packets.erase(packet_it);
      }
    }
    return true;
  };

  while (true) {
    absl::optional<int64_t> next_event_us;
    if (!input_done) {
      next_event_us = next_capture_us;
    }
    if (next_decode_us.has_value() && decoded_duration_ms < input_duration_ms) {
      next_event_us = MinOptionalUs(next_event_us, next_decode_us);
    }
    next_event_us = MinOptionalUs(next_event_us, network.NextDeliveryTimeUs());

    if (!next_event_us.has_value()) {
      break;
    }

    const int64_t now_us = *next_event_us;
    clock.AdvanceTimeMicroseconds(now_us - clock.TimeInMicroseconds());

    if (!input_done && now_us == next_capture_us) {
      std::vector<int16_t> capture_data(kInputSamplesPerChannel10ms *
                                        config.encoder.channels);
      if (!input_file.Read(capture_data.size(), capture_data.data())) {
        input_done = true;
      } else {
        input_frame.timestamp_ = static_cast<uint32_t>(
            input_duration_ms * kInputSampleRateHz / 1000);
        std::copy(capture_data.begin(), capture_data.end(),
                  input_frame.mutable_data());
        packetization_callback.set_send_time_us(now_us);
        if (acm->Add10MsData(input_frame) < 0) {
          result.error = "AudioCodingModule::Add10MsData failed.";
          return result;
        }
        input_duration_ms += kInputFrameMs;
        result.summary.input.input_duration_ms = input_duration_ms;

        for (EncodedPacket& packet : packetization_callback.TakePackets()) {
          ++result.summary.encoded.packets;
          result.summary.encoded.bytes += packet.payload.size();
          textlog.WriteLine("ENCODE packet_id=" +
                            std::to_string(packet.packet_id) + " bytes=" +
                            std::to_string(packet.payload.size()) +
                            " time_ms=" + std::to_string(now_us / 1000));

          if (IsPacketDroppedByFixedLoss(config.network.fixed_loss_events,
                                         packet.send_time_us)) {
            ++result.summary.network_stats.fixed_loss_dropped_packets;
            textlog.WriteLine("FIXED_LOSS packet_id=" +
                              std::to_string(packet.packet_id));
            continue;
          }

          if (!network.EnqueuePacket(PacketInFlightInfo(
                  packet.payload.size(), packet.send_time_us, packet.packet_id))) {
            ++result.summary.network_stats.queue_dropped_packets;
            textlog.WriteLine("QUEUE_DROP packet_id=" +
                              std::to_string(packet.packet_id));
            continue;
          }

          ++result.summary.network_stats.enqueued_packets;
          in_flight_packets.emplace(packet.packet_id, std::move(packet));
        }
        next_capture_us += kInputFrameMs * 1000;
      }
    }

    if (!process_network(now_us)) {
      return result;
    }

    if (next_decode_us.has_value() && now_us == *next_decode_us &&
        decoded_duration_ms < input_duration_ms) {
      AudioFrame output_frame;
      bool muted = false;
      int current_sample_rate_hz = 0;
      if (neteq->GetAudio(&output_frame, &muted, &current_sample_rate_hz) !=
          NetEq::kOK) {
        result.error = "NetEq::GetAudio failed.";
        return result;
      }
      if (!output_sink->Write(
              output_frame.data(),
              output_frame.samples_per_channel_ * output_frame.num_channels_)) {
        result.error = "Failed to write output audio.";
        return result;
      }
      decoded_duration_ms += kInputFrameMs;
      result.summary.output.decoded_duration_ms = decoded_duration_ms;
      result.summary.output.decoded_samples +=
          output_frame.samples_per_channel_ * output_frame.num_channels_;
      result.summary.output.sample_rate_hz =
          current_sample_rate_hz > 0 ? current_sample_rate_hz
                                     : output_frame.sample_rate_hz_;
      UpdateMaxNetEqRates(neteq->CurrentNetworkStatistics(), &result.summary);
      textlog.WriteLine("DECODE time_ms=" + std::to_string(now_us / 1000) +
                        " muted=" + std::string(muted ? "true" : "false"));
      *next_decode_us += kInputFrameMs * 1000;
    }

    if (input_done && decoded_duration_ms >= input_duration_ms &&
        in_flight_packets.empty() && !network.NextDeliveryTimeUs().has_value()) {
      break;
    }
  }

  PopulateFinalNetEqStats(neteq.get(), &result.summary);
  if (!WriteSummaryFile(result.summary, config.summary_json)) {
    result.error = "Failed to write summary JSON.";
    return result;
  }

  result.ok = true;
  return result;
}

}  // namespace test
}  // namespace webrtc
