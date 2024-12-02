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

#ifndef WEBRTC_DTLS_TRANSPORT_H
#define WEBRTC_DTLS_TRANSPORT_H

#include "napi.h"
#include <map>
#include "api/dtls_transport_interface.h"

namespace webrtc {

class NapiPeerConnectionWrapper;

class NapiDtlsTransport : public Napi::ObjectWrap<NapiDtlsTransport>, public DtlsTransportObserverInterface {
public:
    static void Init(Napi::Env env, Napi::Object exports);

    static Napi::Object NewInstance(
        Napi::Env env, rtc::scoped_refptr<DtlsTransportInterface> dtlsTransport, NapiPeerConnectionWrapper* pcWrapper);

    ~NapiDtlsTransport();

protected:
    friend class ObjectWrap;

    explicit NapiDtlsTransport(const Napi::CallbackInfo& info);

protected:
    // JS
    Napi::Value GetIceTransport(const Napi::CallbackInfo& info);
    Napi::Value GetState(const Napi::CallbackInfo& info);

    Napi::Value GetEventHandler(const Napi::CallbackInfo& info);
    void SetEventHandler(const Napi::CallbackInfo& info, const Napi::Value& value);

    Napi::Value GetRemoteCertificates(const Napi::CallbackInfo& info);
    Napi::Value ToJson(const Napi::CallbackInfo& info);

    void OnStateChange(DtlsTransportInformation info) override;
    void OnError(RTCError error) override;

private:
    static Napi::FunctionReference constructor_;

    struct EventHandler {
        Napi::FunctionReference ref;
        Napi::ThreadSafeFunction tsfn;
    };

    mutable std::mutex mutex_;
    std::map<std::string, EventHandler> eventHandlers_;

    rtc::scoped_refptr<DtlsTransportInterface> dtlsTransport_;
    NapiPeerConnectionWrapper* pcWrapper_{};
};

} // namespace webrtc

#endif // WEBRTC_DTLS_TRANSPORT_H
