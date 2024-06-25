#include "ohos_desktop.h"

#include <thread>

#include <multimedia/player_framework/native_avscreen_capture_errors.h>
#include <native_buffer/native_buffer.h>
#include "hilog/log.h"

#include "api/video/i420_buffer.h"
#include "libyuv/convert.h"

namespace webrtc {
namespace ohos {

void OhosDesktop::OnError(OH_AVScreenCapture *capture, int32_t errorCode, void *userData) {
    (void)capture;
    (void)errorCode;
    (void)userData;
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktop", "errorCode = %{public}d", errorCode);
}

void OhosDesktop::OnStateChange(struct OH_AVScreenCapture *capture, OH_AVScreenCaptureStateCode stateCode, 
        void *userData) {
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktop", "stateCode = %{public}d", stateCode);
    OhosDesktop *desktop = static_cast<OhosDesktop *>(userData);
    desktop->SetStateCode(stateCode);
}

void OhosDesktop::OnBufferAvailable(OH_AVScreenCapture *capture, OH_AVBuffer *buffer, 
        OH_AVScreenCaptureBufferType bufferType, int64_t timestamp, void *userData) {
    OhosDesktop *desktop = static_cast<OhosDesktop *>(userData);
    if (bufferType == OH_SCREEN_CAPTURE_BUFFERTYPE_VIDEO) {
        if (!desktop->isRunning_.load()) {
            return;
        }

        std::lock_guard<std::mutex> lock(desktop->mutex_);
        if (desktop->desktopAVBufferQueue_.size() >= desktop->maxQueueSize_) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktop", "desktopAVBufferQueue_ full");
            return;
        }

        // 处理视频buffer
        uint8_t* receiverAddr = OH_AVBuffer_GetAddr(buffer);
        if (receiverAddr == nullptr) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktop", "OH_AVBuffer_GetAddr failed");
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
        desktop->desktopAVBufferQueue_.push({data_buffer, bufferLength, config.stride, config.width, config.height, timestamp});
        desktop->cv_.notify_all();
    } else if (bufferType == OH_SCREEN_CAPTURE_BUFFERTYPE_AUDIO_INNER) {
        // 处理内录buffer
        return;
    } else if (bufferType == OH_SCREEN_CAPTURE_BUFFERTYPE_AUDIO_MIC) {
        // 处理麦克风buffer
        return;
    }
}

int32_t OhosDesktop::Create(int32_t queueSize) {
    // 只初始化一次
    if (capture_ != nullptr) {
        return 0;
    }
    capture_ = OH_AVScreenCapture_Create();
    maxQueueSize_ = queueSize > 0 ? queueSize : 1;
    // 设置回调
    OH_AVScreenCapture_SetErrorCallback(capture_, OnError, this);
    OH_AVScreenCapture_SetStateCallback(capture_, OnStateChange, this);
    OH_AVScreenCapture_SetDataCallback(capture_, OnBufferAvailable, this);
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktop", "create and set callback"); 
    return 0;
}

int32_t OhosDesktop::Config(int32_t width, int32_t height) {
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
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktop", "Config end");
    isConfig_ = true;
    return 0;
}

int32_t OhosDesktop::Start() {
    if (isStarted_) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktop", "capture is stared");
        return 0;
    }

    if (!isConfig_) {
        Create();
        Config();
    }

    // 设置启动线程
    isRunning_.store(true);
    std::thread bufferAvailableThread (&OhosDesktop::DesktopBufferAvailable, this);
    bufferAvailableThread.detach();

    int32_t ret = OH_AVScreenCapture_StartScreenCapture(capture_);
    if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "OH_AVScreenCapture_StartScreenCapture failed");
        Release();
        return -1;
    }
    return 0;
} 

int32_t OhosDesktop::Stop() {
    if (isStarted_) {
        int32_t ret = OH_AVScreenCapture_StopScreenCapture(capture_);
        if (ret != AV_SCREEN_CAPTURE_ERR_OK) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosDesktop", "OH_AVScreenCapture_StopScreenCapture failed");
            Release();
            return -1;
        }
        isRunning_.store(false);
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktop", "capture stoped");
        isStarted_ = false;
        return 0;
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktop", "capture is stoped");
    return -1;
}

void OhosDesktop::RegisterCaptureDataCallback(rtc::VideoSinkInterface<webrtc::VideoFrame> *dataCallback) {
    dataCallback_ = dataCallback;
}

void OhosDesktop::UnregisterCaptureDataCallback() {
    dataCallback_ = nullptr;
}

void OhosDesktop::DesktopBufferOn(uint8_t *buffer, int32_t bufferLength, int32_t stride, int32_t width, int32_t height, int64_t timestamp) {
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktop", "DesktopBufferOn started");
    // 转换buffer为I420格式
    height = (height > 0) ? height : -height; // abs
    
    rtc::scoped_refptr<webrtc::I420Buffer> i420Buffer = webrtc::I420Buffer::Create(width, height);
    // 设置时为RGBA，使用RGBA转换颜色不对，使用ABGR转换是正常的。（可能是大小端序的原因）
    libyuv::ABGRToI420(buffer, stride, i420Buffer.get()->MutableDataY(),
                     i420Buffer.get()->StrideY(), i420Buffer.get()->MutableDataU(), i420Buffer.get()->StrideU(),
                     i420Buffer.get()->MutableDataV(), i420Buffer.get()->StrideV(), width, height);
//    libyuv::ConvertToI420(buffer, bufferLength, i420_buffer.get()->MutableDataY(), i420_buffer.get()->StrideY(),
//                          i420_buffer.get()->MutableDataU(), i420_buffer.get()->StrideU(),
//                          i420_buffer.get()->MutableDataV(), i420_buffer.get()->StrideV(), 0, 0, width, height, width,
//                          height, libyuv::kRotate0, cricket::FOURCC_ABGR);
    // 调整单位和归零处理
    timestamp /= 1000 * 1000;
    if (timestamp < timestampStarts_) {
        timestampStarts_ = timestamp;
    }
    timestamp -= timestampStarts_;
    webrtc::VideoFrame videoFrame = webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(i420Buffer)
                                       .set_timestamp_rtp(0)
                                       .set_timestamp_ms(timestamp)
                                       .set_rotation(webrtc::kVideoRotation_0)
                                       .build();
    if (dataCallback_) {
        dataCallback_->OnFrame(videoFrame);
    }
}

void OhosDesktop::DesktopBufferAvailable() {
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
                return;
            }
            // 取出数据
            data = desktopAVBufferQueue_.front();
            desktopAVBufferQueue_.pop();
        }

        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktop",
                     "stride = %{public}d, width = %{public}d, height = %{public}d", data.stride, data.width,
                     data.height);
        DesktopBufferOn(data.buffer, data.length, data.stride, data.width, data.height, data.timestamp);
        delete[] data.buffer;
    } while (isRunning_.load());
}
 
void OhosDesktop::SetStateCode(OH_AVScreenCaptureStateCode stateCode) {
    stateCode_ = stateCode;
    switch (stateCode_) {
        case OH_SCREEN_CAPTURE_STATE_STARTED: isStarted_ = true; break;
        case OH_SCREEN_CAPTURE_STATE_STOPPED_BY_USER: isStarted_ = false; break;
        case OH_SCREEN_CAPTURE_STATE_CANCELED: isStarted_ = false; break;
    }
}

int32_t OhosDesktop::Release() {
    OH_AVScreenCapture_Release(capture_);
    capture_ = nullptr;
    isConfig_ = false;
    isStarted_ = false;
    isRunning_.store(false);
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktop", "capture release");
    return 0;
}

OhosDesktop::~OhosDesktop() {
    Stop();
    Release();
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktop", "capture release");
}

} // namespace ohos
} // namespace webrtc