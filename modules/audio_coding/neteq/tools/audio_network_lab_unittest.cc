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

#include <cmath>

#include <fstream>
#include <string>
#include <vector>

#include "rtc_base/checks.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {
namespace {

std::string WriteMonoPcm(absl::string_view prefix, int duration_ms) {
  const std::string path = GenerateTempFilename(OutputPath(), prefix);
  std::ofstream out(path, std::ios::binary);
  RTC_CHECK(out.is_open());

  const int total_samples = 48 * duration_ms;
  constexpr double kPi = 3.14159265358979323846;
  for (int i = 0; i < total_samples; ++i) {
    const double t = static_cast<double>(i) / 48000.0;
    const int16_t sample =
        static_cast<int16_t>(std::sin(2.0 * kPi * 440.0 * t) * 12000.0);
    out.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
  }
  return path;
}

AudioNetworkLabConfig CreateBaseConfig(absl::string_view input_pcm) {
  AudioNetworkLabConfig config;
  config.input_pcm = std::string(input_pcm);
  config.summary_json =
      GenerateTempFilename(OutputPath(), "audio_network_lab.json");
  config.output_audio =
      GenerateTempFilename(OutputPath(), "audio_network_lab.wav");
  config.encoder.codec = "opus";
  config.encoder.channels = 1;
  config.encoder.frame_ms = 20;
  config.encoder.bitrate_bps = 32000;
  config.neteq.max_packets = 200;
  return config;
}

TEST(AudioNetworkLabTest, BaselineRunProducesAudioAndSummary) {
  const std::string input_pcm = WriteMonoPcm("audio_network_lab_input", 500);
  AudioNetworkLabConfig config = CreateBaseConfig(input_pcm);

  AudioNetworkLabRunner runner;
  AudioNetworkLabRunResult result = runner.Run(config);

  ASSERT_TRUE(result.ok) << result.error;
  EXPECT_TRUE(FileExists(config.summary_json));
  EXPECT_TRUE(FileExists(config.output_audio));
  EXPECT_GT(GetFileSize(config.summary_json), 0u);
  EXPECT_GT(GetFileSize(config.output_audio), 0u);
  EXPECT_GT(result.summary.encoded.packets, 0);
  EXPECT_EQ(result.summary.output.decoded_duration_ms,
            result.summary.input.input_duration_ms);
}

TEST(AudioNetworkLabTest, OpusFecProducesSecondaryDataUnderLoss) {
  const std::string input_pcm = WriteMonoPcm("audio_network_lab_fec", 2000);
  AudioNetworkLabConfig config = CreateBaseConfig(input_pcm);
  config.encoder.redundancy_mode = AudioNetworkLabRedundancyMode::kOpusFec;
  config.encoder.expected_loss_pct = 30;
  config.network.loss_pct = 30.0;

  AudioNetworkLabRunner runner;
  AudioNetworkLabRunResult result = runner.Run(config);

  ASSERT_TRUE(result.ok) << result.error;
  EXPECT_TRUE(result.summary.neteq_stats.fec_packets_received +
                  result.summary.neteq_stats.fec_packets_discarded >
              0u ||
              result.summary.neteq_stats.max_secondary_decoded_rate > 0.0);
}

TEST(AudioNetworkLabTest, RedModeExercisesSecondaryDecoding) {
  const std::string input_pcm = WriteMonoPcm("audio_network_lab_red", 2000);
  AudioNetworkLabConfig config = CreateBaseConfig(input_pcm);
  config.encoder.redundancy_mode = AudioNetworkLabRedundancyMode::kRed;
  config.network.loss_pct = 20.0;

  AudioNetworkLabRunner runner;
  AudioNetworkLabRunResult result = runner.Run(config);

  ASSERT_TRUE(result.ok) << result.error;
  EXPECT_GT(result.summary.neteq_stats.max_secondary_decoded_rate, 0.0);
}

TEST(AudioNetworkLabTest, FixedLossEventTriggersConcealment) {
  const std::string input_pcm = WriteMonoPcm("audio_network_lab_outage", 1500);
  AudioNetworkLabConfig config = CreateBaseConfig(input_pcm);
  config.network.fixed_loss_events = {{300, 200}};

  AudioNetworkLabRunner runner;
  AudioNetworkLabRunResult result = runner.Run(config);

  ASSERT_TRUE(result.ok) << result.error;
  EXPECT_GT(result.summary.neteq_stats.concealed_samples, 0u);
  EXPECT_GT(result.summary.neteq_stats.concealment_events, 0u);
}

TEST(AudioNetworkLabTest, InvalidBurstLossParametersFailGracefully) {
  const std::string input_pcm = WriteMonoPcm("audio_network_lab_burst_invalid", 200);
  AudioNetworkLabConfig config = CreateBaseConfig(input_pcm);
  config.network.loss_pct = 60.0;
  // SimulatedNetwork requires avg_burst_loss_length > ceil(p/(1-p)) for Gilbert-Elliot.
  config.network.avg_burst_loss_length = 2;

  AudioNetworkLabRunner runner;
  AudioNetworkLabRunResult result = runner.Run(config);

  EXPECT_FALSE(result.ok);
  EXPECT_NE(result.error.find("avg_burst_loss_length"), std::string::npos);
}

TEST(AudioNetworkLabTest, DredModeProducesOutput) {
  const std::string input_pcm = WriteMonoPcm("audio_network_lab_dred", 500);
  AudioNetworkLabConfig config = CreateBaseConfig(input_pcm);
  config.encoder.redundancy_mode = AudioNetworkLabRedundancyMode::kDred;
  // Inject moderate loss so DRED has a chance to recover frames.
  config.network.loss_pct = 20.0;

  AudioNetworkLabRunner runner;
  AudioNetworkLabRunResult result = runner.Run(config);

  EXPECT_TRUE(result.ok) << result.error;
  EXPECT_TRUE(FileExists(config.summary_json));
  EXPECT_TRUE(FileExists(config.output_audio));
}

}  // namespace
}  // namespace test
}  // namespace webrtc
