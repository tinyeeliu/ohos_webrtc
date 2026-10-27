// Copyright (c) 2026. Licensed under the Apache License, Version 2.0.
#include "sei/napi_sei.h"

#include <cstring>
#include <utility>
#include "peer_connection_factory.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
struct EncoderInit {
    std::shared_ptr<PeerConnectionFactoryWrapper> factory;
    rtc::scoped_refptr<PeerConnectionInterface> pc;
    std::shared_ptr<sei::PeerSeiContext> context;
    rtc::scoped_refptr<sei::SeiTransformer> transformer;
};
} // namespace

thread_local Napi::FunctionReference NapiEncoderWrapper::constructor_;

void NapiEncoderWrapper::Init(Napi::Env env, Napi::Object exports)
{
    auto func = DefineClass(env, "EncoderWrapper", {
        InstanceMethod<&NapiEncoderWrapper::WriteSei>("writeSei"),
        InstanceMethod<&NapiEncoderWrapper::ClearSei>("clearSei"),
        InstanceAccessor<&NapiEncoderWrapper::GetReady>("ready"),
        InstanceAccessor<&NapiEncoderWrapper::GetPending>("pendingMessages"),
        InstanceAccessor<&NapiEncoderWrapper::GetInjected>("injectedMessages"),
    });
    func.Set("defaultUuid", Napi::String::New(env, sei::kDefaultSeiUuid));
    func.Set("maxDataBytes", Napi::Number::New(env, sei::kMaxSeiDataBytes));
    func.Set("maxPendingMessages", Napi::Number::New(env, sei::kMaxPendingSeiMessages));
    exports.Set("EncoderWrapper", func);
    constructor_ = Napi::Persistent(func);
    constructor_.SuppressDestruct();
}

Napi::Object NapiEncoderWrapper::NewInstance(Napi::Env env, std::shared_ptr<PeerConnectionFactoryWrapper> factory,
    rtc::scoped_refptr<PeerConnectionInterface> pc, std::shared_ptr<sei::PeerSeiContext> context,
    rtc::scoped_refptr<sei::SeiTransformer> transformer)
{
    EncoderInit init{std::move(factory), std::move(pc), std::move(context), std::move(transformer)};
    return constructor_.New({Napi::External<EncoderInit>::New(env, &init)});
}

NapiEncoderWrapper::NapiEncoderWrapper(const Napi::CallbackInfo& info) : Napi::ObjectWrap<NapiEncoderWrapper>(info)
{
    if (info.Length() != 1 || !info[0].IsExternal()) {
        Napi::TypeError::New(info.Env(), "Use RTCRtpSender.getEncoderWrapper()").ThrowAsJavaScriptException();
        return;
    }
    auto init = info[0].As<Napi::External<EncoderInit>>().Data();
    factory_ = init->factory;
    pc_ = init->pc;
    context_ = init->context;
    transformer_ = init->transformer;
}

Napi::Value NapiEncoderWrapper::WriteSei(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (!context_ || context_->Closed()) {
        Napi::Error::New(env, "PeerConnection is closed").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    if (info.Length() < 1 || !info[0].IsArrayBuffer() ||
        (info.Length() > 1 && !info[1].IsUndefined() && !info[1].IsString())) {
        Napi::TypeError::New(env, "Expected ArrayBuffer data and optional UUID string").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    auto data = info[0].As<Napi::ArrayBuffer>();
    if (data.ByteLength() == 0 || data.ByteLength() > sei::kMaxSeiDataBytes || !data.Data()) {
        Napi::RangeError::New(env, "SEI data must be 1..4096 bytes").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    sei::Message message;
    const auto uuid = info.Length() > 1 && info[1].IsString() ? info[1].As<Napi::String>().Utf8Value() :
        std::string(sei::kDefaultSeiUuid);
    if (!sei::ParseUuid(uuid, &message.uuid)) {
        Napi::TypeError::New(env, "UUID must use xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx format").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    const auto bytes = static_cast<const uint8_t*>(data.Data());
    message.data.assign(bytes, bytes + data.ByteLength()); // Never retain ArkTS-owned memory.
    std::string error;
    if (!transformer_->Enqueue(std::move(message), &error)) {
        Napi::Error::New(env, error).ThrowAsJavaScriptException();
    }
    return env.Undefined();
}

Napi::Value NapiEncoderWrapper::ClearSei(const Napi::CallbackInfo& info)
{
    if (transformer_) { transformer_->Clear(); }
    return info.Env().Undefined();
}
Napi::Value NapiEncoderWrapper::GetReady(const Napi::CallbackInfo& info)
{
    return Napi::Boolean::New(info.Env(), context_ && !context_->Closed() && transformer_->Ready());
}
Napi::Value NapiEncoderWrapper::GetPending(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), transformer_ ? transformer_->Pending() : 0);
}
Napi::Value NapiEncoderWrapper::GetInjected(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), transformer_ ? static_cast<double>(transformer_->Injected()) : 0);
}

struct SeiJsCallback::Notice {
    std::shared_ptr<SeiJsCallback> owner;
    sei::ReceivedMessage message;
};

std::shared_ptr<SeiJsCallback> SeiJsCallback::Create(Napi::Env env, Napi::Function callback,
    std::function<void(napi_env)> finalizer)
{
    auto result = std::make_shared<SeiJsCallback>();
    result->finalizer_ = std::move(finalizer);
    auto retained = new std::shared_ptr<SeiJsCallback>(result);
    auto name = Napi::String::New(env, "H264SeiCallback");
    const auto status = napi_create_threadsafe_function(env, callback, nullptr, name, 64, 1,
        retained, Finalize, nullptr, CallJs, &result->tsfn_);
    if (status != napi_ok) {
        delete retained;
        Napi::Error::New(env, "Unable to create SEI callback").ThrowAsJavaScriptException();
        return nullptr;
    }
    napi_unref_threadsafe_function(env, result->tsfn_);
    return result;
}

void SeiJsCallback::Notify(const sei::ReceivedMessage& message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_.load() || !tsfn_) { return; }
    auto notice = new Notice{shared_from_this(), message};
    if (napi_call_threadsafe_function(tsfn_, notice, napi_tsfn_nonblocking) != napi_ok) {
        delete notice;
        if (dropped_.fetch_add(1) == 0) {
            RTC_LOG(LS_WARNING) << "SEI callback queue full/closing; dropping notifications without blocking video";
        }
    }
}

void SeiJsCallback::Close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    active_.store(false);
    if (tsfn_) {
        napi_release_threadsafe_function(tsfn_, napi_tsfn_abort);
        tsfn_ = nullptr;
    }
}

void SeiJsCallback::Finalize(napi_env env, void* data, void*)
{
    std::unique_ptr<std::shared_ptr<SeiJsCallback>> retained(static_cast<std::shared_ptr<SeiJsCallback>*>(data));
    auto owner = *retained;
    {
        std::lock_guard<std::mutex> lock(owner->mutex_);
        owner->active_.store(false);
        owner->tsfn_ = nullptr;
    }
    // Release the JS receiver reference after queued native data is retired.
    owner->finalizer_(env);
}

void SeiJsCallback::CallJs(napi_env env, napi_value callback, void*, void* data)
{
    std::unique_ptr<Notice> notice(static_cast<Notice*>(data));
    if (!env || !callback || !notice || !notice->owner->Active()) { return; }
    Napi::HandleScope scope(env);
    const auto& received = notice->message;
    auto event = Napi::Object::New(env);
    event.Set("type", "sei");
    event.Set("payloadType", 5);
    event.Set("codec", "video/H264");
    event.Set("uuid", sei::FormatUuid(received.message.uuid));
    event.Set("ssrc", Napi::Number::New(env, received.ssrc));
    event.Set("rtpTimestamp", Napi::Number::New(env, received.rtpTimestamp));
    event.Set("droppedMessages", Napi::Number::New(env, notice->owner->dropped_.load()));
    auto buffer = Napi::ArrayBuffer::New(env, received.message.data.size());
    if (!received.message.data.empty()) {
        std::memcpy(buffer.Data(), received.message.data.data(), received.message.data.size());
    }
    event.Set("data", buffer);
    Napi::Function(env, callback).Call({event});
    if (Napi::Env(env).IsExceptionPending()) {
        Napi::Env(env).GetAndClearPendingException();
        RTC_LOG(LS_ERROR) << "Exception in ArkTS onsei callback";
    }
}
} // namespace webrtc
