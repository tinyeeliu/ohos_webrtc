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

#include "sctp_transport.h"
#include "dtls_transport.h"
#include "peer_connection.h"
#include "peer_connection_factory.h"

#include "rtc_base/logging.h"

namespace webrtc {

using namespace Napi;

const char kEnumRTCSctpTransportStateConnecting[] = "connecting";
const char kEnumRTCSctpTransportStateConnected[] = "connected";
const char kEnumRTCSctpTransportStateClosed[] = "closed";

const char kAttributeNameMaxChannels[] = "maxChannels";
const char kAttributeNameMaxMessageSize[] = "maxMessageSize";
const char kAttributeNameTransport[] = "transport";
const char kAttributeNameState[] = "state";
const char kMethodNameToJson[] = "toJSON";

const char kEventStateChange[] = "statechange";

FunctionReference NapiSctpTransport::constructor_;

void NapiSctpTransport::Init(Napi::Env env, Napi::Object exports)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Function func = DefineClass(
        env, "RTCSctpTransport",
        {
            InstanceAccessor<&NapiSctpTransport::GetEventHandler, &NapiSctpTransport::SetEventHandler>(
                "onstatechange", napi_default, (void*)kEventStateChange),
            InstanceAccessor<&NapiSctpTransport::GetState>(kAttributeNameState),
            InstanceAccessor<&NapiSctpTransport::GetTransport>(kAttributeNameTransport),
            InstanceAccessor<&NapiSctpTransport::GetMaxChannels>(kAttributeNameMaxChannels),
            InstanceAccessor<&NapiSctpTransport::GetMaxMessageSize>(kAttributeNameMaxMessageSize),
            InstanceMethod<&NapiSctpTransport::ToJson>(kMethodNameToJson),
        });
    exports.Set("RTCSctpTransport", func);

    constructor_ = Persistent(func);
}

Napi::Object NapiSctpTransport::NewInstance(
    Napi::Env env, rtc::scoped_refptr<SctpTransportInterface> transport, NapiPeerConnectionWrapper* pcWrapper)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (!transport || !pcWrapper) {
        NAPI_THROW(Error::New(env, "Invalid argument"), Object());
    }

    auto externalTransport = External<SctpTransportInterface>::New(
        env, transport.release(), [](Napi::Env, SctpTransportInterface* transport) { transport->Release(); });

    auto externalPcWrapper = External<NapiPeerConnectionWrapper>::New(env, pcWrapper);

    return constructor_.New({externalTransport, externalPcWrapper});
}

NapiSctpTransport::NapiSctpTransport(const Napi::CallbackInfo& info) : Napi::ObjectWrap<NapiSctpTransport>(info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (info[0].IsExternal()) {
        auto sctpTransport = info[0].As<External<SctpTransportInterface>>().Data();
        sctpTransport_ = rtc::scoped_refptr<SctpTransportInterface>(sctpTransport);
    }

    if (info.Length() > 1 && info[1].IsExternal()) {
        pcWrapper_ = info[1].As<External<NapiPeerConnectionWrapper>>().Data();
    }

    if (!pcWrapper_ || !sctpTransport_) {
        NAPI_THROW_VOID(Error::New(info.Env(), "Invalid argument"));
    }

    auto factoryWrapper = pcWrapper_->GetPeerConnectionFactoryWrapper();
    factoryWrapper->GetNetworkThread()->BlockingCall([this] { sctpTransport_->RegisterObserver(this); });
}

NapiSctpTransport::~NapiSctpTransport()
{
    RTC_DLOG(LS_INFO) << __FUNCTION__;

    auto factoryWrapper = pcWrapper_->GetPeerConnectionFactoryWrapper();
    factoryWrapper->GetNetworkThread()->BlockingCall([this] { sctpTransport_->UnregisterObserver(); });

    std::lock_guard<std::mutex> lock(mutex_);
    if (eventHandler_.tsfn) {
        eventHandler_.tsfn.Release();
    }
}

rtc::scoped_refptr<SctpTransportInterface> NapiSctpTransport::Get() const
{
    return sctpTransport_;
}

Napi::Value NapiSctpTransport::GetMaxChannels(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    auto maxChannels = sctpTransport_->Information().MaxChannels();
    if (maxChannels.has_value()) {
        return Number::New(info.Env(), maxChannels.value());
    }
    return info.Env().Undefined();
}

Napi::Value NapiSctpTransport::GetMaxMessageSize(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    auto maxMessageSize = sctpTransport_->Information().MaxMessageSize();
    if (maxMessageSize.has_value()) {
        return Number::New(info.Env(), maxMessageSize.value());
    }
    return info.Env().Undefined();
}

Napi::Value NapiSctpTransport::GetState(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    switch (sctpTransport_->Information().state()) {
        case SctpTransportState::kConnecting:
            return String::New(info.Env(), kEnumRTCSctpTransportStateConnecting);
        case SctpTransportState::kConnected:
            return String::New(info.Env(), kEnumRTCSctpTransportStateConnected);
        case SctpTransportState::kClosed:
            return String::New(info.Env(), kEnumRTCSctpTransportStateClosed);
        default:
            break;
    }

    NAPI_THROW(Error::New(info.Env(), "Invalid state"), info.Env().Undefined());
}

Napi::Value NapiSctpTransport::GetTransport(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto transport = sctpTransport_->Information().dtls_transport();
    if (!transport) {
        NAPI_THROW(Error::New(info.Env(), "No transport"), info.Env().Undefined());
    }

    return NapiDtlsTransport::NewInstance(info.Env(), transport, pcWrapper_);
}

Napi::Value NapiSctpTransport::GetEventHandler(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    std::lock_guard<std::mutex> lock(mutex_);
    if (eventHandler_.ref.IsEmpty()) {
        return info.Env().Null();
    }
    return eventHandler_.ref.Value();
}

void NapiSctpTransport::SetEventHandler(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (eventHandler_.tsfn) {
            eventHandler_.tsfn.Release();
        }
    }

    if (value.IsFunction()) {
        auto fn = value.As<Napi::Function>();

        Reference<Napi::Value>* context = new Reference<Napi::Value>;
        *context = Persistent(info.This());

        EventHandler handler;
        handler.ref = Persistent(fn);
        handler.tsfn = ThreadSafeFunction::New(
            fn.Env(), fn, "SetEventHandler", 0, 1, context, [](Napi::Env env, Reference<Napi::Value>* ctx) {
                ctx->Reset();
                delete ctx;
            });
        std::lock_guard<std::mutex> lock(mutex_);
        eventHandler_ = std::move(handler);
    } else if (value.IsNull()) {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " value is null";
    } else {
        NAPI_THROW_VOID(Error::New(info.Env(), "value is error"));
    }
}

void NapiSctpTransport::OnStateChange(SctpTransportInformation info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    std::lock_guard<std::mutex> lock(mutex_);
    auto& tsfn = eventHandler_.tsfn;

    if (!tsfn) {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " ThreadSafeFunction is not initialized.";
        return;
    }

    Reference<Napi::Value>* context = tsfn.GetContext();
    napi_status status = tsfn.NonBlockingCall([context](Napi::Env env, Napi::Function jsCallback) {
        auto jsEvent = Object::New(env);
        jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
    });

    if (status != napi_ok) {
        RTC_LOG(LS_ERROR) << " tsfn call error: " << status;
    }
}

Napi::Value NapiSctpTransport::ToJson(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", String::New(info.Env(), "NapiSctpTransport"));
#endif

    return json;
}

} // namespace webrtc
