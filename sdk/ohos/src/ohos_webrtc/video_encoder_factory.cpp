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

#include "video_encoder_factory.h"
#include "render/egl_env.h"
#include "utils/marcos.h"
#include "video_codec/hardware_video_encoder_factory.h"
#include "video_codec/software_video_encoder_factory.h"
#include "video_codec/default_video_encoder_factory.h"

#include <rtc_base/logging.h>

namespace webrtc {

using namespace Napi;

namespace {

// Reads a string-array codec list option (#9); throws TypeError on a present
// but malformed field. Returns true when the field was present and valid.
bool ReadCodecList(const Napi::Object& jsOptions, const char* attributeName, std::vector<std::string>& out)
{
    if (!jsOptions.Has(attributeName)) {
        return false;
    }

    auto jsList = jsOptions.Get(attributeName);
    if (!jsList.IsArray()) {
        NAPI_THROW(TypeError::New(jsOptions.Env(), std::string(attributeName) + " must be an array of strings"),
                   false);
    }

    auto array = jsList.As<Napi::Array>();
    for (uint32_t i = 0; i < array.Length(); i++) {
        auto item = array.Get(i);
        if (!item.IsString()) {
            NAPI_THROW(
                TypeError::New(jsOptions.Env(), std::string(attributeName) + " must be an array of strings"), false);
        }
        out.push_back(item.As<Napi::String>().Utf8Value());
    }
    return true;
}

} // namespace

thread_local FunctionReference NapiHardwareVideoEncoderFactory::constructor_;

void NapiHardwareVideoEncoderFactory::Init(Napi::Env env, Napi::Object exports)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Function func = DefineClass(
        env, kClassName,
        {
            InstanceAccessor<&NapiHardwareVideoEncoderFactory::GetEnableH264HighProfile>(
                kAttributeNameEnableH264HighProfile),
            InstanceAccessor<&NapiHardwareVideoEncoderFactory::GetSharedContext>(kAttributeNameSharedContext),
            InstanceMethod<&NapiHardwareVideoEncoderFactory::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
    constructor_.SuppressDestruct();
}

NapiHardwareVideoEncoderFactory::~NapiHardwareVideoEncoderFactory() = default;

NapiHardwareVideoEncoderFactory::NapiHardwareVideoEncoderFactory(const Napi::CallbackInfo& info) : ObjectWrap(info)
{
    // #9: an options object form is accepted in addition to the legacy
    // (enableH264HighProfile, sharedContext) positional arguments.
    if (info.Length() > 0 && info[0].IsObject()) {
        ApplyEncoderFactoryOptions(info[0].As<Object>());
    } else if (info.Length() > 0 && info[0].IsBoolean()) {
        enableH264HighProfile_ = info[0].As<Boolean>().Value();
    }

    if (info.Length() > 1 && info[1].IsObject()) {
        auto napiCtx = NapiEglContext::Unwrap(info[1].As<Object>());
        if (napiCtx) {
            sharedContext_ = napiCtx->Get();
        }
    }

    if (!sharedContext_) {
        sharedContext_ = EglEnv::GetDefault().GetContext();
    }

    info.This().As<Object>().TypeTag(&kTypeTag);
}

void NapiHardwareVideoEncoderFactory::ApplyEncoderFactoryOptions(const Napi::Object& jsOptions)
{
    if (jsOptions.Has(kAttributeNameEnableH264HighProfile)) {
        auto val = jsOptions.Get(kAttributeNameEnableH264HighProfile);
        if (!val.IsBoolean()) {
            NAPI_THROW_VOID(TypeError::New(jsOptions.Env(), "enableH264HighProfile must be a boolean"));
        }
        enableH264HighProfile_ = val.As<Boolean>().Value();
    }

    if (jsOptions.Has(kAttributeNameSharedContext)) {
        auto napiCtx = NapiEglContext::Unwrap(jsOptions.Get(kAttributeNameSharedContext).As<Object>());
        if (napiCtx) {
            sharedContext_ = napiCtx->Get();
        }
    }

    ApplyCodecForceListOptions(jsOptions);
}

void NapiHardwareVideoEncoderFactory::ApplyCodecForceListOptions(const Napi::Object& jsOptions)
{
    std::vector<std::string> forceSW;
    std::vector<std::string> forceHW;
    bool hasSW = ReadCodecList(jsOptions, kAttributeNameForceSWCodecList, forceSW);
    if (jsOptions.Env().IsExceptionPending()) {
        NAPI_THROW_VOID(jsOptions.Env().GetAndClearPendingException());
    }
    bool hasHW = ReadCodecList(jsOptions, kAttributeNameForceHWCodecList, forceHW);
    if (jsOptions.Env().IsExceptionPending()) {
        NAPI_THROW_VOID(jsOptions.Env().GetAndClearPendingException());
    }
    if (hasSW || hasHW) {
        forceLists_ = adapter::CodecForceLists(std::move(forceSW), std::move(forceHW));
        forceListsSet_ = true;
    }
}

Napi::Value NapiHardwareVideoEncoderFactory::GetEnableH264HighProfile(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    return Boolean::New(info.Env(), enableH264HighProfile_);
}

Napi::Value NapiHardwareVideoEncoderFactory::GetSharedContext(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    return NapiEglContext::NewInstance(info.Env(), sharedContext_);
}

Napi::Value NapiHardwareVideoEncoderFactory::ToJson(const Napi::CallbackInfo& info)
{
    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", "NapiHardwareVideoEncoderFactory");
#endif
    json.Set(kAttributeNameEnableH264HighProfile, Boolean::New(info.Env(), enableH264HighProfile_));
    return json;
}

thread_local FunctionReference NapiSoftwareVideoEncoderFactory::constructor_;

void NapiSoftwareVideoEncoderFactory::Init(Napi::Env env, Napi::Object exports)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Function func = DefineClass(
        env, kClassName,
        {
            InstanceMethod<&NapiSoftwareVideoEncoderFactory::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
    constructor_.SuppressDestruct();
}

NapiSoftwareVideoEncoderFactory::~NapiSoftwareVideoEncoderFactory() = default;

NapiSoftwareVideoEncoderFactory::NapiSoftwareVideoEncoderFactory(const Napi::CallbackInfo& info) : ObjectWrap(info)
{
    info.This().As<Object>().TypeTag(&kTypeTag);
}

Napi::Value NapiSoftwareVideoEncoderFactory::ToJson(const Napi::CallbackInfo& info)
{
    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", "NapiSoftwareVideoEncoderFactory");
#endif
    return json;
}

std::unique_ptr<VideoEncoderFactory> CreateVideoEncoderFactory(const Napi::Object& jsVideoEncoderFactory)
{
    if (NAPI_CHECK_TYPE_TAG(jsVideoEncoderFactory, NapiHardwareVideoEncoderFactory)) {
        auto napiFactory = NapiHardwareVideoEncoderFactory::Unwrap(jsVideoEncoderFactory);
        auto sharedContext = napiFactory->GetSharedContext();
        auto enableH264HighProfile = napiFactory->GetEnableH264HighProfile();
        auto hardwareFactory = std::make_unique<adapter::HardwareVideoEncoderFactory>(
            sharedContext, enableH264HighProfile, napiFactory->GetForceLists());
        // With force lists in play the factory is wrapped by the default
        // factory so force-SW falls back to software and force-HW skips the
        // fallback; without lists the legacy hardware-only behavior is kept.
        if (napiFactory->HasForceLists()) {
            return std::make_unique<adapter::DefaultVideoEncoderFactory>(
                std::move(hardwareFactory), napiFactory->GetForceLists());
        }
        return hardwareFactory;
    } else if (NAPI_CHECK_TYPE_TAG(jsVideoEncoderFactory, NapiSoftwareVideoEncoderFactory)) {
        return std::make_unique<adapter::SoftwareVideoEncoderFactory>();
    }

    return nullptr;
}

std::unique_ptr<VideoEncoderFactory> CreateDefaultVideoEncoderFactory()
{
    return std::make_unique<adapter::DefaultVideoEncoderFactory>(EglEnv::GetDefault().GetContext(), false);
}

} // namespace webrtc
