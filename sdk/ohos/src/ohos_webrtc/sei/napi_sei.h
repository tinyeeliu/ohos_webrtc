// Copyright (c) 2026. Licensed under the Apache License, Version 2.0.
#ifndef OHOS_WEBRTC_SEI_NAPI_SEI_H
#define OHOS_WEBRTC_SEI_NAPI_SEI_H

#include <atomic>
#include <functional>
#include "napi.h"
#include "api/peer_connection_interface.h"
#include "sei/sei_transformer.h"

namespace webrtc {
class PeerConnectionFactoryWrapper;

class NapiEncoderWrapper : public Napi::ObjectWrap<NapiEncoderWrapper> {
public:
    static void Init(Napi::Env env, Napi::Object exports);
    static Napi::Object NewInstance(Napi::Env env, std::shared_ptr<PeerConnectionFactoryWrapper> factory,
        rtc::scoped_refptr<PeerConnectionInterface> pc, std::shared_ptr<sei::PeerSeiContext> context,
        rtc::scoped_refptr<sei::SeiTransformer> transformer);

protected:
    friend class Napi::ObjectWrap<NapiEncoderWrapper>;
    explicit NapiEncoderWrapper(const Napi::CallbackInfo& info);
    Napi::Value WriteSei(const Napi::CallbackInfo& info);
    Napi::Value ClearSei(const Napi::CallbackInfo& info);
    Napi::Value GetReady(const Napi::CallbackInfo& info);
    Napi::Value GetPending(const Napi::CallbackInfo& info);
    Napi::Value GetInjected(const Napi::CallbackInfo& info);

private:
    static thread_local Napi::FunctionReference constructor_;
    std::shared_ptr<PeerConnectionFactoryWrapper> factory_;
    rtc::scoped_refptr<PeerConnectionInterface> pc_;
    std::shared_ptr<sei::PeerSeiContext> context_;
    rtc::scoped_refptr<sei::SeiTransformer> transformer_;
};

// Uses raw N-API TSFN so queued payloads are freed even when env/jsCallback is
// null during environment teardown (the bundled node-addon-api skips that case).
class SeiJsCallback : public sei::Observer, public std::enable_shared_from_this<SeiJsCallback> {
public:
    static std::shared_ptr<SeiJsCallback> Create(Napi::Env env, Napi::Function callback,
        std::function<void(napi_env)> finalizer);
    void Notify(const sei::ReceivedMessage& message) override;
    void Close() override;
    bool Active() const { return active_.load(); }

private:
    struct Notice;
    static void CallJs(napi_env env, napi_value callback, void* context, void* data);
    static void Finalize(napi_env env, void* data, void* hint);
    std::mutex mutex_;
    napi_threadsafe_function tsfn_ = nullptr;
    std::atomic<bool> active_{true};
    std::atomic<uint32_t> dropped_{0};
    std::function<void(napi_env)> finalizer_;
};
} // namespace webrtc
#endif
