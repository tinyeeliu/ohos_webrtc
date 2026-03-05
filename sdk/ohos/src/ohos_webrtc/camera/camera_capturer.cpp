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

Camera_Device* CameraCapturer::SelectCameraDevice()
{
    int32_t deviceIndex = -1;
    Camera_Device* device = nullptr;
    auto devices = CameraManager::GetInstance().GetSupportedCameras();
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
    OnDisplayChange(0);
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
        return;
    }

    GetCameraOrientationAndSceneMode(device);
    auto capability = CameraManager::GetInstance().GetSupportedCameraOutputCapability(device);
    Camera_Profile* previewProfile = FindPreviewProfile(capability, device);
    if (previewProfile == nullptr) {
        return;
    }

    input_ = CameraManager::GetInstance().CreateCameraInput(device);
    if (input_.IsEmpty()) {
        RTC_LOG(LS_ERROR) << "Failed to create camera input";
        return;
    }

    if (!input_.Open()) {
        RTC_LOG(LS_ERROR) << "Failed to open camera input";
        return;
    }

    if (!StartCaptureSession(previewProfile)) {
        return;
    }

    isStarted_ = true;
}

void CameraCapturer::StopInternal()
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (!isStarted_) {
        RTC_LOG(LS_ERROR) << "Capture session is not started";
        return;
    }

    if (!captureSession_.Stop()) {
        RTC_LOG(LS_ERROR) << "Failed to stop capture session";
        return;
    }

    previewOutput_.RemoveObserver(this);
    previewOutput_.Reset();

    input_.Close();
    input_.Reset();
    captureSession_.Reset();
    ohos::DisplayManager::GetInstance().UnregisterChangeCallback(this);
    isStarted_ = false;
}

void CameraCapturer::OnCameraManagerStatusCallback(Camera_StatusInfo* status)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

void CameraCapturer::OnPreviewOutputFrameStart()
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

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
    int32_t cameraOrientation = cameraOrientation_;
    if (cameraOrientation % VideoRotation::kVideoRotation_90 != 0) {
        RTC_LOG(LS_WARNING) << "rotation must be a multiple of 90: " << cameraOrientation;
        cameraOrientation = 0;
    }

    // Undo the mirror that the OS "helps" us with.
    // Also, undo camera orientation, we report it as rotation instead.
    if (buffer->type() == VideoFrameBuffer::Type::kNative) {
        auto textureBuffer = static_cast<TextureBuffer*>(buffer.get());

        Matrix transformMatrix;
        // Perform mirror and rotation around (0.5, 0.5) since that is the center of the texture.
        if (isCameraFrontFacing_) {
            transformMatrix.PreScale(-1.0f, 1.0f, 0.5f, 0.5f);
        }
        transformMatrix.PreRotate(finalRotation_, 0.5f, 0.5f);
        Matrix newMatrix = textureBuffer->GetTransformMatrix();
        newMatrix.PreConcat(transformMatrix);
        buffer = TextureBuffer::Create(
            textureBuffer->GetTexture(), textureBuffer->width(), textureBuffer->height(), newMatrix);
    }

    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnFrameCaptured(buffer, timestampUs, VideoRotation::kVideoRotation_0);
    }
}

void CameraCapturer::OnDisplayChange(uint64_t displayId)
{
    NativeDisplayManager_Rotation displayRotation = ohos::DisplayManager::GetInstance().GetDefaultDisplayRotation();
    RTC_DLOG(LS_VERBOSE) << "cameraRotation: " << cameraOrientation_ << ", displayRotation: " << displayRotation <<
        ", isFrontFacing_: " << isCameraFrontFacing_;
    constexpr int kDegreesPerRotationUnit = 90;
    constexpr int kFullCircleDegrees = 360;
    int displayDegrees = displayRotation * kDegreesPerRotationUnit;
    if (isCameraFrontFacing_) {
        finalRotation_ =
            static_cast<VideoRotation>((0 - displayDegrees + kFullCircleDegrees) % kFullCircleDegrees);
    } else {
        finalRotation_ = static_cast<VideoRotation>((displayDegrees) % kFullCircleDegrees);
    }
}

} // namespace webrtc
