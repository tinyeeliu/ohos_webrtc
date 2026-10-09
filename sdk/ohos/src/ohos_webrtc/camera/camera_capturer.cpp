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

#include "camera_capturer.h"
#include "preview_rotation_policy.h"
#include "../video/video_frame_receiver_gl.h"
#include "../utils/marcos.h"

#include <ohcamera/camera_device.h>
#include <ohcamera/capture_session.h>

#include <algorithm>

#include "rtc_base/logging.h"

namespace webrtc {

using namespace ohos;

std::unique_ptr<CameraCapturer> CameraCapturer::Create(std::string deviceId, video::VideoProfile profile)
{
    return std::make_unique<CameraCapturer>(deviceId, profile);
}

CameraCapturer::CameraCapturer(std::string deviceId, video::VideoProfile profile)
    : deviceId_(deviceId), profile_(profile)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << ": " << this;
}

CameraCapturer::~CameraCapturer()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    Release();
}

void CameraCapturer::NotifyCapturedStart(bool success)
{
    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnCapturerStarted(success);
    }
}

void CameraCapturer::NotifyCapturedStop()
{
    // 摄像头切换期间(StopInternal→StartInternal)屏蔽 stop 通知: 否则 track readyState 会短暂变
    // 'ended', 触发上层 RTCVideoView RemoveSink→capturer->Stop(), 与 SwitchCamera 的 StartInternal
    // 竞态导致新建 session 又被停(卡死)。是否补发由 SwitchCamera 末尾按重建结果决定。
    if (isSwitching_.load()) {
        return;
    }
    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnCapturerStopped();
    }
}

void CameraCapturer::Init(std::unique_ptr<VideoFrameReceiver> dataReceiver, VideoCapturer::Observer* observer)
{
    RTC_LOG(LS_INFO) << __FUNCTION__;
    captureThread_ = rtc::Thread::Current();
    RTC_CHECK(captureThread_);

    {
        UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
        observer_ = observer;
    }

    dataReceiver_ = std::move(dataReceiver);
    dataReceiver_->SetVideoFrameSize(profile_.resolution.width, profile_.resolution.height);
    dataReceiver_->SetCallback(this);
    dataReceiver_->SetTimestampConverter(
        TimestampConverter([](int64_t timestamp) { return TimestampCast<std::nano, std::micro>(timestamp); }));

    isInitialized_ = true;
}

void CameraCapturer::Release()
{
    RTC_LOG(LS_INFO) << __FUNCTION__;
    // The owner releases on the capture thread before joining it. The destructor
    // may run later on another thread, so repeated release must do nothing.
    if (!isInitialized_) {
        return;
    }
    RTC_DCHECK_RUN_ON(captureThread_);
    CancelDisplayRotationTasks();
    Stop();

    {
        UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
        observer_ = nullptr;
    }

    dataReceiver_.reset();

    isInitialized_ = false;
    captureThread_ = nullptr;
}

void CameraCapturer::Start()
{
    RTC_LOG(LS_INFO) << __FUNCTION__ << ": this=" << this;

    StartInternal();
}

void CameraCapturer::Stop()
{
    RTC_LOG(LS_INFO) << __FUNCTION__ << ": this=" << this;

    StopInternal();
}

bool CameraCapturer::IsScreencast()
{
    return false;
}

void CameraCapturer::SwitchCamera()
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    auto devices = CameraManager::GetInstance().GetSupportedCameras();
    const std::size_t minDeviceCount = 2;
    if (devices.Size() < minDeviceCount) {
        RTC_LOG(LS_ERROR) << "No camera to switch to";
        {
            UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
            if (observer_) {
                observer_->OnCameraSwitchError("No camera to switch to");
            }
        }
        return;
    }

    // 定位当前摄像头在设备列表中的下标。
    int currentIndex = -1;
    for (std::size_t i = 0; i < devices.Size(); i++) {
        if (deviceId_ == devices[i]->cameraId) {
            currentIndex = static_cast<int>(i);
            break;
        }
    }

    if (currentIndex == -1) {
        RTC_LOG(LS_ERROR) << "Current camera not found";
        {
            UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
            if (observer_) {
                observer_->OnCameraSwitchError("Current camera not found");
            }
        }
        return;
    }

    // 切换到下一个可用摄像头, 防重入/错误处理交给 SwitchCamera(cameraId) 统一负责。
    std::size_t nextIndex = (static_cast<std::size_t>(currentIndex) + 1) % devices.Size();
    std::string nextCameraId = devices[nextIndex]->cameraId;
    SwitchCamera(nextCameraId);
}

void CameraCapturer::SwitchCamera(const std::string& cameraId)
{
    RTC_LOG(LS_INFO) << __FUNCTION__ << ": Switching to camera " << cameraId;

    // 防重入。
    bool expected = false;
    if (!isSwitching_.compare_exchange_strong(expected, true)) {
        RTC_LOG(LS_WARNING) << "Camera switch already in progress";
        return;
    }

    if (!isStarted_) {
        RTC_LOG(LS_ERROR) << "Camera is not started";
        isSwitching_ = false;
        {
            UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
            if (observer_) {
                observer_->OnCameraSwitchError("Camera is not started");
            }
        }
        return;
    }

    // 校验目标摄像头是否存在。
    auto devices = CameraManager::GetInstance().GetSupportedCameras();
    bool cameraExists = false;
    for (std::size_t i = 0; i < devices.Size(); i++) {
        if (cameraId == devices[i]->cameraId) {
            cameraExists = true;
            break;
        }
    }

    if (!cameraExists) {
        RTC_LOG(LS_ERROR) << "Camera not found: " << cameraId;
        isSwitching_ = false;
        {
            UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
            if (observer_) {
                observer_->OnCameraSwitchError("Camera not found: " + cameraId);
            }
        }
        return;
    }

    StopInternal();
    if (isStarted_) {
        RTC_LOG(LS_ERROR) << "Failed to stop current camera, aborting switch";
        isStarted_ = false;
        {
            UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
            if (observer_) {
                observer_->OnCameraSwitchError("Failed to stop current camera");
            }
        }
        return;
    }
    deviceId_ = cameraId;
    StartInternal();

    isSwitching_ = false;
    if (!isStarted_) {
        NotifyCapturedStop();
        {
            UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
            if (observer_) {
                observer_->OnCameraSwitchError("Failed to start new camera: " + cameraId);
            }
        }
        return;
    }

    // 通知切换成功。
    {
        UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
        if (observer_) {
            observer_->OnCameraSwitchDone(cameraId);
        }
    }
}

Camera_Device* CameraCapturer::SelectCameraDevice()
{
    int32_t deviceIndex = -1;
    Camera_Device* device = nullptr;
    cameraDevices_ = CameraManager::GetInstance().GetSupportedCameras();
    auto& devices = cameraDevices_;
    for (std::size_t i = 0; i < devices.Size(); i++) {
        if (deviceId_ == devices[i]->cameraId) {
            deviceIndex = i;
            device = devices[i];
            break;
        }
    }

    if (deviceIndex == -1) {
        RTC_LOG(LS_ERROR) << "No specific camera device found";
        return nullptr;
    }

    RTC_LOG(LS_VERBOSE) << "device: " << deviceIndex << ", " << device->cameraId;
    isCameraFrontFacing_ = device->cameraPosition == CAMERA_POSITION_FRONT;
    return device;
}

void CameraCapturer::GetCameraOrientationAndSceneMode(Camera_Device* device)
{
    uint32_t orientation = 0;
    Camera_ErrorCode ret = OH_CameraDevice_GetCameraOrientation(device, &orientation);
    if (ret == CAMERA_OK) {
        RTC_DLOG(LS_VERBOSE) << "camera orientation: " << orientation;
        cameraOrientation_ = orientation;
    } else {
        RTC_LOG(LS_ERROR) << "Failed to get camera orientation";
    }

    auto sceneModes = CameraManager::GetInstance().GetSupportedSceneModes(device);
    for (std::size_t i = 0; i < sceneModes.Size(); i++) {
        RTC_DLOG(LS_VERBOSE) << "supported scene mode: " << sceneModes[i];
        if (sceneModes[i] == NORMAL_VIDEO) {
            useVideoSceneMode_ = true;
        }
    }
}

Camera_Profile* CameraCapturer::FindPreviewProfile(CameraOutputCapability& capability, Camera_Device* device)
{
    Camera_Profile* previewProfile = nullptr;
    Camera_VideoProfile* videoProfile = nullptr;
    for (uint32_t i = 0; i < capability.PreviewProfileSize(); i++) {
        Camera_Profile* p = capability.GetPreviewProfile(i);
        RTC_DLOG(LS_VERBOSE) << "preview format: " << p->format;
        RTC_DLOG(LS_VERBOSE) << "preview size: " << p->size.width << "x" << p->size.height;

        if (profile_.format == NativeCameraFormatToPixelFormat(p->format) &&
            profile_.resolution.width == p->size.width && profile_.resolution.height == p->size.height) {
            previewProfile = p;
            break;
        }
    }

    if (!videoProfile && !previewProfile) {
        RTC_LOG(LS_ERROR) << "No specific camera device found";
        return nullptr;
    }
    return previewProfile;
}

bool CameraCapturer::StartCaptureSession(Camera_Profile* previewProfile)
{
    auto surfaceId = std::to_string(dataReceiver_->GetSurfaceId());

    previewOutput_ = CameraManager::GetInstance().CreatePreviewOutput(previewProfile, surfaceId);
    previewRotationAppliedByHal_ = false;
    if (previewOutput_.IsEmpty()) {
        RTC_LOG(LS_ERROR) << "Failed to create camera preview output";
        return false;
    }
    previewOutput_.AddObserver(this);

    captureSession_ = CameraManager::GetInstance().CreateCaptureSession();
    if (useVideoSceneMode_) {
        if (OH_CaptureSession_SetSessionMode(captureSession_.Raw(), NORMAL_VIDEO) != CAMERA_OK) {
            RTC_LOG(LS_ERROR) << "Failed to set scene mode";
        }
    }

    if (!captureSession_.BeginConfig()) {
        RTC_LOG(LS_ERROR) << "Failed to begin capture session config";
        return false;
    }

    captureSession_.AddInput(input_);
    captureSession_.AddPreviewOutput(previewOutput_);
    if (!captureSession_.CommitConfig()) {
        RTC_LOG(LS_ERROR) << "Failed to commit capture session config";
        return false;
    }
    ApplyPreviewRotation();

    if (!captureSession_.Start()) {
        RTC_LOG(LS_ERROR) << "Failed to start capture session";
        NotifyCapturedStart(false);
        return false;
    }
    return true;
}

void CameraCapturer::StartInternal()
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (!isInitialized_) {
        RTC_LOG(LS_ERROR) << "Not initialized";
        NotifyCapturedStart(false);
        return;
    }

    if (isStarted_) {
        RTC_LOG(LS_WARNING) << "Capture session is started";
        return;
    }

    RTC_DCHECK_RUN_ON(captureThread_);

    Camera_Device* device = SelectCameraDevice();
    if (device == nullptr) {
        RTC_LOG(LS_ERROR) << "Failed to select camera device";
        NotifyCapturedStart(false);
        return;
    }

    GetCameraOrientationAndSceneMode(device);
    UpdateRotation();
    auto capability = CameraManager::GetInstance().GetSupportedCameraOutputCapability(device);
    Camera_Profile* previewProfile = FindPreviewProfile(capability, device);
    if (previewProfile == nullptr) {
        RTC_LOG(LS_ERROR) << "Failed to find preview profile";
        NotifyCapturedStart(false);
        return;
    }

    input_ = CameraManager::GetInstance().CreateCameraInput(device);
    if (input_.IsEmpty()) {
        RTC_LOG(LS_ERROR) << "Failed to create camera input";
        NotifyCapturedStart(false);
        return;
    }

    if (!input_.Open()) {
        RTC_LOG(LS_ERROR) << "Failed to open camera input";
        NotifyCapturedStart(false);
        return;
    }
    if (multiDevicePreviewEnabled_.load()) {
        input_.UsePhysicalCameraOrientationIfVariable();
    }

    if (!StartCaptureSession(previewProfile)) {
        // StartCaptureSession already calls NotifyCapturedStart(false) on failure.
        return;
    }

    isStarted_ = true;
    displayRotationSafety_ = PendingTaskSafetyFlag::Create();
    ohos::DisplayManager::GetInstance().RegisterChangeCallback(this);
    // Catch any display change that occurred while the session was starting.
    UpdateRotation();
    NotifyCapturedStart(true);
}

void CameraCapturer::StopInternal()
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (isInitialized_) {
        RTC_DCHECK_RUN_ON(captureThread_);
    }

    if (!isStarted_) {
        RTC_LOG(LS_ERROR) << "Capture session is not started";
        return;
    }

    // 释放顺序: CaptureSession 持有 input/previewOutput 的引用, 必须先停并释放 session,
    // 再释放 input/preview, 否则系统侧异步 UpdateSetting 会报 inputDevice is nullptr。
    // Stop 失败时保留原版行为(直接返回, 不强行释放), 避免对仍在运行的 session 做破坏性释放。
    if (!captureSession_.Stop()) {
        RTC_LOG(LS_ERROR) << "Failed to stop capture session";
        return;
    }
    CancelDisplayRotationTasks();
    captureSession_.Reset();

    previewOutput_.RemoveObserver(this);
    previewOutput_.Reset();
    previewRotationAppliedByHal_ = false;

    input_.Close();
    input_.Reset();

    cameraDevices_.Reset();
    isStarted_ = false;
}

void CameraCapturer::SetZoomRatio(double ratio)
{
    RTC_LOG(LS_INFO) << __FUNCTION__ << ": ratio=" << ratio;

    if (!isStarted_ || !captureSession_.Raw()) {
        RTC_LOG(LS_WARNING) << "SetZoomRatio ignored: capture session not active";
        return;
    }

    float minZoom = 1.0f;
    float maxZoom = 1.0f;
    bool supported = GetZoomRatioRange(&minZoom, &maxZoom);
    if (!supported) {
        RTC_LOG(LS_WARNING) << "SetZoomRatio ignored: zoom not supported by device";
        return;
    }

    float clamped = std::clamp(static_cast<float>(ratio), minZoom, maxZoom);
    RTC_LOG(LS_INFO) << "SetZoomRatio: clamped=" << clamped << " (range " << minZoom << "-" << maxZoom << ")";

    Camera_ErrorCode ret = OH_CaptureSession_SetZoomRatio(captureSession_.Raw(), clamped);
    if (ret != CAMERA_OK) {
        RTC_LOG(LS_ERROR) << "OH_CaptureSession_SetZoomRatio failed, code=" << ret << ", ratio=" << clamped;
    } else {
        RTC_LOG(LS_INFO) << "OH_CaptureSession_SetZoomRatio OK, ratio=" << clamped;
    }
}

bool CameraCapturer::GetZoomRatioRange(float* minZoom, float* maxZoom)
{
    if (!captureSession_.Raw()) {
        RTC_LOG(LS_WARNING) << "GetZoomRatioRange: captureSession_ is null";
        if (minZoom) {
            *minZoom = 1.0f;
        }
        if (maxZoom) {
            *maxZoom = 1.0f;
        }
        return false;
    }
    Camera_ErrorCode ret = OH_CaptureSession_GetZoomRatioRange(captureSession_.Raw(), minZoom, maxZoom);
    if (ret != CAMERA_OK) {
        RTC_LOG(LS_WARNING) << "OH_CaptureSession_GetZoomRatioRange failed, code=" << ret;
        return false;
    }
    RTC_LOG(LS_INFO) << "GetZoomRatioRange OK, min=" << *minZoom << ", max=" << *maxZoom;
    return true;
}

void CameraCapturer::OnCameraManagerStatusCallback(Camera_StatusInfo* status)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

void CameraCapturer::OnPreviewOutputFrameStart()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    NotifyCapturedStart(true);
}

void CameraCapturer::OnPreviewOutputFrameEnd(int32_t frameCount)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    NotifyCapturedStop();
}

void CameraCapturer::OnPreviewOutputError(Camera_ErrorCode errorCode)
{
    RTC_LOG(LS_ERROR) << __FUNCTION__ << ": errorCode=" << errorCode;
    // Notify the reusable session owner. Recovery itself is serialized through
    // the source's capture thread, never performed from this system callback.
    if (multiDevicePreviewEnabled_.load()) { NotifyCapturedStop(); }
}

void CameraCapturer::OnVideoOutputFrameStart()
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    NotifyCapturedStart(true);
}

void CameraCapturer::OnVideoOutputFrameEnd(int32_t frameCount)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    NotifyCapturedStop();
}

void CameraCapturer::OnVideoOutputError(Camera_ErrorCode errorCode)
{
    RTC_LOG(LS_ERROR) << __FUNCTION__ << ": errorCode=" << errorCode;
    if (multiDevicePreviewEnabled_.load()) { NotifyCapturedStop(); }
}

void CameraCapturer::OnFrameAvailable(
    rtc::scoped_refptr<VideoFrameBuffer> buffer, int64_t timestampUs, VideoRotation rotation)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    (void)rotation;
    VideoRotation frameRotation = kVideoRotation_0;
    // captureThread_ may flip this flag mid-frame; one snapshot keeps rotation and dimensions consistent.
    const bool halApplied = previewRotationAppliedByHal_.load();

    if (buffer->type() == VideoFrameBuffer::Type::kNative) {
        auto textureBuffer = static_cast<TextureBuffer*>(buffer.get());
        const VideoRotation previewRotation = finalRotation_.load();

        Matrix transformMatrix;

        if (isCameraFrontFacing_) {
            transformMatrix.PreScale(-1.0f, 1.0f, 0.5f, 0.5f);
        }
        // Software fallback retains the original texture transform.
        if (!halApplied) {
            transformMatrix.PreRotate(previewRotation, 0.5f, 0.5f);
        }
        Matrix newMatrix = textureBuffer->GetTransformMatrix();
        newMatrix.PreConcat(transformMatrix);
        // The profile describes the camera stream BEFORE preview rotation.
        // NativeImage's matrix describes how to SAMPLE the producer's buffer,
        // which HAL may already have rotated. Its axes alone do not describe
        // the orientation of the image relative to the camera profile.
        // Use the camera preview rotation for logical frame dimensions, while
        // retaining the producer matrix for sampling. Otherwise landscape can
        // remain tagged as 720x1280 and every downstream aspect-fit stretches it.
        const bool swapDimensions = halApplied &&
            (previewRotation == kVideoRotation_90 || previewRotation == kVideoRotation_270);
        const int frameWidth = swapDimensions ? textureBuffer->height() : textureBuffer->width();
        const int frameHeight = swapDimensions ? textureBuffer->width() : textureBuffer->height();
        if (frameWidth != deliveredWidth_ || frameHeight != deliveredHeight_) {
            deliveredWidth_ = frameWidth;
            deliveredHeight_ = frameHeight;
            RTC_LOG(LS_INFO) << "[CameraFrameGeometry] profile=" << textureBuffer->width() << "x"
                             << textureBuffer->height() << ", previewRotation=" << previewRotation
                             << ", halApplied=" << halApplied
                             << ", frame=" << frameWidth << "x" << frameHeight
                             << ", samplingMatrix=" << newMatrix;
        }
        buffer = TextureBuffer::Create(
            textureBuffer->GetTexture(), frameWidth, frameHeight, newMatrix);
    } else {
        // CPU 路径：若 HAL 未写入旋转，则通过 rotation 元数据上报。
        if (!halApplied) {
            frameRotation = i420Rotation_;
        }
    }

    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnFrameCaptured(buffer, timestampUs, frameRotation);
    }
}

void CameraCapturer::ApplyPreviewRotation()
{
    if (displayRotationOverride_ >= 0) {
        ApplyPreviewRotation(displayRotationOverride_);
        return;
    }
    NativeDisplayManager_Rotation displayRotation = ohos::DisplayManager::GetInstance().GetDefaultDisplayRotation();
    constexpr int kDegreesPerRotationUnit = 90;
    ApplyPreviewRotation(displayRotation * kDegreesPerRotationUnit);
}

void CameraCapturer::ApplyPreviewRotation(int displayDegrees)
{
    RTC_DCHECK_RUN_ON(captureThread_);
    constexpr int kDegreesPerRotationUnit = 90;
    constexpr int kFullCircleDegrees = 360;
    displayDegrees = ((displayDegrees % kFullCircleDegrees) + kFullCircleDegrees) % kFullCircleDegrees;
    displayDegrees = (displayDegrees / kDegreesPerRotationUnit) * kDegreesPerRotationUnit;
    int cameraDegrees = static_cast<int>(cameraOrientation_);
    if (cameraDegrees % kVideoRotation_90 != 0) {
        RTC_LOG(LS_WARNING) << "camera orientation must be a multiple of 90: " << cameraDegrees;
        cameraDegrees = 0;
    }

    const auto hal = TryApplyPreviewRotation(multiDevicePreviewEnabled_.load(),
        previewRotationAppliedByHal_.load(), displayDegrees,
        [this](int display, int* degrees) {
            Camera_ImageRotation rotation = static_cast<Camera_ImageRotation>(0);
            const bool result = previewOutput_.GetPreviewRotation(display, &rotation);
            *degrees = static_cast<int>(rotation);
            return result;
        },
        [this](int degrees, bool isDisplayLocked) {
            return previewOutput_.SetPreviewRotation(static_cast<Camera_ImageRotation>(degrees), isDisplayLocked);
        });
    if (hal.recoveryRequired) {
        NotifyCapturedStop();
        return;
    }
    const bool halApplied = hal.applied;
    const auto policy = ResolvePreviewRotation(cameraDegrees, displayDegrees,
        isCameraFrontFacing_, halApplied, hal.degrees);
    finalRotation_ = static_cast<VideoRotation>(policy.textureRotation);
    i420Rotation_ = static_cast<VideoRotation>(policy.frameRotation);
    previewRotationAppliedByHal_ = halApplied;
    RTC_LOG(LS_INFO) << "ApplyPreviewRotation: display=" << displayDegrees
                     << ", halApplied=" << halApplied << ", texture=" << policy.textureRotation
                     << ", frame=" << policy.frameRotation;
}

void CameraCapturer::ApplyDisplayRotation(int displayDegrees)
{
    if (!isInitialized_) {
        return;
    }
    RTC_DCHECK_RUN_ON(captureThread_);
    multiDevicePreviewEnabled_ = true;
    displayRotationOverride_ = displayDegrees;
    if (!isStarted_) {
        return;
    }
    ApplyPreviewRotation();
}

void CameraCapturer::UpdateRotation()
{
    ApplyPreviewRotation();
}

void CameraCapturer::OnDisplayChange(uint64_t displayId)
{
    (void)displayId;
    // DisplayManager holds its observer lock here. Never wait for the capture
    // thread, which may be unregistering this observer while stopping capture.
    captureThread_->PostTask(SafeTask(displayRotationSafety_, [this] {
        if (displayRotationOverride_ < 0) {
            UpdateRotation();
        }
    }));
}

void CameraCapturer::CancelDisplayRotationTasks()
{
    if (!displayRotationSafety_) {
        return;
    }
    RTC_DCHECK_RUN_ON(captureThread_);
    ohos::DisplayManager::GetInstance().UnregisterChangeCallback(this);
    displayRotationSafety_->SetNotAlive();
    displayRotationSafety_ = nullptr;
}

} // namespace webrtc
