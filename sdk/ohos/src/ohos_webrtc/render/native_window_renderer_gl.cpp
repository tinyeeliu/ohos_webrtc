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

#include "native_window_renderer_gl.h"
#include "egl_config_attributes.h"

#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <native_buffer/native_buffer.h>

#include "render/gl_drawer.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace adapter {

std::unique_ptr<NativeWindowRendererGl>
NativeWindowRendererGl::Create(ohos::NativeWindow window, std::shared_ptr<EglContext> sharedContext)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (window.IsEmpty()) {
        return nullptr;
    }

    return std::unique_ptr<NativeWindowRendererGl>(
        new NativeWindowRendererGl(std::move(window), sharedContext, "native-window-renderer"));
}

std::unique_ptr<NativeWindowRendererGl> NativeWindowRendererGl::Create(
    ohos::NativeWindow window, std::shared_ptr<EglContext> sharedContext, const std::string& threadName)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (window.IsEmpty()) {
        return nullptr;
    }

    return std::unique_ptr<NativeWindowRendererGl>(
        new NativeWindowRendererGl(std::move(window), sharedContext, threadName));
}

NativeWindowRendererGl::NativeWindowRendererGl(
    ohos::NativeWindow window, std::shared_ptr<EglContext> sharedContext, const std::string& threadName)
    : NativeWindowRenderer(std::move(window)),
      thread_(rtc::Thread::Create()),
      textureDrawer_(std::make_unique<GlGenericDrawer>()),
      videoFrameDrawer_(std::make_unique<VideoFrameDrawer>())
{
    // [height] is before [width]
    int32_t ret = OH_NativeWindow_NativeWindowHandleOpt(window_.Raw(), GET_BUFFER_GEOMETRY, &height_, &width_);
    if (ret != 0) {
        RTC_LOG(LS_ERROR) << "Failed to get buffer geometry: " << ret;
    }
    RTC_DLOG(LS_VERBOSE) << "Window geometry: " << width_ << "x" << height_;

    ret = OH_NativeWindow_NativeWindowHandleOpt(window_.Raw(), GET_USAGE, &usage_);
    if (ret != 0) {
        RTC_LOG(LS_ERROR) << "Failed to get usage: " << ret;
    }
    RTC_DLOG(LS_VERBOSE) << "Window usage: " << usage_;

    ret = OH_NativeWindow_NativeWindowHandleOpt(window_.Raw(), GET_FORMAT, &format_);
    if (ret != 0) {
        RTC_LOG(LS_ERROR) << "Failed to get format: " << ret;
    }
    RTC_DLOG(LS_VERBOSE) << "Window format: " << format_;

    int32_t stride = 0;
    ret = OH_NativeWindow_NativeWindowHandleOpt(window_.Raw(), GET_STRIDE, &stride);
    if (ret != 0) {
        RTC_LOG(LS_ERROR) << "Failed to get stride: " << ret;
    }
    RTC_DLOG(LS_VERBOSE) << "Window stride: " << stride;

    ret = OH_NativeWindow_NativeWindowHandleOpt(window_.Raw(), GET_TRANSFORM, &transform_);
    if (ret != 0) {
        RTC_LOG(LS_ERROR) << "Failed to get transform: " << ret;
    }
    RTC_DLOG(LS_VERBOSE) << "Window transform: " << transform_;

    thread_->SetName(threadName, this);
    thread_->Start();
    thread_->BlockingCall([this, sharedContext] {
        eglEnv_ = EglEnv::Create(sharedContext, EglConfigAttributes::RGBA);
        eglEnv_->CreateWindowSurface(window_);
        eglEnv_->MakeCurrent();
    });
}

NativeWindowRendererGl::~NativeWindowRendererGl()
{
    thread_->Stop();
}

void NativeWindowRendererGl::OnFrame(const VideoFrame& frame)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " this=" << this;
    RTC_DLOG(LS_VERBOSE) << "render frame, id=" << frame.id() << " size=" << frame.width() << "x" << frame.height()
                         << ", timestamp=" << frame.timestamp_us();

    if (!frame.video_frame_buffer()) {
        RTC_LOG(LS_ERROR) << "Buffer is null";
        return;
    }

    thread_->PostTask([this, frame = frame] {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        drawMatrix_ = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
        videoFrameDrawer_->DrawFrame(
            frame, *textureDrawer_, drawMatrix_, 0, 0, eglEnv_->GetSurfaceWidth(), eglEnv_->GetSurfaceHeight());
        eglEnv_->SwapBuffers();
    });
}

void NativeWindowRendererGl::OnDiscardedFrame()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

void NativeWindowRendererGl::OnConstraintsChanged(const webrtc::VideoTrackSourceConstraints& constraints)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

} // namespace adapter
} // namespace webrtc
