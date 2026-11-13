// Copyright 2026 The WebRTC project authors. All Rights Reserved.
// Use of this source code is governed by a BSD-style license in LICENSE.
#ifndef SDK_OHOS_EXTERNAL_VIDEO_CAPTURER_H_
#define SDK_OHOS_EXTERNAL_VIDEO_CAPTURER_H_
#include "video_capturer.h"
namespace webrtc {
// A hardware-free source for application-owned board/still frames. Its calls
// are serialized on OhosVideoTrackSource's thread; no camera is opened.
class ExternalVideoCapturer final : public VideoCapturer {
public:
    void Init(std::unique_ptr<VideoFrameReceiver>, Observer* observer) override { observer_ = observer; }
    void Release() override { Stop(); observer_ = nullptr; }
    void Start() override { if (!started_ && observer_) { started_ = true; observer_->OnCapturerStarted(true); } }
    void Stop() override { if (started_ && observer_) { started_ = false; observer_->OnCapturerStopped(); } }
    bool IsScreencast() override { return true; }
    bool IsExternal() const override { return true; }
private:
    Observer* observer_ = nullptr;
    bool started_ = false;
};
} // namespace webrtc
#endif
