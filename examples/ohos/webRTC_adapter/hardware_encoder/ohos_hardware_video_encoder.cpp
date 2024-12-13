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
#include "surface_helper/yuv_converter.h"
#include "hardware_encoder/ohos_hardware_video_encoder.h"
#include "api/video/i420_buffer.h"
#include "commom/ohos_video_buffer.h"
#include "rtc_base/time_utils.h"
#include <hilog/log.h>
#include <multimedia/player_framework/native_avmemory.h>

namespace webrtc {
namespace ohos {
#define FOUR 4
#define THREE 3
#define TWO 2
#define THOUSAND 1000

// QP scaling thresholds.
static const int kLowH264QpThreshold = 24;
static const int kHighH264QpThreshold = 37;

#define NTP_TIMESTAMP_DELTA 2208988800ull
#include "common_video/libyuv/include/webrtc_libyuv.h"
#if defined(WEBRTC_USE_H264)
    bool g_rtc_use_h264 = true;
#endif

OhosHardwareVideoEncoder::OhosHardwareVideoEncoder(const cricket::VideoCodec& codec)
{
    RTC_CHECK(absl::EqualsIgnoreCase(codec.name, cricket::kH264CodecName));
}

void OhosHardwareVideoEncoder::OutputThread()
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "outputThread", "OhosHardwareVideoEncoder::OutputThread enter");

    std::vector<uint8_t> encodeFrame;
    OH_AVCodecBufferAttr avcodecBuffAttr{0, 0, 0, 0};
    CodecBufferInfo codecBufferInfoReceive;
    EncodedImage encodeImage;
    // 如果是sps和pps，保存到一个静态数据中
    std::vector<uint8_t> paramaterInfo;

    while (running_.load()) {
        int32_t res = codecData_.OutputData(codecBufferInfoReceive);
        if (res != AV_ERR_OK) {
            continue;
        }

        if (curBitrate_ != adjustedBitrate_ && encoder_ != nullptr) {
            uint32_t res = encoder_->UpdateBitrate(adjustedBitrate_);
            if (res != 0) {
                curBitrate_ = adjustedBitrate_;
            }
        }

        encodeFrame.clear();
        codecBufferInfoReceive.GetAttr(&avcodecBuffAttr);
        // sps和pps
        if (avcodecBuffAttr.flags == AVCODEC_BUFFER_FLAGS_CODEC_DATA) {
            paramaterInfo.clear();
            paramaterInfo.insert(paramaterInfo.end(), codecBufferInfoReceive.GetBuff(),
                codecBufferInfoReceive.GetBuff() + avcodecBuffAttr.size);
            continue;
        } else {
            rtc::scoped_refptr<EncodedImageBuffer> encoded_data;
            if (avcodecBuffAttr.flags == AVCODEC_BUFFER_FLAGS_SYNC_FRAME) {
                encodeFrame.insert(encodeFrame.end(), paramaterInfo.begin(), paramaterInfo.end());
                encodeFrame.insert(encodeFrame.end(), codecBufferInfoReceive.GetBuff(),
                    codecBufferInfoReceive.GetBuff() + avcodecBuffAttr.size);
                encoded_data = EncodedImageBuffer::Create(encodeFrame.data(), encodeFrame.size());
            } else {
                encoded_data = EncodedImageBuffer::Create(codecBufferInfoReceive.GetBuff(), avcodecBuffAttr.size);
            }
            encodeImage._encodedWidth = width_;
            encodeImage._encodedHeight = height_;
            encodeImage.SetRtpTimestamp(rtc::TimeMillis());
            encodeImage.SetEncodedData(encoded_data);
            encodeImage.set_size(encoded_data->size());
        }

        encoder_->FreeOutPutData(codecBufferInfoReceive.GetBufferIndex());

        // 创建一个 CodecSpecificInfo 结构体
        CodecSpecificInfo codecSpecificInfo;
        codecSpecificInfo.codecType = kVideoCodecH264;
        // 将 H264 特定的信息复制到 CodecSpecificInfo 结构体中
        codecSpecificInfo.codecSpecific.H264 = {H264PacketizationMode::NonInterleaved};
        encoded_image_callback_->OnEncodedImage(encodeImage, &codecSpecificInfo);
    }
}

OhosHardwareVideoEncoder::~OhosHardwareVideoEncoder()
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "~OhosHardwareVideoEncoder", "leave OhosHardwareVideoEncoder");
    running_.store(false);
    outputThread_->join();
}

int32_t OhosHardwareVideoEncoder::InitEncode(const VideoCodec *codec_settings, int32_t number_of_cores,
    size_t max_payload_size)
{
    width_ = codec_settings->width;
    height_ = codec_settings->height;

    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "InitEncode", "width is: %{public}d", width_);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "InitEncode", "height is: %{public}d", height_);

    encoder_ = std::make_unique<OhosVideoEncoder>();
    encoder_->Create();

    unsigned int startBitrate = codec_settings->startBitrate * THOUSAND; // kilobits/sec.
    unsigned int maxBitrate = codec_settings->maxBitrate * THOUSAND;   // kilobits/sec.
    unsigned int minBitrate = codec_settings->minBitrate * THOUSAND;   // kilobits/sec.

    if (startBitrate < minBitrate) {
        startBitrate = minBitrate;
    }

    if (startBitrate > maxBitrate) {
        startBitrate = maxBitrate;
    }

    curBitrate_ = startBitrate;
    adjustedBitrate_ = curBitrate_;
    formatInfo_.bitrate = curBitrate_;
    formatInfo_.videoWidth = width_;
    formatInfo_.videoHeight = height_;
    formatInfo_.qpMax = kHighH264QpThreshold;
    formatInfo_.qpMin = kLowH264QpThreshold;
    codecData_.formatInfo_ = &formatInfo_;
    encoder_->Config(&codecData_);

    eglRenderContext_ = std::make_unique<EglRenderContext>();
    glDrawer_ = std::make_unique<OhosGLDrawer>();

    nextPTS_ = static_cast<unsigned long long>(time(NULL)) + NTP_TIMESTAMP_DELTA; // 计算NTP时间戳
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "InitEncode", "enter OhosHardwareVideoEncoder");

    fps_ = codec_settings->maxFramerate;

    running_.store(true);
    outputThread_ = std::make_unique<std::thread>(&OhosHardwareVideoEncoder::OutputThread, this);
 
    return WEBRTC_VIDEO_CODEC_OK;
}

int OhosHardwareVideoEncoder::InitEncode(const VideoCodec* codec_settings, const VideoEncoder::Settings& settings)
{
    if (codec_settings == nullptr) {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }

    return InitEncode(codec_settings, settings.number_of_cores, settings.max_payload_size);
}

int32_t OhosHardwareVideoEncoder::RegisterEncodeCompleteCallback(EncodedImageCallback *callback)
{
    encoded_image_callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

void OhosHardwareVideoEncoder::SetRates(const RateControlParameters &parameters)
{
    fps_ = parameters.framerate_fps;
    adjustedBitrate_ = parameters.bitrate.get_sum_bps();

    if (encoder_ != nullptr) {
        if (adjustedBitrate_ > encoder_->GetBitrateRange().maxVal) {
            adjustedBitrate_ = encoder_->GetBitrateRange().maxVal;
        } else if (adjustedBitrate_ < encoder_->GetBitrateRange().minVal) {
            adjustedBitrate_ = encoder_->GetBitrateRange().minVal;
        }
    }
}

int32_t OhosHardwareVideoEncoder::Release()
{
    encoder_->Stop();
    encoder_->Release();
    return WEBRTC_VIDEO_CODEC_OK;
}

void OhosHardwareVideoEncoder::SetEncoderInfo()
{
    bool isHardware = false;
    encoderInfo_.supports_native_handle = true;
    encoderInfo_.implementation_name = "OhosH264";
    encoderInfo_.scaling_settings = VideoEncoder::ScalingSettings(kLowH264QpThreshold, kHighH264QpThreshold);
    if (encoder_ != nullptr) {
        isHardware = encoder_->IsHardware();
        int frameSize = width_ * height_;
        OH_AVRange bitrateRange = encoder_->GetBitrateRange();
        encoderInfo_.resolution_bitrate_limits.clear();
        encoderInfo_.resolution_bitrate_limits.push_back(ResolutionBitrateLimits(frameSize, formatInfo_.bitrate,
            bitrateRange.minVal, bitrateRange.minVal));
    }
    encoderInfo_.is_hardware_accelerated = isHardware;
    encoderInfo_.supports_simulcast = true;
    if (eglSurface_ == EGL_NO_SURFACE) {
        encoderInfo_.preferred_pixel_formats = {VideoFrameBuffer::Type::kI420};
    } else {
        encoderInfo_.preferred_pixel_formats = {VideoFrameBuffer::Type::kNative};
    }
    encoderInfo_.apply_alignment_to_all_simulcast_layers = false;
}

VideoEncoder::EncoderInfo OhosHardwareVideoEncoder::GetEncoderInfo() const
{
    return encoderInfo_;
}

bool OhosHardwareVideoEncoder::SurfeceModeInit(OHOSVideoBuffer::VideoSourceType type)
{
    bool res = encoder_->CreatNativeWindow(width_, height_);
    if (!res) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "h264Encoder", "CreatNativeWindow fail!");
        return false;
    }

    std::shared_ptr<EGLContextResource> eglResource = OhosEGLContextManage::GetInstance().GetEGLContextResource(type);
    if (eglResource == nullptr) {
        return false;
    }

    res = eglRenderContext_->Init(eglResource->eglContext_);
    if (!res) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "h264Encoder", "eglRenderContext_->Init fail!");
        return false;
    }

    if (eglSurface_ == EGL_NO_SURFACE) {
        OHNativeWindow *nativeWindow = encoder_->GetNativeWindow();
        if (nativeWindow == nullptr) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "h264Encoder", "GetNativeWindow fail!");
            return false;
        }

        eglSurface_ = eglRenderContext_->CreateEglSurface(static_cast<EGLNativeWindowType>(nativeWindow));
        if (eglSurface_ == EGL_NO_SURFACE) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "h264Encoder", "CreateEglSurface fail!");
            return false;
        }
    }

    eglRenderContext_->MakeCurrent(eglSurface_);
    converter_ = std::make_shared<YuvConverter>();
    glDrawer_->CreateGLResources();
    return true;
}

int32_t OhosHardwareVideoEncoder::EncodeByteBuffer(const VideoFrame &frame)
{
    int32_t res = WEBRTC_VIDEO_CODEC_ERROR;
    int32_t bufferIndex = 0;
    CodecData::I420Info i420Info;
    i420Info.srcY = frame.video_frame_buffer().get()->GetI420()->DataY();
    i420Info.srcStrideY = frame.video_frame_buffer().get()->GetI420()->StrideY();
    i420Info.srcU = frame.video_frame_buffer().get()->GetI420()->DataU();
    i420Info.srcStrideU = frame.video_frame_buffer().get()->GetI420()->StrideU();
    i420Info.srcV = frame.video_frame_buffer().get()->GetI420()->DataV();
    i420Info.srcStrideV = frame.video_frame_buffer().get()->GetI420()->StrideV();
    i420Info.width = frame.width();
    i420Info.height = frame.height();
    res = codecData_.InputData(i420Info, bufferIndex, encoder_->GetEncoderStride());
    if (res == 0) {
        encoder_->PushInputData(bufferIndex);
    } else {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "h264Encoder", "Failed to PushInputData");
        return WEBRTC_VIDEO_CODEC_ENCODER_FAILURE;
    }
    return res;
}

int32_t OhosHardwareVideoEncoder::EncodeTextureBuffer(const VideoFrame &frame)
{
    OHOSVideoBuffer *videoBuffer = nullptr;
    videoBuffer = static_cast<OHOSVideoBuffer*>(frame.video_frame_buffer().get());
    {
        std::lock_guard<std::mutex>(OhosEGLContextManage::GetInstance().
            GetEGLContextResource(videoBuffer->GetSourceType())->textureIDMutex_);
        glDrawer_->DrawFrame(videoBuffer->GetVideoFrameBuffer(), matrix_, 0, 0, 0, 0, width_, height_);
    }
    eglRenderContext_->SwapBuffers(eglSurface_);
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t OhosHardwareVideoEncoder::Encode(const VideoFrame &frame, const std::vector<VideoFrameType> *frame_types)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "h264Encoder", "Encode start");
    OHOSVideoBuffer *videoBuffer = nullptr;
    videoBuffer = static_cast<OHOSVideoBuffer*>(frame.video_frame_buffer().get());
    // todo ... 动态检测宽高， 如果宽高不一致，需要重启initencode流程

    bool isTexture = frame.is_texture();

    if (encoder_->Stat() == OhosVideoEncoder::EncodeStat::CONFIGUED) {
        if (isTexture && !SurfeceModeInit(videoBuffer->GetSourceType())) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "h264Encoder", "SurfeceModeInit fail");
            return WEBRTC_VIDEO_CODEC_ERROR;
        }
        encoder_->Start();
        SetEncoderInfo();
    }

    if (encoder_->Stat() == OhosVideoEncoder::EncodeStat::RUNNING) {
        codecData_.Start();
    } else {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "h264Encoder", "codecData_ shutdown");
        codecData_.ShutDown();
        return WEBRTC_VIDEO_CODEC_ERROR;
    }
    rtc::scoped_refptr<::webrtc::I420BufferInterface> i420Buffer;
    if (isTexture) {
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "ToI420", "Start");
        std::lock_guard<std::mutex>(OhosEGLContextManage::GetInstance().
            GetEGLContextResource(videoBuffer->GetSourceType())->textureIDMutex_);
        videoBuffer->SetConverter(converter_);
        i420Buffer = videoBuffer->ToI420();
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "ToI420", "End");
    }
    if (!isTexture) {
        return EncodeByteBuffer(frame);
    } else {
        return EncodeTextureBuffer(frame);
    }
    return WEBRTC_VIDEO_CODEC_OK;
}

std::unique_ptr<OhosHardwareVideoEncoder> OhosHardwareVideoEncoder::Create()
{
    return Create(cricket::CreateVideoCodec(cricket::kH264CodecName));
}

std::unique_ptr<OhosHardwareVideoEncoder> OhosHardwareVideoEncoder::Create(const cricket::VideoCodec &codec)
{
    RTC_DCHECK(H264Encoder::IsSupported());
#if defined(WEBRTC_USE_H264)
    RTC_CHECK(g_rtc_use_h264);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "h264Encoder", "Create OhosHardwareVideoEncoder");
    return std::make_unique<OhosHardwareVideoEncoder>(codec);
#else
    OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "h264Encoder", "not Create OhosHardwareVideoEncoder");
    RTC_DCHECK_NOTREACHED();
    return nullptr;
#endif
}

}
}
