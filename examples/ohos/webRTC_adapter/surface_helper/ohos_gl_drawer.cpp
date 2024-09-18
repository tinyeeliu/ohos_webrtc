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
#include "ohos_gl_drawer.h"

namespace webrtc {
namespace ohos {

#define CASE_GL_STR(value)       \
    case value:                  \
        return #value
const char *GetGLErrorString()
{
    GLenum error = glGetError();
    switch (error) {
        CASE_GL_STR(GL_NO_ERROR);
        CASE_GL_STR(GL_INVALID_ENUM);
        CASE_GL_STR(GL_OUT_OF_MEMORY);
        CASE_GL_STR(GL_INVALID_VALUE);
        CASE_GL_STR(GL_INVALID_OPERATION);
        CASE_GL_STR(GL_STACK_OVERFLOW);
        CASE_GL_STR(GL_STACK_UNDERFLOW);
        CASE_GL_STR(GL_INVALID_FRAMEBUFFER_OPERATION);
        CASE_GL_STR(GL_CONTEXT_LOST);
    default:
        return "EGL_UNKOWN_ERROR";
    }
}
#undef CASE_GL_STR

GLfloat vertices[] = {
    // positions       // texture coords
    -1.0f, 1.0f,  0.0f, 0.0f, 1.0f, // top left
    -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, // bottom left
    1.0f,  -1.0f, 0.0f, 1.0f, 0.0f, // bottom right
    1.0f,  1.0f,  0.0f, 1.0f, 1.0f  // top right
};

GLuint indices[] = {
    0, 1, 2, // first triangle
    0, 2, 3  // second triangle
};

static std::string OES_FRAGMENT_SHADER = R"delimiter(
#extension GL_OES_EGL_image_external : require
precision highp float;
varying vec2 vTexCoord;
uniform samplerExternalOES texture;

void main()
{
    gl_FragColor = texture2D(texture, vTexCoord).rgba;
}
)delimiter";

static std::string RGB_FRAGMENT_SHADER = R"delimiter(
precision highp float;
varying vec2 vTexCoord;
uniform sampler2D texture;

void main()
{
    gl_FragColor = texture2D(texture, vTexCoord).rgba;
}
)delimiter";

static std::string YUV_FRAGMENT_SHADER = R"delimiter(
precision highp float;
varying vec2 vTexCoord;
uniform sampler2D tex_y;
uniform sampler2D tex_u;
uniform sampler2D tex_v;

vec4 sample(vec2 p) {
    float y = texture2D(tex_y, p).r * 1.16438;
    float u = texture2D(tex_u, p).r;
    float v = texture2D(tex_v, p).r;
    return vec4(y + 1.59603 * v - 0.874202,
    y - 0.391762 * u - 0.812968 * v + 0.531668,
    y + 2.01723 * u - 1.08563, 1);
}

void main()
{
    gl_FragColor = sample(vTexCoord);
}
)delimiter";

std::map<OHOSVideoBuffer::TextureBuffer::Type, std::string> OhosGLDrawer::shaderMap_ = {
    {OHOSVideoBuffer::TextureBuffer::Type::OES, OES_FRAGMENT_SHADER},
    {OHOSVideoBuffer::TextureBuffer::Type::RGB, RGB_FRAGMENT_SHADER},
    {OHOSVideoBuffer::TextureBuffer::Type::YUV, YUV_FRAGMENT_SHADER}
};

OhosGLDrawer::OhosGLDrawer(std::string fragmentShader, std::shared_ptr<ShaderCallbacks> shaderCallbacks)
{
    fragmentShader_ = fragmentShader;
    shaderCallbacks_ = shaderCallbacks;
}

OhosGLDrawer::~OhosGLDrawer()
{
    CleanGLResources();
}

bool OhosGLDrawer::PrePareShader(OHOSVideoBuffer::TextureBuffer::Type frameType, float *matrix, int frameWidth,
    int frameHeight, int viewportX, int viewportY, int viewportWidth, int viewportHeight)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosGLDrawer", "ShaderProgram Enter");
    if (frameType != currentFrameType_ || shaderProgram_ == nullptr) {
        currentFrameType_ = frameType;
        if (fragmentShader_.empty()) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_DOMAIN, "OhosGLDrawer", "fragmentShader_ empty");
            fragmentShader_ = shaderMap_[frameType];
        }
        shaderProgram_ = std::make_unique<ShaderProgram>(g_defaultVertexShader, fragmentShader_);
        
        if (shaderCallbacks_ != nullptr) {
            shaderCallbacks_->OnNewShader(shaderProgram_);
        }
    } else {
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OhosGLDrawer", "ShaderProgram Exist");
    }
    if (!shaderProgram_->Valid()) {
        return false;
    }
    if (!shaderProgram_->Use()) {
        return false;
    }
    if (frameType == OHOSVideoBuffer::TextureBuffer::Type::YUV) {
        shaderProgram_->SetInt("tex_y", 0);
        shaderProgram_->SetInt("tex_u", 1);
        shaderProgram_->SetInt("tex_v", 2);
    } else {
        shaderProgram_->SetInt("texture", 0);
    }
    shaderProgram_->SetMatrix4v("matTransform", matrix, 16, false);

    if (shaderCallbacks_ != nullptr) {
        shaderCallbacks_->OnPrepareShader(shaderProgram_, matrix, frameWidth, frameHeight,
            viewportWidth, viewportHeight);
    }
    return true;
}

void OhosGLDrawer::CreateGLResources()
{
    glGenVertexArrays(1, &vertexArrayObject_);
    glGenBuffers(1, &vertexBufferObject_);

    glBindVertexArray(vertexArrayObject_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void OhosGLDrawer::CleanGLResources()
{
    glDeleteVertexArrays(1, &vertexArrayObject_);
    glDeleteBuffers(1, &vertexBufferObject_);
}

void OhosGLDrawer::DrawFrame(OHOSVideoBuffer::TextureBuffer buffer, float *matrix, int frameWidth,
    int frameHeight, int viewportX, int viewportY, int viewportWidth, int viewportHeight)
{
    if (frameWidth <= 0 || frameHeight <= 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "DrawFrame", "frameWidth OR frameHeight error");
        return;
    }
    if (!PrePareShader(buffer.type, matrix, frameWidth, frameHeight, viewportX, viewportY, viewportWidth,
        viewportHeight)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "DrawFrame", "ShaderProgram Create Failed");
        return;
    }
    
    switch (buffer.type) {
        case OHOSVideoBuffer::TextureBuffer::Type::OES:
            DrawOES(buffer.textureID, matrix, frameWidth, frameHeight,
                    viewportX, viewportY, viewportWidth, viewportHeight);
            break;
        case OHOSVideoBuffer::TextureBuffer::Type::RGB:
            DrawRgb(buffer.textureID, matrix, frameWidth, frameHeight,
                    viewportX, viewportY, viewportWidth, viewportHeight);
            break;
        case OHOSVideoBuffer::TextureBuffer::Type::YUV:
            DrawYuv(buffer.yuvTexture, matrix, frameWidth, frameHeight, viewportX,
                    viewportY, viewportWidth, viewportHeight);
            break;
        default:
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "UNKOWN Frame Type");
            break;
    }
}

void OhosGLDrawer::DrawOES(GLuint textureID, float *matrix, int frameWidth, int frameHeight,
                           int viewportX, int viewportY, int viewportWidth, int viewportHeight)
{
    if (!glIsTexture(textureID)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "glIsTexture false");
        return;
    }
    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureID);
    glBindVertexArray(vertexArrayObject_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, indices);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    glBindVertexArray(0);
}

void OhosGLDrawer::DrawRgb(int textureID, float *matrix, int frameWidth, int frameHeight,
                           int viewportX, int viewportY, int viewportWidth, int viewportHeight)
{
    if (!glIsTexture(textureID)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosVideoEncoder", "glIsTexture false");
        return;
    }
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glBindVertexArray(vertexArrayObject_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, indices);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}

void OhosGLDrawer::DrawYuv(int *yuvTextures, float *matrix, int frameWidth, int frameHeight,
                           int viewportX, int viewportY, int viewportWidth, int viewportHeight)
{
    for (int i = 0; i < 3; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, yuvTextures[i]);
    }
    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    for (int i = 0; i < 3; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
}
}