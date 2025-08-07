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

#include "media_source.h"

#include "rtc_base/logging.h"

namespace webrtc {

using namespace Napi;

NapiMediaSource::~NapiMediaSource() = default;

void NapiAudioSource::Init(Napi::Env env, Napi::Object exports)
{
    Function func = DefineClass(
        env, kClassName,
        {
            InstanceAccessor<&NapiAudioSource::GetState>(kAttributeNameState),
            InstanceMethod<&NapiAudioSource::Release>(kMethodNameRelease),
            InstanceMethod<&NapiAudioSource::SetVolume>(kMethodNameSetVolume),
            InstanceMethod<&NapiAudioSource::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    Constructor() = Persistent(func);
}

Napi::Object NapiAudioSource::NewInstance(Napi::Env env, rtc::scoped_refptr<OhosLocalAudioSource> source)
{
    if (!source) {
        NAPI_THROW(Error::New(env, "Invalid argument"), Object::New(env));
    }

    auto external = External<OhosLocalAudioSource>::New(
        env, source.release(), [](Napi::Env /*env*/, OhosLocalAudioSource* source) { source->Release(); });

    return Constructor().New({external});
}

Napi::FunctionReference& NapiAudioSource::Constructor()
{
    static Napi::FunctionReference constructor;
    return constructor;
}

NapiAudioSource::NapiAudioSource(const Napi::CallbackInfo& info) : Napi::ObjectWrap<NapiAudioSource>(info)
{
    if (info[0].IsExternal()) {
        auto source = info[0].As<External<OhosLocalAudioSource>>().Data();
        source_ = rtc::scoped_refptr<OhosLocalAudioSource>(source);
    }
}

NapiAudioSource::~NapiAudioSource()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

rtc::scoped_refptr<OhosLocalAudioSource> NapiAudioSource::Get() const
{
    return source_;
}

Napi::Value NapiAudioSource::GetState(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!source_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    auto state = source_->state();
    switch (state) {
        case MediaSourceInterface::kInitializing:
            return String::New(info.Env(), kEnumNameSourceStateInitializing);
        case MediaSourceInterface::kLive:
            return String::New(info.Env(), kEnumNameSourceStateLive);
        case MediaSourceInterface::kEnded:
            return String::New(info.Env(), kEnumNameSourceStateEnded);
        case MediaSourceInterface::kMuted:
            return String::New(info.Env(), kEnumNameSourceStateMuted);
        default:
            break;
    }

    NAPI_THROW(Error::New(info.Env(), "Invalid state"), info.Env().Undefined());
}

Napi::Value NapiAudioSource::Release(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!source_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    source_ = nullptr;

    return info.Env().Undefined();
}

Napi::Value NapiAudioSource::SetVolume(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 1) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsNumber()) {
        NAPI_THROW(TypeError::New(info.Env(), "The argument is not number"), info.Env().Undefined());
    }

    if (!source_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    double volume = info[0].As<Number>().DoubleValue();
    source_->SetVolume(volume);

    return info.Env().Undefined();
}

Value NapiAudioSource::ToJson(const CallbackInfo& info)
{
    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", String::New(info.Env(), "NapiAudioSource"));
#endif

    return json;
}

void NapiVideoSource::Init(Napi::Env env, Napi::Object exports)
{
    Function func = DefineClass(
        env, kClassName,
        {
            InstanceAccessor<&NapiVideoSource::GetState>(kAttributeNameState),
            InstanceAccessor<&NapiVideoSource::GetEventHandler, &NapiVideoSource::SetEventHandler>(
                kAttributeNameOnCapturerStarted, napi_default, (void*)kEventNameCapturerStarted),
            InstanceAccessor<&NapiVideoSource::GetEventHandler, &NapiVideoSource::SetEventHandler>(
                kAttributeNameOnCapturerStopped, napi_default, (void*)kEventNameCapturerStopped),
            InstanceMethod<&NapiVideoSource::Release>(kMethodNameRelease),
            InstanceMethod<&NapiVideoSource::StartCapture>(kMethodNameStartCapture),
            InstanceMethod<&NapiVideoSource::StopCapture>(kMethodNameStopCapture),
            InstanceMethod<&NapiVideoSource::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    Constructor() = Persistent(func);
}

Napi::Object NapiVideoSource::NewInstance(Napi::Env env, rtc::scoped_refptr<OhosVideoTrackSource> source)
{
    if (!source) {
        NAPI_THROW(Error::New(env, "Invalid argument"), Object());
    }

    auto external = External<rtc::scoped_refptr<OhosVideoTrackSource>>::New(env, &source);

    return Constructor().New({external});
}

Napi::FunctionReference& NapiVideoSource::Constructor()
{
    static Napi::FunctionReference constructor;
    return constructor;
}

NapiVideoSource::NapiVideoSource(const Napi::CallbackInfo& info) : NapiEventTarget<NapiVideoSource>(info)
{
    if (info[0].IsExternal()) {
        source_ = *info[0].As<External<rtc::scoped_refptr<OhosVideoTrackSource>>>().Data();
    }

    if (source_) {
        source_->SetCapturerObserver(this);
    }
}

NapiVideoSource::~NapiVideoSource()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (source_) {
        source_->SetCapturerObserver(nullptr);
        source_ = nullptr;
    }
}

Napi::Value NapiVideoSource::GetState(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!source_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    auto source = static_cast<VideoTrackSourceInterface*>(source_.get());
    auto state = source->state();
    switch (state) {
        case MediaSourceInterface::kInitializing:
            return String::New(info.Env(), kEnumNameSourceStateInitializing);
        case MediaSourceInterface::kLive:
            return String::New(info.Env(), kEnumNameSourceStateLive);
        case MediaSourceInterface::kEnded:
            return String::New(info.Env(), kEnumNameSourceStateEnded);
        case MediaSourceInterface::kMuted:
            return String::New(info.Env(), kEnumNameSourceStateMuted);
        default:
            break;
    }

    NAPI_THROW(Error::New(info.Env(), "Invalid state"), info.Env().Undefined());
}

Napi::Value NapiVideoSource::Release(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    if (!source_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    source_->SetCapturerObserver(nullptr);
    source_ = nullptr;

    Stop();

    return info.Env().Undefined();
}

Napi::Value NapiVideoSource::GetEventHandler(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (!source_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    const auto type = (const char*)info.Data();

    Function func;
    if (!NapiEventTarget<NapiVideoSource>::GetEventHandler(type, func)) {
        return info.Env().Null();
    }

    return func;
}

void NapiVideoSource::SetEventHandler(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (!value.IsFunction() && !value.IsNull()) {
        NAPI_THROW_VOID(Error::New(info.Env(), "Invalid argument"));
    }

    if (!source_) {
        NAPI_THROW_VOID(Error::New(info.Env(), "Illegal state"));
    }

    const auto type = (const char*)info.Data();

    if (value.IsFunction()) {
        auto fn = value.As<Function>();
        NapiEventTarget<NapiVideoSource>::SetEventHandler(type, fn);
    } else if (value.IsNull()) {
        RTC_DLOG_F(LS_VERBOSE) << "Value is null";
        RemoveEventHandler(type);
    }
}

Napi::Value NapiVideoSource::StartCapture(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    if (!source_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    source_->Start();

    return info.Env().Undefined();
}

Napi::Value NapiVideoSource::StopCapture(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    if (!source_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    source_->Stop();

    return info.Env().Undefined();
}

Value NapiVideoSource::ToJson(const CallbackInfo& info)
{
    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", String::New(info.Env(), "NapiVideoSource"));
#endif

    return json;
}

void NapiVideoSource::OnCapturerStarted()
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Dispatch(
        CallbackEvent<NapiVideoSource>::Create([this](NapiVideoSource& target) {
            RTC_LOG(LS_VERBOSE) << "Callback event: " << kEventNameCapturerStarted;
            RTC_DCHECK_EQ(this, &target);

            auto env = target.Env();
            Napi::HandleScope scope(env);
            auto jsEvent = Object::New(env);
            jsEvent.Set("type", String::New(env, kEventNameCapturerStarted));
            // jsEvent.Set("success", Boolean::New(env, success));
            target.MakeCallback(kEventNameCapturerStarted, {jsEvent});
        }));
}

void NapiVideoSource::OnCapturerStopped()
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Dispatch(
        CallbackEvent<NapiVideoSource>::Create([this](NapiVideoSource& target) {
            RTC_LOG(LS_VERBOSE) << "Callback event: " << kEventNameCapturerStopped;
            RTC_DCHECK_EQ(this, &target);

            auto env = target.Env();
            Napi::HandleScope scope(env);
            auto jsEvent = Object::New(env);
            jsEvent.Set("type", String::New(env, kEventNameCapturerStopped));
            jsEvent.Set("stopType", String::New(env, "normal"));
            target.MakeCallback(kEventNameCapturerStopped, {jsEvent});
        }));
}

void NapiVideoSource::OnCapturerError(int32_t code, const std::string& message)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Dispatch(
        CallbackEvent<NapiVideoSource>::Create([this, code, message](NapiVideoSource& target) {
            RTC_LOG(LS_ERROR) << "Callback event: " << kEventNameError;
            RTC_DCHECK_EQ(this, &target);

            auto env = target.Env();
            Napi::HandleScope scope(env);

            auto jsEvent = Object::New(env);
            jsEvent.Set("type", String::New(env, kEventNameError));
            jsEvent.Set("errorCode", Number::New(env, code));
            jsEvent.Set("errorText", String::New(env, message));
            target.MakeCallback(kEventNameError, {jsEvent});
        }));
}

} // namespace webrtc
