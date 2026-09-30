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

#include <cerrno>
#include <cstdlib>
#include <cmath>
#include <limits>

namespace webrtc {

using namespace ohos;
using namespace adapter;
using namespace Napi;

const char kAttributeNameOnFrameResolutionChanged[] = "onFrameResolutionChanged";
constexpr int kCameraPreviewApiVersion = 2;
constexpr size_t kSurfaceSizeArgumentCount = 2;

const char kEventFrameResolutionChanged[] = "frameResolutionChanged";

thread_local FunctionReference NapiNativeVideoRenderer::constructor_;

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
            InstanceMethod<&NapiNativeVideoRenderer::SetSurfaceSize>(kMethodNameSetSurfaceSize),
            InstanceMethod<&NapiNativeVideoRenderer::Init>(kMethodNameInit),
            InstanceMethod<&NapiNativeVideoRenderer::Release>(kMethodNameRelease),
            InstanceMethod<&NapiNativeVideoRenderer::ToJson>(kMethodNameToJson),
            InstanceAccessor<&NapiNativeVideoRenderer::GetEventHandler, &NapiNativeVideoRenderer::SetEventHandler>(
                kAttributeNameOnFrameResolutionChanged, napi_default, (void*)kEventFrameResolutionChanged),
        });
    func.Set("cameraPreviewApiVersion", Number::New(env, kCameraPreviewApiVersion));
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
    constructor_.SuppressDestruct();
}

NapiNativeVideoRenderer::NapiNativeVideoRenderer(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<NapiNativeVideoRenderer>(info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

NapiNativeVideoRenderer::~NapiNativeVideoRenderer()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    RemoveSink();
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
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() == 0) {
        return info.Env().Undefined();
    }

    if (info[0].IsNull()) {
        RemoveSink();
        jsTrackRef_.Reset();
        return info.Env().Undefined();
    }

    if (!info[0].IsObject()) {
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), Value());
    }

    auto jsTrack = info[0].As<Object>();
    auto napiTrack = NapiMediaStreamTrack::Unwrap(jsTrack);
    if (!napiTrack) {
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), Value());
    }

    if (!napiTrack->IsVideoTrack()) {
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), Value());
    }

    // Resizing a Surface must not detach the same track. Removing the last
    // sink stops capture, so a redundant binding otherwise restarts the camera.
    if (surfaceSizeConfigured_ && !jsTrackRef_.IsEmpty() && jsTrackRef_.Value().StrictEquals(jsTrack)) {
        return info.Env().Undefined();
    }

    RemoveSink();
    jsTrackRef_ = Weak(jsTrack);
    AddSink();

    return info.Env().Undefined();
}

Napi::Value NapiNativeVideoRenderer::Init(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 1) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsString()) {
        NAPI_THROW(Error::New(info.Env(), "The first argument is not string"), info.Env().Undefined());
    }

    surfaceId_ = info[0].As<String>().Utf8Value();

    if (info.Length() > 1 && info[1].IsObject()) {
        auto jsSharedContext = info[1].As<Object>();
        auto napiSharedContext = NapiEglContext::Unwrap(jsSharedContext);
        if (napiSharedContext) {
            sharedContext_ = napiSharedContext->Get();
        }
    } else {
        sharedContext_ = EglEnv::GetDefault().GetContext();
    }

    const std::string& surfaceIdStr = *surfaceId_;
    char* endPtr = nullptr;
    errno = 0;
    unsigned long long surfaceIdValue = std::strtoull(surfaceIdStr.c_str(), &endPtr, 10);
    if (errno != 0 || endPtr == surfaceIdStr.c_str() || *endPtr != '\0') {
        NAPI_THROW(Error::New(info.Env(), "Invalid surfaceId"), info.Env().Undefined());
    }
    auto nativeWindow = ohos::NativeWindow::CreateFromSurfaceId(surfaceIdValue);
    if (nativeWindow.IsEmpty()) {
        NAPI_THROW(Error::New(info.Env(), "Failed to create native window"), info.Env().Undefined());
    }

#if defined(__x86_64__)
    // x86_64 模拟器的 dgles 不支持外部纹理(EGLImage)，渲染会触发 memcpy 崩溃，
    // 改用 CPU 光栅渲染(I420 -> RGBA)。
    renderer_ = NativeWindowRendererRaster::Create(nativeWindow);
#else
    renderer_ = NativeWindowRendererGl::Create(nativeWindow, sharedContext_, "native-window-renderer");
#endif
    if (renderer_) {
        renderer_->SetFrameSizeChangeCallback([this](int width, int height, int rotation) {
            RTC_DLOG(LS_VERBOSE) << "[NapiNativeVideoRenderer] Frame size changed: " << width << "x" << height;
            ThreadSafeFunction tsfn;
            if (!GetEventHandler(kEventFrameResolutionChanged, tsfn)) {
                return;
            }
            NapiNativeVideoRenderer* context = tsfn.GetContext();
            napi_status status =
                tsfn.BlockingCall([context, width, height, rotation](Napi::Env env, Napi::Function jsCallback) {
                    auto jsEvent = Object::New(env);
                    jsEvent.Set("type", String::New(env, kEventFrameResolutionChanged));
                    jsEvent.Set("width", Number::New(env, width));
                    jsEvent.Set("height", Number::New(env, height));
                    jsEvent.Set("rotation", Number::New(env, rotation));
                    jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
                });
            if (status != napi_ok) {
                RTC_LOG(LS_ERROR) << "tsfn call error: " << status;
            }
        });
        // setVideoTrack 可能在 init 之前调用，此时 AddSink 会跳过；init 完成后补挂 sink。
        AddSink();
    }

    return info.Env().Undefined();
}

Napi::Value NapiNativeVideoRenderer::GetEventHandler(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    Function fn;
    if (!GetEventHandler((const char*)info.Data(), fn)) {
        return info.Env().Null();
    }

    return fn;
}

bool NapiNativeVideoRenderer::GetEventHandler(const std::string& type, Napi::Function& fn)
{
    RTC_DLOG(LS_VERBOSE) << "NapiNativeVideoRenderer::GetEventHandler type: " << type;

    UNUSED std::lock_guard<std::mutex> lock(eventMutex_);
    auto it = eventHandlers_.find(type);
    if (it == eventHandlers_.end()) {
        RTC_LOG(LS_VERBOSE) << "No event handler for type: " << type;
        return false;
    }

    fn = it->second.ref.Value();

    return true;
}

bool NapiNativeVideoRenderer::GetEventHandler(const std::string& type, Napi::ThreadSafeFunction& tsfn)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " type=" << type;

    UNUSED std::lock_guard<std::mutex> lock(eventMutex_);
    auto it = eventHandlers_.find(type);
    if (it == eventHandlers_.end()) {
        RTC_LOG(LS_VERBOSE) << "No event handler for type: " << type;
        return false;
    }

    tsfn = it->second.tsfn;

    return true;
}

void NapiNativeVideoRenderer::SetEventHandler(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RemoveEventHandler((const char*)info.Data());

    if (value.IsFunction()) {
        Function cb = value.As<Function>();
        if (!SetEventHandler((const char*)info.Data(), std::move(cb), info.This())) {
            NAPI_THROW_VOID(Napi::Error::New(info.Env(), "Failed to set event handler"));
        }
    }
}

bool NapiNativeVideoRenderer::SetEventHandler(const std::string& type, Napi::Function fn, Napi::Value receiver)
{
    RTC_DLOG(LS_VERBOSE) << "SetEventHandler type: " << type;

    this->Ref();
    EventHandler handler;
    handler.ref = Persistent(fn);
    handler.tsfn =
        ThreadSafeFunction::New(fn.Env(), fn, type, 0, 1, this, [] (Napi::Env, NapiNativeVideoRenderer* ctx) {
            ctx->Unref();
        });

    UNUSED std::lock_guard<std::mutex> lock(eventMutex_);
    eventHandlers_[type] = std::move(handler);

    return true;
}

bool NapiNativeVideoRenderer::RemoveEventHandler(const std::string& type)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    UNUSED std::lock_guard<std::mutex> lock(eventMutex_);
    auto it = eventHandlers_.find(type);
    if (it != eventHandlers_.end()) {
        it->second.tsfn.Release();
        eventHandlers_.erase(it);
    }

    return true;
}

void NapiNativeVideoRenderer::RemoveAllEventHandlers()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    UNUSED std::lock_guard<std::mutex> lock(eventMutex_);
    for (auto& handler : eventHandlers_) {
        handler.second.tsfn.Release();
    }
    eventHandlers_.clear();
}

Napi::Value NapiNativeVideoRenderer::Release(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RemoveAllEventHandlers();
    RemoveSink();

    surfaceId_.reset();
    renderer_.reset();
    surfaceSizeConfigured_ = false;
    jsTrackRef_.Reset();

    return info.Env().Undefined();
}

Napi::Value NapiNativeVideoRenderer::SetMirror(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 1) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsBoolean()) {
        NAPI_THROW(Error::New(info.Env(), "The first argument is not boolean"), info.Env().Undefined());
    }

    if (renderer_) {
        renderer_->SetMirrorHorizontally(info[0].As<Boolean>().Value());
    }

    return info.Env().Undefined();
}

Napi::Value NapiNativeVideoRenderer::SetMirrorVertically(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 1) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsBoolean()) {
        NAPI_THROW(Error::New(info.Env(), "The first argument is not boolean"), info.Env().Undefined());
    }

    if (renderer_) {
        renderer_->SetMirrorVertically(info[0].As<Boolean>().Value());
    }

    return info.Env().Undefined();
}

Napi::Value NapiNativeVideoRenderer::SetScalingMode(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 1) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsNumber()) {
        NAPI_THROW(Error::New(info.Env(), "The first argument is not number"), info.Env().Undefined());
    }

    auto mode = info[0].As<Number>().Int32Value();
    if (renderer_) {
        renderer_->SetScalingMode(static_cast<NativeWindowRenderer::ScalingMode>(mode));
    }

    return info.Env().Undefined();
}

Napi::Value NapiNativeVideoRenderer::SetSurfaceSize(const Napi::CallbackInfo& info)
{
    if (info.Length() < kSurfaceSizeArgumentCount || !info[0].IsNumber() || !info[1].IsNumber()) {
        NAPI_THROW(Error::New(info.Env(), "Expected surface width and height in pixels"), info.Env().Undefined());
    }
    const double width = info[0].As<Number>().DoubleValue();
    const double height = info[1].As<Number>().DoubleValue();
    if (!std::isfinite(width) || !std::isfinite(height) || width < 1 || height < 1 ||
        width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max()) {
        NAPI_THROW(Error::New(info.Env(), "Invalid surface dimensions"), info.Env().Undefined());
    }
    if (renderer_) {
        surfaceSizeConfigured_ = true;
        renderer_->SetSurfaceSize(static_cast<int>(width), static_cast<int>(height));
    }
    return info.Env().Undefined();
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

void NapiNativeVideoRenderer::AddSink()
{
    if (!renderer_) {
        RTC_DLOG(LS_VERBOSE) << "renderer not initialized yet";
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
    if (!napiTrack) {
        RTC_LOG(LS_ERROR) << "AddSink failed: invalid track";
        return;
    }
    napiTrack->AddSink(renderer_.get());
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
    if (!napiTrack) {
        RTC_LOG(LS_ERROR) << "RemoveSink failed: invalid track";
        return;
    }
    napiTrack->RemoveSink(renderer_.get());
}

} // namespace webrtc
