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
#include "libyuv.h"
#include <cstdint>
#include "yuv_converter.h"

namespace webrtc {
namespace ohos {

std::string YuvConverter::fragmentShader_ = R"delimiter(
#extension GL_OES_EGL_image_external : require
precision highp float;
varying vec2 vTexCoord;
uniform samplerExternalOES texture;
uniform vec2 xUnit;
uniform vec4 coeffs;
void main() {
    gl_FragColor.r = coeffs.a + dot(coeffs.rgb, texture2D(texture, vTexCoord - 1.5 * xUnit).rgb);
    gl_FragColor.g = coeffs.a + dot(coeffs.rgb, texture2D(texture, vTexCoord - 0.5 * xUnit).rgb);
    gl_FragColor.b = coeffs.a + dot(coeffs.rgb, texture2D(texture, vTexCoord + 0.5 * xUnit).rgb);
    gl_FragColor.a = coeffs.a + dot(coeffs.rgb, texture2D(texture, vTexCoord + 1.5 * xUnit).rgb);
})delimiter";

YuvConverter::YuvConverter()
{
    Init();
}

YuvConverter::~YuvConverter()
{
    drawer_->CleanGLResources();
}

void YuvConverter::Init()
{
    converterShaderCallbacks_ = std::make_shared<ShaderCallback>();
    drawer_ = std::make_shared<OhosGLDrawer>(fragmentShader_, converterShaderCallbacks_);
    drawer_->CreateGLResources();
}

rtc::scoped_refptr<webrtc::I420Buffer> YuvConverter::Convert(OHOSVideoBuffer *videoBuffer)
{
    int frameWidth = videoBuffer->width();
    int frameHeight = videoBuffer->height();
    int stride = ((frameWidth + 3) / 4) * 4;
    int uvHeight = (frameHeight + 1) / 2;
    int totalHeight = frameHeight + uvHeight;
    int viewportWidth = stride / 4;

    if (drawer_ == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "YuvConverter", "YuvConverter::drawer_ nullptr");
        return nullptr;
    }
    
    if (!glTextureFrameBuffer_.SetSize(viewportWidth, totalHeight)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "YuvConverter",
            "SetSize Failed %{public}d, %{public}d", videoBuffer->width(), videoBuffer->height());
        return nullptr;
    }
 
    glBindFramebuffer(GL_FRAMEBUFFER, glTextureFrameBuffer_.GetFrameBufferID());
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "YuvConverter", "glBindFramebuffer %{public}s", GetGLErrorString());
    
    float *matrix = videoBuffer->GetVideoFrameBuffer().matrix;
    converterShaderCallbacks_->SetPlanY();
    drawer_->DrawFrame(videoBuffer->GetVideoFrameBuffer(), matrix, frameWidth, frameHeight, 0, 0,
                       viewportWidth, frameHeight);

    converterShaderCallbacks_->SetPlanU();
    drawer_->DrawFrame(videoBuffer->GetVideoFrameBuffer(), matrix, frameWidth, frameHeight, 0, frameHeight,
                       viewportWidth / 2, uvHeight);

    converterShaderCallbacks_->SetPlanV();
    drawer_->DrawFrame(videoBuffer->GetVideoFrameBuffer(), matrix, frameWidth, frameHeight, viewportWidth / 2,
                       frameHeight, viewportWidth / 2, uvHeight);

    uint8_t *yuvP = new uint8_t[viewportWidth * totalHeight * 4 * sizeof(uint8_t)];
    if (yuvP == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "YuvConverter", "glReadPixels malloc failed!");
    }
    glReadPixels(0, 0, viewportWidth, totalHeight, GL_RGBA, GL_UNSIGNED_BYTE, yuvP);
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "YuvConverter", "glReadPixels %{public}s", GetGLErrorString());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    uint8_t *yData = yuvP;
    int32_t ySize = stride * frameHeight;
    uint8_t *uData = yData + ySize;
    uint8_t *vData = uData + stride / 2;
    int32_t uvLineSize = stride / 2;
    int32_t uvStride = stride;

#ifdef DEBUG_SURFACE
    // 落盘测试，播放： .\ffplay.exe -f rawvideo -pixel_format yuv420p -video_size 480x640 -i .\desktop.yuv
    FILE *fp = nullptr;
    fp = fopen("/data/storage/el2/base/files/desktop.yuv", "a");
    // write y
    fwrite(yData, 1, ySize, fp);
    // write u
    uint8_t *src = uData;
    for (int i = 0; i < (uvHeight - 1); i++) {
        fwrite(src, 1, uvLineSize, fp);
        src += uvStride;
    }
    fwrite(src, 1, uvLineSize, fp);
    // write v
    src = vData;
    for (int i = 0; i < (uvHeight - 1); i++) {
        fwrite(src, 1, uvLineSize, fp);
        src += uvStride;
    }
    fwrite(src, 1, uvLineSize, fp);
    fclose(fp);
#endif
    // 构造webrtc::I420Buffer，这里会有memcpy。
    // 用户可根据自己的软编码器或者其他buffer模式后处理函数的入参类型酌情决定是否转换成webrtc::I420Buffer
    rtc::scoped_refptr<webrtc::I420Buffer> i420Buffer = ::webrtc::I420Buffer::Copy(stride, frameHeight, yData, stride,
        uData, uvStride, vData, uvStride);
    delete [] yuvP;
    return i420Buffer;
}

void YuvConverter::ShaderCallback::SetPlanY()
{
    coeffs = yCoeffs;
    stepSize = 1.0f;
}

void YuvConverter::ShaderCallback::SetPlanU()
{
    coeffs = uCoeffs;
    stepSize = 2.0f;
}

void YuvConverter::ShaderCallback::SetPlanV()
{
    coeffs = vCoeffs;
    stepSize = 2.0f;
}

void YuvConverter::ShaderCallback::OnNewShader(std::shared_ptr<ShaderProgram> shader)
{
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "ShaderCallback", "OnNewShader");
}

void YuvConverter::ShaderCallback::OnPrepareShader(std::shared_ptr<ShaderProgram> shader, float *matrix,
    int frameWidth, int frameHeight, int viewportWidth, int viewportHeight)
{
    shader->SetFloat4v("coeffs", coeffs, 4);
    shader->SetFloat2v("xUnit", stepSize * matrix[0] / frameWidth, stepSize * matrix[4] / frameWidth);
}

}
}
