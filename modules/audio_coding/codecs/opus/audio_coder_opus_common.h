/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_OPUS_AUDIO_CODER_OPUS_COMMON_H_
#define MODULES_AUDIO_CODING_CODECS_OPUS_AUDIO_CODER_OPUS_COMMON_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/audio_codecs/audio_decoder.h"
#include "api/audio_codecs/audio_format.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/string_to_number.h"

namespace webrtc {

// Local DRED model/ABI version. Bumped in lockstep with any DRED model upgrade
// (e.g. the eventual switch from the Opus 1.5.2 DRED model to the Opus 1.6
// one, which is not wire-compatible). Advertised via SDP as `opusdredver`.
constexpr int kOpusDredLocalVersion = 1;
constexpr char kOpusDredLocalVersionStr[] = "1";

absl::optional<std::string> GetFormatParameter(const SdpAudioFormat& format,
                                               absl::string_view param);

template <typename T>
absl::optional<T> GetFormatParameter(const SdpAudioFormat& format,
                                     absl::string_view param) {
  return rtc::StringToNumber<T>(GetFormatParameter(format, param).value_or(""));
}

template <>
absl::optional<std::vector<unsigned char>> GetFormatParameter(
    const SdpAudioFormat& format,
    absl::string_view param);

class OpusFrame : public AudioDecoder::EncodedAudioFrame {
 public:
  OpusFrame(AudioDecoder* decoder,
            rtc::Buffer&& payload,
            bool is_primary_payload)
      : decoder_(decoder),
        payload_(std::move(payload)),
        is_primary_payload_(is_primary_payload),
        dred_index_(-1),
        primary_timestamp_(0) {}

  // DRED frame constructor: carries parsed DRED state and an offset index.
  OpusFrame(AudioDecoder* decoder,
            rtc::CopyOnWriteBuffer dred_data,
            int dred_index,
            uint32_t primary_timestamp)
      : decoder_(decoder),
        is_primary_payload_(false),
        dred_data_(std::move(dred_data)),
        dred_index_(dred_index),
        primary_timestamp_(primary_timestamp) {}

  size_t Duration() const override {
    if (dred_index_ >= 0) {
      return static_cast<size_t>(decoder_->SampleRateHz() / 100);
    }
    int ret;
    if (is_primary_payload_) {
      ret = decoder_->PacketDuration(payload_.data(), payload_.size());
    } else {
      ret = decoder_->PacketDurationRedundant(payload_.data(), payload_.size());
    }
    return (ret < 0) ? 0 : static_cast<size_t>(ret);
  }

  bool IsDtxPacket() const override {
    if (dred_index_ >= 0)
      return false;
    return payload_.size() <= 2;
  }

  absl::optional<DecodeResult> Decode(
      rtc::ArrayView<int16_t> decoded) const override {
    AudioDecoder::SpeechType speech_type = AudioDecoder::kSpeech;
    int ret;
    if (dred_index_ >= 0) {
      ret = decoder_->DecodeDred(dred_data_.data(), dred_data_.size(),
                                 primary_timestamp_, decoded.data(),
                                 dred_index_);
    } else if (is_primary_payload_) {
      ret = decoder_->Decode(
          payload_.data(), payload_.size(), decoder_->SampleRateHz(),
          decoded.size() * sizeof(int16_t), decoded.data(), &speech_type);
    } else {
      ret = decoder_->DecodeRedundant(
          payload_.data(), payload_.size(), decoder_->SampleRateHz(),
          decoded.size() * sizeof(int16_t), decoded.data(), &speech_type);
    }

    if (ret < 0)
      return absl::nullopt;

    return DecodeResult{static_cast<size_t>(ret), speech_type};
  }

 private:
  AudioDecoder* const decoder_;
  const rtc::Buffer payload_;
  const bool is_primary_payload_;
  const rtc::CopyOnWriteBuffer dred_data_;
  const int dred_index_;
  const uint32_t primary_timestamp_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_CODECS_OPUS_AUDIO_CODER_OPUS_COMMON_H_
