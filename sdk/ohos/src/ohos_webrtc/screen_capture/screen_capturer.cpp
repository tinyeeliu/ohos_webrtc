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

#include "screen_capturer.h"
#include "system_audio_receiver.h"
#include "../video/video_frame_receiver_native.h"
#include "../audio_device/audio_common.h"
#include "../helper/native_window.h"
#include "../utils/marcos.h"

#include <cstdint>

namespace webrtc {

namespace {

constexpr int32_t kVideoFrameWidth_Default = 720;
constexpr int32_t kVideoFrameHeight_Default = 1280;

} // namespace

std::unique_ptr<ScreenCapturer>
ScreenCapturer::Create(ScreenCaptureOptions options, std::shared_ptr<SystemAudioReceiver> systemAudioReceiver)
{
    RTC_DLOG(LS_INFO) << __FUNCTION__;

    return std::make_unique<ScreenCapturer>(std::move(options), std::move(systemAudioReceiver));
}

ScreenCapturer::ScreenCapturer(ScreenCaptureOptions options, std::shared_ptr<SystemAudioReceiver> systemAudioReceiver)
    : options_(std::move(options)),
      systemAudioReceiver_(systemAudioReceiver),
      screenCapture_(ohos::AVScreenCapture::Create())
{
    RTC_DLOG(LS_INFO) << __FUNCTION__;
}

ScreenCapturer::~ScreenCapturer()
{
    RTC_DLOG(LS_INFO) << __FUNCTION__;

    Release();
}

void ScreenCapturer::Init(std::unique_ptr<VideoFrameReceiver> dataReceiver, Observer* observer)
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    {
        UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
        observer_ = observer;
    }

    int32_t width = options_.videoFrameWidth.value_or(kVideoFrameWidth_Default);
    int32_t height = options_.videoFrameHeight.value_or(kVideoFrameHeight_Default);

    dataReceiver_ = std::move(dataReceiver);
    dataReceiver_->SetVideoFrameSize(width, height);
    dataReceiver_->SetCallback(this);
    // Convert timestamp from nanosecond to microsecond
    dataReceiver_->SetTimestampConverter(
        TimestampConverter([](int64_t timestamp) { return TimestampCast<std::nano, std::micro>(timestamp); }));

    OH_AVScreenCaptureConfig config{};
    SetupConfig(width, height, config);

    OH_AVSCREEN_CAPTURE_ErrCode ret = OH_AVScreenCapture_Init(screenCapture_.Raw(), config);
    if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
        RTC_LOG(LS_ERROR) << "Failed to init: " << ret;
        return;
    }

    ret = OH_AVScreenCapture_SetMicrophoneEnabled(screenCapture_.Raw(), false);
    RTC_LOG_IF(LS_ERROR, ret != AV_SCREEN_CAPTURE_ERR_OK) << "Failed to set microphone enabled: " << ret;

    ret = OH_AVScreenCapture_SetErrorCallback(screenCapture_.Raw(), ScreenCapturer::OnError1, this);
    RTC_LOG_IF(LS_ERROR, ret != AV_SCREEN_CAPTURE_ERR_OK) << "Failed to set data callback: " << ret;
    ret = OH_AVScreenCapture_SetStateCallback(screenCapture_.Raw(), ScreenCapturer::OnStateChange1, this);
    RTC_LOG_IF(LS_ERROR, ret != AV_SCREEN_CAPTURE_ERR_OK) << "Failed to set state callback: " << ret;
    ret = OH_AVScreenCapture_SetDataCallback(screenCapture_.Raw(), ScreenCapturer::OnBufferAvailable1, this);
    RTC_LOG_IF(LS_ERROR, ret != AV_SCREEN_CAPTURE_ERR_OK) << "Failed to set data callback: " << ret;

    auto contentFilter = ohos::AVScreenCaptureContentFilter::Create();
    SetupContentFilter(contentFilter);
    ret = OH_AVScreenCapture_ExcludeContent(screenCapture_.Raw(), contentFilter.Raw());
    if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
        RTC_LOG(LS_ERROR) << "Failed to exclude content: " << ret;
        return;
    }

    if (!options_.skipPrivacyModeWindowIds.empty()) {
        ret = OH_AVScreenCapture_SkipPrivacyMode(
            screenCapture_.Raw(), const_cast<int32_t*>(options_.skipPrivacyModeWindowIds.data()),
            options_.skipPrivacyModeWindowIds.size());
        if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
            RTC_LOG(LS_ERROR) << "Failed to skip privacy mode: " << ret;
            return;
        }
    }

    isInitialized_ = true;
}

void ScreenCapturer::Release()
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    screenCapture_.Reset();
    dataReceiver_.reset();

    {
        UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
        observer_ = nullptr;
    }
}

void ScreenCapturer::Start()
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    if (isStarted_) {
        RTC_LOG(LS_WARNING) << "Capture is started";
        return;
    }

    if (!isInitialized_) {
        RTC_LOG(LS_ERROR) << "Not initialized";
        NotifyCaptureStart(false);
        return;
    }

    uint64_t surfaceId = dataReceiver_->GetSurfaceId();
    RTC_DLOG(LS_INFO) << "surfaceId: " << surfaceId;

    OHNativeWindow* window = nullptr;
    int32_t ret = OH_NativeWindow_CreateNativeWindowFromSurfaceId(surfaceId, &window);
    if (ret != 0) {
        RTC_LOG(LS_ERROR) << "Failed to create native window from surface id: " << ret;
        return;
    }

    ret = OH_AVScreenCapture_StartScreenCaptureWithSurface(screenCapture_.Raw(), window);
    if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
        RTC_LOG(LS_ERROR) << "Failed to start screen capture width surface: " << ret;
        return;
    }

    isStarted_ = true;
}

void ScreenCapturer::Stop()
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

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

bool ScreenCapturer::IsScreencast()
{
    return true;
}

void ScreenCapturer::SetupConfig(int32_t width, int32_t height, OH_AVScreenCaptureConfig& config)
{
    OH_CaptureMode captureMode = static_cast<OH_CaptureMode>(options_.captureMode.value_or(OH_CAPTURE_HOME_SCREEN));

    // Audio
    OH_AudioInfo audioInfo{};
    if (systemAudioReceiver_) {
        OH_AudioCaptureInfo innerCapInfo{};
        // !!!: Asume sample format is s16le
        innerCapInfo.audioSampleRate = systemAudioReceiver_->GetSampleRate();
        innerCapInfo.audioChannels = systemAudioReceiver_->GetChannelCount();
        innerCapInfo.audioSource = static_cast<OH_AudioCaptureSourceType>(systemAudioReceiver_->GetAudioSource());
        audioInfo.innerCapInfo = innerCapInfo;
    }

    // Video
    OH_VideoInfo videoInfo{};
    OH_VideoCaptureInfo videoCapInfo{};
    if (captureMode == OH_CAPTURE_SPECIFIED_SCREEN) {
        videoCapInfo.displayId = options_.displayId.value_or(-1);
    } else if (captureMode == OH_CAPTURE_SPECIFIED_WINDOW) {
        videoCapInfo.missionIDs = const_cast<int32_t*>(options_.missionIds.data());
        videoCapInfo.missionIDsLen = options_.missionIds.size();
    }
    videoCapInfo.videoFrameWidth = width;
    videoCapInfo.videoFrameHeight = height;
    videoCapInfo.videoSource = OH_VIDEO_SOURCE_SURFACE_RGBA;
    videoInfo.videoCapInfo = videoCapInfo;

    // Config
    config.captureMode = captureMode;
    config.dataType = OH_ORIGINAL_STREAM;
    config.audioInfo = audioInfo;
    config.videoInfo = videoInfo;
}

bool ScreenCapturer::SetupContentFilter(ohos::AVScreenCaptureContentFilter& contentFilter)
{
    if (!options_.filteredWindowIds.empty()) {
        auto ret = OH_AVScreenCapture_ContentFilter_AddWindowContent(
            contentFilter.Raw(), const_cast<int32_t*>(options_.filteredWindowIds.data()),
            options_.filteredWindowIds.size());
        if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
            RTC_LOG(LS_ERROR) << "Failed to add audio content filter: " << ret;
            return false;
        }
    }

    if (!options_.filteredAudioContents.empty()) {
        for (const auto& audioContent : options_.filteredAudioContents) {
            if (audioContent != OH_SCREEN_CAPTURE_NOTIFICATION_AUDIO &&
                audioContent != OH_SCREEN_CAPTURE_CURRENT_APP_AUDIO)
            {
                RTC_LOG(LS_WARNING) << "Invalid audio content: " << audioContent;
                continue;
            }

            auto ret = OH_AVScreenCapture_ContentFilter_AddAudioContent(
                contentFilter.Raw(), static_cast<OH_AVScreenCaptureFilterableAudioContent>(audioContent));
            if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
                RTC_LOG(LS_ERROR) << "Failed to add audio content filter: " << ret;
                return false;
            }
        }
    }

    return true;
}

void ScreenCapturer::NotifyCaptureStart(bool success)
{
    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnCapturerStarted(success);
    }
}

void ScreenCapturer::NotifyCaptureStop()
{
    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnCapturerStopped();
    }
}

void ScreenCapturer::OnFrameAvailable(
    rtc::scoped_refptr<VideoFrameBuffer> buffer, int64_t timestampUs, VideoRotation rotation)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnFrameCaptured(buffer, timestampUs, rotation);
    }
}

void ScreenCapturer::OnError1(OH_AVScreenCapture* capture, int32_t errorCode, void* userData)
{
    ScreenCapturer* self = (ScreenCapturer*)userData;
    self->OnError(capture, errorCode);
}

void ScreenCapturer::OnStateChange1(
    struct OH_AVScreenCapture* capture, OH_AVScreenCaptureStateCode stateCode, void* userData)
{
    ScreenCapturer* self = (ScreenCapturer*)userData;
    self->OnStateChange(capture, stateCode);
}

void ScreenCapturer::OnBufferAvailable1(
    OH_AVScreenCapture* capture, OH_AVBuffer* buffer, OH_AVScreenCaptureBufferType bufferType, int64_t timestamp,
    void* userData)
{
    // surface模式时不触发
    ScreenCapturer* self = (ScreenCapturer*)userData;
    self->OnBufferAvailable(capture, buffer, bufferType, timestamp);
}

void ScreenCapturer::OnError(OH_AVScreenCapture* capture, int32_t errorCode)
{
    RTC_LOG(LS_ERROR) << "Error: " << errorCode;
    (void)capture;
}

void ScreenCapturer::OnStateChange(struct OH_AVScreenCapture* capture, OH_AVScreenCaptureStateCode stateCode)
{
    RTC_LOG(LS_INFO) << "State change: " << stateCode;
    (void)capture;

    switch (stateCode) {
        case OH_SCREEN_CAPTURE_STATE_STARTED: {
            NotifyCaptureStart(true);
            if (systemAudioReceiver_) {
                systemAudioReceiver_->OnStarted(true);
            }
        } break;
        case OH_SCREEN_CAPTURE_STATE_CANCELED:
        case OH_SCREEN_CAPTURE_STATE_STOPPED_BY_USER: {
            NotifyCaptureStop();
            if (systemAudioReceiver_) {
                systemAudioReceiver_->OnStopped();
            }
            break;
        }
        case OH_SCREEN_CAPTURE_STATE_INTERRUPTED_BY_OTHER:
        case OH_SCREEN_CAPTURE_STATE_STOPPED_BY_CALL:
        case OH_SCREEN_CAPTURE_STATE_STOPPED_BY_USER_SWITCHES:
            NotifyCaptureStop();
            if (systemAudioReceiver_) {
                systemAudioReceiver_->OnError(0, "Interrupted");
            }
            break;
        default:
            break;
    }
}

void ScreenCapturer::OnBufferAvailable(
    OH_AVScreenCapture* capture, OH_AVBuffer* buffer, OH_AVScreenCaptureBufferType bufferType, int64_t timestamp)
{
    RTC_DLOG(LS_VERBOSE) << "Buffer available: " << bufferType << ", " << timestamp;
    (void)capture;

    OH_AVCodecBufferAttr attr;
    OH_AVBuffer_GetBufferAttr(buffer, &attr);
    RTC_DLOG(LS_VERBOSE) << "Buffer attr: offset=" << attr.offset << ", size=" << attr.size << ", pts=" << attr.pts
                         << ", flags=" << attr.flags;

    switch (bufferType) {
        case OH_SCREEN_CAPTURE_BUFFERTYPE_AUDIO_INNER: {
            // 处理内录buffer
            if (systemAudioReceiver_) {
                auto addr = OH_AVBuffer_GetAddr(buffer);
                systemAudioReceiver_->OnData(addr + attr.offset, attr.size, timestamp);
            }
            break;
        }
        case OH_SCREEN_CAPTURE_BUFFERTYPE_AUDIO_MIC:
            // Ignore
            break;
        case OH_SCREEN_CAPTURE_BUFFERTYPE_VIDEO:
            // Ignore (no video buffer in surface mode)
            break;
        default:
            break;
    }
}

} // namespace webrtc
