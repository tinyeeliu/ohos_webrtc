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

#ifndef WEBRTC_CAMERA_CAPTURER_H
#define WEBRTC_CAMERA_CAPTURER_H

#include "camera_device_info.h"
#include "../video/video_capturer.h"
#include "../video/video_frame_receiver.h"
#include "../render/egl_context.h"
#include "../helper/camera.h"
#include "helper/display_manager.h"
#include "../helper/posture_observer.h"

#include <memory>
#include <cstdint>
#include <mutex>
#include <atomic>

#include <ohcamera/camera_manager.h>

#include "api/video/video_frame_buffer.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "rtc_base/thread.h"

namespace webrtc {

class CameraCapturer : public VideoCapturer,
                       public VideoFrameReceiver::Callback,
                       public ohos::CameraPreviewOutput::Observer,
                       public ohos::CameraVideoOutput::Observer,
                       public ohos::DisplayManager::ChangeObserver,
                       public ohos::PostureObserver::Observer {
public:
    static std::unique_ptr<CameraCapturer> Create(std::string deviceId, video::VideoProfile profile);

    // Do not use this constructor directly, use 'Create' above.
    CameraCapturer(std::string deviceId, video::VideoProfile profile);
    ~CameraCapturer() override;

    void Init(std::unique_ptr<VideoFrameReceiver>, VideoCapturer::Observer* observer) override;
    void Release() override;
    void Start() override;
    void Stop() override;
    bool IsScreencast() override;
    void SwitchCamera() override;
    void SwitchCamera(const std::string& cameraId) override;
    void ApplyDisplayRotation(int displayDegrees) override;

    void SetZoomRatio(double ratio) override;
    bool GetZoomRatioRange(float* minZoom, float* maxZoom) override;

protected:
    void NotifyCapturedStart(bool success);
    void NotifyCapturedStop();

protected:
    void StartInternal();
    void StopInternal();

    void OnCameraManagerStatusCallback(Camera_StatusInfo* status);
    void OnPreviewOutputFrameStart() override;
    void OnPreviewOutputFrameEnd(int32_t frameCount) override;
    void OnPreviewOutputError(Camera_ErrorCode errorCode) override;
    void OnVideoOutputFrameStart() override;
    void OnVideoOutputFrameEnd(int32_t frameCount) override;
    void OnVideoOutputError(Camera_ErrorCode errorCode) override;

    void
    OnFrameAvailable(rtc::scoped_refptr<VideoFrameBuffer> buffer, int64_t timestampUs, VideoRotation rotation) override;
    void OnDisplayChange(uint64_t displayId) override;
    void OnPostureFamilyChanged(ohos::PostureFamily family) override;

private:
    Camera_Device* SelectCameraDevice();
    void GetCameraOrientationAndSceneMode(Camera_Device* device);
    Camera_Profile* FindPreviewProfile(ohos::CameraOutputCapability& capability, Camera_Device* device);
    bool StartCaptureSession(Camera_Profile* previewProfile);
    void UpdateRotation();
    void ApplyPreviewRotation();
    void ApplyPreviewRotation(int displayDegrees);
    void CancelDisplayRotationTasks();
    // issue #200: 前置姿态错配补偿评估(帧回调线程逐帧调用)。相机 HAL 对前置流
    // 存在姿态跟随的像素级旋转(真机实测, 不经系统变换矩阵/帧元数据, 预览旋转
    // 声明亦不废除), 窗口方向锁定时姿态族与窗口族错配使画面相对窗口倒置 180 度;
    // 含持续性确认以过滤自动旋转时传感器与 display 变化的瞬态错配。
    int EvaluatePostureCompensation();

    // Written only while the display observer is unregistered. Unregister waits
    // for in-flight callbacks; queued tasks use a fresh safety flag per session.
    rtc::Thread* captureThread_{nullptr};
    rtc::scoped_refptr<PendingTaskSafetyFlag> displayRotationSafety_;
    // -1 follows the default display; otherwise the host window owns rotation.
    int displayRotationOverride_{-1};
    std::atomic<bool> multiDevicePreviewEnabled_{false};

    // 切换摄像头时需更新为新的设备 ID, 故非 const。

    // 切换摄像头时需更新为新的设备 ID, 故非 const。
    std::string deviceId_;
    const video::VideoProfile profile_;

    bool isInitialized_{false};
    bool isStarted_{false};
    // 摄像头切换进行中标记(防重入; 切换期间屏蔽 NotifyCapturedStop)。
    std::atomic<bool> isSwitching_{false};

    bool useVideoSceneMode_{false};
    bool isCameraFrontFacing_{false};
    uint32_t cameraOrientation_{0};

    ohos::CameraDevices cameraDevices_;
    ohos::CameraInput input_;
    ohos::CameraPreviewOutput previewOutput_;
    ohos::CameraVideoOutput videoOutput_;
    ohos::CameraCaptureSession captureSession_;

    std::unique_ptr<VideoFrameReceiver> dataReceiver_;

    VideoCapturer::Observer* observer_{};
    std::mutex obsMutex_;
    std::atomic<VideoRotation> finalRotation_{kVideoRotation_0};
    std::atomic<VideoRotation> i420Rotation_{kVideoRotation_0};
    std::atomic<bool> previewRotationAppliedByHal_{false};
    // issue #200: display rotation 的族归属(0/180 为竖族), capture 线程写入,
    // 帧回调线程读取。
    std::atomic<bool> displayIsPortraitFamily_{true};
    // 姿态错配持续性确认状态, 仅帧回调线程访问。
    int64_t postureMismatchSinceMs_{0};
    int postureCompensationDegrees_{0};
    // Accessed only by the frame receiver callback thread.
    int deliveredWidth_{0};
    int deliveredHeight_{0};
};

} // namespace webrtc

#endif // WEBRTC_CAMERA_CAPTURER_H
