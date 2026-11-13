// Copyright 2026 SuperMama contributors. Licensed under Apache-2.0.
#ifndef OHOS_WEBRTC_VIDEO_FRAME_RATE_GATE_H_
#define OHOS_WEBRTC_VIDEO_FRAME_RATE_GATE_H_

#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>

namespace webrtc {
// One control per factory, one scheduling clock per encoder. No process globals.
// -1 preserves SDK defaults; 0 blocks encoding; positive values cap FPS.
struct VideoFrameRateLimit {
  std::atomic<double> fps{-1.0};
};

class VideoFrameRateGate {
 public:
  explicit VideoFrameRateGate(std::shared_ptr<VideoFrameRateLimit> limit)
      : limit_(std::move(limit)) {}
  void Reset() { last_us_ = -1; previous_fps_ = -1; }
  bool Accept(int64_t now_us) {
    const double fps = limit_->fps.load(std::memory_order_acquire);
    if (fps != previous_fps_) { last_us_ = -1; previous_fps_ = fps; }
    if (fps < 0) return true;
    if (fps == 0) return false;
    const double interval_us = std::ceil(1000000.0 / fps);
    if (last_us_ >= 0 && now_us >= last_us_ && now_us - last_us_ < interval_us) return false;
    last_us_ = now_us;
    return true;
  }
 private:
  std::shared_ptr<VideoFrameRateLimit> limit_;
  int64_t last_us_ = -1;
  double previous_fps_ = -1;
};
}  // namespace webrtc
#endif
