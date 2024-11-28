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

#include "peer_connection.h"
#include "peer_connection_factory.h"

#include <future>
#include <thread>

#include <hilog/log.h>

#include "rtc_base/logging.h"

#include "configuration.h"
#include "data_channel.h"
#include "ice_candidate.h"
#include "media_stream.h"
#include "media_stream_track.h"
#include "rtp_receiver.h"
#include "rtp_sender.h"
#include "rtp_transceiver.h"
#include "sctp_transport.h"
#include "session_description.h"
#include "utils/marcos.h"
#include "async_work/async_worker_certificate.h"
#include "async_work/async_worker_get_stats.h"

namespace webrtc {

using namespace Napi;

const char kEnumSignalingStateStable[] = "stable";
const char kEnumSignalingStateHaveLocalOffer[] = "have-local-offer";
const char kEnumSignalingStateHaveLocalPranswer[] = "have-local-pranswer";
const char kEnumSignalingStateHaveRemoteOffer[] = "have-remote-offer";
const char kEnumSignalingStateHaveRemotePranswer[] = "have-remote-pranswer";
const char kEnumSignalingStateClosed[] = "closed";

const char kEnumIceGatheringStateNew[] = "new";
const char kEnumIceGatheringStateGathering[] = "gathering";
const char kEnumIceGatheringStateComplete[] = "complete";

const char kEnumIceConnectionStateNew[] = "new";
const char kEnumIceConnectionStateChecking[] = "checking";
const char kEnumIceConnectionStateCompleted[] = "completed";
const char kEnumIceConnectionStateConnected[] = "connected";
const char kEnumIceConnectionStateDisconnected[] = "disconnected";
const char kEnumIceConnectionStateFailed[] = "failed";
const char kEnumIceConnectionStateClosed[] = "closed";

const char kEnumPeerConnectionStateNew[] = "new";
const char kEnumPeerConnectionStateConnecting[] = "connecting";
const char kEnumPeerConnectionStateConnected[] = "connected";
const char kEnumPeerConnectionStateDisconnected[] = "disconnected";
const char kEnumPeerConnectionStateFailed[] = "failed";
const char kEnumPeerConnectionStateClosed[] = "closed";

const char kClassName[] = "RTCPeerConnection";

const char kAttributeNameCanTrickleIceCandidates[] = "canTrickleIceCandidates";
const char kAttributeNameSignalingState[] = "signalingState";
const char kAttributeNameIceGatheringState[] = "iceGatheringState";
const char kAttributeNameIceConnectionState[] = "iceConnectionState";
const char kAttributeNameConnectionState[] = "connectionState";
const char kAttributeNameLocalDescription[] = "localDescription";
const char kAttributeNameRemoteDescription[] = "remoteDescription";
const char kAttributeNameCurrentLocalDescription[] = "currentLocalDescription";
const char kAttributeNameCurrentRemoteDescription[] = "currentRemoteDescription";
const char kAttributeNamePendingLocalDescription[] = "pendingLocalDescription";
const char kAttributeNamePendingRemoteDescription[] = "pendingRemoteDescription";
const char kAttributeNameSctp[] = "sctp";
const char kAttributeNameOnConnectionStateChange[] = "onconnectionstatechange";
const char kAttributeNameOnIceCandidate[] = "onicecandidate";
const char kAttributeNameOnIceCandidateError[] = "onicecandidateerror";
const char kAttributeNameOnIceConnectionStateChange[] = "oniceconnectionstatechange";
const char kAttributeNameOnIceGatheringStateChange[] = "onicegatheringstatechange";
const char kAttributeNameOnNegotiationNeeded[] = "onnegotiationneeded";
const char kAttributeNameOnSignalingStateChange[] = "onsignalingstatechange";
const char kAttributeNameOnTrack[] = "ontrack";
const char kAttributeNameOnDataChannel[] = "ondatachannel";

const char kMethodNameAddTrack[] = "addTrack";
const char kMethodNameRemoveTrack[] = "removeTrack";
const char kMethodNameSetLocalDescription[] = "setLocalDescription";
const char kMethodNameSetRemoteDescription[] = "setRemoteDescription";
const char kMethodNameCreateOffer[] = "createOffer";
const char kMethodNameCreateAnswer[] = "createAnswer";
const char kMethodNameCreateDataChannel[] = "createDataChannel";
const char kMethodNameAddIceCandidate[] = "addIceCandidate";
const char kMethodNameGetSenders[] = "getSenders";
const char kMethodNameGetReceivers[] = "getReceivers";
const char kMethodNameGetTransceivers[] = "getTransceivers";
const char kMethodNameGetConfiguration[] = "getConfiguration";
const char kMethodNameRestartIce[] = "restartIce";
const char kMethodNameSetConfiguration[] = "setConfiguration";
const char kMethodNameAddTransceiver[] = "addTransceiver";
const char kMethodNameClose[] = "close";
const char kMethodNameGetStats[] = "getStats";
const char kMethodNameToJson[] = "toJSON";

const char kStaticMethodNameGenerateCertificate[] = "generateCertificate";

const char kEventConnectionStateChange[] = "connectionstatechange";
const char kEventIceCandidate[] = "icecandidate";
const char kEventIceCandidateError[] = "icecandidateerror";
const char kEventIceConnectionStateChange[] = "iceconnectionstatechange";
const char kEventIceGatheringStateChange[] = "icegatheringstatechange";
const char kEventNegotiationNeeded[] = "negotiationneeded";
const char kEventSignalingStateChange[] = "signalingstatechange";
const char kEventTrack[] = "track";
const char kEventDataChannel[] = "datachannel";

template <typename T>
class BaseSdpObserver : public T {
public:
    virtual ~BaseSdpObserver()
    {
        tsfn_.Release();
    }

    Promise GetPromise()
    {
        return deferred_.Promise();
    }

protected:
    explicit BaseSdpObserver(Promise::Deferred deferred) : deferred_(deferred) {}

    ThreadSafeFunction tsfn_;
    Promise::Deferred deferred_;
};

class CreateSdpObserver : public BaseSdpObserver<CreateSessionDescriptionObserver> {
public:
    explicit CreateSdpObserver(Napi::Env env) : CreateSdpObserver(env, Promise::Deferred::New(env)) {}

    CreateSdpObserver(Napi::Env env, Promise::Deferred deferred)
        : BaseSdpObserver<CreateSessionDescriptionObserver>(deferred)
    {
        tsfn_ = ThreadSafeFunction::New(
            env,
            Function::New(
                env,
                [deferred](const CallbackInfo& info) {
                    auto success = info[0].As<Boolean>().Value();
                    if (success) {
                        auto desc = info[1].As<External<SessionDescriptionInterface>>().Data();

                        std::string sdp;
                        desc->ToString(&sdp);
                        RTC_DLOG(LS_VERBOSE) << "sdp: " << sdp;

                        auto result = Napi::Object::New(info.Env());
                        result.Set("sdp", Napi::String::New(info.Env(), sdp));
                        result.Set("type", Napi::String::New(info.Env(), webrtc::SdpTypeToString(desc->GetType())));
                        deferred.Resolve(result);
                    } else {
                        auto error = info[1].As<External<RTCError>>().Data();
                        auto message = error->message();
                        deferred.Reject(
                            Error::New(info.Env(), (message && strlen(message) > 0) ? message : "unknown error")
                                .Value());
                    }
                }),
            "CreateSdpObserver", 0, 1);
    }

protected:
    void OnSuccess(SessionDescriptionInterface* desc) override
    {
        RTC_LOG(LS_INFO) << "CreateSessionDescription success: " << desc;

        tsfn_.BlockingCall([desc](Napi::Env env, Napi::Function jsCallback) {
            auto externalDesc = External<SessionDescriptionInterface>::New(
                env, desc, [](Napi::Env, SessionDescriptionInterface* desc) { delete desc; });
            jsCallback.Call({Boolean::New(env, true), externalDesc});
        });
    }

    void OnFailure(RTCError error) override
    {
        RTC_LOG(LS_ERROR) << "CreateSessionDescription failed";

        tsfn_.BlockingCall([error = new RTCError(std::move(error))](Napi::Env env, Napi::Function jsCallback) {
            auto externalError =
                External<RTCError>::New(env, new RTCError(RTCError::OK()), [](Napi::Env, RTCError* e) { delete e; });
            jsCallback.Call({Boolean::New(env, false), externalError});
        });
    }
};

class SetLocalSdpObserver : public BaseSdpObserver<SetLocalDescriptionObserverInterface> {
public:
    explicit SetLocalSdpObserver(Napi::Env env) : SetLocalSdpObserver(env, Promise::Deferred::New(env)) {}

    SetLocalSdpObserver(Napi::Env env, Promise::Deferred deferred)
        : BaseSdpObserver<SetLocalDescriptionObserverInterface>(deferred)
    {
        tsfn_ = ThreadSafeFunction::New(
            env,
            Function::New(
                env,
                [deferred](const CallbackInfo& info) {
                    auto error = info[0].As<External<RTCError>>().Data();
                    if (error->ok()) {
                        deferred.Resolve(info.Env().Undefined());
                    } else {
                        auto message = error->message();
                        deferred.Reject(
                            Error::New(info.Env(), (message && strlen(message) > 0) ? message : "unknown error")
                                .Value());
                    }
                }),
            "SetLocalSdpObserver", 0, 1);
    }

protected:
    void OnSetLocalDescriptionComplete(RTCError error) override
    {
        RTC_DLOG(LS_INFO) << __FUNCTION__;
        if (!error.ok()) {
            RTC_LOG(LS_ERROR) << "Error: " << error.type() << ", " << error.message();
        }

        tsfn_.BlockingCall([error = new RTCError(std::move(error))](Napi::Env env, Napi::Function jsCallback) {
            jsCallback.Call({External<RTCError>::New(env, error, [](Napi::Env, RTCError* e) { delete e; })});
        });
    }
};

class SetRemoteSdpObserver : public BaseSdpObserver<SetRemoteDescriptionObserverInterface> {
public:
    explicit SetRemoteSdpObserver(Napi::Env env) : SetRemoteSdpObserver(env, Promise::Deferred::New(env)) {}

    SetRemoteSdpObserver(Napi::Env env, Promise::Deferred deferred)
        : BaseSdpObserver<SetRemoteDescriptionObserverInterface>(deferred)
    {
        tsfn_ = ThreadSafeFunction::New(
            env,
            Function::New(
                env,
                [deferred](const CallbackInfo& info) {
                    auto error = info[0].As<External<RTCError>>().Data();
                    if (error->ok()) {
                        deferred.Resolve(info.Env().Undefined());
                    } else {
                        auto message = error->message();
                        deferred.Reject(
                            Error::New(info.Env(), (message && strlen(message) > 0) ? message : "unknown error")
                                .Value());
                    }
                }),
            "SetRemoteSdpObserver", 0, 1);
    }

protected:
    void OnSetRemoteDescriptionComplete(RTCError error) override
    {
        RTC_DLOG(LS_INFO) << __FUNCTION__;
        if (!error.ok()) {
            RTC_LOG(LS_ERROR) << "Error: " << error.type() << ", " << error.message();
        }

        tsfn_.BlockingCall([error = new RTCError(std::move(error))](Napi::Env env, Napi::Function jsCallback) {
            jsCallback.Call({External<RTCError>::New(env, error, [](Napi::Env, RTCError* e) { delete e; })});
        });
    }
};

/// NapiPeerConnectionWrapper
std::unique_ptr<NapiPeerConnectionWrapper> NapiPeerConnectionWrapper::Create(
    std::shared_ptr<PeerConnectionFactoryWrapper> factoryWrapper,
    const PeerConnectionInterface::RTCConfiguration& config)
{
    RTC_DLOG(LS_INFO) << __FUNCTION__;

    if (!factoryWrapper) {
        RTC_DLOG(LS_INFO) << "Get default factory";
        factoryWrapper = PeerConnectionFactoryWrapper::GetDefault();
    }

    if (!factoryWrapper || !factoryWrapper->GetFactory()) {
        RTC_LOG(LS_ERROR) << "No factory";
        return nullptr;
    }

    auto factory = factoryWrapper->GetFactory();
    auto observer = std::unique_ptr<NapiPeerConnectionWrapper>(new NapiPeerConnectionWrapper);

    PeerConnectionDependencies dependencies(observer.get());
    auto result = factory->CreatePeerConnectionOrError(config, std::move(dependencies));
    if (!result.ok()) {
        RTC_LOG(LS_ERROR) << "CreatePeerConnection failed: " << result.error().message();
        return nullptr;
    }

    observer->SetPeerConnectionFactoryWrapper(std::move(factoryWrapper));
    observer->SetPeerConnection(result.value());

    return observer;
}

NapiPeerConnectionWrapper::NapiPeerConnectionWrapper() : eventThread_(rtc::Thread::Create())
{
    eventThread_->SetName("pc_event_thread", nullptr);
    if (!eventThread_->Start()) {
        RTC_LOG(LS_ERROR) << "Failed to start event thread";
    }
}

NapiPeerConnectionWrapper::~NapiPeerConnectionWrapper()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RemoveAllEventHandlers();

    eventThread_->Stop();
}

bool NapiPeerConnectionWrapper::GetEventHandler(const std::string& type, Napi::Function& fn)
{
    RTC_DLOG(LS_VERBOSE) << "NapiPeerConnectionWrapper::GetEventHandler type: " << type;

    UNUSED std::lock_guard<std::mutex> lock(eventMutex_);
    auto it = eventHandlers_.find(type);
    if (it == eventHandlers_.end()) {
        RTC_LOG(LS_VERBOSE) << "No event handler for type: " << type;
        return false;
    }

    fn = it->second.ref.Value();

    return true;
}

bool NapiPeerConnectionWrapper::SetEventHandler(const std::string& type, Napi::Function fn, Napi::Value receiver)
{
    RTC_DLOG(LS_VERBOSE) << "SetEventHandler type: " << type;

    Reference<Napi::Value>* context = new Reference<Napi::Value>;
    *context = Persistent(receiver);

    EventHandler handler;
    handler.ref = Persistent(fn);
    handler.tsfn =
        ThreadSafeFunction::New(fn.Env(), fn, type, 0, 1, context, [](Napi::Env, Reference<Napi::Value>* ctx) {
            ctx->Reset();
            delete ctx;
        });

    UNUSED std::lock_guard<std::mutex> lock(eventMutex_);
    eventHandlers_[type] = std::move(handler);

    return true;
}

bool NapiPeerConnectionWrapper::RemoveEventHandler(const std::string& type)
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

void NapiPeerConnectionWrapper::RemoveAllEventHandlers()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    UNUSED std::lock_guard<std::mutex> lock(eventMutex_);
    for (auto& handler : eventHandlers_) {
        handler.second.tsfn.Release();
    }
    eventHandlers_.clear();
}

bool NapiPeerConnectionWrapper::GetEventHandler(const std::string& type, Napi::ThreadSafeFunction& tsfn)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " type=" << type;
    RTC_DCHECK(eventThread_->IsCurrent());

    UNUSED std::lock_guard<std::mutex> lock(eventMutex_);
    auto it = eventHandlers_.find(type);
    if (it == eventHandlers_.end()) {
        RTC_LOG(LS_VERBOSE) << "No event handler for type: " << type;
        return false;
    }

    tsfn = it->second.tsfn;

    return true;
}

void NapiPeerConnectionWrapper::OnIceCandidate(const IceCandidateInterface* candidate)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!candidate) {
        RTC_LOG(LS_ERROR) << "The candidate is nullptr";
        return;
    }

    std::string sdp;
    if (!candidate->ToString(&sdp)) {
        RTC_LOG(LS_ERROR) << "Failed to convert candidate to string, got so far: " << sdp;
        return;
    }

    auto sdpMid = candidate->sdp_mid();
    auto sdpMLineIndex = candidate->sdp_mline_index();
    auto can = candidate->candidate();

    // We do not have ownership of the candidate, so deep copy here
    eventThread_->PostTask([this, sdp, sdpMid, sdpMLineIndex, can] {
        ThreadSafeFunction tsfn;
        if (!GetEventHandler(kEventIceCandidate, tsfn)) {
            return;
        }

        Reference<Napi::Value>* context = tsfn.GetContext();
        napi_status status =
            tsfn.BlockingCall([context, sdp, sdpMid, sdpMLineIndex, can](Napi::Env env, Napi::Function jsCallback) {
                auto jsEvent = Object::New(env);
                jsEvent.Set("candidate", NativeToJsCandidate(env, sdpMid, sdpMLineIndex, sdp, can));
                jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
            });
        if (status != napi_ok) {
            RTC_LOG(LS_ERROR) << "tsfn call error: " << status;
        }
    });
}

void NapiPeerConnectionWrapper::OnIceCandidateError(
    const std::string& address, int port, const std::string& url, int error_code, const std::string& error_text)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    eventThread_->PostTask([this, address, port, url, error_code, error_text] {
        ThreadSafeFunction tsfn;
        if (!GetEventHandler(kEventIceCandidateError, tsfn)) {
            return;
        }

        Reference<Napi::Value>* context = tsfn.GetContext();
        napi_status status = tsfn.BlockingCall(
            [context, address, port, url, error_code, error_text](Napi::Env env, Napi::Function jsCallback) {
                auto jsEvent = Object::New(env);
                jsEvent.Set("address", String::New(env, address));
                jsEvent.Set("port", Number::New(env, port));
                jsEvent.Set("url", String::New(env, url));
                jsEvent.Set("errorCode", Number::New(env, error_code));
                jsEvent.Set("errorText", String::New(env, error_text));

                jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
            });
        if (status != napi_ok) {
            RTC_LOG(LS_ERROR) << "tsfn call error: " << status;
        }
    });
}

void NapiPeerConnectionWrapper::OnIceCandidatesRemoved(const std::vector<cricket::Candidate>& /*candidates*/)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

void NapiPeerConnectionWrapper::OnSignalingChange(PeerConnectionInterface::SignalingState new_state)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " new_state=" << new_state;

    eventThread_->PostTask([this] {
        ThreadSafeFunction tsfn;
        if (!GetEventHandler(kEventSignalingStateChange, tsfn)) {
            return;
        }

        Reference<Napi::Value>* context = tsfn.GetContext();
        napi_status status = tsfn.BlockingCall([context](Napi::Env env, Napi::Function jsCallback) {
            auto jsEvent = Object::New(env);
            jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
        });
        if (status != napi_ok) {
            RTC_LOG(LS_ERROR) << "tsfn call error: " << status;
        }
    });
}

void NapiPeerConnectionWrapper::OnIceConnectionChange(PeerConnectionInterface::IceConnectionState new_state)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " new_state=" << new_state;
    // use OnStandardizedIceConnectionChange
}

void NapiPeerConnectionWrapper::OnStandardizedIceConnectionChange(PeerConnectionInterface::IceConnectionState new_state)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " new_state=" << new_state;

    eventThread_->PostTask([this] {
        ThreadSafeFunction tsfn;
        if (!GetEventHandler(kEventIceConnectionStateChange, tsfn)) {
            return;
        }

        Reference<Napi::Value>* context = tsfn.GetContext();
        napi_status status = tsfn.BlockingCall([context](Napi::Env env, Napi::Function jsCallback) {
            auto jsEvent = Object::New(env);
            jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
        });
        if (status != napi_ok) {
            RTC_LOG(LS_ERROR) << "tsfn call error: " << status;
        }
    });
}

void NapiPeerConnectionWrapper::OnConnectionChange(PeerConnectionInterface::PeerConnectionState new_state)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " new_state=" << new_state;

    eventThread_->PostTask([this] {
        ThreadSafeFunction tsfn;
        if (!GetEventHandler(kEventConnectionStateChange, tsfn)) {
            return;
        }

        Reference<Napi::Value>* context = tsfn.GetContext();
        napi_status status = tsfn.BlockingCall([context](Napi::Env env, Napi::Function jsCallback) {
            auto jsEvent = Object::New(env);
            jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
        });
        if (status != napi_ok) {
            RTC_LOG(LS_ERROR) << "tsfn call error: " << status;
        }
    });
}

void NapiPeerConnectionWrapper::OnIceConnectionReceivingChange(bool /*receiving*/)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

void NapiPeerConnectionWrapper::OnIceGatheringChange(PeerConnectionInterface::IceGatheringState new_state)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " new_state=" << new_state;

    eventThread_->PostTask([this] {
        ThreadSafeFunction tsfn;
        if (!GetEventHandler(kEventIceGatheringStateChange, tsfn)) {
            return;
        }

        Reference<Napi::Value>* context = tsfn.GetContext();
        napi_status status = tsfn.BlockingCall([context](Napi::Env env, Napi::Function jsCallback) {
            auto jsEvent = Object::New(env);
            jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
        });
        if (status != napi_ok) {
            RTC_LOG(LS_ERROR) << "tsfn call error: " << status;
        }
    });
}

void NapiPeerConnectionWrapper::OnIceSelectedCandidatePairChanged(const cricket::CandidatePairChangeEvent& /*event*/)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

void NapiPeerConnectionWrapper::OnAddStream(rtc::scoped_refptr<MediaStreamInterface> /*stream*/)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

void NapiPeerConnectionWrapper::OnRemoveStream(rtc::scoped_refptr<MediaStreamInterface> /*stream*/)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

void NapiPeerConnectionWrapper::OnDataChannel(rtc::scoped_refptr<DataChannelInterface> channel)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!channel) {
        RTC_LOG(LS_ERROR) << "The channel is nullptr";
        return;
    }

    eventThread_->PostTask([this, channel] {
        ThreadSafeFunction tsfn;
        if (!GetEventHandler(kEventDataChannel, tsfn)) {
            return;
        }

        auto observer = NapiDataChannel::Observer::Create(channel.get());
        channel->RegisterObserver(observer.get());

        Reference<Napi::Value>* context = tsfn.GetContext();
        napi_status status =
            tsfn.BlockingCall([context, channel, obs = observer.release()](Napi::Env env, Napi::Function jsCallback) {
                auto jsEvent = Object::New(env);
                jsEvent.Set("channel", NapiDataChannel::NewInstance(env, channel, obs));
                jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
            });
        if (status != napi_ok) {
            RTC_LOG(LS_ERROR) << "tsfn call error: " << status;
        }
    });
}

void NapiPeerConnectionWrapper::OnRenegotiationNeeded()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    eventThread_->PostTask([this] {
        ThreadSafeFunction tsfn;
        if (!GetEventHandler(kEventNegotiationNeeded, tsfn)) {
            return;
        }

        Reference<Napi::Value>* context = tsfn.GetContext();
        napi_status status = tsfn.BlockingCall([context](Napi::Env env, Napi::Function jsCallback) {
            auto jsEvent = Object::New(env);
            jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
        });
        if (status != napi_ok) {
            RTC_LOG(LS_ERROR) << "tsfn call error: " << status;
        }
    });
}

void NapiPeerConnectionWrapper::OnNegotiationNeededEvent(uint32_t event_id)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " event_id=" << event_id;
}

void NapiPeerConnectionWrapper::OnAddTrack(
    rtc::scoped_refptr<RtpReceiverInterface> /*receiver*/,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& /*streams*/)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    // use OnTrack
}

void NapiPeerConnectionWrapper::OnTrack(rtc::scoped_refptr<RtpTransceiverInterface> transceiver)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    eventThread_->PostTask([this, transceiver] {
        ThreadSafeFunction tsfn;
        if (!GetEventHandler(kEventTrack, tsfn)) {
            return;
        }

        auto receiver = transceiver->receiver();
        if (!receiver) {
            RTC_LOG(LS_ERROR) << "No receiver in the transceiver";
            return;
        }

        Reference<Napi::Value>* context = tsfn.GetContext();
        napi_status status = tsfn.BlockingCall(
            [this, context, transceiver, pc = GetPeerConnection()](Napi::Env env, Napi::Function jsCallback) {
                auto receiver = transceiver->receiver();
                auto track = MediaStreamTrackWrapper::Create(receiver->track());

                auto streams = receiver->streams();
                auto jsStreams = Array::New(env, streams.size());
                for (uint32_t i = 0; i < streams.size(); i++) {
                    jsStreams[i] = NapiMediaStream::NewInstance(env, streams[i]);
                }

                auto jsEvent = Object::New(env);
                jsEvent.Set("streams", jsStreams);
                jsEvent.Set("track", NapiMediaStreamTrack::NewInstance(env, std::move(track)));
                jsEvent.Set("receiver", NapiRtpReceiver::NewInstance(env, receiver, this));
                jsEvent.Set("transceiver", NapiRtpTransceiver::NewInstance(env, transceiver, this));

                jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
            });
        if (status != napi_ok) {
            RTC_LOG(LS_ERROR) << "tsfn call error: " << status;
        }
    });
}

void NapiPeerConnectionWrapper::OnRemoveTrack(rtc::scoped_refptr<RtpReceiverInterface> /*receiver*/)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

/// NapiPeerConnection
FunctionReference NapiPeerConnection::constructor_;

void NapiPeerConnection::Init(class Env env, Object exports)
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    Function func = DefineClass(
        env, kClassName,
        {
            InstanceAccessor<&NapiPeerConnection::GetCanTrickleIceCandidates>(kAttributeNameCanTrickleIceCandidates),
            InstanceAccessor<&NapiPeerConnection::GetSignalingState>(kAttributeNameSignalingState),
            InstanceAccessor<&NapiPeerConnection::GetIceGatheringState>(kAttributeNameIceGatheringState),
            InstanceAccessor<&NapiPeerConnection::GetIceConnectionState>(kAttributeNameIceConnectionState),
            InstanceAccessor<&NapiPeerConnection::GetConnectionState>(kAttributeNameConnectionState),
            InstanceAccessor<&NapiPeerConnection::GetLocalDescription>(kAttributeNameLocalDescription),
            InstanceAccessor<&NapiPeerConnection::GetRemoteDescription>(kAttributeNameRemoteDescription),
            InstanceAccessor<&NapiPeerConnection::GetCurrentLocalDescription>(kAttributeNameCurrentLocalDescription),
            InstanceAccessor<&NapiPeerConnection::GetCurrentRemoteDescription>(kAttributeNameCurrentRemoteDescription),
            InstanceAccessor<&NapiPeerConnection::GetPendingLocalDescription>(kAttributeNamePendingLocalDescription),
            InstanceAccessor<&NapiPeerConnection::GetPendingRemoteDescription>(kAttributeNamePendingRemoteDescription),
            InstanceAccessor<&NapiPeerConnection::GetSctp>(kAttributeNameSctp),
            InstanceAccessor<&NapiPeerConnection::GetEventHandler, &NapiPeerConnection::SetEventHandler>(
                kAttributeNameOnIceCandidate, napi_default, (void*)kEventIceCandidate),
            InstanceAccessor<&NapiPeerConnection::GetEventHandler, &NapiPeerConnection::SetEventHandler>(
                kAttributeNameOnIceCandidateError, napi_default, (void*)kEventIceCandidateError),
            InstanceAccessor<&NapiPeerConnection::GetEventHandler, &NapiPeerConnection::SetEventHandler>(
                kAttributeNameOnIceConnectionStateChange, napi_default, (void*)kEventIceConnectionStateChange),
            InstanceAccessor<&NapiPeerConnection::GetEventHandler, &NapiPeerConnection::SetEventHandler>(
                kAttributeNameOnIceGatheringStateChange, napi_default, (void*)kEventIceGatheringStateChange),
            InstanceAccessor<&NapiPeerConnection::GetEventHandler, &NapiPeerConnection::SetEventHandler>(
                kAttributeNameOnConnectionStateChange, napi_default, (void*)kEventConnectionStateChange),
            InstanceAccessor<&NapiPeerConnection::GetEventHandler, &NapiPeerConnection::SetEventHandler>(
                kAttributeNameOnSignalingStateChange, napi_default, (void*)kEventSignalingStateChange),
            InstanceAccessor<&NapiPeerConnection::GetEventHandler, &NapiPeerConnection::SetEventHandler>(
                kAttributeNameOnNegotiationNeeded, napi_default, (void*)kEventNegotiationNeeded),
            InstanceAccessor<&NapiPeerConnection::GetEventHandler, &NapiPeerConnection::SetEventHandler>(
                kAttributeNameOnTrack, napi_default, (void*)kEventTrack),
            InstanceAccessor<&NapiPeerConnection::GetEventHandler, &NapiPeerConnection::SetEventHandler>(
                kAttributeNameOnDataChannel, napi_default, (void*)kEventDataChannel),
            InstanceMethod<&NapiPeerConnection::AddTrack>(kMethodNameAddTrack),
            InstanceMethod<&NapiPeerConnection::RemoveTrack>(kMethodNameRemoveTrack),
            InstanceMethod<&NapiPeerConnection::SetLocalDescription>(kMethodNameSetLocalDescription),
            InstanceMethod<&NapiPeerConnection::SetRemoteDescription>(kMethodNameSetRemoteDescription),
            InstanceMethod<&NapiPeerConnection::CreateOffer>(kMethodNameCreateOffer),
            InstanceMethod<&NapiPeerConnection::CreateAnswer>(kMethodNameCreateAnswer),
            InstanceMethod<&NapiPeerConnection::CreateDataChannel>(kMethodNameCreateDataChannel),
            InstanceMethod<&NapiPeerConnection::AddIceCandidate>(kMethodNameAddIceCandidate),
            InstanceMethod<&NapiPeerConnection::GetSenders>(kMethodNameGetSenders),
            InstanceMethod<&NapiPeerConnection::GetReceivers>(kMethodNameGetReceivers),
            InstanceMethod<&NapiPeerConnection::GetTransceivers>(kMethodNameGetTransceivers),
            InstanceMethod<&NapiPeerConnection::GetConfiguration>(kMethodNameGetConfiguration),
            InstanceMethod<&NapiPeerConnection::RestartIce>(kMethodNameRestartIce),
            InstanceMethod<&NapiPeerConnection::SetConfiguration>(kMethodNameSetConfiguration),
            InstanceMethod<&NapiPeerConnection::AddTransceiver>(kMethodNameAddTransceiver),
            InstanceMethod<&NapiPeerConnection::Close>(kMethodNameClose),
            InstanceMethod<&NapiPeerConnection::GetStats>(kMethodNameGetStats),
            InstanceMethod<&NapiPeerConnection::ToJson>(kMethodNameToJson),
            StaticMethod<&NapiPeerConnection::GenerateCertificate>(kStaticMethodNameGenerateCertificate),
        });

    if (func.IsEmpty() || func.IsUndefined()) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "NapiPeerConnection", "func is empty or undefined");
    }
    constructor_ = Persistent(func);
    OH_LOG_Print(
        LOG_APP, LOG_INFO, LOG_DOMAIN, "NapiPeerConnection", "constructor_=%{public}p", (napi_ref)constructor_);

    exports.Set(kClassName, func);
}

Napi::Value NapiPeerConnection::NewInstance(Napi::Env env, std::unique_ptr<NapiPeerConnectionWrapper> observer)
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    if (!observer || !observer->GetPeerConnection()) {
        RTC_LOG(LS_ERROR) << "Invalid argument";
        return env.Undefined();
    }

    auto externalObserver = External<NapiPeerConnectionWrapper>::New(env, observer.release());

    return constructor_.New({externalObserver});
}

NapiPeerConnection::NapiPeerConnection(const CallbackInfo& info) : ObjectWrap<NapiPeerConnection>(info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() > 0 && info[0].IsExternal()) {
        auto observer = info[0].As<External<NapiPeerConnectionWrapper>>().Data();
        wrapper_ = std::unique_ptr<NapiPeerConnectionWrapper>(observer);
        pc_ = wrapper_->GetPeerConnection();
    }
}

NapiPeerConnection::~NapiPeerConnection()
{
    RTC_LOG(LS_INFO) << __FUNCTION__;
}

Napi::Value NapiPeerConnection::GetCanTrickleIceCandidates(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (pc_->can_trickle_ice_candidates()) {
        return Boolean::New(info.Env(), pc_->can_trickle_ice_candidates().value());
    }

    return info.Env().Undefined();
}

Napi::Value NapiPeerConnection::GetSignalingState(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    switch (pc_->signaling_state()) {
        case PeerConnectionInterface::kStable:
            return String::New(info.Env(), kEnumSignalingStateStable);
        case PeerConnectionInterface::kHaveLocalOffer:
            return String::New(info.Env(), kEnumSignalingStateHaveLocalOffer);
        case PeerConnectionInterface::kHaveLocalPrAnswer:
            return String::New(info.Env(), kEnumSignalingStateHaveLocalPranswer);
        case PeerConnectionInterface::kHaveRemoteOffer:
            return String::New(info.Env(), kEnumSignalingStateHaveRemoteOffer);
        case PeerConnectionInterface::kHaveRemotePrAnswer:
            return String::New(info.Env(), kEnumSignalingStateHaveRemotePranswer);
        case PeerConnectionInterface::kClosed:
            return String::New(info.Env(), kEnumSignalingStateClosed);
        default:
            RTC_LOG(LS_WARNING) << "Invalid value of signalingState";
            break;
    }

    NAPI_THROW(Error::New(info.Env(), "Invalid value"), info.Env().Undefined());
}

Napi::Value NapiPeerConnection::GetIceGatheringState(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    switch (pc_->ice_gathering_state()) {
        case PeerConnectionInterface::kIceGatheringNew:
            return String::New(info.Env(), kEnumIceGatheringStateNew);
        case PeerConnectionInterface::kIceGatheringGathering:
            return String::New(info.Env(), kEnumIceGatheringStateGathering);
        case PeerConnectionInterface::kIceGatheringComplete:
            return String::New(info.Env(), kEnumIceGatheringStateComplete);
        default:
            RTC_LOG(LS_WARNING) << "Invalid value of iceGatheringState";
            break;
    }

    NAPI_THROW(Error::New(info.Env(), "Invalid value"), info.Env().Undefined());
}

Napi::Value NapiPeerConnection::GetIceConnectionState(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    switch (pc_->ice_connection_state()) {
        case PeerConnectionInterface::kIceConnectionNew:
            return String::New(info.Env(), kEnumIceConnectionStateNew);
        case PeerConnectionInterface::kIceConnectionChecking:
            return String::New(info.Env(), kEnumIceConnectionStateChecking);
        case PeerConnectionInterface::kIceConnectionConnected:
            return String::New(info.Env(), kEnumIceConnectionStateConnected);
        case PeerConnectionInterface::kIceConnectionCompleted:
            return String::New(info.Env(), kEnumIceConnectionStateCompleted);
        case PeerConnectionInterface::kIceConnectionFailed:
            return String::New(info.Env(), kEnumIceConnectionStateFailed);
        case PeerConnectionInterface::kIceConnectionDisconnected:
            return String::New(info.Env(), kEnumIceConnectionStateDisconnected);
        case PeerConnectionInterface::kIceConnectionClosed:
            return String::New(info.Env(), kEnumIceConnectionStateClosed);
        default:
            RTC_LOG(LS_WARNING) << "Invalid value of iceConnectionState";
            break;
    }

    NAPI_THROW(Error::New(info.Env(), "Invalid value"), info.Env().Undefined());
}

Napi::Value NapiPeerConnection::GetConnectionState(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    switch (pc_->peer_connection_state()) {
        case PeerConnectionInterface::PeerConnectionState::kNew:
            return String::New(info.Env(), kEnumPeerConnectionStateNew);
        case PeerConnectionInterface::PeerConnectionState::kConnecting:
            return String::New(info.Env(), kEnumPeerConnectionStateConnecting);
        case PeerConnectionInterface::PeerConnectionState::kConnected:
            return String::New(info.Env(), kEnumPeerConnectionStateConnected);
        case PeerConnectionInterface::PeerConnectionState::kDisconnected:
            return String::New(info.Env(), kEnumPeerConnectionStateDisconnected);
        case PeerConnectionInterface::PeerConnectionState::kFailed:
            return String::New(info.Env(), kEnumPeerConnectionStateFailed);
        case PeerConnectionInterface::PeerConnectionState::kClosed:
            return String::New(info.Env(), kEnumPeerConnectionStateClosed);
        default:
            RTC_LOG(LS_WARNING) << "Invalid value of connectionState";
            break;
    }

    NAPI_THROW(Error::New(info.Env(), "Invalid value"), info.Env().Undefined());
}

Napi::Value NapiPeerConnection::GetLocalDescription(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    // It's only safe to operate on SessionDescriptionInterface on the signaling thread
    std::string sdp;
    std::string type;
    pc_->signaling_thread()->BlockingCall([pc = pc_, &sdp, &type] {
        const SessionDescriptionInterface* desc = pc->local_description();
        if (desc) {
            if (desc->ToString(&sdp)) {
                type = desc->type();
            }
        }
    });

    if (sdp.empty()) {
        return info.Env().Undefined();
    }

    auto sdpType = SdpTypeFromString(type);
    if (!sdpType) {
        NAPI_THROW(Error::New(info.Env(), "Invalid value"), info.Env().Undefined());
    }

    return NapiSessionDescription::NewInstance(info.Env(), sdp, sdpType.value());
}

Napi::Value NapiPeerConnection::GetRemoteDescription(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    // It's only safe to operate on SessionDescriptionInterface on the signaling thread
    std::string sdp;
    std::string type;
    pc_->signaling_thread()->BlockingCall([pc = pc_, &sdp, &type] {
        const SessionDescriptionInterface* desc = pc->remote_description();
        if (desc) {
            if (desc->ToString(&sdp)) {
                type = desc->type();
            }
        }
    });

    if (sdp.empty()) {
        return info.Env().Undefined();
    }

    auto sdpType = SdpTypeFromString(type);
    if (!sdpType) {
        NAPI_THROW(Error::New(info.Env(), "Invalid value"), info.Env().Undefined());
    }

    return NapiSessionDescription::NewInstance(info.Env(), sdp, sdpType.value());
}

Napi::Value NapiPeerConnection::GetCurrentLocalDescription(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    // It's only safe to operate on SessionDescriptionInterface on the signaling thread
    std::string sdp;
    std::string type;
    pc_->signaling_thread()->BlockingCall([pc = pc_, &sdp, &type] {
        const SessionDescriptionInterface* desc = pc->current_local_description();
        if (desc) {
            if (desc->ToString(&sdp)) {
                type = desc->type();
            }
        }
    });

    if (sdp.empty()) {
        return info.Env().Undefined();
    }

    auto sdpType = SdpTypeFromString(type);
    if (!sdpType) {
        NAPI_THROW(Error::New(info.Env(), "Invalid value"), info.Env().Undefined());
    }

    return NapiSessionDescription::NewInstance(info.Env(), sdp, sdpType.value());
}

Napi::Value NapiPeerConnection::GetCurrentRemoteDescription(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    // It's only safe to operate on SessionDescriptionInterface on the signaling thread
    std::string sdp;
    std::string type;
    pc_->signaling_thread()->BlockingCall([pc = pc_, &sdp, &type] {
        const SessionDescriptionInterface* desc = pc->current_remote_description();
        if (desc) {
            if (desc->ToString(&sdp)) {
                type = desc->type();
            }
        }
    });

    if (sdp.empty()) {
        return info.Env().Undefined();
    }

    auto sdpType = SdpTypeFromString(type);
    if (!sdpType) {
        NAPI_THROW(Error::New(info.Env(), "Invalid value"), info.Env().Undefined());
    }

    return NapiSessionDescription::NewInstance(info.Env(), sdp, sdpType.value());
}

Napi::Value NapiPeerConnection::GetPendingLocalDescription(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    // It's only safe to operate on SessionDescriptionInterface on the signaling thread
    std::string sdp;
    std::string type;
    pc_->signaling_thread()->BlockingCall([pc = pc_, &sdp, &type] {
        const SessionDescriptionInterface* desc = pc->pending_local_description();
        if (desc) {
            if (desc->ToString(&sdp)) {
                type = desc->type();
            }
        }
    });

    if (sdp.empty()) {
        return info.Env().Undefined();
    }

    auto sdpType = SdpTypeFromString(type);
    if (!sdpType) {
        NAPI_THROW(Error::New(info.Env(), "Invalid value"), info.Env().Undefined());
    }

    return NapiSessionDescription::NewInstance(info.Env(), sdp, sdpType.value());
}

Napi::Value NapiPeerConnection::GetPendingRemoteDescription(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    // It's only safe to operate on SessionDescriptionInterface on the signaling thread
    std::string sdp;
    std::string type;
    pc_->signaling_thread()->BlockingCall([pc = pc_, &sdp, &type] {
        const SessionDescriptionInterface* desc = pc->pending_remote_description();
        if (desc) {
            if (desc->ToString(&sdp)) {
                type = desc->type();
            }
        }
    });

    if (sdp.empty()) {
        return info.Env().Undefined();
    }

    auto sdpType = SdpTypeFromString(type);
    if (!sdpType) {
        NAPI_THROW(Error::New(info.Env(), "Invalid value"), info.Env().Undefined());
    }

    return NapiSessionDescription::NewInstance(info.Env(), sdp, sdpType.value());
}

Napi::Value NapiPeerConnection::GetSctp(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!sctpTransportRef_.IsEmpty()) {
        return sctpTransportRef_.Value();
    }

    auto transport = pc_->GetSctpTransport();
    if (!transport) {
        return info.Env().Undefined();
    }

    auto sctpTransport = NapiSctpTransport::NewInstance(info.Env(), transport, wrapper_.get());
    sctpTransportRef_ = Persistent(sctpTransport);

    return sctpTransport;
}

Napi::Value NapiPeerConnection::GetEventHandler(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Function fn;
    if (!wrapper_->GetEventHandler((const char*)info.Data(), fn)) {
        return info.Env().Null();
    }

    return fn;
}

void NapiPeerConnection::SetEventHandler(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    wrapper_->RemoveEventHandler((const char*)info.Data());

    if (value.IsFunction()) {
        Function cb = value.As<Function>();
        if (!wrapper_->SetEventHandler((const char*)info.Data(), std::move(cb), info.This())) {
            NAPI_THROW_VOID(Napi::Error::New(info.Env(), "Failed to set event handler"));
        }
    }
}

Napi::Value NapiPeerConnection::AddTrack(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 1) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsObject()) {
        NAPI_THROW(Error::New(info.Env(), "First argument is not object"), info.Env().Undefined());
    }

    auto jsTrack = info[0].As<Object>();
    if (jsTrack.Has("type")) {
        auto type = jsTrack.Get("type").As<String>().Utf8Value();
        RTC_DLOG(LS_VERBOSE) << "type=" << type;
    }

    std::vector<std::string> streamIds;
    if (info.Length() >= 2) {
        for (uint32_t i = 1; i < info.Length(); i++) {
            if (!info[i].IsObject()) {
                NAPI_THROW(Error::New(info.Env(), "The argument is not object"), info.Env().Undefined());
            }

            auto jsStream = info[i].As<Object>();
            auto napiStream = NapiMediaStream::Unwrap(jsStream);
            if (!napiStream) {
                NAPI_THROW(Error::New(info.Env(), "The argument is not MediaStream"), info.Env().Undefined());
            }

            auto stream = napiStream->Get();
            if (stream) {
                streamIds.push_back(stream->id());
            }
        }
    } else {
        streamIds.push_back("stream_id");
    }

    auto track = NapiMediaStreamTrack::Unwrap(jsTrack);
    auto result_or_error = pc_->AddTrack(track->Get(), streamIds);
    if (!result_or_error.ok()) {
        RTC_LOG(LS_ERROR) << "Failed to add audio track to PeerConnection: " << result_or_error.error().message();
        NAPI_THROW(Error::New(info.Env(), result_or_error.error().message()), info.Env().Undefined());
    }
    return NapiRtpSender::NewInstance(info.Env(), result_or_error.value(), wrapper_.get());
}

Napi::Value NapiPeerConnection::RemoveTrack(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 1) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsObject()) {
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), info.Env().Undefined());
    }

    auto jsSender = info[0].As<Object>();
    auto sender = NapiRtpSender::Unwrap(jsSender);

    auto error = pc_->RemoveTrackOrError(sender->Get());
    if (!error.ok()) {
        RTC_LOG(LS_ERROR) << "Failed to remove track: " << error.message();
        std::string message;
        switch (error.type()) {
            case RTCErrorType::INVALID_PARAMETER:
                message = "Invalid argument";
                break;
            case RTCErrorType::INVALID_STATE:
                message = "Invalid state";
                break;
            default:
                message = "unknown error";
                break;
        }
        NAPI_THROW(Error::New(info.Env(), message), info.Env().Undefined());
    }

    return info.Env().Undefined();
}

Napi::Value NapiPeerConnection::SetLocalDescription(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    std::unique_ptr<SessionDescriptionInterface> desc;

    if (info.Length() > 0) {
        if (!info[0].IsObject()) {
            NAPI_THROW(TypeError::New(info.Env(), "first argument must be a object"), info.Env().Undefined());
        }

        Object jsSdp = info[0].As<Object>();
        std::string sdp = "";
        if (jsSdp.Has("sdp")) {
            sdp = jsSdp.Get("sdp").As<String>();
        }
        std::string type = jsSdp.Get("type").As<String>();

        auto sdpType = SdpTypeFromString(type);
        if (!sdpType) {
            NAPI_THROW(Error::New(info.Env(), "invalid argument"), info.Env().Undefined());
        }

        SdpParseError error;
        desc = CreateSessionDescription(*sdpType, sdp, &error);
        if (!desc) {
            RTC_DLOG(LS_WARNING) << "Can't parse received session description message. SdpParseError was: "
                                 << error.description;
            NAPI_THROW(Error::New(info.Env(), "Invalid argument"), info.Env().Undefined());
        }
    }

    auto observer = rtc::make_ref_counted<SetLocalSdpObserver>(info.Env());
    if (desc) {
        pc_->SetLocalDescription(std::move(desc), observer);
    } else {
        pc_->SetLocalDescription(observer);
    }

    return observer->GetPromise();
}

Napi::Value NapiPeerConnection::SetRemoteDescription(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 1) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of argument"), info.Env().Undefined());
    }

    if (!info[0].IsObject()) {
        NAPI_THROW(TypeError::New(info.Env(), "First argument is not object"), info.Env().Undefined());
    }

    Object jsSdp = info[0].As<Object>();
    std::string sdp = "";
    if (jsSdp.Has("sdp")) {
        sdp = jsSdp.Get("sdp").As<String>();
    }
    std::string type = jsSdp.Get("type").As<String>();

    auto sdpType = SdpTypeFromString(type);
    if (!sdpType) {
        NAPI_THROW(Error::New(info.Env(), "Invalid sdp type"), info.Env().Undefined());
    }

    SdpParseError error;
    auto desc = CreateSessionDescription(*sdpType, sdp, &error);
    if (!desc) {
        RTC_DLOG(LS_WARNING) << "Can't parse received session description message. SdpParseError was: "
                             << error.description;
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), info.Env().Undefined());
    }

    auto observer = rtc::make_ref_counted<SetRemoteSdpObserver>(info.Env());
    pc_->SetRemoteDescription(std::move(desc), observer);

    return observer->GetPromise();
}

Napi::Value NapiPeerConnection::CreateOffer(const CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    PeerConnectionInterface::RTCOfferAnswerOptions options;
    if (info.Length() > 0 && info[0].IsObject()) {
        auto jsOptions = info[0].As<Object>();
        if (jsOptions.Has("iceRestart")) {
            options.ice_restart = jsOptions.Get("iceRestart").As<Boolean>();
        }
    }

    auto observer = rtc::make_ref_counted<CreateSdpObserver>(info.Env());
    pc_->CreateOffer(observer.get(), options);

    return observer->GetPromise();
}

Napi::Value NapiPeerConnection::CreateAnswer(const CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    // ignore the argument, RTCAnswerOptions is empty
    PeerConnectionInterface::RTCOfferAnswerOptions options;

    auto observer = rtc::make_ref_counted<CreateSdpObserver>(info.Env());
    pc_->CreateAnswer(observer.get(), options);

    return observer->GetPromise();
}

Napi::Value NapiPeerConnection::CreateDataChannel(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 1) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsString()) {
        NAPI_THROW(TypeError::New(info.Env(), "First argument is not string"), info.Env().Undefined());
    }

    auto label = info[0].As<String>().Utf8Value();

    if (info.Length() < 2) {
        auto result = pc_->CreateDataChannelOrError(label, nullptr);
        if (!result.ok()) {
            auto& error = result.error();
            RTC_LOG(LS_ERROR) << "create data channel error: " << error.type() << ", " << error.message();
            NAPI_THROW(
                Error::New(info.Env(), error.message() ? error.message() : "unknown error"), info.Env().Undefined());
        }

        return NapiDataChannel::NewInstance(info.Env(), result.value());
    }

    if (!info[1].IsObject()) {
        NAPI_THROW(TypeError::New(info.Env(), "Second argument is not object"), info.Env().Undefined());
    }

    DataChannelInit options;
    JsToNativeDataChannelInit(info[1].As<Object>(), options);

    auto result = pc_->CreateDataChannelOrError(label, &options);
    if (!result.ok()) {
        auto& error = result.error();
        RTC_LOG(LS_ERROR) << "create data channel error: " << error.type() << ", " << error.message();
        NAPI_THROW(Error::New(info.Env(), error.message() ? error.message() : "unknown error"), info.Env().Undefined());
    }

    return NapiDataChannel::NewInstance(info.Env(), result.value());
}

Napi::Value NapiPeerConnection::AddIceCandidate(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    std::string sdp;
    std::string sdpMid;
    int sdpMLineIndex = 0;

    if (info.Length() > 0) {
        auto jsCandidate = info[0].As<Object>();
        sdp = jsCandidate.Get("candidate").As<String>().Utf8Value();

        if (jsCandidate.Has("sdpMid")) {
            sdpMid = jsCandidate.Get("sdpMid").As<String>().Utf8Value();
        }

        if (jsCandidate.Has("sdpMLineIndex")) {
            sdpMLineIndex = jsCandidate.Get("sdpMLineIndex").As<Number>().Uint32Value();
        }

        // ignore usernameFragment
    }

    auto deferred = Promise::Deferred::New(info.Env());

    SdpParseError error;
    auto candidate = CreateIceCandidate(sdpMid, sdpMLineIndex, sdp, &error);
    if (!candidate) {
        RTC_LOG(LS_ERROR) << "Can't parse received candidate message. SdpParseError was: " << error.line << ", "
                          << error.description;
        deferred.Reject(Error::New(info.Env(), "Invalid argument").Value());
        return deferred.Promise();
    }

    auto tsfn = ThreadSafeFunction::New(
        info.Env(),
        Function::New(
            info.Env(),
            [deferred](const CallbackInfo& info) {
                auto error = info[0].As<External<RTCError>>().Data();
                if (error->ok()) {
                    deferred.Resolve(info.Env().Undefined());
                } else {
                    auto type = error->type();
                    auto message = error->message();
                    RTC_LOG(LS_ERROR) << "AddIceCandidate failed: " << type << ", " << message;
                    deferred.Reject(
                        Error::New(info.Env(), (message && strlen(message) > 0) ? message : "unknown error").Value());
                }
            }),
        "AddIceCandidate", 0, 1);

    pc_->AddIceCandidate(std::unique_ptr<IceCandidateInterface>(candidate), [tsfn](RTCError error) {
        RTC_DLOG(LS_INFO) << "AddIceCandidate complete: " << error.ok();
        tsfn.BlockingCall([error = new RTCError(std::move(error))](Napi::Env env, Napi::Function jsCallback) {
            jsCallback.Call({External<RTCError>::New(env, error, [](Napi::Env, RTCError* e) { delete e; })});
        });
        tsfn.Release();
    });

    return deferred.Promise();
}

Napi::Value NapiPeerConnection::GetSenders(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    auto senders = pc_->GetSenders();
    auto jsSenders = Array::New(info.Env(), senders.size());
    for (uint32_t i = 0; i < senders.size(); i++) {
        jsSenders.Set(i, NapiRtpSender::NewInstance(info.Env(), senders[i], wrapper_.get()));
    }

    return jsSenders;
}

Napi::Value NapiPeerConnection::GetReceivers(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    auto receivers = pc_->GetReceivers();
    auto jsReceivers = Array::New(info.Env(), receivers.size());
    for (uint32_t i = 0; i < receivers.size(); i++) {
        jsReceivers.Set(i, NapiRtpReceiver::NewInstance(info.Env(), receivers[i], wrapper_.get()));
    }

    return jsReceivers;
}

Napi::Value NapiPeerConnection::GetTransceivers(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    auto transceivers = pc_->GetTransceivers();
    auto jsTransceivers = Array::New(info.Env(), transceivers.size());
    for (uint32_t i = 0; i < transceivers.size(); i++) {
        jsTransceivers.Set(i, NapiRtpTransceiver::NewInstance(info.Env(), transceivers[i], wrapper_.get()));
    }

    return jsTransceivers;
}

Napi::Value NapiPeerConnection::GetConfiguration(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    auto configuration = pc_->GetConfiguration();
    auto jsConfiguration = Object::New(info.Env());

    if (!NativeToJsConfiguration(configuration, jsConfiguration)) {
        RTC_LOG(LS_ERROR) << "NativeToJsConfiguration failed";
    }

    return jsConfiguration;
}

Napi::Value NapiPeerConnection::SetConfiguration(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    PeerConnectionInterface::RTCConfiguration config;
    if (info.Length() > 0) {
        auto jsConfiguration = info[0].As<Object>();
        if (!JsToNativeConfiguration(jsConfiguration, config)) {
            RTC_LOG(LS_ERROR) << "JsToNativeConfiguration failed";
        }
    }

    auto error = pc_->SetConfiguration(config);
    if (!error.ok()) {
        std::string message;
        switch (error.type()) {
            case RTCErrorType::INVALID_STATE:
                message = "Invalid state";
                break;
            case RTCErrorType::INVALID_MODIFICATION:
                message = "Invalid modification";
                break;
            case RTCErrorType::INVALID_RANGE:
                message = "Invalid range";
                break;
            case RTCErrorType::SYNTAX_ERROR:
                message = "Syntax error";
                break;
            case RTCErrorType::INVALID_PARAMETER:
                message = "Invalid argument";
                break;
            case RTCErrorType::INTERNAL_ERROR:
                message = "Internal error";
                break;
            default:
                message = "Unknown error";
                break;
        }
        NAPI_THROW(Error::New(info.Env(), message), info.Env().Undefined());
    }

    return info.Env().Undefined();
}

Napi::Value NapiPeerConnection::RestartIce(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;
    pc_->RestartIce();
    return info.Env().Undefined();
}

Napi::Value NapiPeerConnection::AddTransceiver(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;
    if (info.Length() < 1) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsObject() && !info[0].IsString()) {
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), info.Env().Undefined());
    }

    if (info[0].IsObject()) {
        auto jsTrack = info[0].As<Object>();
        void* unwrapped;
        NAPI_THROW_IF_FAILED(info.Env(), napi_unwrap(info.Env(), jsTrack, &unwrapped), info.Env().Undefined());
        auto track = static_cast<NapiMediaStreamTrack*>(unwrapped);

        if (info.Length() > 1) {
            auto jsInit = info[1].As<Object>();
            webrtc::RtpTransceiverInit init;
            PopulateTransceiverInit(jsInit, init);
            auto result_or_error = pc_->AddTransceiver(track->Get(), init);
            if (!result_or_error.ok()) {
                NAPI_THROW(Error::New(info.Env(), result_or_error.error().message()), info.Env().Undefined());
            }
            return NapiRtpTransceiver::NewInstance(info.Env(), result_or_error.value(), wrapper_.get());
        } else {
            auto result_or_error = pc_->AddTransceiver(track->Get());
            if (!result_or_error.ok()) {
                NAPI_THROW(Error::New(info.Env(), result_or_error.error().message()), info.Env().Undefined());
            }
            return NapiRtpTransceiver::NewInstance(info.Env(), result_or_error.value(), wrapper_.get());
        }
    } else {
        std::string jsMediatype = info[0].As<String>().Utf8Value();
        cricket::MediaType mediaType;
        if (jsMediatype == "audio") {
            mediaType = cricket::MEDIA_TYPE_AUDIO;
        } else if (jsMediatype == "video") {
            mediaType = cricket::MEDIA_TYPE_VIDEO;
        } else {
            NAPI_THROW(Error::New(info.Env(), "Media type is not audio or video"), info.Env().Undefined());
        }

        if (info.Length() > 1) {
            auto jsInit = info[1].As<Object>();
            webrtc::RtpTransceiverInit init;
            PopulateTransceiverInit(jsInit, init);
            auto result_or_error = pc_->AddTransceiver(mediaType, init);
            if (!result_or_error.ok()) {
                NAPI_THROW(Error::New(info.Env(), result_or_error.error().message()), info.Env().Undefined());
            }
            return NapiRtpTransceiver::NewInstance(info.Env(), result_or_error.value(), wrapper_.get());
        } else {
            auto result_or_error = pc_->AddTransceiver(mediaType);
            if (!result_or_error.ok()) {
                NAPI_THROW(Error::New(info.Env(), result_or_error.error().message()), info.Env().Undefined());
            }
            return NapiRtpTransceiver::NewInstance(info.Env(), result_or_error.value(), wrapper_.get());
        }
    }
    NAPI_THROW(Error::New(info.Env(), "Invalid argument type for the first parameter"), info.Env().Undefined());
}

Napi::Value NapiPeerConnection::Close(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    // Close操作可能耗时较长，放到信令线程执行以避免阻塞主线程
    pc_->signaling_thread()->PostTask([pc = pc_] {
        RTC_DLOG(LS_INFO) << "Do Close";
        pc->Close();
    });

    return info.Env().Undefined();
}

Napi::Value NapiPeerConnection::GetStats(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() == 0 || info[0].IsNull() || info[0].IsUndefined()) {
        auto asyncWorker = AsyncWorkerGetStats::Create(info.Env(), "GetStats");
        pc_->GetStats(asyncWorker->GetCallback().get());
        return asyncWorker->GetPromise();
    }

    if (!info[0].IsObject()) {
        auto deferred = Promise::Deferred::New(info.Env());
        deferred.Reject(Error::New(info.Env(), "Invalid argument").Value());
        return deferred.Promise();
    }

    auto napiTrack = NapiMediaStreamTrack::Unwrap(info[0].As<Object>());
    if (!napiTrack) {
        auto deferred = Promise::Deferred::New(info.Env());
        deferred.Reject(Error::New(info.Env(), "Invalid argument").Value());
        return deferred.Promise();
    }

    auto track = napiTrack->Get();
    auto senders = pc_->GetSenders();
    auto receivers = pc_->GetReceivers();

    auto numOfSenders = std::count_if(senders.cbegin(), senders.cend(), [track](const auto& sender) {
        auto t = sender->track();
        return t && t->id() == track->id();
    });
    auto numOfReceivers = std::count_if(receivers.cbegin(), receivers.cend(), [track](const auto& receiver) {
        return receiver->track()->id() == track->id();
    });

    if ((numOfSenders + numOfReceivers) != 1) {
        // reject with InvalidAccessError
        auto deferred = Promise::Deferred::New(info.Env());
        deferred.Reject(Error::New(info.Env(), "Invalid access").Value());
        return deferred.Promise();
    }

    auto asyncWorker = AsyncWorkerGetStats::Create(info.Env(), "GetStats");
    if (numOfSenders == 1) {
        auto sender = std::find_if(senders.cbegin(), senders.cend(), [track](const auto& sender) {
            auto t = sender->track();
            return t && t->id() == track->id();
        });
        pc_->GetStats(*sender, asyncWorker->GetCallback());
    } else {
        auto receiver = std::find_if(receivers.cbegin(), receivers.cend(), [track](const auto& receiver) {
            return receiver->track()->id() == track->id();
        });
        pc_->GetStats(*receiver, asyncWorker->GetCallback());
    }

    return asyncWorker->GetPromise();
}

Napi::Value NapiPeerConnection::ToJson(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", String::New(info.Env(), "NapiPeerConnection"));
#endif

    return json;
}

Napi::Value NapiPeerConnection::GenerateCertificate(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto asyncWorker = new AsyncWorkerCertificate(info.Env(), "GenerateCertificateWorker");
    auto deferred = asyncWorker->GetDeferred();

    if (info.Length() < 1 || info[0].IsObject()) {
        deferred.Reject(Error::New(info.Env(), "Invalid argument").Value());
        return asyncWorker->GetPromise();
    }

    rtc::KeyParams key_params;
    auto keyParamsName = info[0].As<Napi::String>().Utf8Value();

    if (keyParamsName == "RSA") {
        key_params = rtc::KeyParams::RSA();
    } else if (keyParamsName == "ECDSA") {
        key_params = rtc::KeyParams::ECDSA();
    } else {
        RTC_DLOG(LS_ERROR) << "Unsupported key algorithm";
    }

    asyncWorker->start(key_params, absl::nullopt);
    return asyncWorker->GetPromise();
}

} // namespace webrtc
