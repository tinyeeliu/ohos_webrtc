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

// 应用窗口方向声明。采集器无法从系统 API 获知应用通过
// window.setPreferredOrientation 锁定的窗口方向, 需由应用显式告知,
// 用于窗口方向与设备物理方向不一致时(如锁定横屏但竖握设备)的前置摄像头
// 旋转补偿 (issue #200)。
enum class WindowOrientation {
    kAuto = 0,      // 未声明/自动旋转: 窗口方向跟随物理方向(默认, 保持既有行为)
    kLandscape = 1, // 应用锁定横屏窗口
    kPortrait = 2,  // 应用锁定竖屏窗口
};

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

    // 应用声明窗口方向。默认 no-op, 仅 CameraCapturer override;
    // 录屏等其他采集器无需处理。
    virtual void SetWindowOrientation(WindowOrientation orientation) { (void)orientation; }
};

} // namespace webrtc

#endif // WEBRTC_VIDEO_VIDEO_CAPTURER_H
