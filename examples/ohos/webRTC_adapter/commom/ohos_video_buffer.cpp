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
#include "ohos_video_buffer.h"
#include "api/make_ref_counted.h"
#include "surface_helper/yuv_converter.h"

namespace webrtc {
namespace ohos {
rtc::scoped_refptr<OHOSVideoBuffer> OHOSVideoBuffer::Creat(int width, int height, const TextureBuffer& textureBuffer,
                                                           VideoSourceType sourceType)
{
    return rtc::make_ref_counted<OHOSVideoBuffer>(width, height, textureBuffer, sourceType);
}

rtc::scoped_refptr<VideoFrameBuffer> OHOSVideoBuffer::CropAndScale(int crop_x, int crop_y, int crop_width,
                                                                   int crop_height, int scale_width, int scale_height)
{
    return nullptr;
}

OHOSVideoBuffer::TextureBuffer OHOSVideoBuffer::GetVideoFrameBuffer() const
{
    return textureBuffer_;
}

OHOSVideoBuffer::OHOSVideoBuffer(int width, int height, const TextureBuffer& textureBuffer, VideoSourceType sourceType)
    : width_(width), height_(height), textureBuffer_(textureBuffer), sourceType_(sourceType)
{}

VideoFrameBuffer::Type OHOSVideoBuffer::type() const
{
    return Type::kNative;
}

int OHOSVideoBuffer::width() const
{
    return width_;
}

int OHOSVideoBuffer::height() const
{
    return height_;
}

void OHOSVideoBuffer::SetConverter(std::shared_ptr<YuvConverter> converter)
{
    if (converter != nullptr) {
        converter_ = converter;
    } else {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "SetConverter", "nullptr");
    }
}

rtc::scoped_refptr<I420BufferInterface> OHOSVideoBuffer::ToI420()
{
    return converter_->Convert(this);
}

OHOSVideoBuffer::VideoSourceType OHOSVideoBuffer::GetSourceType()
{
    return sourceType_;
}

}   // namespace ohos
}   // namespace webrtc
