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

#ifndef WEBRTC_VIDEO_VIDEO_CAPTURER_H
#define WEBRTC_VIDEO_VIDEO_CAPTURER_H

#include "../video/video_frame_receiver.h"

#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"

#include <memory>
#include <mutex>

namespace webrtc {

class VideoCapturer {
public:
    class Observer {
    public:
        virtual ~Observer() {}
        virtual void OnCapturerStarted(bool success) = 0;
        virtual void OnCapturerStopped() = 0;
        virtual void
        OnFrameCaptured(rtc::scoped_refptr<VideoFrameBuffer> buffer, int64_t timestampUs, VideoRotation rotation) = 0;

        // 摄像头切换结果通知。带默认空实现, 仅需要响应的 Observer(如 OhosVideoTrackSource)
        // 才 override, 不破坏已有实现者(如录屏 ScreenCapturer 的使用者)。
        virtual void OnCameraSwitchDone(const std::string& newCameraId) { (void)newCameraId; }
        virtual void OnCameraSwitchError(const std::string& errorMessage) { (void)errorMessage; }
    };

    virtual ~VideoCapturer() = default;

    virtual void Init(std::unique_ptr<VideoFrameReceiver>, Observer* observer) = 0;
    virtual void Release() = 0;
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual bool IsScreencast() = 0;

    // 切换到下一个可用摄像头 / 切换到指定 cameraId。带默认空实现:
    // 录屏(ScreenCapturer)不支持切换, 继承空实现即可, 无需改动。
    virtual void SwitchCamera() {}
    virtual void SwitchCamera(const std::string& cameraId) { (void)cameraId; }
};

} // namespace webrtc

#endif // WEBRTC_VIDEO_VIDEO_CAPTURER_H
