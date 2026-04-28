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

#include "texture_buffer.h"
#include "../render/yuv_converter.h"

#include "api/make_ref_counted.h"
#include "rtc_base/logging.h"

#include <cstdint>

namespace webrtc {

rtc::scoped_refptr<TextureBuffer>
TextureBuffer::Create(std::weak_ptr<TextureData> texture, int width, int height, const Matrix& transformMatrix)
{
    return rtc::make_ref_counted<TextureBuffer>(texture, width, height, transformMatrix);
}

TextureBuffer::TextureBuffer(std::weak_ptr<TextureData> texture, int width, int height, const Matrix& transformMatrix)
    : texture_(texture), width_(width), height_(height), transformMatrix_(transformMatrix)
{
    RTC_DLOG(LS_VERBOSE) << "TextureBuffer ctor";
}

TextureBuffer::~TextureBuffer()
{
    RTC_DLOG(LS_VERBOSE) << "TextureBuffer dtor";
}

rtc::scoped_refptr<I420BufferInterface> TextureBuffer::ToI420()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    auto texture = texture_.lock();
    if (!texture) {
        RTC_LOG(LS_ERROR) << __FUNCTION__ << " failed: texture expired";
        return nullptr;
    }

    rtc::Thread* toI420Handler = texture->getToI420Handler();
    YuvConverter* yuvConverter = texture->getYuvConverter();
    if (!toI420Handler || !yuvConverter) {
        return nullptr;
    }

    auto textureBuffer = rtc::scoped_refptr<TextureBuffer>(this);
    return toI420Handler->BlockingCall([yuvConverter, textureBuffer] { return yuvConverter->Convert(textureBuffer); });
}

rtc::scoped_refptr<VideoFrameBuffer> TextureBuffer::CropAndScale(
    int offset_x, int offset_y, int crop_width, int crop_height, int scaled_width, int scaled_height)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (crop_width <= 0 || crop_height <= 0 || scaled_width <= 0 || scaled_height <= 0) {
        RTC_LOG(LS_ERROR) << "Invalid crop/scale size.";
        return nullptr;
    }
    const int64_t crop_right = static_cast<int64_t>(offset_x) + static_cast<int64_t>(crop_width);
    const int64_t crop_bottom = static_cast<int64_t>(offset_y) + static_cast<int64_t>(crop_height);
    if (offset_x < 0 || offset_y < 0 || crop_right > static_cast<int64_t>(width_) ||
        crop_bottom > static_cast<int64_t>(height_)) {
        RTC_LOG(LS_ERROR) << "Crop rect out of bounds.";
        return nullptr;
    }

    if (!texture_.lock()) {
        RTC_LOG(LS_ERROR) << __FUNCTION__ << " failed: texture expired";
        return nullptr;
    }

    if (offset_x == 0 && offset_y == 0 && crop_width == width_ && crop_height == height_ && scaled_width == width_ &&
        scaled_height == height_) {
        return rtc::scoped_refptr<VideoFrameBuffer>(this);
    }

    const int cropYFromBottom =
        static_cast<int>(static_cast<int64_t>(height_) - crop_bottom);
    Matrix cropMatrix;
    cropMatrix.PreTranslate(offset_x / static_cast<float>(width_), cropYFromBottom / static_cast<float>(height_));
    cropMatrix.PreScale(
        crop_width / static_cast<float>(width_), crop_height / static_cast<float>(height_), 0.0f, 0.0f);

    Matrix newMatrix = transformMatrix_;
    newMatrix.PreConcat(cropMatrix);
    return TextureBuffer::Create(texture_, scaled_width, scaled_height, newMatrix);
}

} // namespace webrtc
