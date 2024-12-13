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
#include "hilog/log.h"
#include "ohos_codec_data.h"

namespace webrtc {
namespace ohos {

#define TWO 2
#define THREE 3
#define FOUR 4

CodecData::~CodecData()
{
    CodecData::ShutDown();
    formatInfo_ = nullptr;
    inputFrameCount_ = 0;
    outputFrameCount_ = 0;
}

void CodecData::Start()
{
    // 输入输出公用同一个 running flag？
    isRunning_.store(true);
    inputCond_.notify_all();
    outputCond_.notify_all();
}

void CodecData::ShutDown()
{
    isRunning_.store(false);
    inputCond_.notify_all();
    outputCond_.notify_all();
}

int32_t CodecData::I420Copy(const uint8_t* srcY, int srcStrideY, const uint8_t* srcU, int srcStrideU,
    const uint8_t* srcV, int srcStrideV, uint8_t* dstY, int dstStrideY, uint8_t* dstU, int dstStrideU, uint8_t* dstV,
    int dstStrideV, int width, int height)
{
    int halfWidth = (width + 1) >> 1;
    int halfHeight = (height + 1) >> 1;
    if ((!srcY && dstY) || !srcU || !srcV || !dstU || !dstV || width <= 0 || height == 0) {
        return -1;
    }
    if (height < 0) {
        height = -height;
        halfHeight = (height + 1) >> 1;
        srcY = srcY + (height - 1) * srcStrideY;
        srcU = srcU + (halfHeight - 1) * srcStrideU;
        srcV = srcV + (halfHeight - 1) * srcStrideV;
        srcStrideY = -srcStrideY;
        srcStrideU = -srcStrideU;
        srcStrideV = -srcStrideV;
    }
    if (dstY) {
        CodecData::CopyPlane(srcY, srcStrideY, dstY, dstStrideY, width, height);
    }
    CodecData::CopyPlane(srcU, srcStrideU, dstU, dstStrideU, halfWidth, halfHeight);
    CodecData::CopyPlane(srcV, srcStrideV, dstV, dstStrideV, halfWidth, halfHeight);
    return 0;
}

void CodecData::CopyPlane(const uint8_t* srcData, int srcStride, uint8_t* dstData, int dstStride, int width, int height)
{
    if (width <= 0 || height == 0) {
        return;
    }
    if (height < 0) {
        height = -height;
        dstData = dstData + (height - 1) * dstStride;
        dstStride = -dstStride;
    }
    if (srcStride == width && dstStride == width) {
        width *= height;
        height = 1;
        srcStride = dstStride = 0;
    }
    if (srcData == dstData && srcStride == dstStride) {
        return;
    }
    for (int index = 0; index < height; ++index) {
        std::memcpy(dstData, srcData, width);
        srcData += srcStride;
        dstData += dstStride;
    }
}

int32_t CodecData::InputData(const CodecData::I420Info &i420Info, int32_t &bufferIndex,
    const StrideInfo &encoderStride, OH_AVCodecBufferAttr attr, std::chrono::milliseconds time)
{
    CodecBufferInfo bufferInfo;
    {
        std::unique_lock<std::mutex> lock(this->inputMutex_);
        bool condRet = this->inputCond_.wait_for(lock, time, [this]() {
            return !this->inputBufferInfoQueue_.empty() || !isRunning_.load();
        });
        if (!isRunning_.load()) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "CoderData", "CoderData !isRunning");
            return AV_ERR_IO;
        }
        if (this->inputBufferInfoQueue_.empty()) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "CoderData",
                "inputBufferInfoQueue_ is empty, overtime, cond ret: %{public}d", condRet);
            return AV_ERR_TIMEOUT;
        }
        bufferInfo = this->inputBufferInfoQueue_.front();
        this->inputBufferInfoQueue_.pop();
        this->inputFrameCount_++;
    }
    
    int32_t dstStrideY = encoderStride.wStride;
    int32_t dstStrideUV = dstStrideY / TWO;
    uint8_t *dstY = bufferInfo.GetBuff();
    uint8_t *dstU = dstY + (dstStrideY * i420Info.height);
    uint8_t *dstV = dstU + (dstStrideUV * i420Info.height / TWO);
    int32_t ret = CodecData::I420Copy(i420Info.srcY, i420Info.srcStrideY, i420Info.srcU, i420Info.srcStrideU,
        i420Info.srcV, i420Info.srcStrideV, dstY, dstStrideY, dstU, dstStrideUV,
        dstV, dstStrideUV, i420Info.width, i420Info.height);
    if (ret != 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "CoderData", "libyuv::I420Copy failed");
        return AV_ERR_IO;
    }
    attr.size = dstStrideY * i420Info.height * THREE/ TWO; //attr.size为编码器计算
    attr.offset = 0; //offset默认为0
    bufferInfo.SetAttr(&attr);
    if (attr.flags & AVCODEC_BUFFER_FLAGS_EOS) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CoderData", "Catch EOS");
    }
    bufferIndex = bufferInfo.GetBufferIndex();
    return AV_ERR_OK;
}

int32_t CodecData::OutputData(CodecBufferInfo &info)
{
    std::unique_lock<std::mutex> lock(this->outputMutex_);
    this->outputCond_.wait(lock,
        [this]() { return !this->outputBufferInfoQueue_.empty() || !isRunning_.load(); });
    if (!isRunning_.load()) {
        return AV_ERR_IO;
    }
    if (this->outputBufferInfoQueue_.empty()) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "CoderData",
            "OutputData outputBufferInfoQueue_.empty()");
        return AV_ERR_UNKNOWN;
    }
    info = this->outputBufferInfoQueue_.front();
    this->outputBufferInfoQueue_.pop();
    this->outputFrameCount_++;
    return AV_ERR_OK;
}

void CodecData::DataCallback::OnCodecError(OH_AVCodec *videoEnc, int32_t errorCode, void *userData)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "CodecCallback", "AVCodec errorCode %{public}d", errorCode);
    (void)videoEnc;
    (void)errorCode;
    (void)userData;
}

void CodecData::DataCallback::OnCodecFormatChange(OH_AVCodec *videoEnc, OH_AVFormat *format, void *userData)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "CodecCallback", "AVCodec FormatChange");
    (void)videoEnc;
    (void)format;
    (void)userData;
}

void CodecData::DataCallback::OnNeedInputBuffer(OH_AVCodec *codec, uint32_t index, OH_AVBuffer *buffer, void *userData)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosVideoEncoder", "OnNeedInputBuffer enter");
    if (userData == nullptr) {
        return;
    }
    (void)codec;
    CodecData *codecUserData = static_cast<CodecData *>(userData);
    std::unique_lock<std::mutex> lock(codecUserData->inputMutex_);
    codecUserData->inputBufferInfoQueue_.emplace(index, buffer);
    codecUserData->inputCond_.notify_all();
}

void CodecData::DataCallback::OnNewOutputBuffer(OH_AVCodec *codec, uint32_t index, OH_AVBuffer *buffer, void *userData)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosVideoEncoder", "OnNewOutputBuffer enter");
    if (userData == nullptr) {
        return;
    }
    (void)codec;
    CodecData *codecUserData = static_cast<CodecData *>(userData);
    std::unique_lock<std::mutex> lock(codecUserData->outputMutex_);
    codecUserData->outputBufferInfoQueue_.emplace(index, buffer);
    codecUserData->outputCond_.notify_all();
}

}
}