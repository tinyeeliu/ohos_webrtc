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

#include "dtls_transport.h"
#include "ice_transport.h"
#include "peer_connection.h"
#include "peer_connection_factory.h"

#include "rtc_base/logging.h"

namespace webrtc {

using namespace Napi;

const char kEnumDtlsTransportStateNew[] = "new";
const char kEnumDtlsTransportStateConnecting[] = "connecting";
const char kEnumDtlsTransportStateConnected[] = "connected";
const char kEnumDtlsTransportStateClosed[] = "closed";
const char kEnumDtlsTransportStateFailed[] = "failed";

const char kClassName[] = "RTCDtlsTransport";

const char kAttributeNameIceTransport[] = "iceTransport";
const char kAttributeNameState[] = "state";
const char kAttributeNameOnStateChange[] = "onstatechange";
const char kAttributeNameOnError[] = "onerror";

const char kMethodNameGetRemoteCertificates[] = "getRemoteCertificates";
const char kMethodNameToJson[] = "toJSON";

const char kEventNameStateChange[] = "statechange";
const char kEventNameError[] = "error";

FunctionReference NapiDtlsTransport::constructor_;

void NapiDtlsTransport::Init(Napi::Env env, Napi::Object exports)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Function func = DefineClass(
        env, kClassName,
        {
            InstanceAccessor<&NapiDtlsTransport::GetIceTransport>(kAttributeNameIceTransport),
            InstanceAccessor<&NapiDtlsTransport::GetState>(kAttributeNameState),
            InstanceAccessor<&NapiDtlsTransport::GetEventHandler, &NapiDtlsTransport::SetEventHandler>(
                kAttributeNameOnStateChange, napi_default, (void*)kEventNameStateChange),
            InstanceAccessor<&NapiDtlsTransport::GetEventHandler, &NapiDtlsTransport::SetEventHandler>(
                kAttributeNameOnError, napi_default, (void*)kEventNameError),
            InstanceMethod<&NapiDtlsTransport::GetRemoteCertificates>(kMethodNameGetRemoteCertificates),
            InstanceMethod<&NapiDtlsTransport::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
}

NapiDtlsTransport::~NapiDtlsTransport()
{
    RTC_DLOG(LS_INFO) << __FUNCTION__;

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& handler : eventHandlers_) {
        handler.second.tsfn.Release();
    }
}

Napi::Object NapiDtlsTransport::NewInstance(
    Napi::Env env, rtc::scoped_refptr<DtlsTransportInterface> dtlsTransport, NapiPeerConnectionWrapper* pcWrapper)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (!dtlsTransport || !pcWrapper) {
        NAPI_THROW(Error::New(env, "Invalid argument"), Object::New(env));
    }

    auto externalTransport = External<DtlsTransportInterface>::New(
        env, dtlsTransport.release(), [](Napi::Env env, DtlsTransportInterface* transport) { transport->Release(); });

    auto externalPcWrapper = External<NapiPeerConnectionWrapper>::New(env, pcWrapper);

    return constructor_.New({externalTransport, externalPcWrapper});
}

NapiDtlsTransport::NapiDtlsTransport(const Napi::CallbackInfo& info) : Napi::ObjectWrap<NapiDtlsTransport>(info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    rtc::scoped_refptr<DtlsTransportInterface> dtlsTransport;

    if (info.Length() > 0 && info[0].IsExternal()) {
        dtlsTransport = info[0].As<External<DtlsTransportInterface>>().Data();
    } else {
        // nothing to do now
    }

    if (!dtlsTransport) {
        NAPI_THROW_VOID(Error::New(info.Env(), "Invalid argument"));
    }

    dtlsTransport_ = dtlsTransport;

    if (info.Length() > 1 && info[1].IsExternal()) {
        pcWrapper_ = info[1].As<External<NapiPeerConnectionWrapper>>().Data();
    }

    auto factoryWrapper = pcWrapper_->GetPeerConnectionFactoryWrapper();
    factoryWrapper->GetNetworkThread()->PostTask([this] { dtlsTransport_->RegisterObserver(this); });
}

Napi::Value NapiDtlsTransport::GetIceTransport(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    return NapiIceTransport::NewInstance(info.Env(), dtlsTransport_->ice_transport(), pcWrapper_);
}

Napi::Value NapiDtlsTransport::GetState(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto information = dtlsTransport_->Information();

    switch (information.state()) {
        case DtlsTransportState::kNew:
            return String::New(info.Env(), kEnumDtlsTransportStateNew);
        case DtlsTransportState::kConnecting:
            return String::New(info.Env(), kEnumDtlsTransportStateConnecting);
        case DtlsTransportState::kConnected:
            return String::New(info.Env(), kEnumDtlsTransportStateConnected);
        case DtlsTransportState::kClosed:
            return String::New(info.Env(), kEnumDtlsTransportStateClosed);
        case DtlsTransportState::kFailed:
            return String::New(info.Env(), kEnumDtlsTransportStateFailed);
        default:
            break;
    }
    NAPI_THROW(Error::New(info.Env(), "Invalid state"), info.Env().Undefined());
}

Napi::Value NapiDtlsTransport::GetEventHandler(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = eventHandlers_.find((const char*)info.Data());
    if (it == eventHandlers_.end()) {
        return info.Env().Null();
    }

    return it->second.ref.Value();
}

void NapiDtlsTransport::SetEventHandler(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    const auto type = (const char*)info.Data();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = eventHandlers_.find(type);
        if (it != eventHandlers_.end()) {
            it->second.tsfn.Release();
            eventHandlers_.erase(it);
        }
    }

    if (value.IsFunction()) {
        auto fn = value.As<Function>();

        Reference<Napi::Value>* context = new Reference<Napi::Value>;
        *context = Persistent(info.This());

        EventHandler handler;
        handler.ref = Persistent(fn);
        handler.tsfn =
            ThreadSafeFunction::New(fn.Env(), fn, type, 0, 1, context, [](Napi::Env env, Reference<Napi::Value>* ctx) {
                ctx->Reset();
                delete ctx;
            });
        std::lock_guard<std::mutex> lock(mutex_);
        eventHandlers_[type] = std::move(handler);
    } else if (value.IsNull()) {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " value is null";
    } else {
        NAPI_THROW_VOID(Error::New(info.Env(), "value is error"));
    }
}

Napi::Value NapiDtlsTransport::GetRemoteCertificates(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto information = dtlsTransport_->Information();

    auto certChain = information.remote_ssl_certificates();
    if (!certChain || certChain->GetSize() == 0) {
        RTC_DLOG(LS_ERROR) << "Certificate chain is empty!";
        return info.Env().Null();
    }

    Napi::ArrayBuffer certArray = Napi::ArrayBuffer::New(info.Env(), certChain->GetSize());

    for (size_t i = 0; i < certChain->GetSize(); i++) {
        const rtc::SSLCertificate& certificate = certChain->Get(i);
        certArray.Set(i, Napi::String::New(info.Env(), certificate.ToPEMString()));
    }

    return certArray;
}

Napi::Value NapiDtlsTransport::ToJson(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", "NapiDtlsTransport");
#endif

    return json;
}

void NapiDtlsTransport::OnStateChange(DtlsTransportInformation info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = eventHandlers_.find(kEventNameStateChange);
    if (it == eventHandlers_.end()) {
        return;
    }

    auto& tsfn = it->second.tsfn;
    Reference<Napi::Value>* context = tsfn.GetContext();
    napi_status status = tsfn.NonBlockingCall([context](Napi::Env env, Napi::Function jsCallback) {
        auto jsEvent = Object::New(env);
        jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
    });
    if (status != napi_ok) {
        RTC_LOG(LS_ERROR) << " tsfn call error: " << status;
    }
}

void NapiDtlsTransport::OnError(RTCError error)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = eventHandlers_.find(kEventNameError);
    if (it == eventHandlers_.end()) {
        return;
    }

    auto& tsfn = it->second.tsfn;
    Reference<Napi::Value>* context = tsfn.GetContext();
    napi_status status = tsfn.NonBlockingCall([context](Napi::Env env, Napi::Function jsCallback) {
        auto jsEvent = Object::New(env);
        jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
    });
    if (status != napi_ok) {
        RTC_LOG(LS_ERROR) << " tsfn call error: " << status;
    }
}

} // namespace webrtc
