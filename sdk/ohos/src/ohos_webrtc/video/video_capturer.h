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
    virtual bool IsExternal() const { return false; }

    // Camera zoom. Default no-op; non-camera capturers (e.g. ScreenCapturer) need not override.
    virtual void SetZoomRatio(double ratio) { (void)ratio; }
    virtual bool GetZoomRatioRange(float* minZoom, float* maxZoom)
    {
        if (minZoom) {
            *minZoom = 1.0f;
        }
        if (maxZoom) {
            *maxZoom = 1.0f;
        }
        return false;
    }

    virtual void SwitchCamera() {}
    virtual void SwitchCamera(const std::string& cameraId) { (void)cameraId; }

    // 宿主提供当前已生效的显示角度；-1 恢复默认显示器自动模式。
    virtual void ApplyDisplayRotation(int displayDegrees) { (void)displayDegrees; }
};

} // namespace webrtc

#endif // WEBRTC_VIDEO_VIDEO_CAPTURER_H
