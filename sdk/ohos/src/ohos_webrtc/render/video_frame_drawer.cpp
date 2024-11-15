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

#include "gl_drawer.h"
#include "video_frame_drawer.h"

#include "rtc_base/logging.h"

#include <GLES3/gl3.h>

namespace webrtc {

void VideoFrameDrawer::DrawFrame(const VideoFrame& frame, GlDrawer& drawer, const Matrix4f& texMatrix)
{
    DrawFrame(frame, drawer, texMatrix, 0, 0, frame.width(), frame.height());
}

void VideoFrameDrawer::DrawFrame(
    const VideoFrame& frame, GlDrawer& drawer, const Matrix4f& texMatrix, int viewportX, int viewportY,
    int viewportWidth, int viewportHeight)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (frame.is_texture()) {
        auto buffer = static_cast<TextureBuffer*>(frame.video_frame_buffer().get());
        DrawTexture(
            rtc::scoped_refptr<TextureBuffer>(buffer), drawer, texMatrix, frame.width(), frame.width(), viewportX,
            viewportY, viewportWidth, viewportHeight);
    } else {
        if (yuvTextures_.size() == 0) {
            yuvTextures_.resize(3);

            glGenTextures(3, yuvTextures_.data());
            for (uint32_t i = 0; i < yuvTextures_.size(); i++) {
                glBindTexture(GL_TEXTURE_2D, yuvTextures_[i]);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
        }

        auto buffer = frame.video_frame_buffer()->ToI420();

        // int strides[] = {buffer->StrideY(), buffer->StrideU(), buffer->StrideV()};
        const uint8_t* planes[] = {buffer->DataY(), buffer->DataU(), buffer->DataV()};
        int planeWidths[] = {buffer->width(), buffer->width() / 2, buffer->width() / 2};
        int planeHeights[] = {buffer->height(), buffer->height() / 2, buffer->height() / 2};

        for (uint32_t i = 0; i < 3; i++) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, yuvTextures_[i]);

            glTexImage2D(
                GL_TEXTURE_2D, 0, GL_LUMINANCE, planeWidths[i], planeHeights[i], 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                planes[i]);
        }

        drawer.DrawYuv(
            yuvTextures_, texMatrix, frame.width(), frame.height(), viewportX, viewportY, viewportWidth,
            viewportHeight);
    }
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

void VideoFrameDrawer::DrawTexture(
    rtc::scoped_refptr<TextureBuffer> buffer, GlDrawer& drawer, const Matrix4f& texMatrix, int frameWidth,
    int frameHeight, int viewportX, int viewportY, int viewportWidth, int viewportHeight)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    
    if (!buffer) {
        RTC_LOG(LS_ERROR) << "Buffer is null";
        return;
    }

    auto textureData = buffer->GetTexture();
    if (!textureData) {
        RTC_LOG(LS_ERROR) << "Buffer is released";
        return;
    }
    
    textureData->Lock();

    switch (textureData->GetType()) {
        case TextureData::Type::OES:
            drawer.DrawOes(
                textureData->GetId(), texMatrix, frameWidth, frameHeight, viewportX, viewportY, viewportWidth,
                viewportHeight);
            break;
        case TextureData::Type::RGB:
            drawer.DrawRgb(
                textureData->GetId(), texMatrix, frameWidth, frameHeight, viewportX, viewportY, viewportWidth,
                viewportHeight);
            break;
        default:
            RTC_LOG(LS_ERROR) << "Unknown texture type: " << textureData->GetType();
            break;
    }

    textureData->Unlock();
}

} // namespace webrtc
