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
#include "../video/video_frame_receiver_gl.h"
#include "../utils/marcos.h"

#include <ohcamera/camera_device.h>
#include <ohcamera/capture_session.h>
#include <ohcamera/preview_output.h>

#include <algorithm>
#include <chrono>

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
    ohos::DisplayManager::GetInstance().UnregisterChangeCallback(this);
    // 安全网: 覆盖启动失败路径(未走到 StopInternal)与析构场景, 重复注销无副作用。
    ohos::PostureObserver::GetInstance().UnregisterObserver(this);
    Stop();

    {
        UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
        observer_ = nullptr;
    }

    dataReceiver_.reset();

    isInitialized_ = false;
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
    ohos::DisplayManager::GetInstance().RegisterChangeCallback(this);
    if (isCameraFrontFacing_) {
        // issue #200: 相机侧姿态跟随像素旋转仅作用于前置流, 仅前置需要姿态感知补偿。
        ohos::PostureObserver::GetInstance().RegisterObserver(this);
    }
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

    // issue #200: 官方《适配相机旋转角度(C/C++)》规定, CommitConfig 之后、Start 之前
    // 必须声明预览旋转角度, 否则相机服务对预览流采用默认的姿态跟随调整
    // (锁定窗口方向时导致画面相对窗口倒置)。
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

    if (!StartCaptureSession(previewProfile)) {
        // StartCaptureSession already calls NotifyCapturedStart(false) on failure.
        return;
    }

    isStarted_ = true;
    NotifyCapturedStart(true);
}

void CameraCapturer::StopInternal()
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

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
    captureSession_.Reset();

    previewOutput_.RemoveObserver(this);
    previewOutput_.Reset();

    input_.Close();
    input_.Reset();

    cameraDevices_.Reset();
    ohos::DisplayManager::GetInstance().UnregisterChangeCallback(this);
    ohos::PostureObserver::GetInstance().UnregisterObserver(this);
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
}

void CameraCapturer::OnFrameAvailable(
    rtc::scoped_refptr<VideoFrameBuffer> buffer, int64_t timestampUs, VideoRotation rotation)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    (void)rotation;
    VideoRotation frameRotation = kVideoRotation_0;
    // issue #200: 姿态错配补偿逐帧评估(含持续性确认, 仅官方声明成功路径生效)。
    int compensationDegrees = EvaluatePostureCompensation();
    constexpr int kFullCircleDegrees = 360;

    if (buffer->type() == VideoFrameBuffer::Type::kNative) {
        auto textureBuffer = static_cast<TextureBuffer*>(buffer.get());

        Matrix transformMatrix;

        if (isCameraFrontFacing_) {
            transformMatrix.PreScale(-1.0f, 1.0f, 0.5f, 0.5f);
        }
        transformMatrix.PreRotate(
            (static_cast<int>(finalRotation_.load()) + compensationDegrees) % kFullCircleDegrees, 0.5f, 0.5f);
        Matrix newMatrix = textureBuffer->GetTransformMatrix();
        newMatrix.PreConcat(transformMatrix);
        buffer = TextureBuffer::Create(
            textureBuffer->GetTexture(), textureBuffer->width(), textureBuffer->height(), newMatrix);
    } else {
        // CPU 路径（x86_64 模拟器 ImageReceiver）：无系统变换矩阵，通过 rotation 元数据上报。
        frameRotation = static_cast<VideoRotation>(
            (static_cast<int>(i420Rotation_.load()) + compensationDegrees) % kFullCircleDegrees);
    }

    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnFrameCaptured(buffer, timestampUs, frameRotation);
    }
}

void CameraCapturer::UpdateRotation()
{
    NativeDisplayManager_Rotation displayRotation = ohos::DisplayManager::GetInstance().GetDefaultDisplayRotation();
    constexpr int kDegreesPerRotationUnit = 90;
    constexpr int kFullCircleDegrees = 360;
    int displayDegrees = displayRotation * kDegreesPerRotationUnit;
    int cameraDegrees = static_cast<int>(cameraOrientation_);
    if (cameraDegrees % kVideoRotation_90 != 0) {
        RTC_LOG(LS_WARNING) << "camera orientation must be a multiple of 90: " << cameraDegrees;
        cameraDegrees = 0;
    }

    if (isCameraFrontFacing_) {
        finalRotation_ =
            static_cast<VideoRotation>((kFullCircleDegrees - displayDegrees) % kFullCircleDegrees);
        i420Rotation_ = static_cast<VideoRotation>((cameraDegrees + displayDegrees) % kFullCircleDegrees);
    } else {
        finalRotation_ = static_cast<VideoRotation>(displayDegrees % kFullCircleDegrees);
        i420Rotation_ = static_cast<VideoRotation>(
            (cameraDegrees - displayDegrees + kFullCircleDegrees) % kFullCircleDegrees);
    }

    RTC_LOG(LS_INFO) << "UpdateRotation: camera=" << cameraDegrees << ", display=" << displayDegrees
                     << ", finalRotation=" << finalRotation_ << ", i420Rotation=" << i420Rotation_
                     << ", isFrontFacing=" << isCameraFrontFacing_;
}

void CameraCapturer::ApplyPreviewRotation()
{
    if (previewOutput_.IsEmpty()) {
        return;
    }

    constexpr int kDegreesPerRotationUnit = 90;
    constexpr int kInvertedDegrees = 180;
    NativeDisplayManager_Rotation displayRotation = ohos::DisplayManager::GetInstance().GetDefaultDisplayRotation();
    int displayDegrees = displayRotation * kDegreesPerRotationUnit;
    // issue #200: 记录窗口方向族归属, 供帧回调的姿态错配补偿评估使用。
    displayIsPortraitFamily_.store(displayDegrees == 0 || displayDegrees == kInvertedDegrees,
        std::memory_order_relaxed);

    Camera_ImageRotation previewRotation = IAMGE_ROTATION_0;
    Camera_ErrorCode ret = OH_PreviewOutput_GetPreviewRotation(previewOutput_.Raw(), displayDegrees, &previewRotation);
    if (ret != CAMERA_OK) {
        RTC_LOG(LS_WARNING) << "OH_PreviewOutput_GetPreviewRotation failed, code=" << ret
                            << ", fallback to legacy rotation formula";
        previewRotationApplied_.store(false, std::memory_order_relaxed);
        UpdateRotation();
        return;
    }

    // isDisplayLocked: Surface 在屏幕旋转时是否锁定方向。本库渲染 Surface(NativeImage)
    // 不随窗口旋转联动, 按官方示例取 false。
    bool isDisplayLocked = false;
    ret = OH_PreviewOutput_SetPreviewRotation(previewOutput_.Raw(), previewRotation, isDisplayLocked);
    if (ret == CAMERA_OK) {
        // issue #200: 声明生效后, 相机服务输出的预览流已由系统按当前显示转正,
        // 渲染端不再叠加旋转(真机实测: 自动旋转各姿态画面正向且无闪跳),
        // 前置仅保留镜像; 旧经验公式作废。锁定窗口期间相机侧仍存在姿态跟随
        // 的像素旋转, 由 EvaluatePostureCompensation 的姿态错配补偿覆盖。
        previewRotationApplied_.store(true, std::memory_order_relaxed);
        finalRotation_ = kVideoRotation_0;
        i420Rotation_ = kVideoRotation_0;
    } else {
        RTC_LOG(LS_WARNING) << "OH_PreviewOutput_SetPreviewRotation failed, code=" << ret
                            << ", fallback to legacy rotation formula";
        previewRotationApplied_.store(false, std::memory_order_relaxed);
        UpdateRotation();
    }
    RTC_LOG(LS_INFO) << "ApplyPreviewRotation: displayDegrees=" << displayDegrees
                     << ", previewRotation=" << static_cast<int>(previewRotation)
                     << ", isDisplayLocked=" << isDisplayLocked << ", setResult=" << ret
                     << ", applied=" << previewRotationApplied_.load(std::memory_order_relaxed);
}

int CameraCapturer::EvaluatePostureCompensation()
{
    constexpr int kPostureMismatchCompensationDegrees = 180;
    constexpr int64_t kMismatchConfirmMs = 300;
    constexpr int kNoCompensation = 0;

    // 仅官方声明成功路径叠加姿态补偿; 回退路径保持旧公式原行为(零变更)。
    if (!isCameraFrontFacing_ || !previewRotationApplied_.load(std::memory_order_relaxed)) {
        return kNoCompensation;
    }

    // issue #200: 声明生效后相机侧仍依据物理姿态对前置流做像素级旋转(真机实测:
    // 锁横屏竖握画面倒置 180 度, 横握正常), 姿态族与窗口方向族错配时补偿。
    // 传感器不可用(宿主未声明 ohos.permission.ACCELEROMETER 等)时姿态族为
    // UNKNOWN, 不补偿, 优雅降级。
    ohos::PostureFamily postureFamily = ohos::PostureObserver::GetInstance().GetPostureFamily();
    bool mismatched = postureFamily != ohos::PostureFamily::UNKNOWN
        && (postureFamily == ohos::PostureFamily::PORTRAIT) != displayIsPortraitFamily_.load(std::memory_order_relaxed);

    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!mismatched) {
        postureMismatchSinceMs_ = 0;
    } else if (postureMismatchSinceMs_ == 0) {
        postureMismatchSinceMs_ = nowMs;
    }

    // 持续性确认: 自动旋转时传感器与 display 变化存在先后, 产生瞬态错配
    // (实测 <=146ms), 不应补偿; 窗口锁定场景错配为持久状态, 确认后稳定补偿。
    int compensation = (mismatched && nowMs - postureMismatchSinceMs_ >= kMismatchConfirmMs)
        ? kPostureMismatchCompensationDegrees
        : kNoCompensation;

    if (compensation != compensationDegrees_) {
        compensationDegrees_ = compensation;
        RTC_LOG(LS_INFO) << "EvaluatePostureCompensation: compensation=" << compensation
                         << ", postureFamily=" << static_cast<int>(postureFamily)
                         << ", displayIsPortraitFamily=" << displayIsPortraitFamily_.load(std::memory_order_relaxed);
    }
    return compensation;
}

void CameraCapturer::OnDisplayChange(uint64_t displayId)
{
    (void)displayId;
    // issue #200: 官方要求窗口旋转时重新声明预览旋转角度;
    // 声明失败时内部回退旧公式(UpdateRotation)。
    ApplyPreviewRotation();
}

void CameraCapturer::OnPostureFamilyChanged(ohos::PostureFamily family)
{
    // 传感器服务线程回调: 补偿由帧回调逐帧评估(含持续性确认), 此处仅记录事件。
    RTC_LOG(LS_INFO) << "OnPostureFamilyChanged: family=" << static_cast<int>(family);
}

} // namespace webrtc
