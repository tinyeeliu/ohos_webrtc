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
#include "gl_texture_framebuffer.h"
#include "ohos_gl_drawer.h"

namespace webrtc {
namespace ohos {

bool GLTextureFrameBuffer::SetSize(int width, int height)
{
    if (width <= 0 || height <= 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "GLTextureFrameBuffer", "SetSize Failed");
        return false;
    }
    if (width_ == width && height_ == height) {
        return true;
    }
    width_ = width;
    height_ = height;
    if (textureID_ == 0) {
        // 生成 texture
        glGenTextures(1, &textureID_);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "GLTextureFrameBuffer", "glGenTextures %{public}s",
            GetGLErrorString());
    }
    if (fbo_ == 0) {
        glGenFramebuffers(1, &fbo_);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "GLTextureFrameBuffer", "glGenFramebuffers %{public}s",
            GetGLErrorString());
    }
    
    glActiveTexture(GL_TEXTURE0);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "GLTextureFrameBuffer", "glActiveTexture %{public}s",
        GetGLErrorString());
    glBindTexture(GL_TEXTURE_2D, textureID_);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "GLTextureFrameBuffer", "glBindTexture %{public}s",
        GetGLErrorString());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "GLTextureFrameBuffer", "glTexImage2D %{public}s",
        GetGLErrorString());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "GLTextureFrameBuffer", "glTexParameteri %{public}s",
        GetGLErrorString());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "GLTextureFrameBuffer", "glTexParameteri %{public}s",
        GetGLErrorString());

    // fbo 绑定
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "GLTextureFrameBuffer", "glBindFramebuffer %{public}s",
        GetGLErrorString());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID_, 0);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "GLTextureFrameBuffer", "glFramebufferTexture2D %{public}s",
        GetGLErrorString());
    
    //判断 fbo 有效性
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "GLTextureFrameBuffer",
            "glCheckFramebufferStatus failed!");
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        return false;
    }
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

GLuint GLTextureFrameBuffer::GetFrameBufferID()
{
    return fbo_;
}

int GLTextureFrameBuffer::GetWidth()
{
    return width_;
}

int GLTextureFrameBuffer::GetHeight()
{
    return height_;
}
}
}
