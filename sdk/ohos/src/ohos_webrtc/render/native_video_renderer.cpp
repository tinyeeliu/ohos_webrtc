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

#include "native_video_renderer.h"
#include "media_source.h"
#include "media_stream.h"
#include "media_stream_track.h"
#include "render/native_window_renderer_gl.h"
#include "render/native_window_renderer_raster.h"

#include "rtc_base/logging.h"

namespace webrtc {

using namespace ohos;
using namespace adapter;
using namespace Napi;

class NativeVideoRenderer : public rtc::VideoSinkInterface<VideoFrame> {
public:
    NativeVideoRenderer(std::unique_ptr<NativeWindowRenderer> renderer) : renderer_(std::move(renderer)) {}
    ~NativeVideoRenderer() override = default;

    void SetFirstFrameRenderedCallback(std::function<void()> callback)
    {
        std::lock_guard<std::mutex> lock(cbMut_);
        onFirstFrameRenderedCallback_ = callback;
    }

    void
    SetFrameResolutionChangedCallback(std::function<void(int32_t width, int32_t height, int32_t rotation)> callback)
    {
        std::lock_guard<std::mutex> lock(cbMut_);
        onFrameResolutionChangedCallback_ = callback;
    }

    int32_t SetMirrorHorizontally(bool mirror)
    {
        return renderer_->SetMirrorHorizontally(mirror);
    }

    int32_t SetMirrorVertically(bool mirror)
    {
        return renderer_->SetMirrorVertically(mirror);
    }

    int32_t SetScalingMode(NativeWindowRenderer::ScalingMode scaleMode)
    {
        return renderer_->SetScalingMode(scaleMode);
    }

protected:
    void OnFrame(const VideoFrame& frame) override
    {
        UpdateFrameDimensionsAndReportEvents(frame);
        renderer_->RenderFrame(frame);
    }

    void OnDiscardedFrame() override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    }

    void OnConstraintsChanged(const webrtc::VideoTrackSourceConstraints& constraints) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    }

protected:
    void UpdateFrameDimensionsAndReportEvents(const VideoFrame& frame);

private:
    std::unique_ptr<NativeWindowRenderer> renderer_;

    bool isFirstFrameRendered_{false};
    int32_t rotatedFrameWidth_{0};
    int32_t rotatedFrameHeight_{0};
    int32_t frameRotation_{0};

    std::mutex cbMut_;
    std::function<void()> onFirstFrameRenderedCallback_;
    std::function<void(int32_t width, int32_t height, int32_t rotation)> onFrameResolutionChangedCallback_;
};

void NativeVideoRenderer::UpdateFrameDimensionsAndReportEvents(const VideoFrame& frame)
{
    std::lock_guard<std::mutex> lock(cbMut_);

    if (!isFirstFrameRendered_) {
        isFirstFrameRendered_ = true;
        RTC_DLOG(LS_INFO) << __FUNCTION__ << "Reporting first rendered frame.";
        if (onFirstFrameRenderedCallback_) {
            onFirstFrameRenderedCallback_();
        }
    }

    int32_t rotatedFrameWidth = (frame.rotation() % kVideoRotation_180 == 0) ? frame.width() : frame.height();
    int32_t rotatedFrameHeight = (frame.rotation() % kVideoRotation_180 == 0) ? frame.height() : frame.width();
    int32_t frameRotation = frame.rotation();
    if (rotatedFrameWidth_ != rotatedFrameWidth || rotatedFrameHeight_ != rotatedFrameHeight ||
        frameRotation_ != frameRotation)
    {
        RTC_DLOG(LS_INFO) << __FUNCTION__ << "Reporting frame resolution changed to " << frame.width() << "x"
                          << frame.height() << " with rotation " << frameRotation;

        rotatedFrameWidth_ = rotatedFrameWidth;
        rotatedFrameHeight_ = rotatedFrameHeight;
        frameRotation_ = frameRotation;

        if (onFrameResolutionChangedCallback_) {
            onFrameResolutionChangedCallback_(frame.width(), frame.height(), frameRotation);
        }
    }
}

void NapiNativeVideoRenderer::Init(Napi::Env env, Napi::Object exports)
{
    Function func = DefineClass(
        env, kClassName,
        {
            InstanceAccessor<&NapiNativeVideoRenderer::GetSurfaceId>(kAttributeNameSurfaceId),
            InstanceAccessor<&NapiNativeVideoRenderer::GetSharedContext>(kAttributeNameSharedContext),
            InstanceAccessor<&NapiNativeVideoRenderer::GetVideoTrack>(kAttributeNameVideoTrack),
            InstanceMethod<&NapiNativeVideoRenderer::SetVideoTrack>(kMethodNameSetVideoTrack),
            InstanceMethod<&NapiNativeVideoRenderer::SetMirror>(kMethodNameSetMirror),
            InstanceMethod<&NapiNativeVideoRenderer::SetMirrorVertically>(kMethodNameSetMirrorVertically),
            InstanceMethod<&NapiNativeVideoRenderer::SetScalingMode>(kMethodNameSetScalingMode),
            InstanceMethod<&NapiNativeVideoRenderer::Release>(kMethodNameRelease),
            InstanceMethod<&NapiNativeVideoRenderer::ToJson>(kMethodNameToJson),
            InstanceAccessor<&NapiNativeVideoRenderer::GetEventHandler, &NapiNativeVideoRenderer::SetEventHandler>(
                kAttributeNameOnFirstFrameRendered, napi_default, (void*)kEventNameFirstFrameRendered),
            InstanceAccessor<&NapiNativeVideoRenderer::GetEventHandler, &NapiNativeVideoRenderer::SetEventHandler>(
                kAttributeNameOnFrameResolutionChanged, napi_default, (void*)kEventNameFrameResolutionChanged),
        });
    exports.Set(kClassName, func);

    Constructor() = Persistent(func);
}

Napi::FunctionReference& NapiNativeVideoRenderer::Constructor()
{
    static Napi::FunctionReference constructor;
    return constructor;
}

NapiNativeVideoRenderer::NapiNativeVideoRenderer(const Napi::CallbackInfo& info)
    : NapiEventTarget<NapiNativeVideoRenderer>(info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 1 || !info[0].IsString()) {
        NAPI_THROW_VOID(Error::New(info.Env(), "Invalid arguments"));
    }

    surfaceId_ = info[0].As<String>().Utf8Value();
    if (surfaceId_->empty()) {
        NAPI_THROW_VOID(Error::New(info.Env(), "Invalid arguments"));
    }

    if (info.Length() > 1 && info[1].IsObject()) {
        auto jsSharedContext = info[1].As<Object>();
        auto napiSharedContext = NapiEglContext::Unwrap(jsSharedContext);
        if (napiSharedContext) {
            sharedContext_ = napiSharedContext->Get();
        }
    } else {
        sharedContext_ = EglEnv::GetDefault().GetContext();
    }

    if (!Init()) {
        NAPI_THROW_VOID(Error::New(info.Env(), "Failed to init"));
    }
}

NapiNativeVideoRenderer::~NapiNativeVideoRenderer()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

Napi::Value NapiNativeVideoRenderer::GetSurfaceId(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (surfaceId_) {
        return String::New(info.Env(), *surfaceId_);
    }

    return info.Env().Undefined();
}

Napi::Value NapiNativeVideoRenderer::GetSharedContext(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (sharedContext_) {
        return NapiEglContext::NewInstance(info.Env(), sharedContext_);
    }

    return info.Env().Undefined();
}

Napi::Value NapiNativeVideoRenderer::GetVideoTrack(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    return jsTrackRef_.IsEmpty() ? info.Env().Null() : jsTrackRef_.Value();
}

Napi::Value NapiNativeVideoRenderer::SetVideoTrack(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    if (info.Length() < 1 || (!info[0].IsNull() && !info[0].IsObject())) {
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), Value());
    }

    if (!renderer_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    if (info[0].IsNull()) {
        RemoveSink();
        jsTrackRef_.Reset();
        return info.Env().Undefined();
    }

    auto jsTrack = info[0].As<Object>();
    if (!jsTrackRef_.IsEmpty() && jsTrackRef_.Value().StrictEquals(jsTrack)) {
        RTC_DLOG(LS_VERBOSE) << "Same video track";
        return info.Env().Undefined();
    }

    auto napiTrack = NapiMediaStreamTrack::Unwrap(jsTrack);
    if (!napiTrack) {
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), Value());
    }

    if (!napiTrack->IsVideoTrack()) {
        RTC_LOG(LS_ERROR) << "Not video track";
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), Value());
    }

    jsTrackRef_ = Persistent(jsTrack);
    napiTrack->AddSink(renderer_.get());

    return info.Env().Undefined();
}

Napi::Value NapiNativeVideoRenderer::Release(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    RemoveSink();
    jsTrackRef_.Reset();
    renderer_.reset();
    surfaceId_.reset();

    return info.Env().Undefined();
}

Napi::Value NapiNativeVideoRenderer::SetMirror(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    if (info.Length() < 1 || !info[0].IsBoolean()) {
        NAPI_THROW(Error::New(info.Env(), "Invalid arguments"), info.Env().Undefined());
    }

    if (!renderer_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    if (renderer_->SetMirrorHorizontally(info[0].As<Boolean>().Value()) != 0) {
        NAPI_THROW(Error::New(info.Env(), "Invalid operation"), info.Env().Undefined());
    }

    return info.Env().Undefined();
}

Napi::Value NapiNativeVideoRenderer::SetMirrorVertically(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    if (info.Length() < 1 || !info[0].IsBoolean()) {
        NAPI_THROW(Error::New(info.Env(), "Invalid arguments"), info.Env().Undefined());
    }

    if (!renderer_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    if (renderer_->SetMirrorVertically(info[0].As<Boolean>().Value()) != 0) {
        NAPI_THROW(Error::New(info.Env(), "Invalid operation"), info.Env().Undefined());
    }

    return info.Env().Undefined();
}

Napi::Value NapiNativeVideoRenderer::SetScalingMode(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    if (info.Length() < 1 || !info[0].IsNumber()) {
        NAPI_THROW(Error::New(info.Env(), "Invalid arguments"), info.Env().Undefined());
    }

    if (!renderer_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    auto mode = info[0].As<Number>().Int32Value();
    if (renderer_->SetScalingMode(static_cast<NativeWindowRenderer::ScalingMode>(mode)) != 0) {
        NAPI_THROW(Error::New(info.Env(), "Invalid operation"), info.Env().Undefined());
    }

    return info.Env().Undefined();
}

Napi::Value NapiNativeVideoRenderer::GetEventHandler(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    const auto type = (const char*)info.Data();

    Function func;
    if (!NapiEventTarget<NapiNativeVideoRenderer>::GetEventHandler(type, func)) {
        return info.Env().Null();
    }

    return func;
}

void NapiNativeVideoRenderer::SetEventHandler(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    const auto type = (const char*)info.Data();

    if (value.IsFunction()) {
        auto fn = value.As<Function>();
        NapiEventTarget<NapiNativeVideoRenderer>::SetEventHandler(type, fn);
    } else if (value.IsNull()) {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " value is null";
        RemoveEventHandler(type);
    } else {
        NAPI_THROW_VOID(Error::New(info.Env(), "value is error"));
    }
}

Napi::Value NapiNativeVideoRenderer::ToJson(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", String::New(info.Env(), "NapiNativeVideoRenderer"));
#endif

    return json;
}

bool NapiNativeVideoRenderer::Init()
{
    auto nativeWindow = ohos::NativeWindow::CreateFromSurfaceId(std::stoull(*surfaceId_));
    if (nativeWindow.IsEmpty()) {
        return false;
    }

    auto windowRenderer = NativeWindowRendererGl::Create(nativeWindow, sharedContext_, "native-window-renderer");
    if (!windowRenderer) {
        return false;
    }

    renderer_ = std::make_unique<NativeVideoRenderer>(std::move(windowRenderer));
    renderer_->SetFirstFrameRenderedCallback([this]() { NotifyFirstFrameRendered(); });
    renderer_->SetFrameResolutionChangedCallback([this](int32_t width, int32_t height, int32_t rotation) {
        NotifyFrameResolutionChanged(width, height, rotation);
    });

    return true;
}

void NapiNativeVideoRenderer::RemoveSink()
{
    if (!renderer_) {
        RTC_DLOG(LS_VERBOSE) << "renderer is null";
        return;
    }

    if (jsTrackRef_.IsEmpty()) {
        RTC_DLOG(LS_VERBOSE) << "track ref is empty";
        return;
    }

    auto jsTrack = jsTrackRef_.Value();
    if (jsTrack.IsEmpty()) {
        RTC_DLOG(LS_VERBOSE) << "track is empty";
        return;
    }

    auto napiTrack = NapiMediaStreamTrack::Unwrap(jsTrack);
    napiTrack->RemoveSink(renderer_.get());
}

void NapiNativeVideoRenderer::NotifyFirstFrameRendered()
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Dispatch(CallbackEvent<NapiNativeVideoRenderer>::Create([this](NapiNativeVideoRenderer& target) {
        RTC_LOG(LS_VERBOSE) << __FUNCTION__;
        RTC_DCHECK_EQ(this, &target);

        auto env = target.Env();
        Napi::HandleScope scope(env);
        auto jsEvent = Object::New(env);
        jsEvent.Set("type", String::New(env, kEventNameFirstFrameRendered));
        target.MakeCallback(kEventNameFirstFrameRendered, {jsEvent});
    }));
}

void NapiNativeVideoRenderer::NotifyFrameResolutionChanged(int32_t width, int32_t height, int32_t rotation)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Dispatch(CallbackEvent<NapiNativeVideoRenderer>::Create(
        [this, width, height, rotation](NapiNativeVideoRenderer& target) {
            RTC_DCHECK_EQ(this, &target);

            auto env = target.Env();
            Napi::HandleScope scope(env);
            auto jsEvent = Object::New(env);
            jsEvent.Set("type", String::New(env, kEventNameFrameResolutionChanged));
            jsEvent.Set("width", Number::New(env, width));
            jsEvent.Set("height", Number::New(env, height));
            jsEvent.Set("rotation", Number::New(env, rotation));
            target.MakeCallback(kEventNameFrameResolutionChanged, {jsEvent});
        }));
}

} // namespace webrtc
