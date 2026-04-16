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

#include <stdio.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/string_view.h"

ABSL_FLAG(std::string, input_pcm, "", "Path to raw PCM input.");
ABSL_FLAG(std::string,
          output_audio,
          "",
          "Optional output path. `.wav` writes WAV, anything else writes PCM.");
ABSL_FLAG(std::string,
          summary_json,
          "",
          "Required summary JSON output path.");
ABSL_FLAG(std::string,
          textlog,
          "",
          "Optional step-by-step text log output path.");

ABSL_FLAG(std::string, codec, "opus", "Encoder codec. Only `opus` is supported.");
ABSL_FLAG(int, bitrate_bps, 32000, "Opus target bitrate.");
ABSL_FLAG(int, frame_ms, 20, "Opus packetization frame length in ms.");
ABSL_FLAG(int, channels, 1, "Input/output channel count: 1 or 2.");
ABSL_FLAG(bool, enable_dtx, false, "Enable Opus DTX.");
ABSL_FLAG(std::string,
          redundancy_mode,
          "none",
          "One of: none, opus_fec, red, dred.");
ABSL_FLAG(int,
          expected_loss_pct,
          0,
          "Packet loss hint passed to encoder, in percent.");

ABSL_FLAG(int,
          bandwidth_kbps,
          0,
          "Network link capacity in kbps. 0 means unlimited.");
ABSL_FLAG(int, delay_ms, 0, "Base one-way network delay in ms.");
ABSL_FLAG(int, jitter_stddev_ms, 0, "Extra network delay standard deviation.");
ABSL_FLAG(double, loss_pct, 0.0, "Uniform network loss in percent.");
ABSL_FLAG(int,
          avg_burst_loss_length,
          0,
          "Average burst loss length. <=1 disables burst loss model.");
ABSL_FLAG(bool, allow_reorder, false, "Allow packet reordering in the network.");
ABSL_FLAG(int,
          queue_len_packets,
          0,
          "Maximum packets in network queue. 0 means unlimited.");
ABSL_FLAG(std::string,
          fixed_loss_events_ms,
          "",
          "Comma-separated outage events as start:duration pairs in ms.");
ABSL_FLAG(uint64_t,
          network_random_seed,
          1,
          "RNG seed for SimulatedNetwork (loss/jitter/reorder).");

ABSL_FLAG(int,
          neteq_max_packets,
          200,
          "NetEq max packets in buffer.");
ABSL_FLAG(bool,
          neteq_fast_accelerate,
          false,
          "Enable NetEq fast accelerate.");
ABSL_FLAG(int,
          neteq_min_delay_ms,
          0,
          "NetEq minimum delay in ms.");

namespace {

bool ParseIntStrict(absl::string_view text, int* value) {
  std::string token(text);
  char* end = nullptr;
  errno = 0;
  const long parsed = std::strtol(token.c_str(), &end, 10);
  if (errno != 0 || end != token.c_str() + token.size() ||
      parsed < std::numeric_limits<int>::min() ||
      parsed > std::numeric_limits<int>::max()) {
    return false;
  }
  *value = static_cast<int>(parsed);
  return true;
}

bool ParseFixedLossEvents(
    absl::string_view spec,
    std::vector<webrtc::test::FixedLossEvent>* events,
    std::string* error) {
  events->clear();
  if (spec.empty()) {
    return true;
  }

  std::stringstream stream{std::string(spec)};
  std::string token;
  while (std::getline(stream, token, ',')) {
    const size_t separator = token.find(':');
    if (separator == std::string::npos) {
      *error = "Invalid fixed loss event. Expected start:duration pairs.";
      return false;
    }
    webrtc::test::FixedLossEvent event;
    if (!ParseIntStrict(token.substr(0, separator), &event.start_ms) ||
        !ParseIntStrict(token.substr(separator + 1), &event.duration_ms)) {
      *error = "Invalid fixed loss event. Expected integer start:duration pairs.";
      return false;
    }
    if (event.start_ms < 0 || event.duration_ms <= 0) {
      *error = "Fixed loss events require start>=0 and duration>0.";
      return false;
    }
    events->push_back(event);
  }
  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);

  webrtc::test::AudioNetworkLabConfig config;
  config.input_pcm = absl::GetFlag(FLAGS_input_pcm);
  config.output_audio = absl::GetFlag(FLAGS_output_audio);
  config.summary_json = absl::GetFlag(FLAGS_summary_json);
  config.textlog = absl::GetFlag(FLAGS_textlog);
  config.encoder.codec = absl::GetFlag(FLAGS_codec);
  config.encoder.bitrate_bps = absl::GetFlag(FLAGS_bitrate_bps);
  config.encoder.frame_ms = absl::GetFlag(FLAGS_frame_ms);
  config.encoder.channels = absl::GetFlag(FLAGS_channels);
  config.encoder.enable_dtx = absl::GetFlag(FLAGS_enable_dtx);
  config.encoder.expected_loss_pct = absl::GetFlag(FLAGS_expected_loss_pct);
  config.network.bandwidth_kbps = absl::GetFlag(FLAGS_bandwidth_kbps);
  config.network.delay_ms = absl::GetFlag(FLAGS_delay_ms);
  config.network.jitter_stddev_ms = absl::GetFlag(FLAGS_jitter_stddev_ms);
  config.network.loss_pct = absl::GetFlag(FLAGS_loss_pct);
  config.network.avg_burst_loss_length =
      absl::GetFlag(FLAGS_avg_burst_loss_length);
  config.network.allow_reorder = absl::GetFlag(FLAGS_allow_reorder);
  config.network.queue_len_packets = absl::GetFlag(FLAGS_queue_len_packets);
  config.network.random_seed = absl::GetFlag(FLAGS_network_random_seed);
  const int neteq_max_packets = absl::GetFlag(FLAGS_neteq_max_packets);
  if (neteq_max_packets <= 0) {
    fprintf(stderr, "--neteq_max_packets must be > 0.\n");
    return 1;
  }
  config.neteq.max_packets = static_cast<size_t>(neteq_max_packets);
  config.neteq.fast_accelerate = absl::GetFlag(FLAGS_neteq_fast_accelerate);
  config.neteq.min_delay_ms = absl::GetFlag(FLAGS_neteq_min_delay_ms);

  auto redundancy_mode = webrtc::test::ParseAudioNetworkLabRedundancyMode(
      absl::GetFlag(FLAGS_redundancy_mode));
  if (!redundancy_mode.has_value()) {
    fprintf(stderr, "Invalid --redundancy_mode.\n");
    return 1;
  }
  config.encoder.redundancy_mode = *redundancy_mode;

  std::string parse_error;
  if (!ParseFixedLossEvents(absl::GetFlag(FLAGS_fixed_loss_events_ms),
                            &config.network.fixed_loss_events,
                            &parse_error)) {
    fprintf(stderr, "%s\n", parse_error.c_str());
    return 1;
  }

  webrtc::test::AudioNetworkLabRunner runner;
  webrtc::test::AudioNetworkLabRunResult result = runner.Run(config);
  if (!result.ok) {
    fprintf(stderr, "%s\n", result.error.c_str());
    return 1;
  }

  printf("audio_network_lab completed.\nsummary_json=%s\n",
         config.summary_json.c_str());
  return 0;
}
