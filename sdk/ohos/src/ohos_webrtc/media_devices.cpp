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

#include "media_devices.h"
#include "media_track_constraints.h"
#include "async_work/async_worker_enumerate_devices.h"
#include "async_work/async_worker_get_user_media.h"
#include "async_work/async_worker_get_display_media.h"
#include "utils/marcos.h"

#include "rtc_base/logging.h"

namespace webrtc {

using namespace Napi;

class NapiDeviceChangeEvent : public Napi::ObjectWrap<NapiDeviceChangeEvent> {
public:
    NAPI_CLASS_NAME_DECLARE(DeviceChangeEvent);
    NAPI_ATTRIBUTE_NAME_DECLARE(Devices, devices);
    NAPI_ATTRIBUTE_NAME_DECLARE(UserInsertedDevices, userInsertedDevices);

    static void Init(Napi::Env env, Napi::Object exports)
    {
        Function func = DefineClass(
            env, kClassName,
            {
                InstanceAccessor<&NapiDeviceChangeEvent::GetDevices>(kAttributeNameDevices),
                InstanceAccessor<&NapiDeviceChangeEvent::GetUserInsertedDevices>(kAttributeNameUserInsertedDevices),
            });
        exports.Set(kClassName, func);

        constructor_ = Persistent(func);
    }

protected:
    friend class ObjectWrap;

    NapiDeviceChangeEvent(const Napi::CallbackInfo& info) : ObjectWrap(info) {}

    Napi::Value GetDevices(const Napi::CallbackInfo& info)
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
        NAPI_THROW(Error::New(info.Env(), "Not implemented"), info.Env().Undefined());
    }

    Napi::Value GetUserInsertedDevices(const Napi::CallbackInfo& info)
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
        NAPI_THROW(Error::New(info.Env(), "Not implemented"), info.Env().Undefined());
    }

private:
    static Napi::FunctionReference constructor_;
};

FunctionReference NapiDeviceChangeEvent::constructor_;

FunctionReference NapiMediaDevices::constructor_;

void NapiMediaDevices::Init(Napi::Env env, Napi::Object exports)
{
    NapiDeviceChangeEvent::Init(env, exports);

    Function func = DefineClass(
        env, kClassName,
        {
            InstanceMethod<&NapiMediaDevices::EnumerateDevices>(kMethodNameEnumerateDevices),
            InstanceMethod<&NapiMediaDevices::GetSupportedConstraints>(kMethodNameGetSupportedConstraints),
            InstanceMethod<&NapiMediaDevices::GetUserMedia>(kMethodNameGetUserMedia),
            InstanceMethod<&NapiMediaDevices::GetDisplayMedia>(kMethodNameGetDisplayMedia),
            InstanceMethod<&NapiMediaDevices::ToJson>(kMethodNameToJson),
            InstanceAccessor<&NapiMediaDevices::GetEventHandler, &NapiMediaDevices::SetEventHandler>(
                kAttributeNameOnDeviceChange, napi_default, (void*)kEventNameDeviceChange),
        });
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
}

NapiMediaDevices::NapiMediaDevices(const Napi::CallbackInfo& info) : Napi::ObjectWrap<NapiMediaDevices>(info) {}

Napi::Value NapiMediaDevices::EnumerateDevices(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto asyncWorker = AsyncWorkerEnumerateDevices::Create(info.Env(), "enumerateDevices");
    asyncWorker->Queue();

    return asyncWorker->GetPromise();
}

Napi::Value NapiMediaDevices::GetSupportedConstraints(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto result = Object::New(info.Env());

#define NAPI_SET_ATTRIBUTE(name)                                                                                       \
    result.Set(name, Boolean::New(info.Env(), NapiMediaConstraints::IsConstraintSupported(name)))

    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameWidth);
    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameHeight);
    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameAspectRatio);
    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameFrameRate);
    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameFacingMode);
    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameResizeMode);
    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameSampleRate);
    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameSampleSize);
    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameEchoCancellation);
    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameAutoGainControl);
    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameNoiseSuppression);
    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameLatency);
    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameChannelCount);
    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameDeviceId);
    NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameGroupId);
    // NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameBackgroundBlur);
    // NAPI_SET_ATTRIBUTE(NapiMediaConstraints::kAttributeNameDisplaySurface);

#undef NAPI_SET_ATTRIBUTE

    return result;
}

Napi::Value NapiMediaDevices::GetUserMedia(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto asyncWorker = AsyncWorkerGetUserMedia::Create(info.Env(), "getUserMedia");
    auto deferred = asyncWorker->GetDeferred();

    if (info.Length() == 0 || !info[0].IsObject()) {
        deferred.Reject(TypeError::New(info.Env(), "Invalid argument").Value());
        return asyncWorker->GetPromise();
    }

    MediaTrackConstraints audio;
    MediaTrackConstraints video;

    auto jsConstraints = info[0].As<Object>();
    if (jsConstraints.Has("audio")) {
        NapiMediaConstraints::JsToNative(jsConstraints.Get("audio"), audio);
        if (info.Env().IsExceptionPending()) {
            deferred.Reject(info.Env().GetAndClearPendingException().Value());
            return asyncWorker->GetPromise();
        }
    }

    if (jsConstraints.Has("video")) {
        NapiMediaConstraints::JsToNative(jsConstraints.Get("video"), video);
        if (info.Env().IsExceptionPending()) {
            deferred.Reject(info.Env().GetAndClearPendingException().Value());
            return asyncWorker->GetPromise();
        }
    }

    RTC_DLOG(LS_VERBOSE) << "audio constraints: " << audio.ToString();

    if (audio.IsNull() && video.IsNull()) {
        deferred.Reject(TypeError::New(info.Env(), "At least one of audio and video must be requested").Value());
        return asyncWorker->GetPromise();
    }

//     asyncWorker->Queue();
//     deferred.Reject(Error::New(info.Env(), "Not implemented").Value());
    asyncWorker->Start(std::move(audio), std::move(video));
    return asyncWorker->GetPromise();
}

Napi::Value NapiMediaDevices::GetDisplayMedia(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto asyncWorker = AsyncWorkerGetDisplayMedia::Create(info.Env(), "getDisplayMedia");
    auto deferred = asyncWorker->GetDeferred();

    if (info.Length() == 0 || !info[0].IsObject()) {
        deferred.Reject(TypeError::New(info.Env(), "Invalid argument").Value());
        return asyncWorker->GetPromise();
    }

    MediaTrackConstraints audio;
    MediaTrackConstraints video;

    auto jsConstraints = info[0].As<Object>();
    if (jsConstraints.Has("audio")) {
        NapiMediaConstraints::JsToNative(jsConstraints.Get("audio"), audio);
        if (info.Env().IsExceptionPending()) {
            deferred.Reject(info.Env().GetAndClearPendingException().Value());
            return asyncWorker->GetPromise();
        }
    }

    if (jsConstraints.Has("video")) {
        NapiMediaConstraints::JsToNative(jsConstraints.Get("video"), video);
        if (info.Env().IsExceptionPending()) {
            deferred.Reject(info.Env().GetAndClearPendingException().Value());
            return asyncWorker->GetPromise();
        }
    }

    RTC_DLOG(LS_VERBOSE) << "audio constraints: " << audio.ToString();

    if (audio.IsNull() && video.IsNull()) {
        deferred.Reject(TypeError::New(info.Env(), "At least one of audio and video must be requested").Value());
        return asyncWorker->GetPromise();
    }

    asyncWorker->Start(std::move(audio), std::move(video));
    return asyncWorker->GetPromise();
}

Napi::Value NapiMediaDevices::GetEventHandler(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << ": " << (const char*)info.Data();
    NAPI_THROW(Error::New(info.Env(), "Not implemented"), info.Env().Undefined());
}

void NapiMediaDevices::SetEventHandler(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << ": " << (const char*)info.Data();
    NAPI_THROW_VOID(Error::New(info.Env(), "Not implemented"));
}

Napi::Value NapiMediaDevices::ToJson(const Napi::CallbackInfo& info)
{
    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", "NapiMediaDevices");
#endif

    return json;
}

} // namespace webrtc
