/*
# Copyright (c) 2024 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/
#include "ohos_camera.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_rotation.h"
#include "libyuv/convert.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"
#include "hilog/log.h"
#include <GLES2/gl2ext.h>
#include <arm-linux-ohos/bits/alltypes.h>
#include <locale.h>
#include <multimedia/image_framework/image_mdk.h>
#include <multimedia/image_framework/image_pixel_map_mdk.h>
#include <multimedia/image_framework/image_pixel_map_napi.h>
#include <multimedia/image_framework/image_receiver_mdk.h>
#include <malloc.h>
#include <multimedia/player_framework/native_avbuffer.h>
#include <mutex>
#include <native_buffer/native_buffer.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "ohos_camera"

namespace webrtc {
namespace ohos {
constexpr auto CAMERA = OHOSVideoBuffer::VideoSourceType::CAMERA;

class ImageReceiverOption {
public:
    ImageReceiverOption(uint32_t width, uint32_t height, uint32_t capacity)
        : width_(width), height_(height), capacity_(capacity)
    {
        init();
    };

    int32_t init()
    {
        Image_ErrorCode ret = OH_ImageReceiverOptions_Create(&options_);
        if (ret != IMAGE_SUCCESS) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                         "OH_ImageReceiverOptions_Create failed with code = %{public}d", ret);
            options_ = nullptr;
            return -1;
        }

        ret = OH_ImageReceiverOptions_SetSize(options_, {width_, height_});
        if (ret != IMAGE_SUCCESS) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                         "OH_ImageReceiverOptions_SetSize failed with code = %{public}d", ret);
            release();
            return -1;
        }

        ret = OH_ImageReceiverOptions_SetCapacity(options_, capacity_);
        if (ret != IMAGE_SUCCESS) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                         "OH_ImageReceiverOptions_SetCapacity failed with code = %{public}d", ret);
            release();
            return -1;
        }

        return 0;
    }

    OH_ImageReceiverOptions* GetImageReceiverOptions()
    {
        return options_;
    }

    int32_t release()
    {
        if (options_) {
            Image_ErrorCode ret = OH_ImageReceiverOptions_Release(options_);
            options_ = nullptr;
            if (ret != IMAGE_SUCCESS) {
                OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                             "OH_ImageReceiverOptions_Release failed with code = %{public}d", ret);
                return -1;
            }
        }

        return 0;
    }

    ~ImageReceiverOption()
    {
        release();
    }

private:
    uint32_t width_;
    uint32_t height_;
    uint32_t capacity_;
    OH_ImageReceiverOptions *options_ {nullptr};
};


ImageReceiverOnManager& ImageReceiverOnManager::GetInstance()
{
    static ImageReceiverOnManager manager;
    return manager;
}

int32_t ImageReceiverOnManager::AddImageReceiverOn(uint64_t id, std::shared_ptr<OhosImageReveiveOn> imgRecOn)
{
    MutexLock lock(&mapMutex_);
    receiverOnMap_[id] = imgRecOn;
    return 0;
}

int32_t ImageReceiverOnManager::DelImageReceiverOn(uint64_t id)
{
    MutexLock lock(&mapMutex_);
    receiverOnMap_.erase(id);
    return 0;
}

OhosImageReveiveOn* ImageReceiverOnManager::GetReceiverOnCb(const uint64_t imageReceiverID)
{
    std::map<uint64_t, std::weak_ptr<OhosImageReveiveOn> >::iterator iter = receiverOnMap_.find(imageReceiverID);
    if (iter == receiverOnMap_.end()) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "GetReceiverOnCb not find key: %{public}llu", imageReceiverID);
        return nullptr;
    }
    std::shared_ptr<OhosImageReveiveOn> imgRecOnPtr = iter->second.lock();
    return imgRecOnPtr.get();
}

void ImageReceiverOnManager::ImageReceiverCallback(OH_ImageReceiverNative* receiver)
{
    uint64_t imageReceiverID = OhosImageReveiveOn::GetImageReceiverSurfaceID(receiver);
    if (imageReceiverID == 0) {
        return;
    }

    OhosImageReveiveOn *ohosCamera = ImageReceiverOnManager::GetInstance().GetReceiverOnCb(imageReceiverID);
    if (ohosCamera) {
        ohosCamera->ImageReceiverCallback(receiver);
    }
}

uint64_t OhosImageReveiveOn::GetImageReceiverSurfaceID(OH_ImageReceiverNative *receiver)
{
    uint64_t id;
    Image_ErrorCode ret = OH_ImageReceiverNative_GetReceivingSurfaceId(receiver, &id);
    if (ret != IMAGE_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "OH_ImageReceiverNative_On failed with code = %{public}d",
                     ret);
        return 0;
    }
    return id;
}

uint64_t OhosImageReveiveOn::GetImageReceiverID()
{
    return imageReceiverID_;
}

OH_ImageReceiverNative* OhosImageReveiveOn::GetImageReceiver()
{
    return imageReceiverNative_;
}

int32_t OhosCamera::ImageReceiverCallback(OH_ImageReceiverNative* receiver)
{
    OH_ImageNative *image;
    Image_ErrorCode ret = OH_ImageReceiverNative_ReadLatestImage(receiver, &image);
    if (ret != IMAGE_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_ImageReceiverNative_ReadLatestImage failed with code = %{public}d", ret);
        return -1;
    }

    Image_Size imageSize;
    ret = OH_ImageNative_GetImageSize(image, &imageSize);
    if (ret != IMAGE_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_ImageNative_GetImageSize failed with code = %{public}d", ret);
        OH_ImageNative_Release(image);
        return -1;
    }

    uint32_t* types;
    size_t typeSize;
    ret = OH_ImageNative_GetComponentTypes(image, nullptr, &typeSize);
    if (ret != IMAGE_SUCCESS || typeSize <= 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_ImageNative_GetComponentTypes failed with code = %{public}d", ret);
        OH_ImageNative_Release(image);
        return -1;
    }

    types = new uint32_t[typeSize];
    ret = OH_ImageNative_GetComponentTypes(image, &types, &typeSize);
    if (ret != IMAGE_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_ImageNative_GetComponentTypes failed with code = %{public}d", ret);
        OH_ImageNative_Release(image);
        delete [] types;
        return -1;
    }

    int32_t rowStride;
    ret = OH_ImageNative_GetRowStride(image, types[0], &rowStride);
    if (ret != IMAGE_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_ImageNative_GetRowStride failed with code = %{public}d", ret);
        OH_ImageNative_Release(image);
        delete[] types;
        return -1;
    }

    uint8_t *addr = nullptr;
    OH_NativeBuffer *buffer;
    size_t bufferSize;
    ret = OH_ImageNative_GetByteBuffer(image, types[0], &buffer);
    if (ret != IMAGE_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_ImageNative_GetByteBuffer failed with code = %{public}d", ret);
        OH_ImageNative_Release(image);
        delete[] types;
        return -1;
    }
    ret = OH_ImageNative_GetBufferSize(image, types[0], &bufferSize);
    if (ret != IMAGE_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_ImageNative_GetBufferSize failed with code = %{public}d", ret);
        OH_ImageNative_Release(image);
        delete[] types;
        return -1;
    }

    delete[] types;
    int32_t retInt = OH_NativeBuffer_Map(buffer, (void**)&addr);
    if (retInt != 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_NativeBuffer_Map failed with code = %{public}d", retInt);
        OH_ImageNative_Release(image);
        return -1;
    }

    ImageReceiverOn(addr, imageSize.width, imageSize.height, rowStride, bufferSize);

    ret = OH_ImageNative_Release(image);
    if (ret != IMAGE_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_ImageNative_Release  failed with code = %{public}d", ret);
        return -1;
    }

    return 0;
}

OhosCamera::OhosCamera(CaptureType captureType)
{
    captureType_ = captureType;
    InitCamera();
}

int32_t OhosImageReveiveOn::InitImageReceiver(int32_t width, int32_t height)
{
    int32_t returnCode = -1;
    ImageReceiverOption imageReceiverOption(640, 480, 8);
    Image_ErrorCode ret = OH_ImageReceiverNative_Create(imageReceiverOption.GetImageReceiverOptions(), &imageReceiverNative_);
    if (ret != IMAGE_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "OH_ImageReceiverNative_Create failed with code = %{public}d", ret);
        return returnCode;
    }

    ret = OH_ImageReceiverNative_On(imageReceiverNative_, ImageReceiverOnManager::ImageReceiverCallback);
    if (ret != IMAGE_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "OH_ImageReceiverNative_On failed with code = %{public}d", ret);
        return returnCode;
    }

    imageReceiverID_ = GetImageReceiverSurfaceID(imageReceiverNative_);
    if (imageReceiverID_ == 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "GetImageReceiverSurfaceID failed");
        return returnCode;
    }

    returnCode = 0;
    return returnCode;
}

int32_t OhosCamera::InitCamera(int32_t width, int32_t height)
{
    int32_t return_code = -1;
    width_ = width;
    height_ = height;
    InitImageReceiver(width_, height_);
    Camera_ErrorCode ret = OH_Camera_GetCameraManager(&camera_manager_);
    if (camera_manager_ == nullptr || ret != CAMERA_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_Camera_GetCameraManager failed, ret = %{public}d", ret);
        return return_code;
    }

    ret = OH_CameraManager_GetSupportedCameras(camera_manager_, &cameras_, &cameras_size_);
    if (cameras_ == nullptr || ret != CAMERA_OK || cameras_size_ <= 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_CameraManager_GetSupportedCameras failed, ret = %{public}d", ret);
        return return_code;
    }

    return_code = 0;
    return return_code;
}

int32_t OhosCamera::CameraInputCreateAndOpen()
{
    Camera_ErrorCode ret;
    ret = OH_CameraManager_CreateCameraInput(camera_manager_, &cameras_[camera_dev_index_], &camera_input_);
    if (camera_input_ == nullptr || ret != CAMERA_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_CameraManager_CreateCaptureSession failed, ret = %{public}d", ret);
        return -1;
    }

    ret = OH_CameraInput_Open(camera_input_);
    if (ret != CAMERA_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "OH_CameraInput_Open failed, ret = %{public}d", ret);
        return -1;
    }

    return 0;
}

int32_t OhosCamera::CameraInputRelease()
{
    Camera_ErrorCode ret;
    if (camera_input_ != nullptr) {
        ret = OH_CameraInput_Close(camera_input_);
        if (ret != CAMERA_OK) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "OH_CameraInput_Close failed, ret = %{public}d", ret);
        }
        ret = OH_CameraInput_Release(camera_input_);
        if (ret != CAMERA_OK) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "OH_CameraInput_Release failed, ret = %{public}d", ret);
        }
    }

    camera_input_ = nullptr;
    return 0;
}

int32_t OhosCamera::PreviewOutputCreate()
{
    Camera_ErrorCode ret;
    ret = OH_CameraManager_GetSupportedCameraOutputCapability(camera_manager_, &cameras_[camera_dev_index_],
                                                              &camera_output_capability_);
    if (camera_output_capability_ == nullptr || ret != CAMERA_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_CameraManager_GetSupportedCameraOutputCapability failed, ret = %{public}d", ret);
        return -1;
    }

    if (camera_output_capability_->metadataProfilesSize <= 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "metadataProfilesSize <= 0");
        return -1;
    }

    for (int i = 0; i < camera_output_capability_->previewProfilesSize; i++) {
        preview_profile_ = camera_output_capability_->previewProfiles[i];
        if (preview_profile_->size.width == width_ && preview_profile_->size.height == height_ ) {
            profile_index_ = i;
        }
    }

    preview_profile_ = camera_output_capability_->previewProfiles[profile_index_];
    int32_t IDLength = 24;
    char surfaceID[IDLength];

    if (captureType_ == CaptureType::SURFACE) {
        if (InitRenderContext() == false) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosCamera", "InitRenderContext failed");
            return -1;
        }

        if (CreateNativeImage() == false) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosCamera", "CreateNativeImage failed");
            return -1;
        }

        if (GetSurfaceID() != 0) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosCamera", "GetSurfaceID failed");
            return -1;
        }

        snprintf(surfaceID, IDLength, "%llu", surfaceId_);
        ret = OH_CameraManager_CreatePreviewOutput(camera_manager_, preview_profile_, surfaceID, &preview_output_);
    } else {
        snprintf(surfaceID, IDLength, "%llu", GetImageReceiverID());
        ret = OH_CameraManager_CreatePreviewOutput(camera_manager_, preview_profile_, surfaceID, &preview_output_);
    }
    if (preview_output_ == nullptr || ret != CAMERA_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_CameraManager_CreatePreviewOutput failed, ret = %{public}d", ret);
        return -1;
    }

    return 0;
}

int32_t OhosCamera::PreviewOutputRelease()
{
    Camera_ErrorCode ret;
    if (preview_output_ != nullptr) {
        ret = OH_PreviewOutput_Release(preview_output_);
        if (ret != CAMERA_OK) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                         "OH_PreviewOutput_Release failed, ret = %{public}d", ret);
        }
    }

    preview_output_ = nullptr;
    return 0;
}

int32_t OhosCamera::CaptureSessionSetting()
{
    Camera_ErrorCode ret;
    ret = OH_CameraManager_CreateCaptureSession(camera_manager_, &capture_session_);
    if (capture_session_ == nullptr || ret != CAMERA_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_CameraManager_CreateCaptureSession failed, ret = %{public}d", ret);
        return -1;
    }

    ret = OH_CaptureSession_BeginConfig(capture_session_);
    if (ret != CAMERA_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_CaptureSession_BeginConfig failed, ret = %{public}d", ret);
        return -1;
    }

    if (camera_input_) {
        ret = OH_CaptureSession_AddInput(capture_session_, camera_input_);
        if (ret != CAMERA_OK) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                         "OH_CaptureSession_AddInput failed, ret = %{public}d", ret);
            return -1;
        }
    }

    if (preview_output_) {
        ret = OH_CaptureSession_AddPreviewOutput(capture_session_, preview_output_);
        if (ret != CAMERA_OK) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                         "OH_CaptureSession_AddPreviewOutput failed, ret = %{public}d", ret);
            return -1;
        }
    }

    ret = OH_CaptureSession_CommitConfig(capture_session_);
    if (ret != CAMERA_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                    "OH_CaptureSession_CommitConfig failed, ret = %{public}d", ret);
        return -1;
    }
    return 0;
}

int32_t OhosCamera::CaptureSessionUnsetting()
{
    Camera_ErrorCode ret;
    if (capture_session_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                    "CaptureSessionUnsetting failed, capture_session_ is nullptr");
        return -1;
    }

    ret = OH_CaptureSession_Release(capture_session_);
    capture_session_ = nullptr;
    if (ret != CAMERA_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                    "OH_CaptureSession_Release failed, ret = %{public}d", ret);
        return -1;
    }

    return 0;
}

int32_t OhosCamera::StartCamera()
{
    isRunning_.store(true);
    if (is_camera_started_) {
        // 先关闭相机、会话、预览流，然后再重新打开
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "StartCamera: Camera has been started");
        StopCamera();
        CaptureSessionUnsetting();
        CameraInputRelease();
        PreviewOutputRelease();
    }
    Camera_ErrorCode ret = CAMERA_OK;

    if (camera_input_ == nullptr) {
        CameraInputCreateAndOpen();
    }
    if (preview_output_ == nullptr) {
        PreviewOutputCreate();
    }
    if (capture_session_ == nullptr) {
        CaptureSessionSetting();
    }
    ret = OH_CaptureSession_Start(capture_session_);
    if (ret != CAMERA_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "OH_CaptureSession_Start failed, ret = %{public}d", ret);
        return -1;
    }

    is_camera_started_ = true;
    return 0;
}

int32_t OhosCamera::StopCamera()
{
    Camera_ErrorCode ret = CAMERA_OK;
    isRunning_.store(false);

    if (is_camera_started_ == false) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "StopCamera: Camera not started");
        return 0;
    }

    if (capture_session_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "StopCamera: capture_session_ is nullptr");
        return -1;
    }

    ret = OH_CaptureSession_Stop(capture_session_);
    if (ret != CAMERA_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "OH_CaptureSession_Stop failed, ret = %{public}d", ret);
        return -1;
    }

    is_camera_started_ = false;
    return 0;
}

int32_t OhosImageReveiveOn::ImageReceiverRelease()
{
    if (imageReceiverNative_ == nullptr) {
        return 0;
    }
    Image_ErrorCode ret = OH_ImageReceiverNative_Off(imageReceiverNative_);
    if (ret != IMAGE_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "OH_ImageReceiverNative_Off failed, ret = %{public}d", ret);
    }

    ret = OH_ImageReceiverNative_Release(imageReceiverNative_);
    if (ret != IMAGE_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                     "OH_ImageReceiverNative_Release failed, ret = %{public}d",ret);
    }

    return 0;
}
int32_t OhosCamera::DeleteCameraOutputCapability()
{
    if (camera_output_capability_ != nullptr) {
        // TODO
        // Camera_ErrorCode ret = OH_CameraManager_DeleteSupportedCameraOutputCapability(camera_manager_,
        // camera_output_capability_); if (ret != CAMERA_OK) {
        //   OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,"OH_CameraManager_DeleteSupportedCameraOutputCapability\
        //   failed, ret = %{public}d", ret);
        // }
        preview_profile_ = nullptr;
        camera_output_capability_ = nullptr;
    }

    return 0;
}

int32_t OhosCamera::DeleteCameras()
{
    if (cameras_ != nullptr) {
        Camera_ErrorCode ret = OH_CameraManager_DeleteSupportedCameras(camera_manager_, cameras_, cameras_size_);
        if (ret != CAMERA_OK) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                        "OH_CameraManager_DeleteSupportedCameras failed, ret = %{public}d", ret);
        }
        cameras_ = nullptr;
    }

    return 0;
}

int32_t OhosCamera::DeleteCameraManage()
{
    if (camera_manager_ != nullptr) {
        Camera_ErrorCode ret = OH_Camera_DeleteCameraManager(camera_manager_);
        if (ret != CAMERA_OK) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG,
                "OH_Camera_DeleteCameraManager failed, ret =%{public}d", ret);
        }
        camera_manager_ = nullptr;
    }

    return 0;
}

int32_t OhosCamera::CameraRelease()
{
    if (is_camera_started_) {
        StopCamera();
    }
    CaptureSessionUnsetting();
    CameraInputRelease();
    PreviewOutputRelease();
    ImageReceiverRelease();

    if (captureType_ == CaptureType::SURFACE) {
        DestroyNativeImage();
        DestroyRenderContext();
    }

    DeleteCameraOutputCapability();
    DeleteCameras();
    DeleteCameraManage();
    return 0;
}

uint32_t OhosCamera::GetCameraIndex()
{
    return camera_dev_index_;
}

int32_t OhosCamera::SetCameraIndex(uint32_t camera_index)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, LOG_TAG, "camera_dev_index_ = %{public}d", camera_dev_index_);
    if (camera_index >= 0 && camera_index <= cameras_size_) {
        camera_dev_index_ = camera_index;
    }
    return 0;
}

bool OhosCamera::ImageReceiverOn(uint8_t *buffer, int32_t width, int32_t height, int32_t stride, size_t bufferSize)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, LOG_TAG, "imageReceiverOn started");
    height = (height > 0) ? height : -height; // abs
    width = stride;
    size_t size = stride * height + stride * height / 2;
    if (bufferSize < size) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, "imageReceiverOn failed");
        return false;
    }

    rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer = webrtc::I420Buffer::Create(width, height);
    libyuv::NV21ToI420(buffer, width, buffer + width * height, width, i420_buffer.get()->MutableDataY(),
                       i420_buffer.get()->StrideY(), i420_buffer.get()->MutableDataU(), i420_buffer.get()->StrideU(),
                       i420_buffer.get()->MutableDataV(), i420_buffer.get()->StrideV(), width, height);

    webrtc::VideoFrame video_frame = webrtc::VideoFrame::Builder()
                                         .set_video_frame_buffer(i420_buffer)
                                         .set_timestamp_rtp(0)
                                         .set_timestamp_ms(rtc::TimeMillis())
                                         .set_rotation(webrtc::kVideoRotation_90)
                                         .build();
    if (data_callback_) {
        data_callback_->OnFrame(video_frame);
    }
    return true;
}

void OhosCamera::RegisterCaptureDataCallback(rtc::VideoSinkInterface<webrtc::VideoFrame> *data_callback)
{
    data_callback_ = data_callback;
}

void OhosCamera::UnregisterCaptureDataCallback()
{
    data_callback_ = nullptr;
}

void OhosCamera::OnNativeImageFrameAvailable(void *data)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "OnNativeImageFrameAvailable called");
    OhosCamera *camera = static_cast<OhosCamera*>(data);
    camera->FrameAvailable();
}

int32_t OhosCamera::FrameAvailable()
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "makeing fuction %{public}s ", GetEglErrorString());
    if (isRunning_.load() == false) {
        return -1;
    }

    int32_t ret;
    auto contextLock = OhosEGLContextManage::GetInstance().GetEGLContextResource(CAMERA);
    renderContext_->MakeCurrent(eglSurface_);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "FrameAvailable MakeCurrent %{public}s ",
                 GetEglErrorString());
    {
        std::lock_guard<std::mutex> lock(contextLock->textureIDMutex_);
        ret = OH_NativeImage_UpdateSurfaceImage(nativeImage_);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "FrameAvailable ImageTextureID = %{public}d ",
                     nativeImageTextureID_);
    }
    renderContext_->MakeCurrent();
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "FrameAvailable MakeEmptyCurrent %{public}s ",
                 GetEglErrorString());
    if (ret != 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosCamera",
                     "OH_NativeImage_UpdateSurfaceImage failed, ret: %{public}d", ret);
        return -1;
    }

    // set textrueBuffer_
    OHOSVideoBuffer::TextureBuffer textureBuffer;
    textureBuffer.textureID = nativeImageTextureID_;
    textureBuffer.type = OHOSVideoBuffer::TextureBuffer::Type::OES;
    ret = OH_NativeImage_GetTransformMatrix(nativeImage_, textureBuffer.matrix);    

    if (ret != 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosCamera",
                     "OH_NativeImage_GetTransformMatrix failed, ret is %{public}d.", ret);
        return -1;
    }
    rtc::scoped_refptr<OHOSVideoBuffer> videoBuffer = OHOSVideoBuffer::Creat(width_, height_, textureBuffer, CAMERA);
    webrtc::VideoFrame videoFrame = webrtc::VideoFrame::Builder()
                                        .set_video_frame_buffer(videoBuffer)
                                        .set_timestamp_rtp(0)
                                        .set_timestamp_ms(rtc::TimeMillis())
                                        .set_rotation(webrtc::kVideoRotation_90)
                                        .build();
    if (data_callback_) {
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "datacallback in %{public}d ", videoFrame.size());
        data_callback_->OnFrame(videoFrame);
    } else {
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_DOMAIN, "OhosCamera", "FrameAvailable  data_callback_ nullptr");
    }
    return 0;
}

bool OhosCamera::InitRenderContext()
{
    renderContext_ = std::make_unique<EglRenderContext>();
    if (renderContext_->Init() == false) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosCamera", "EglRenderContext Init() failed");
        return false;
    }
    OhosEGLContextManage::GetInstance().AddEGLContext(CAMERA, renderContext_->GetEGLContext());
    EGLint pBufferAttributes[] = {EGL_WIDTH, width_, EGL_HEIGHT, height_, EGL_NONE};
    eglSurface_ = renderContext_->CreatePBufferEglSurface(pBufferAttributes);
    if (eglSurface_ == EGL_NO_SURFACE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosCamera", "CreateEglSurface %{public}s ", GetEglErrorString());
        return false;
    }
    return true;
}

int32_t OhosCamera::DestroyRenderContext()
{
    if (eglSurface_ != EGL_NO_SURFACE && renderContext_ != nullptr) {
        renderContext_->DestroyEglSurface(eglSurface_);
        eglSurface_ = EGL_NO_SURFACE;
    }
    if (renderContext_ != nullptr) {
        OhosEGLContextManage::GetInstance().DelEGLContext(CAMERA);
        renderContext_.reset();
        renderContext_ = nullptr;
    }
    return 0;
}

bool OhosCamera::CreateNativeImage()
{
    renderContext_->MakeCurrent(eglSurface_);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "CreateNativeImage MakeCurrent %{public}s ",
                 GetEglErrorString());

    glGenTextures(1, &nativeImageTextureID_);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "textureID = %{public}u", nativeImageTextureID_);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "glGenTextures %{public}s ", GetGLErrorString());
    {
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, nativeImageTextureID_);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "glBindTexture %{public}s ", GetGLErrorString());

        // set the texture wrapping parameters
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_REPEAT);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "glTexParameteri %{public}s ", GetGLErrorString());
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_REPEAT);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "glTexParameteri %{public}s ", GetGLErrorString());

        // set texture filtering parameters
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "glTexParameteri %{public}s ", GetGLErrorString());
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "glTexParameteri %{public}s ", GetGLErrorString());

        nativeImage_ = OH_NativeImage_Create(nativeImageTextureID_, GL_TEXTURE_EXTERNAL_OES);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "OH_NativeImage_Create %{public}s ",
                     GetGLErrorString());

        glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "glBindTexture %{public}s ", GetGLErrorString());
    }

    renderContext_->MakeCurrent();
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "CreateNativeImage MakeEmptyCurrent %{public}s ",
                 GetEglErrorString());

    if (nativeImage_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosCamera", "OH_NativeImage_Create failed.");
        return false;
    }

    nativeImageFrameAvailableListener_.context = this;
    nativeImageFrameAvailableListener_.onFrameAvailable = &OhosCamera::OnNativeImageFrameAvailable;
    int32_t ret = OH_NativeImage_SetOnFrameAvailableListener(nativeImage_, nativeImageFrameAvailableListener_);
    if (ret != 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosCamera",
                     "OH_NativeImage_SetOnFrameAvailableListener failed, ret is %{public}d.", ret);
        return false;
    }

    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "CreateNativeImage end");
    return true;
}

int32_t OhosCamera::DestroyNativeImage()
{
    if (nativeImage_ != nullptr) {
        (void)OH_NativeImage_UnsetOnFrameAvailableListener(nativeImage_);
        OH_NativeImage_Destroy(&nativeImage_);
        nativeImage_ = nullptr;
    }
    if (nativeImageTextureID_ != 0U && renderContext_ != nullptr) {
        auto contextLock = OhosEGLContextManage::GetInstance().GetEGLContextResource(CAMERA);
        renderContext_->MakeCurrent(eglSurface_);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "DestroyNativeImage MakeCurrent %{public}s ",
                     GetEglErrorString());
        {
            std::lock_guard<std::mutex> lock(contextLock->textureIDMutex_);
            glDeleteTextures(1, &nativeImageTextureID_);
            nativeImageTextureID_ = 0U;
        }

        renderContext_->MakeCurrent();
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "DestroyNativeImage MakeEmptyCurrent %{public}s ",
                     GetEglErrorString());
    }

    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosCamera", "DestroyNativeImage end");
    return 0;
}

int32_t OhosCamera::GetSurfaceID()
{
    int32_t ret = OH_NativeImage_GetSurfaceId(nativeImage_, &surfaceId_);
    return ret;
}

OhosCamera::~OhosCamera()
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, LOG_TAG, "OhosCamera::~OhosCamera start");
    CameraRelease();

    camera_dev_index_ = 0;
    profile_index_ = 0;
}
} // namespace ohos
} // namespace webrtc
