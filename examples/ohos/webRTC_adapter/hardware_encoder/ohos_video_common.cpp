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
#include "ohos_video_common.h"

namespace webrtc {
namespace ohos {

CodecBufferInfo::CodecBufferInfo(uint32_t index, OH_AVBuffer *buff)
{
    createAVBuff_ = false;
    bufferIndex_ = index;
    buff_ = buff;
}

CodecBufferInfo::CodecBufferInfo(const OH_AVCodecBufferAttr &attr)
{
    createAVBuff_ = true;
    buff_ = OH_AVBuffer_Create(attr.size);
    OH_AVBuffer_SetBufferAttr(buff_, &attr);
}

CodecBufferInfo::~CodecBufferInfo()
{
    bufferIndex_ = -1;
    if (createAVBuff_ == true) {
        Destory();
    }
}

void CodecBufferInfo::Destory()
{
    if (buff_ != nullptr) {
        OH_AVBuffer_Destroy(buff_);
        buff_ = nullptr;
    }
}

int32_t CodecBufferInfo::GetBufferIndex()
{
    return bufferIndex_;
}

void CodecBufferInfo::SetBufferIndex(int32_t index)
{
    bufferIndex_ = index;
}

uint8_t *CodecBufferInfo::GetBuff()
{
    return OH_AVBuffer_GetAddr(buff_);
}

OH_AVBuffer *CodecBufferInfo::GetAVBuff()
{
    return buff_;
}

int32_t CodecBufferInfo::SetAttr(OH_AVCodecBufferAttr *attr)
{
    int32_t ret = OH_AVBuffer_SetBufferAttr(buff_, attr);
    return ret;
}

int32_t CodecBufferInfo::GetAttr(OH_AVCodecBufferAttr *attr)
{
    int32_t ret = OH_AVBuffer_GetBufferAttr(buff_, attr);
    return ret;
}

}
}
