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

#include "video_decoder_factory.h"
#include "utils/marcos.h"
#include "video_codec/hardware_video_decoder_factory.h"
#include "video_codec/software_video_decoder_factory.h"
#include "video_codec/default_video_decoder_factory.h"

#include <rtc_base/logging.h>

namespace webrtc {

using namespace Napi;

namespace {

// Same string-array codec list reader as the encoder side (#9).
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

thread_local FunctionReference NapiHardwareVideoDecoderFactory::constructor_;

void NapiHardwareVideoDecoderFactory::Init(Napi::Env env, Napi::Object exports)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Function func = DefineClass(
        env, kClassName,
        {
            InstanceMethod<&NapiHardwareVideoDecoderFactory::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
    constructor_.SuppressDestruct();
}

NapiHardwareVideoDecoderFactory::~NapiHardwareVideoDecoderFactory() = default;

NapiHardwareVideoDecoderFactory::NapiHardwareVideoDecoderFactory(const Napi::CallbackInfo& info) : ObjectWrap(info)
{
    if (info.Length() > 0 && info[0].IsObject()) {
        auto jsArg = info[0].As<Object>();
        auto napiCtx = NapiEglContext::Unwrap(jsArg);
        if (napiCtx) {
            // Legacy form: a bare EglContext instance.
            sharedContext_ = napiCtx->Get();
        } else {
            ApplyDecoderFactoryOptions(jsArg);
        }
    }

    if (!sharedContext_) {
        sharedContext_ = EglEnv::GetDefault().GetContext();
    }

    info.This().As<Object>().TypeTag(&kTypeTag);
}

void NapiHardwareVideoDecoderFactory::ApplyDecoderFactoryOptions(const Napi::Object& jsOptions)
{
    auto env = jsOptions.Env();

    if (jsOptions.Has(NapiHardwareVideoDecoderFactory::kAttributeNameSharedContext)) {
        auto napiCtx = NapiEglContext::Unwrap(
            jsOptions.Get(NapiHardwareVideoDecoderFactory::kAttributeNameSharedContext).As<Object>());
        if (napiCtx) {
            sharedContext_ = napiCtx->Get();
        }
    }

    std::vector<std::string> forceSW;
    std::vector<std::string> forceHW;
    bool hasSW = ReadCodecList(jsOptions, NapiHardwareVideoDecoderFactory::kAttributeNameForceSWCodecList, forceSW);
    if (env.IsExceptionPending()) {
        NAPI_THROW_VOID(env.GetAndClearPendingException());
    }
    bool hasHW = ReadCodecList(jsOptions, NapiHardwareVideoDecoderFactory::kAttributeNameForceHWCodecList, forceHW);
    if (env.IsExceptionPending()) {
        NAPI_THROW_VOID(env.GetAndClearPendingException());
    }
    if (hasSW || hasHW) {
        forceLists_ = adapter::CodecForceLists(std::move(forceSW), std::move(forceHW));
        forceListsSet_ = true;
    }
}

Napi::Value NapiHardwareVideoDecoderFactory::GetSharedContext(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    return NapiEglContext::NewInstance(info.Env(), sharedContext_);
}

Napi::Value NapiHardwareVideoDecoderFactory::ToJson(const Napi::CallbackInfo& info)
{
    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", "NapiHardwareVideoDecoderFactory");
#endif
    return json;
}

thread_local FunctionReference NapiSoftwareVideoDecoderFactory::constructor_;

void NapiSoftwareVideoDecoderFactory::Init(Napi::Env env, Napi::Object exports)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Function func = DefineClass(
        env, kClassName,
        {
            InstanceMethod<&NapiSoftwareVideoDecoderFactory::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
    constructor_.SuppressDestruct();
}

NapiSoftwareVideoDecoderFactory::~NapiSoftwareVideoDecoderFactory() = default;

NapiSoftwareVideoDecoderFactory::NapiSoftwareVideoDecoderFactory(const Napi::CallbackInfo& info) : ObjectWrap(info)
{
    info.This().As<Object>().TypeTag(&kTypeTag);
}

Napi::Value NapiSoftwareVideoDecoderFactory::ToJson(const Napi::CallbackInfo& info)
{
    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", "NapiSoftwareVideoDecoderFactory");
#endif
    return json;
}

std::unique_ptr<VideoDecoderFactory> CreateVideoDecoderFactory(const Napi::Object& jsVideoDecoderFactory)
{
    if (NAPI_CHECK_TYPE_TAG(jsVideoDecoderFactory, NapiHardwareVideoDecoderFactory)) {
        auto napiFactory = NapiHardwareVideoDecoderFactory::Unwrap(jsVideoDecoderFactory);
        auto sharedContext = napiFactory->GetSharedContext();
        auto hardwareFactory =
            std::make_unique<adapter::HardwareVideoDecoderFactory>(sharedContext, napiFactory->GetForceLists());
        // With force lists in play the factory is wrapped by the default
        // factory so force-SW falls back to software and force-HW skips the
        // fallback; without lists the legacy hardware-only behavior is kept.
        if (napiFactory->HasForceLists()) {
            return std::make_unique<adapter::DefaultVideoDecoderFactory>(
                std::move(hardwareFactory), napiFactory->GetForceLists());
        }
        return hardwareFactory;
    } else if (NAPI_CHECK_TYPE_TAG(jsVideoDecoderFactory, NapiSoftwareVideoDecoderFactory)) {
        return std::make_unique<adapter::SoftwareVideoDecoderFactory>();
    }

    return nullptr;
}

std::unique_ptr<VideoDecoderFactory> CreateDefaultVideoDecoderFactory()
{
    return std::make_unique<adapter::DefaultVideoDecoderFactory>(EglEnv::GetDefault().GetContext());
}

} // namespace webrtc
