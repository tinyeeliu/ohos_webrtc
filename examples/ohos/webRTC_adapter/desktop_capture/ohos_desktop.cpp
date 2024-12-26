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
#include "ohos_desktop.h"
#include <thread>
#include <multimedia/player_framework/native_avscreen_capture_errors.h>
#include <native_buffer/native_buffer.h>
#include "hilog/log.h"
#include "api/video/i420_buffer.h"
#include "libyuv/convert.h"
#include "rtc_base/time_utils.h"
#include "surface_helper/ohos_eglContext_manage.h"
#include "commom/ohos_video_buffer.h"
#include "surface_helper/ohos_gl_drawer.h"

namespace webrtc {
namespace ohos {

constexpr auto DESKTOP = OHOSVideoBuffer::VideoSourceType::DESKTOP;

void OhosDesktop::OnError(OH_AVScreenCapture *capture, int32_t errorCode, void *userData)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "errorCode = %{public}d", errorCode);
    (void)capture;
    (void)errorCode;
    (void)userData;
}

void OhosDesktop::OnStateChange(struct OH_AVScreenCapture *capture, OH_AVScreenCaptureStateCode stateCode,
        void *userData)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "stateCode = %{public}d", stateCode);
    OhosDesktop *desktop = static_cast<OhosDesktop *>(userData);
    desktop->SetStateCode(stateCode);
}

void OhosDesktop::OnBufferAvailable(OH_AVScreenCapture *capture, OH_AVBuffer *buffer,
        OH_AVScreenCaptureBufferType bufferType, int64_t timestamp, void *userData)
{
    OhosDesktop *desktop = static_cast<OhosDesktop *>(userData);
    if (bufferType == OH_SCREEN_CAPTURE_BUFFERTYPE_VIDEO) {
        if (!desktop->isRunning_.load()) {
            return;
        }

        std::lock_guard<std::mutex> lock(desktop->mutex_);
        if (desktop->desktopAVBufferQueue_.size() >= desktop->maxQueueSize_) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_DOMAIN, "OhosDesktop", "desktopAVBufferQueue_ full");
            return;
        }

        // 处理视频buffer
        uint8_t* receiverAddr = OH_AVBuffer_GetAddr(buffer);
        if (receiverAddr == nullptr) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "OH_AVBuffer_GetAddr failed");
            return;
        }
        int32_t bufferLength = OH_AVBuffer_GetCapacity(buffer);
        OH_NativeBuffer *nativeBuffer = OH_AVBuffer_GetNativeBuffer(buffer);
        if (nativeBuffer == nullptr) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "OH_AVBuffer_GetNativeBuffer failed");
            return;
        }

        OH_NativeBuffer_Config config;
        OH_NativeBuffer_GetConfig(nativeBuffer, &config);

        uint8_t* data_buffer = new uint8_t[bufferLength];
        memcpy(data_buffer, receiverAddr, bufferLength);
        OH_NativeBuffer_Unreference(nativeBuffer);

        // 将数据写入队列
        desktop->desktopAVBufferQueue_.push({data_buffer, bufferLength, config.stride, config.width,
                                             config.height, timestamp});
        desktop->cv_.notify_all();
    } else if (bufferType == OH_SCREEN_CAPTURE_BUFFERTYPE_AUDIO_INNER) {
        // 处理内录buffer
        return;
    } else if (bufferType == OH_SCREEN_CAPTURE_BUFFERTYPE_AUDIO_MIC) {
        // 处理麦克风buffer
        return;
    }
}

int32_t OhosDesktop::Create(CaptureType captureType, int32_t queueSize)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "create and set callback");
    // 只初始化一次
    if (capture_ != nullptr) {
        return 0;
    }
    capture_ = OH_AVScreenCapture_Create();
    captureType_ = captureType;
    maxQueueSize_ = queueSize > 0 ? queueSize : 1;
    // 设置回调
    OH_AVScreenCapture_SetErrorCallback(capture_, OnError, this);
    OH_AVScreenCapture_SetStateCallback(capture_, OnStateChange, this);
    OH_AVScreenCapture_SetDataCallback(capture_, OnBufferAvailable, this);
    return 0;
}

int32_t OhosDesktop::Config(int32_t width, int32_t height)
{
    width_ = width;
    height_ = height;
    OH_AudioCaptureInfo micCaptureInfo = {.audioSampleRate = 0, .audioChannels = 0, .audioSource = OH_MIC};
    OH_VideoCaptureInfo videoCaptureInfo = {
        .videoFrameWidth = width, .videoFrameHeight = height, .videoSource = OH_VIDEO_SOURCE_SURFACE_RGBA};

    OH_AudioInfo audioInfo = {
        .micCapInfo = micCaptureInfo,
    };
    OH_VideoEncInfo videoEncInfo = {.videoCodec = OH_H264, .videoBitrate = 20000, .videoFrameRate = 30};
    OH_VideoInfo videoInfo = {.videoCapInfo = videoCaptureInfo, .videoEncInfo = videoEncInfo};

    OH_AVScreenCaptureConfig config = {.captureMode = OH_CAPTURE_HOME_SCREEN,
                                       .dataType = OH_ORIGINAL_STREAM,
                                       .audioInfo = audioInfo,
                                       .videoInfo = videoInfo};

    if (capture_ == nullptr) {
        Create();
    }
    int32_t ret = OH_AVScreenCapture_Init(capture_, config);
    if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "OH_AVScreenCapture_Init failed");
        return -1;
    }
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "Config end");
    isConfig_ = true;
    return 0;
}

int32_t OhosDesktop::Start()
{
    if (isStarted_) {
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_DOMAIN, "OhosDesktop", "capture is stared");
        return 0;
    }

    if (!isConfig_) {
        Create();
        Config();
    }

    if (captureType_ == CaptureType::SURFACE) {
        if (InitRenderContext() == false) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "InitRenderContext failed");
            return -1;
        }

        if (CreateNativeImage() == false) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "CreateNativeImage failed");
            return -1;
        }

        if (CreateNativeWindow() == false) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "CreateNativeWindow failed");
            return -1;
        }

        isRunning_.store(true);
        int32_t ret = OH_AVScreenCapture_StartScreenCaptureWithSurface(capture_, nativeWindow_);
        if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop",
                         "OH_AVScreenCapture_StartScreenCaptureWithSurface failed");
            Release();
            return -1;
        }
    } else {
        isRunning_.store(true);
        // 设置启动线程
        bufferAvailableThread_ = std::make_unique<std::thread>(&OhosDesktop::DesktopBufferAvailable, this);

        int32_t ret = OH_AVScreenCapture_StartScreenCapture(capture_);
        if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "OH_AVScreenCapture_StartScreenCapture failed");
            Release();
            return -1;
        }
    }

    return 0;
}

int32_t OhosDesktop::Stop()
{
    if (isStarted_) {
        int32_t ret = OH_AVScreenCapture_StopScreenCapture(capture_);
        if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "OH_AVScreenCapture_StopScreenCapture failed");
            Release();
            return -1;
        }
    }

    isRunning_.store(false);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "capture stoped");
    isStarted_ = false;

    return 0;
}

void OhosDesktop::RegisterCaptureDataCallback(rtc::VideoSinkInterface<webrtc::VideoFrame> *dataCallback)
{
    dataCallback_ = dataCallback;
}

void OhosDesktop::UnregisterCaptureDataCallback()
{
    dataCallback_ = nullptr;
}

void OhosDesktop::DesktopBufferOn(uint8_t *buffer, int32_t bufferLength, int32_t stride, int32_t width, int32_t height, int64_t timestamp)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "DesktopBufferOn started");
    // 转换buffer为I420格式
    height = (height > 0) ? height : -height; // abs

    rtc::scoped_refptr<webrtc::I420Buffer> i420Buffer = webrtc::I420Buffer::Create(width, height);
    // 设置时为RGBA，使用RGBA转换颜色不对，使用ABGR转换是正常的。（可能是大小端序的原因）
    int32_t ret = libyuv::ABGRToI420(buffer, stride, i420Buffer.get()->MutableDataY(),
                     i420Buffer.get()->StrideY(), i420Buffer.get()->MutableDataU(), i420Buffer.get()->StrideU(),
                     i420Buffer.get()->MutableDataV(), i420Buffer.get()->StrideV(), width, height);
    if (ret != 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "libyuv::ABGRToI420 failed");
        return;
    }

    webrtc::VideoFrame videoFrame = webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(i420Buffer)
                                       .set_timestamp_rtp(0)
                                       .set_timestamp_ms(rtc::TimeMillis())
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build();
    if (dataCallback_) {
        dataCallback_->OnFrame(videoFrame);
    }
}

void OhosDesktop::DesktopBufferAvailable()
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "DesktopBufferAvailable enter");
    do {
        BufferData data;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {return !desktopAVBufferQueue_.empty() ||  !isRunning_.load();});
            if (!isRunning_.load()) {
                // 清空队列，执行结束
                while (!desktopAVBufferQueue_.empty()) {
                    delete[]  desktopAVBufferQueue_.front().buffer;
                    desktopAVBufferQueue_.pop();
                }
                OH_LOG_Print(LOG_APP, LOG_WARN, LOG_DOMAIN, "OhosDesktop", "DesktopBufferAvailable A return");
                return;
            }
            // 取出数据
            data = desktopAVBufferQueue_.front();
            desktopAVBufferQueue_.pop();
        }

        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop",
                     "stride = %{public}d, width = %{public}d, height = %{public}d", data.stride, data.width,
                     data.height);
        DesktopBufferOn(data.buffer, data.length, data.stride, data.width, data.height, data.timestamp);
        delete[] data.buffer;
    } while (isRunning_.load());
}

void OhosDesktop::SetStateCode(OH_AVScreenCaptureStateCode stateCode)
{
    stateCode_ = stateCode;
    switch (stateCode_) {
        case OH_SCREEN_CAPTURE_STATE_STARTED:
            isStarted_ = true;
            break;
        case OH_SCREEN_CAPTURE_STATE_STOPPED_BY_USER:
            isStarted_ = false;
            break;
        case OH_SCREEN_CAPTURE_STATE_CANCELED:
            isStarted_ = false;
            break;
        default:
            break;
    }
}

void OhosDesktop::OnNativeImageFrameAvailable(void *data)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "OnNativeImageFrameAvailable called");
    OhosDesktop* desktop = static_cast<OhosDesktop*>(data);
    desktop->FrameAvailable();
}

int32_t OhosDesktop::FrameAvailable()
{
    if (isRunning_.load() == false) {
        return -1;
    }

    int32_t ret;
    auto contextResource = OhosEGLContextManage::GetInstance().GetEGLContextResource(DESKTOP);
    renderContext_->MakeCurrent(eglSurface_);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "FrameAvailable MakeCurrent %{public}s ",
                     GetEglErrorString());
    {
        std::lock_guard<std::mutex> lock(contextResource->textureIDMutex_);
        ret = OH_NativeImage_UpdateSurfaceImage(nativeImage_);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "FrameAvailable ImageTextureID = %{public}d ", nativeImageTextureID_);
    }

    renderContext_->MakeCurrent();
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "FrameAvailable MakeEmptyCurrent %{public}s ",
                      GetEglErrorString());

    if (ret != 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop",
                     "OH_NativeImage_UpdateSurfaceImage failed, ret: %{public}d", ret);
        return -1;
    }

    // set textrueBuffer_
    OHOSVideoBuffer::TextureBuffer textureBuffer;
    textureBuffer.textureID = nativeImageTextureID_;
    textureBuffer.type = OHOSVideoBuffer::TextureBuffer::Type::OES;
    ret = OH_NativeImage_GetTransformMatrix(nativeImage_, textureBuffer.matrix);    
    if (ret != 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop",
                     "OH_NativeImage_GetTransformMatrix failed, ret is %{public}d.", ret);
        return false;
    }

    // create video frame
    rtc::scoped_refptr<OHOSVideoBuffer> videoBuffer = OHOSVideoBuffer::Creat(width_, height_, textureBuffer, DESKTOP);
    webrtc::VideoFrame videoFrame = webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(videoBuffer)
                                       .set_timestamp_rtp(0)
                                       .set_timestamp_ms(rtc::TimeMillis())
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build();

    if (dataCallback_) {
        dataCallback_->OnFrame(videoFrame);
    }
    return 0;
}

bool OhosDesktop::InitRenderContext()
{
    renderContext_ = std::make_unique<EglRenderContext>();
    if (renderContext_->Init() == false) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "EglRenderContext Init() failed");
        return false;
    }

    OhosEGLContextManage::GetInstance().AddEGLContext(DESKTOP, renderContext_->GetEGLContext());

    // 创建eglSurface_
    EGLint pBufferAttributes[] = {EGL_WIDTH, width_, EGL_HEIGHT, height_, EGL_NONE};
    eglSurface_ = renderContext_->CreatePBufferEglSurface(pBufferAttributes);
    if (eglSurface_ == EGL_NO_SURFACE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "CreateEglSurface %{public}s ", GetEglErrorString());
        return false;
    }

    return true;
}

int32_t OhosDesktop::DestroyRenderContext()
{
    if (eglSurface_ != EGL_NO_SURFACE && renderContext_ != nullptr) {
        renderContext_->DestroyEglSurface(eglSurface_);
        eglSurface_ = EGL_NO_SURFACE;
    }

    if (renderContext_ != nullptr) {
        OhosEGLContextManage::GetInstance().DelEGLContext(DESKTOP);
        renderContext_.reset();
        renderContext_ = nullptr;
    }

    return 0;
}

bool OhosDesktop::CreateNativeWindow()
{
    nativeWindow_ = OH_NativeImage_AcquireNativeWindow(nativeImage_);
    if (nativeWindow_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "OH_NativeImage_AcquireNativeWindow failed");
        return false;
    }

    int32_t ret = OH_NativeWindow_NativeWindowHandleOpt(nativeWindow_, SET_BUFFER_GEOMETRY, width_, height_);
    if (ret != 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "OH_NativeWindow_NativeWindowHandleOpt failed");
        return false;
    }

    return true;
}

int32_t OhosDesktop::DestroyNativeWindow()
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "DestroyNativeWindow started");
    if (nativeWindow_ != nullptr) {
        OH_NativeWindow_DestroyNativeWindow(nativeWindow_);
        nativeWindow_ = nullptr;
    }
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "DestroyNativeWindow end");
    return 0;
}

bool OhosDesktop::CreateNativeImage()
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "CreateNativeImage started");
    renderContext_->MakeCurrent(eglSurface_);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "CreateNativeImage MakeCurrent %{public}s ",
                 GetEglErrorString());

    glGenTextures(1, &nativeImageTextureID_);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "glGenTextures %{public}s ", GetGLErrorString());
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "textureID = %{public}u", nativeImageTextureID_);

    {
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, nativeImageTextureID_);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "glBindTexture %{public}s ", GetGLErrorString());

        // set the texture wrapping parameters
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "glTexParameteri %{public}s ", GetGLErrorString());
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "glTexParameteri %{public}s ", GetGLErrorString());

        // set texture filtering parameters
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "glTexParameteri %{public}s ", GetGLErrorString());
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "glTexParameteri %{public}s ", GetGLErrorString());

        nativeImage_ = OH_NativeImage_Create(nativeImageTextureID_, GL_TEXTURE_EXTERNAL_OES);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "OH_NativeImage_Create %{public}s ",
                      GetGLErrorString());

        glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "glBindTexture %{public}s ",
                      GetGLErrorString());
    }

    renderContext_->MakeCurrent();
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "CreateNativeImage MakeEmptyCurrent %{public}s ",
                  GetEglErrorString());

    if (nativeImage_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "OH_NativeImage_Create failed.");
        return false;
    }

    nativeImageFrameAvailableListener_.context = this;
    nativeImageFrameAvailableListener_.onFrameAvailable = &OhosDesktop::OnNativeImageFrameAvailable;
    int32_t ret = OH_NativeImage_SetOnFrameAvailableListener(nativeImage_, nativeImageFrameAvailableListener_);
    if (ret != 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop",
                     "OH_NativeImage_SetOnFrameAvailableListener failed, ret is %{public}d.", ret);
        return false;
    }

    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "CreateNativeImage end");
    return true;
}

int32_t OhosDesktop::DestroyNativeImage()
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "DestroyNativeImage start");

    if (nativeImage_ != nullptr) {
        (void)OH_NativeImage_UnsetOnFrameAvailableListener(nativeImage_);
        OH_NativeImage_Destroy(&nativeImage_);
        nativeImage_ = nullptr;
    }

    if (nativeImageTextureID_ != 0U && renderContext_ != nullptr) {
        auto contextResource = OhosEGLContextManage::GetInstance().GetEGLContextResource(DESKTOP);
        renderContext_->MakeCurrent(eglSurface_);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "DestroyNativeImage MakeCurrent %{public}s ",
                     GetEglErrorString());

        {
            std::lock_guard<std::mutex> lock(contextResource->textureIDMutex_);
            glDeleteTextures(1, &nativeImageTextureID_);
            nativeImageTextureID_ = 0U;
        }

        renderContext_->MakeCurrent();
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "DestroyNativeImage MakeEmptyCurrent %{public}s ",
                     GetEglErrorString());
    }

    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "DestroyNativeImage end");
    return 0;
}


int32_t OhosDesktop::Release()
{
    OH_AVScreenCapture_Release(capture_);
    capture_ = nullptr;
    isConfig_ = false;
    isStarted_ = false;
    nativeImageFrameAvailableListener_ = {nullptr, nullptr};

    isRunning_.store(false);
    cv_.notify_all();

    if (bufferAvailableThread_) {
        if (bufferAvailableThread_->joinable()) {
            bufferAvailableThread_->join();
        }
        bufferAvailableThread_ = nullptr;
    }

    // Destroy NativeWindow NativeImage RenderContext
    if (captureType_ == CaptureType::SURFACE) {
        DestroyNativeWindow();
        DestroyNativeImage();
        DestroyRenderContext();
    }
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "capture release");
    return 0;
}

OhosDesktop::~OhosDesktop()
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosDesktop", "capture release");
    Stop();
    Release();
}

} // namespace ohos
} // namespace webrtc