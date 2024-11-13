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

#include "desktop_capturer.h"
#include "../video/video_frame_receiver_native.h"
#include "../helper/native_window.h"
#include "../utils/marcos.h"

#include <cstdint>

namespace webrtc {

OH_VideoSourceType PixelFormatToVideoSourceType(video::PixelFormat format)
{
    switch (format) {
        case video::PixelFormat::RGBA:
            return OH_VIDEO_SOURCE_SURFACE_RGBA;
        default:
            return OH_VIDEO_SOURCE_BUTT;
    }
}

std::unique_ptr<DesktopCapturer> DesktopCapturer::Create(video::VideoProfile profile)
{
    return std::unique_ptr<DesktopCapturer>(new DesktopCapturer(profile));
}

DesktopCapturer::DesktopCapturer(video::VideoProfile profile)
    : profile_(profile), screenCapture_(ohos::AVScreenCapture::Create())
{
}

DesktopCapturer::~DesktopCapturer()
{
    Release();
}

void DesktopCapturer::NotifyCapturedStart(bool success)
{
    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnCapturerStarted(success);
    }
}

void DesktopCapturer::NotifyCapturedStop()
{
    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnCapturerStopped();
    }
}

void DesktopCapturer::Init(std::unique_ptr<VideoFrameReceiver> dataReceiver, Observer* observer)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    {
        UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
        observer_ = observer;
    }

    dataReceiver_ = std::move(dataReceiver);
    dataReceiver_->SetVideoFrameSize(profile_.resolution.width, profile_.resolution.height);
    dataReceiver_->SetCallback(this);

    OH_AudioCaptureInfo micCapInfo{};
    micCapInfo.audioSampleRate = 16000;
    micCapInfo.audioChannels = 2;
    micCapInfo.audioSource = OH_MIC;

    OH_AudioInfo audioInfo{};
    audioInfo.micCapInfo = micCapInfo;

    OH_VideoCaptureInfo videoCapInfo{};
    videoCapInfo.videoFrameWidth = static_cast<int32_t>(profile_.resolution.width);
    videoCapInfo.videoFrameHeight = static_cast<int32_t>(profile_.resolution.height);
    videoCapInfo.videoSource = PixelFormatToVideoSourceType(profile_.format);
    // int32_t missionIds[] = { 1 };
    // videoCapInfo.missionIDs = missionIds;
    // videoCapInfo.missionIDsLen = 1;

    OH_VideoInfo videoInfo{};
    videoInfo.videoCapInfo = videoCapInfo;

    OH_AVScreenCaptureConfig config{};
    config.captureMode = OH_CAPTURE_HOME_SCREEN;
    // config.captureMode = OH_CAPTURE_SPECIFIED_WINDOW;
    config.dataType = OH_ORIGINAL_STREAM;
    config.audioInfo = audioInfo;
    config.videoInfo = videoInfo;

    OH_AVSCREEN_CAPTURE_ErrCode ret = OH_AVScreenCapture_Init(screenCapture_.Raw(), config);
    if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
        RTC_LOG(LS_ERROR) << "Failed to init: " << ret;
        return;
    }

    ret = OH_AVScreenCapture_SetMicrophoneEnabled(screenCapture_.Raw(), false);
    if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
        RTC_LOG(LS_ERROR) << "Failed to set microphone enabled: " << ret;
        return;
    }

    ret = OH_AVScreenCapture_SetErrorCallback(screenCapture_.Raw(), DesktopCapturer::OnError1, this);
    if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
        RTC_LOG(LS_ERROR) << "Failed to set error callback: " << ret;
        return;
    }

    ret = OH_AVScreenCapture_SetStateCallback(screenCapture_.Raw(), DesktopCapturer::OnStateChange1, this);
    if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
        RTC_LOG(LS_ERROR) << "Failed to set state callback: " << ret;
        return;
    }

    ret = OH_AVScreenCapture_SetDataCallback(screenCapture_.Raw(), DesktopCapturer::OnBufferAvailable1, this);
    if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
        RTC_LOG(LS_ERROR) << "Failed to set data callback: " << ret;
        return;
    }

    isInitialized_ = true;
}

void DesktopCapturer::Release()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    screenCapture_.Reset();
    dataReceiver_.reset();

    {
        UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
        observer_ = nullptr;
    }
}

void DesktopCapturer::Start()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (isStarted_) {
        RTC_LOG(LS_WARNING) << "Capture is started";
        return;
    }

    if (!isInitialized_) {
        RTC_LOG(LS_ERROR) << "Not initialized";
        NotifyCapturedStart(false);
        return;
    }

    uint64_t surfaceId = dataReceiver_->GetSurfaceId();
    RTC_DLOG(LS_VERBOSE) << "surfaceId: " << surfaceId;

    OHNativeWindow* window = nullptr;
    int32_t ret = OH_NativeWindow_CreateNativeWindowFromSurfaceId(surfaceId, &window);
    if (ret != 0) {
        RTC_LOG(LS_ERROR) << "Failed to create native window from surface id: " << ret;
        return;
    }

    // OH_AVScreenCapture_StartScreenCapture(screenCapture_.Raw());
    ret = OH_AVScreenCapture_StartScreenCaptureWithSurface(screenCapture_.Raw(), window);
    if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
        RTC_LOG(LS_ERROR) << "Failed to start screen capture width surface: " << ret;
        return;
    }

    isStarted_ = true;
}

void DesktopCapturer::Stop()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!isStarted_) {
        RTC_LOG(LS_ERROR) << "Capture is not started";
        return;
    }

    OH_AVSCREEN_CAPTURE_ErrCode ret = OH_AVScreenCapture_StopScreenCapture(screenCapture_.Raw());
    if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
        RTC_LOG(LS_ERROR) << "Failed to stop screen capture: " << ret;
        return;
    }
    
    isStarted_ = false;
}

bool DesktopCapturer::IsScreencast()
{
    return true;
}

void DesktopCapturer::OnFrameAvailable(
    rtc::scoped_refptr<VideoFrameBuffer> buffer, int64_t timestampUs, VideoRotation rotation)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnFrameCaptured(buffer, timestampUs, rotation);
    }
}

void DesktopCapturer::OnError1(OH_AVScreenCapture* capture, int32_t errorCode, void* userData)
{
    DesktopCapturer* self = (DesktopCapturer*)userData;
    self->OnError(capture, errorCode);
}

void DesktopCapturer::OnStateChange1(
    struct OH_AVScreenCapture* capture, OH_AVScreenCaptureStateCode stateCode, void* userData)
{
    DesktopCapturer* self = (DesktopCapturer*)userData;
    self->OnStateChange(capture, stateCode);
}

void DesktopCapturer::OnBufferAvailable1(
    OH_AVScreenCapture* capture, OH_AVBuffer* buffer, OH_AVScreenCaptureBufferType bufferType, int64_t timestamp,
    void* userData)
{
    // surface模式时不触发
    DesktopCapturer* self = (DesktopCapturer*)userData;
    self->OnBufferAvailable(capture, buffer, bufferType, timestamp);
}

void DesktopCapturer::OnError(OH_AVScreenCapture* capture, int32_t errorCode)
{
    RTC_LOG(LS_ERROR) << "Error: " << errorCode;
    (void)capture;
}

void DesktopCapturer::OnStateChange(struct OH_AVScreenCapture* capture, OH_AVScreenCaptureStateCode stateCode)
{
    RTC_LOG(LS_INFO) << "State change: " << stateCode;
    (void)capture;

    switch (stateCode) {
        case OH_SCREEN_CAPTURE_STATE_STARTED: {
            NotifyCapturedStart(true);
        } break;
        case OH_SCREEN_CAPTURE_STATE_CANCELED: {
            NotifyCapturedStart(false);
        } break;
        case OH_SCREEN_CAPTURE_STATE_STOPPED_BY_USER:
        case OH_SCREEN_CAPTURE_STATE_INTERRUPTED_BY_OTHER: {
            NotifyCapturedStop();
        } break;
        case OH_SCREEN_CAPTURE_STATE_STOPPED_BY_CALL:
            // 通话中断状态处理
            break;
        default:
            break;
    }
}

void DesktopCapturer::OnBufferAvailable(
    OH_AVScreenCapture* capture, OH_AVBuffer* buffer, OH_AVScreenCaptureBufferType bufferType, int64_t timestamp)
{
    RTC_DLOG(LS_VERBOSE) << "Buffer available: " << bufferType << ", " << timestamp;
    (void)capture;

    OH_AVCodecBufferAttr attr;
    OH_AVBuffer_GetBufferAttr(buffer, &attr);
    RTC_DLOG(LS_VERBOSE) << "Buffer attr: offset=" << attr.offset << ", size=" << attr.size << ", pts=" << attr.pts
                         << ", flags=" << attr.flags;

    //     // 获取解码后信息 可以参考编解码接口
    //     OH_NativeBuffer* nativeBuffer = OH_AVBuffer_GetNativeBuffer(buffer);
    //     OH_NativeBuffer_Config config;
    //     OH_NativeBuffer_GetConfig(nativeBuffer, &config);
    //     int32_t videoSize = config.height * config.width * 4;
    //     uint8_t* buf = OH_AVBuffer_GetAddr(buffer);
    switch (bufferType) {
        case OH_SCREEN_CAPTURE_BUFFERTYPE_AUDIO_INNER:
            // 处理内录buffer
            break;
        case OH_SCREEN_CAPTURE_BUFFERTYPE_AUDIO_MIC:
            // 处理麦克风buffer
            break;
        case OH_SCREEN_CAPTURE_BUFFERTYPE_VIDEO:
            // 处理视频buffer
            break;
        default:
            break;
    }
}

} // namespace webrtc
