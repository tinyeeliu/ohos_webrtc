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

#include "ice_transport.h"
#include "peer_connection.h"
#include "peer_connection_factory.h"

#include "rtc_base/logging.h"

namespace webrtc {

using namespace Napi;

const char kEnumRTCIceRoleUnKnown[] = "unknown";
const char kEnumRTCIceRoleControlling[] = "controlling";
const char kEnumRTCIceRoleControlled[] = "controlled";

const char kEnumRTCIceTransportStateChecking[] = "checking";
const char kEnumRTCIceTransportStateClosed[] = "closed";
const char kEnumRTCIceTransportStateCompleted[] = "completed";
const char kEnumRTCIceTransportStateConnected[] = "connected";
const char kEnumRTCIceTransportStateDisconnected[] = "disconnected";
const char kEnumRTCIceTransportStateFailed[] = "failed";
const char kEnumRTCIceTransportStateNew[] = "new";

const char kEnumRTCIceComponentRtp[] = "rtp";
const char kEnumRTCIceComponentRtcp[] = "rtcp";

const char kEnumRTCIceGathererStateComplete[] = "complete";
const char kEnumRTCIceGathererStateGathering[] = "gathering";
const char kEnumRTCIceGathererStateNew[] = "new";

const char kClassName[] = "RTCIceTransport";

const char kAttributeNameRole[] = "role";
const char kAttributeNameComponent[] = "component";
const char kAttributeNameState[] = "state";
const char kAttributeNameGatheringState[] = "gatheringState";
const char kAttributeNameOnStateChange[] = "onstatechange";
const char kAttributeNameOnGatheringStateChange[] = "ongatheringstatechange";
const char kAttributeNameOnSelectedCandidatePairChange[] = "onselectedcandidatepairchange";

const char kEventNameStateChange[] = "statechange";
const char kEventNameGatheringStateChange[] = "gatheringstatechange";
const char kEventNameSelectedCandidatePairChange[] = "selectedcandidatepairchange";

const char kMethodNameGetSelectedCandidatePair[] = "getSelectedCandidatePair";
const char kMethodNameToJson[] = "toJSON";

FunctionReference NapiIceTransport::constructor_;

void NapiIceTransport::Init(Napi::Env env, Napi::Object exports)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Function func = DefineClass(
        env, kClassName,
        {
            InstanceAccessor<&NapiIceTransport::GetRole>(kAttributeNameRole),
            InstanceAccessor<&NapiIceTransport::GetComponent>(kAttributeNameComponent),
            InstanceAccessor<&NapiIceTransport::GetState>(kAttributeNameState),
            InstanceAccessor<&NapiIceTransport::GetGatheringState>(kAttributeNameGatheringState),
            InstanceAccessor<&NapiIceTransport::GetEventHandler, &NapiIceTransport::SetEventHandler>(
                kAttributeNameOnStateChange, napi_default, (void*)kEventNameStateChange),
            InstanceAccessor<&NapiIceTransport::GetEventHandler, &NapiIceTransport::SetEventHandler>(
                kAttributeNameOnGatheringStateChange, napi_default, (void*)kEventNameGatheringStateChange),
            InstanceAccessor<&NapiIceTransport::GetEventHandler, &NapiIceTransport::SetEventHandler>(
                kAttributeNameOnSelectedCandidatePairChange, napi_default,
                (void*)kEventNameSelectedCandidatePairChange),
            InstanceMethod<&NapiIceTransport::GetSelectedCandidatePair>(kMethodNameGetSelectedCandidatePair),
            InstanceMethod<&NapiIceTransport::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
}

NapiIceTransport::~NapiIceTransport()
{
    RTC_DLOG(LS_INFO) << __FUNCTION__;

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& handler : eventHandlers_) {
        handler.second.tsfn.Release();
    }
}

Napi::Object NapiIceTransport::NewInstance(
    Napi::Env env, rtc::scoped_refptr<IceTransportInterface> iceTransport, NapiPeerConnectionWrapper* pcWrapper)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (!iceTransport || !pcWrapper) {
        NAPI_THROW(Error::New(env, "Invalid argument"), Object::New(env));
    }

    auto externalTransport = External<IceTransportInterface>::New(
        env, iceTransport.release(), [](Napi::Env env, IceTransportInterface* transport) { transport->Release(); });

    auto externalPcWrapper = External<NapiPeerConnectionWrapper>::New(env, pcWrapper);

    return constructor_.New({externalTransport, externalPcWrapper});
}

NapiIceTransport::NapiIceTransport(const Napi::CallbackInfo& info) : Napi::ObjectWrap<NapiIceTransport>(info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    rtc::scoped_refptr<IceTransportInterface> iceTransport;

    if (info[0].IsExternal()) {
        auto iceTransport = info[0].As<External<IceTransportInterface>>().Data();
        iceTransport_ = rtc::scoped_refptr<IceTransportInterface>(iceTransport);
    }

    if (info.Length() > 1 && info[1].IsExternal()) {
        pcWrapper_ = info[1].As<External<NapiPeerConnectionWrapper>>().Data();

        auto factoryWrapper = pcWrapper_->GetPeerConnectionFactoryWrapper();
        cricket::IceTransportInternal* internal = nullptr;

        factoryWrapper->GetNetworkThread()->BlockingCall([&internal, this] {
            internal = iceTransport_->internal();
            iceTransportState_ = internal->GetIceTransportState();
            iceGatheringState_ = internal->gathering_state();

            internal->SignalIceTransportStateChanged.connect(this, &NapiIceTransport::OnStateChange);
            internal->SignalGatheringState.connect(this, &NapiIceTransport::OnGatheringStateChange);
            internal->SignalCandidatePairChanged.connect(this, &NapiIceTransport::OnSelectedCandidatePairChange);
        });
    }
}

Napi::Value NapiIceTransport::GetRole(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    auto factoryWrapper = pcWrapper_->GetPeerConnectionFactoryWrapper();

    cricket::IceRole iceRole;

    factoryWrapper->GetNetworkThread()->BlockingCall(
        [&iceRole, this] { iceRole = iceTransport_->internal()->GetIceRole(); });

    switch (iceRole) {
        case cricket::ICEROLE_CONTROLLING:
            return String::New(info.Env(), kEnumRTCIceRoleControlling);
        case cricket::ICEROLE_CONTROLLED:
            return String::New(info.Env(), kEnumRTCIceRoleControlled);
        case cricket::ICEROLE_UNKNOWN:
            return String::New(info.Env(), kEnumRTCIceRoleUnKnown);
        default:
            break;
    }

    NAPI_THROW(Error::New(info.Env(), "Invalid role"), info.Env().Undefined());
}

Napi::Value NapiIceTransport::GetComponent(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    auto factoryWrapper = pcWrapper_->GetPeerConnectionFactoryWrapper();

    int component = 0;

    factoryWrapper->GetNetworkThread()->BlockingCall(
        [&component, this] { component = iceTransport_->internal()->component(); });

    // https://www.w3.org/TR/webrtc/#dfn-candidate-attribute
    // https://www.rfc-editor.org/rfc/rfc5245#section-15.1
    if (component == 1) {
        return String::New(info.Env(), kEnumRTCIceComponentRtp);
    }
    if (component == 2) {
        return String::New(info.Env(), kEnumRTCIceComponentRtcp);
    }

    NAPI_THROW(Error::New(info.Env(), "Invalid component"), info.Env().Undefined());
}

Napi::Value NapiIceTransport::GetState(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    IceTransportState iceTransportState = iceTransportState_;
    switch (iceTransportState) {
        case IceTransportState::kNew:
            return String::New(info.Env(), kEnumRTCIceTransportStateNew);
        case IceTransportState::kChecking:
            return String::New(info.Env(), kEnumRTCIceTransportStateChecking);
        case IceTransportState::kConnected:
            return String::New(info.Env(), kEnumRTCIceTransportStateConnected);
        case IceTransportState::kCompleted:
            return String::New(info.Env(), kEnumRTCIceTransportStateCompleted);
        case IceTransportState::kFailed:
            return String::New(info.Env(), kEnumRTCIceTransportStateFailed);
        case IceTransportState::kDisconnected:
            return String::New(info.Env(), kEnumRTCIceTransportStateDisconnected);
        case IceTransportState::kClosed:
            return String::New(info.Env(), kEnumRTCIceTransportStateClosed);
        default:
            break;
    }

    NAPI_THROW(Error::New(info.Env(), "Invalid state"), info.Env().Undefined());
}

Napi::Value NapiIceTransport::GetGatheringState(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    cricket::IceGatheringState iceGatheringState = iceGatheringState_;
    switch (iceGatheringState) {
        case cricket::kIceGatheringNew:
            return String::New(info.Env(), kEnumRTCIceGathererStateNew);
        case cricket::kIceGatheringGathering:
            return String::New(info.Env(), kEnumRTCIceGathererStateGathering);
        case cricket::kIceGatheringComplete:
            return String::New(info.Env(), kEnumRTCIceGathererStateComplete);
        default:
            break;
    }

    NAPI_THROW(Error::New(info.Env(), "Invalid gathering state"), info.Env().Undefined());
}

Napi::Value NapiIceTransport::GetEventHandler(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = eventHandlers_.find((const char*)info.Data());
    if (it == eventHandlers_.end()) {
        return info.Env().Null();
    }

    return it->second.ref.Value();
}

void NapiIceTransport::SetEventHandler(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

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

Napi::Value NapiIceTransport::GetSelectedCandidatePair(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    auto factoryWrapper = pcWrapper_->GetPeerConnectionFactoryWrapper();

    absl::optional<cricket::CandidatePair> candidatePair;
    factoryWrapper->GetNetworkThread()->BlockingCall(
        [&candidatePair, this] { candidatePair = iceTransport_->internal()->GetSelectedCandidatePair(); });

    if (!candidatePair) {
        return info.Env().Null();
    }

    auto obj = Object::New(info.Env());
    obj.Set("local", NativeToJsCandidate(info.Env(), candidatePair->local_candidate()));
    obj.Set("remote", NativeToJsCandidate(info.Env(), candidatePair->remote_candidate()));
    return obj;
}

Napi::Value NapiIceTransport::ToJson(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", String::New(info.Env(), "NapiIceTransport"));
#endif

    return json;
}

void NapiIceTransport::OnStateChange(cricket::IceTransportInternal* iceTransport)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    iceTransportState_ = iceTransport_->internal()->GetIceTransportState();

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

void NapiIceTransport::OnGatheringStateChange(cricket::IceTransportInternal* iceTransport)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    iceGatheringState_ = iceTransport->gathering_state();

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = eventHandlers_.find(kEventNameGatheringStateChange);
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

void NapiIceTransport::OnSelectedCandidatePairChange(const cricket::CandidatePairChangeEvent& event)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = eventHandlers_.find(kEventNameSelectedCandidatePairChange);
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