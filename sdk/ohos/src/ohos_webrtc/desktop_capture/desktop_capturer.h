/**
 * Copyright (c) 2024 Archermind Technology (Nanjing) Co. Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WEBRTC_DESKTOP_CAPTURER_H
#define WEBRTC_DESKTOP_CAPTURER_H

#include "../video/video_info.h"
#include "../video/video_capturer.h"
#include "../video/video_frame_receiver.h"
#include "../helper/screen_capture.h"

#include <memory>
#include <cstdint>
#include <mutex>

#include <api/video/video_rotation.h>
#include "api/video/video_frame_buffer.h"
#include "rtc_base/thread.h"

namespace webrtc {

class DesktopCapturer : public VideoCapturer, public VideoFrameReceiver::Callback {
public:
    static std::unique_ptr<DesktopCapturer> Create(video::VideoProfile profile);

    ~DesktopCapturer();

protected:
    DesktopCapturer(video::VideoProfile profile);

    void NotifyCapturedStart(bool success);
    void NotifyCapturedStop();

protected:
    void Init(std::unique_ptr<VideoFrameReceiver>, Observer* observer) override;
    void Release() override;

    void Start() override;
    void Stop() override;

    bool IsScreencast() override;

    void
    OnFrameAvailable(rtc::scoped_refptr<VideoFrameBuffer> buffer, int64_t timestampUs, VideoRotation rotation) override;

    static void OnError1(OH_AVScreenCapture* capture, int32_t errorCode, void* userData);
    static void
    OnStateChange1(struct OH_AVScreenCapture* capture, OH_AVScreenCaptureStateCode stateCode, void* userData);
    static void OnBufferAvailable1(
        OH_AVScreenCapture* capture, OH_AVBuffer* buffer, OH_AVScreenCaptureBufferType bufferType, int64_t timestamp,
        void* userData);

    void OnError(OH_AVScreenCapture* capture, int32_t errorCode);
    void OnStateChange(struct OH_AVScreenCapture* capture, OH_AVScreenCaptureStateCode stateCode);
    void OnBufferAvailable(
        OH_AVScreenCapture* capture, OH_AVBuffer* buffer, OH_AVScreenCaptureBufferType bufferType, int64_t timestamp);

private:
    const video::VideoProfile profile_;

    bool isInitialized_{false};
    bool isStarted_{false};

    ohos::AVScreenCapture screenCapture_;

    std::unique_ptr<VideoFrameReceiver> dataReceiver_;

    Observer* observer_{};
    std::mutex obsMutex_;
};

} // namespace webrtc

#endif // WEBRTC_DESKTOP_CAPTURER_H
