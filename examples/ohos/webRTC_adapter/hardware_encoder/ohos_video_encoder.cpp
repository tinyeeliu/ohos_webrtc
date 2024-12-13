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
#include <native_window/external_window.h>
#include <multimedia/player_framework/native_avbuffer.h>
#include "hilog/log.h"
#include "ohos_video_encoder.h"
#include "ohos_camera.h"

namespace webrtc {
namespace ohos {

#define KEY_FRAME_INTERVAL 2000

OhosVideoEncoder::~OhosVideoEncoder()
{
    Release();
}

bool OhosVideoEncoder::IsHardware() const
{
    return isHardware_;
}

OH_AVRange OhosVideoEncoder::GetBitrateRange() const
{
    return bitrateRange_;
}

uint32_t OhosVideoEncoder::UpdateBitrate(uint32_t bitrate)
{
    OH_AVFormat *format = OH_AVFormat_Create();
    OH_AVFormat_SetLongValue(format, OH_MD_KEY_BITRATE, bitrate);
    if (encoder_ != nullptr) {
        int32_t ret = OH_VideoEncoder_SetParameter(encoder_, format);
        if (ret != AV_ERR_OK) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "UpdateRates", "OH_VideoEncoder_SetParameter fail!");
        } else {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "UpdateRates", "curBitrate_ %{public}u", bitrate);
            return bitrate;
        }
    }
    return 0;
}

int32_t OhosVideoEncoder::Create()
{
    OH_AVCapability *capability = OH_AVCodec_GetCapability(OH_AVCODEC_MIMETYPE_VIDEO_AVC, true);
    isHardware_ = OH_AVCapability_IsHardware(capability);
    if (!isHardware_) {
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_DOMAIN, "OhosVideoEncoder", "Hardware Not Support");
    }

    OH_AVErrCode ret = OH_AVCapability_GetEncoderBitrateRange(capability, &bitrateRange_);
    if (ret != AV_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "OH_AVCapability_GetEncoderBitrateRange fail");
        return AV_ERR_UNKNOWN;
    }

    const char *codecName = OH_AVCapability_GetName(capability);
    encoder_ = OH_VideoEncoder_CreateByName(codecName);
    if (encoder_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "OH_VideoEncoder_CreateByName fail");
        return AV_ERR_UNKNOWN;
    } else {
        stat_ = EncodeStat::NONE;
        return AV_ERR_OK;
    }
}

int32_t OhosVideoEncoder::Config(CodecData *codecData)
{
    int32_t ret = AV_ERR_INVALID_VAL;
    if (encoder_ == nullptr || codecData == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "Config nullptr");
        return AV_ERR_UNKNOWN;
    }
    
    ret = Configure(codecData->formatInfo_);
    if (ret != AV_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "Configure failed");
        return ret;
    }
	
    ret = SetCallback(codecData);
    if (ret != AV_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "SetCallback failed");
        return ret;
    }
	
    stat_ = EncodeStat::CONFIGUED;
    return ret;
}

int32_t OhosVideoEncoder::Start()
{
    if (encoder_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "OhosVideoEncoder nullptr");
        return AV_ERR_INVALID_VAL;
    }
    int32_t ret = OH_VideoEncoder_Prepare(encoder_);
    if (ret != AV_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "OH_VideoEncoder_Prepare Failed");
        return ret;
    }
    // 启动编码器，开始编码
    ret = OH_VideoEncoder_Start(encoder_);
    if (ret != AV_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "OH_VideoEncoder_Start Failed");
        return ret;
    }
	
    stat_ = EncodeStat::RUNNING;
    return ret;
}

int32_t OhosVideoEncoder::PushInputData(uint32_t bufferIndex)
{
    if (encoder_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "OhosVideoEncoder nullptr");
        return AV_ERR_INVALID_VAL;
    }
	
    int32_t ret = AV_ERR_INVALID_VAL;
    ret = OH_VideoEncoder_PushInputBuffer(encoder_, bufferIndex);
    if (ret != AV_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "Push input data failed");
        return ret;
    }
    return AV_ERR_OK;
}

int32_t OhosVideoEncoder::FreeOutPutData(uint32_t bufferIndex)
{
    if (encoder_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "OhosVideoEncoder nullptr");
        return AV_ERR_INVALID_VAL;
    }
	
    int32_t ret = AV_ERR_OK;
    ret = OH_VideoEncoder_FreeOutputBuffer(encoder_, bufferIndex);
    if (ret != AV_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN,
            "OhosVideoEncoder", "Free output data failed, ret: %{public}d", ret);
    }
    return ret;
}

int32_t OhosVideoEncoder::Stop()
{
    if (encoder_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "OhosVideoEncoder nullptr");
        return AV_ERR_INVALID_VAL;
    }
	
    int32_t ret = AV_ERR_INVALID_VAL;
    ret = OH_VideoEncoder_Stop(encoder_);
    if (ret != AV_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "OH_VideoEncoder_Stop Failed");
        return ret;
    }
	
    // 刷新编码器
    ret = OH_VideoEncoder_Flush(encoder_);
    if (ret != AV_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "OH_VideoEncoder_Flush Failed");
        return ret;
    }
	
    stat_ = EncodeStat::STOP;
    return AV_ERR_OK;
}

void OhosVideoEncoder::Release()
{
    int32_t ret = AV_ERR_OK;
    if (nativeWindow_ != nullptr) {
        NativeWindowDestroy();
    }
    if (encoder_ != nullptr) {
        ret = OH_VideoEncoder_Destroy(encoder_);
        encoder_ = nullptr;
        stat_ = EncodeStat::RELEASE;
    }
	
    if (ret != AV_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "OH_VideoEncoder_Destroy Failed");
    }
}

int32_t OhosVideoEncoder::Configure(FormatInfo *formatInfo)
{
    // 配置编码器
    if (encoder_ == nullptr || formatInfo == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "encoder_ or formatInfo nullptr");
        return AV_ERR_INVALID_VAL;
    }
    if (formatInfo->videoHeight == 0 || formatInfo->videoWidth == 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "Configure Parameter Invalid");
        return AV_ERR_INVALID_VAL;
    }
    OH_AVFormat *format_ = OH_AVFormat_Create();
    if (format_ == nullptr) {
        return AV_ERR_UNKNOWN;
    }
    // 写入format
    OH_AVFormat_SetIntValue(format_, OH_MD_KEY_WIDTH, formatInfo->videoWidth);
    OH_AVFormat_SetIntValue(format_, OH_MD_KEY_HEIGHT, formatInfo->videoHeight);
    OH_AVFormat_SetDoubleValue(format_, OH_MD_KEY_FRAME_RATE, formatInfo->frameRate);
    OH_AVFormat_SetIntValue(format_, OH_MD_KEY_PIXEL_FORMAT, formatInfo->pixelFormat);
    OH_AVFormat_SetIntValue(format_, OH_MD_KEY_RANGE_FLAG, formatInfo->rangeFlag);
    OH_AVFormat_SetIntValue(format_, OH_MD_KEY_VIDEO_ENCODE_BITRATE_MODE, formatInfo->rateMode);
    OH_AVFormat_SetLongValue(format_, OH_MD_KEY_BITRATE, formatInfo->bitrate);
    OH_AVFormat_SetIntValue(format_, OH_MD_KEY_I_FRAME_INTERVAL, KEY_FRAME_INTERVAL);
    OH_AVFormat_SetIntValue(format_, OH_MD_KEY_PROFILE, static_cast<int32_t>(OH_AVCProfile::AVC_PROFILE_BASELINE));
    OH_AVFormat_SetIntValue(format_, OH_MD_KEY_TRANSFER_CHARACTERISTICS,
        static_cast<int32_t>(OH_TransferCharacteristic::TRANSFER_CHARACTERISTIC_BT709));
    OH_AVFormat_SetIntValue(format_, OH_MD_KEY_COLOR_PRIMARIES,
        static_cast<int32_t>(OH_ColorPrimary::COLOR_PRIMARY_BT709));
    if (formatInfo->qpMax >= 0) {
        OH_AVFormat_SetIntValue(format_, OH_MD_KEY_VIDEO_ENCODER_QP_MAX, formatInfo->qpMax);
    }
    if (formatInfo->qpMin >= 0) {
        OH_AVFormat_SetIntValue(format_, OH_MD_KEY_VIDEO_ENCODER_QP_MIN, formatInfo->qpMin);
    }
    
    int32_t ret = OH_VideoEncoder_Configure(encoder_, format_);
    if (ret != AV_ERR_OK) {
        // 异常处理
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "OH_VideoEncoder_Configure Failed");
        return ret;
    } else {
        OH_AVFormat_Destroy(format_);
        format_ = nullptr;
    }
    return ret;
}

int32_t OhosVideoEncoder::SetCallback(CodecData *codecData)
{
    // 配置异步回调，调用 OH_VideoEncoder_SetCallback 接口
    OH_AVCodecCallback cb = {&CodecData::DataCallback::OnCodecError, &CodecData::DataCallback::OnCodecFormatChange,
                             &CodecData::DataCallback::OnNeedInputBuffer, &CodecData::DataCallback::OnNewOutputBuffer};
    int32_t ret = OH_VideoEncoder_RegisterCallback(encoder_, cb, codecData);
    if (ret != AV_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "SetCallback Failed");
        return ret;
    }
    return ret;
}

const OhosVideoEncoder::EncodeStat &OhosVideoEncoder::Stat()
{
    return stat_;
}

bool OhosVideoEncoder::CreatNativeWindow(int32_t width, int32_t height)
{
    if (encoder_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "CreatNativeWindow", "encoder_ nullptr");
        return false;
    }
    if (nativeWindow_ != nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "CreatNativeWindow", "nativeWindow_ exist");
        return false;
    }
    int32_t ret = OH_VideoEncoder_GetSurface(encoder_, &nativeWindow_);
    if (ret != AV_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "CreatNativeWindow", "GetSurface failed");
        return false;
    }
    ret = OH_NativeWindow_NativeWindowHandleOpt(nativeWindow_, SET_BUFFER_GEOMETRY, width, height);
    if (ret != AV_ERR_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "CreatNativeWindow", "setNativeWindowOpt failed");
        return false;
    }
    return true;
}

OHNativeWindow *OhosVideoEncoder::GetNativeWindow()
{
    return nativeWindow_;
}

bool OhosVideoEncoder::NativeWindowDestroy()
{
    if (nativeWindow_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "DestroyNativeWindow", "nativeWindow_ nullptr");
        return false;
    } else {
        OH_NativeWindow_DestroyNativeWindow(nativeWindow_);
        nativeWindow_ = nullptr;
    }
    return true;
}

const StrideInfo &OhosVideoEncoder::GetEncoderStride()
{
    if (encoderStride_.wStride == 0 || encoderStride_.hStride == 0) {
        OH_AVFormat *format = OH_VideoEncoder_GetInputDescription(encoder_);
        OH_AVFormat_GetIntValue(format, OH_MD_KEY_VIDEO_STRIDE, &encoderStride_.wStride);
        OH_AVFormat_GetIntValue(format, OH_MD_KEY_VIDEO_SLICE_HEIGHT, &encoderStride_.hStride);
        OH_AVFormat_Destroy(format);
    }
    return encoderStride_;
}

}
}