// Copyright 2026 SuperMama contributors. Licensed under Apache-2.0.
#ifndef OHOS_WEBRTC_VIDEO_FRAME_RATE_FACTORY_H_
#define OHOS_WEBRTC_VIDEO_FRAME_RATE_FACTORY_H_

#include "video_frame_rate_gate.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
// Some Harmony hardware encoders accept maxFramerate parameters but still
// encode every input frame. Enforce the optional limit before any encoder,
// including the software fallback. Native preview remains on the original source.
class FrameRateLimitedEncoder final : public VideoEncoder {
 public:
  FrameRateLimitedEncoder(std::unique_ptr<VideoEncoder> encoder,
                          std::shared_ptr<VideoFrameRateLimit> limit)
      : encoder_(std::move(encoder)), gate_(std::move(limit)) {}
  int InitEncode(const VideoCodec* codec, const Settings& settings) override {
    gate_.Reset(); return encoder_->InitEncode(codec, settings);
  }
  int32_t RegisterEncodeCompleteCallback(EncodedImageCallback* callback) override {
    callback_ = callback; return encoder_->RegisterEncodeCompleteCallback(callback);
  }
  int32_t Release() override { gate_.Reset(); callback_ = nullptr; return encoder_->Release(); }
  int32_t Encode(const VideoFrame& frame, const std::vector<VideoFrameType>* types) override {
    if (!gate_.Accept(rtc::TimeMicros())) {
      if (callback_) callback_->OnDroppedFrame(EncodedImageCallback::DropReason::kDroppedByEncoder);
      return WEBRTC_VIDEO_CODEC_OK;
    }
    return encoder_->Encode(frame, types);
  }
  void SetRates(const RateControlParameters& rates) override { encoder_->SetRates(rates); }
  EncoderInfo GetEncoderInfo() const override { return encoder_->GetEncoderInfo(); }
  void SetFecControllerOverride(FecControllerOverride* control) override { encoder_->SetFecControllerOverride(control); }
  void OnPacketLossRateUpdate(float rate) override { encoder_->OnPacketLossRateUpdate(rate); }
  void OnRttUpdate(int64_t rtt) override { encoder_->OnRttUpdate(rtt); }
  void OnLossNotification(const LossNotification& loss) override { encoder_->OnLossNotification(loss); }
 private:
  std::unique_ptr<VideoEncoder> encoder_;
  VideoFrameRateGate gate_;
  EncodedImageCallback* callback_ = nullptr;
};

class FrameRateLimitedEncoderFactory final : public VideoEncoderFactory {
 public:
  FrameRateLimitedEncoderFactory(std::unique_ptr<VideoEncoderFactory> factory,
                                 std::shared_ptr<VideoFrameRateLimit> limit)
      : factory_(std::move(factory)), limit_(std::move(limit)) {}
  std::vector<SdpVideoFormat> GetSupportedFormats() const override { return factory_->GetSupportedFormats(); }
  std::vector<SdpVideoFormat> GetImplementations() const override { return factory_->GetImplementations(); }
  CodecSupport QueryCodecSupport(const SdpVideoFormat& format, absl::optional<std::string> mode) const override {
    return factory_->QueryCodecSupport(format, mode);
  }
  std::unique_ptr<EncoderSelectorInterface> GetEncoderSelector() const override { return factory_->GetEncoderSelector(); }
  std::unique_ptr<VideoEncoder> CreateVideoEncoder(const SdpVideoFormat& format) override {
    auto encoder = factory_->CreateVideoEncoder(format);
    if (!encoder) return nullptr;
    return std::make_unique<FrameRateLimitedEncoder>(std::move(encoder), limit_);
  }
 private:
  std::unique_ptr<VideoEncoderFactory> factory_;
  std::shared_ptr<VideoFrameRateLimit> limit_;
};
}  // namespace webrtc
#endif
