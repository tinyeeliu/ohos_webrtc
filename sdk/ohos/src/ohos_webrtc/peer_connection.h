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

#ifndef WEBRTC_PEER_CONNECTION_H
#define WEBRTC_PEER_CONNECTION_H

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "napi.h"

#include "api/peer_connection_interface.h"

namespace webrtc {

class PeerConnectionFactoryWrapper;

class NapiPeerConnectionWrapper : public PeerConnectionObserver {
public:
    static std::unique_ptr<NapiPeerConnectionWrapper> Create(
        std::shared_ptr<PeerConnectionFactoryWrapper> factoryWrapper,
        const PeerConnectionInterface::RTCConfiguration& config);

    // Do not use this constructor directly, use 'Create' above.
    NapiPeerConnectionWrapper();
    ~NapiPeerConnectionWrapper() override;

    const std::shared_ptr<PeerConnectionFactoryWrapper> GetPeerConnectionFactoryWrapper() const
    {
        return factory_;
    }

    const rtc::scoped_refptr<PeerConnectionInterface>& GetPeerConnection() const
    {
        return pc_;
    }

    bool GetEventHandler(const std::string& type, Napi::Function& fn);
    bool SetEventHandler(const std::string& type, Napi::Function fn, Napi::Value receiver);
    bool RemoveEventHandler(const std::string& type);
    void RemoveAllEventHandlers();

protected:
    friend class NapiPeerConnection;

    void SetPeerConnectionFactoryWrapper(std::shared_ptr<PeerConnectionFactoryWrapper> factoryWrapper)
    {
        factory_ = std::move(factoryWrapper);
    }

    void SetPeerConnection(rtc::scoped_refptr<PeerConnectionInterface> pc)
    {
        pc_ = pc;
    }

    bool GetEventHandler(const std::string& type, Napi::ThreadSafeFunction& tsfn);

protected:
    void OnIceCandidate(const IceCandidateInterface* candidate) override;
    void OnIceCandidateError(
        const std::string& address, int port, const std::string& url, int error_code,
        const std::string& error_text) override;

    void OnIceCandidatesRemoved(const std::vector<cricket::Candidate>& candidates) override;
    void OnSignalingChange(PeerConnectionInterface::SignalingState new_state) override;
    void OnIceConnectionChange(PeerConnectionInterface::IceConnectionState new_state) override;
    void OnStandardizedIceConnectionChange(PeerConnectionInterface::IceConnectionState new_state) override;
    void OnConnectionChange(PeerConnectionInterface::PeerConnectionState new_state) override;
    void OnIceConnectionReceivingChange(bool receiving) override;
    void OnIceGatheringChange(PeerConnectionInterface::IceGatheringState new_state) override;
    void OnIceSelectedCandidatePairChanged(const cricket::CandidatePairChangeEvent& event) override;
    void OnAddStream(rtc::scoped_refptr<MediaStreamInterface> stream) override;
    void OnRemoveStream(rtc::scoped_refptr<MediaStreamInterface> stream) override;
    void OnDataChannel(rtc::scoped_refptr<DataChannelInterface> channel) override;
    void OnRenegotiationNeeded() override;
    void OnNegotiationNeededEvent(uint32_t event_id) override;
    void OnAddTrack(
        rtc::scoped_refptr<RtpReceiverInterface> receiver,
        const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) override;
    void OnTrack(rtc::scoped_refptr<RtpTransceiverInterface> transceiver) override;
    void OnRemoveTrack(rtc::scoped_refptr<RtpReceiverInterface> receiver) override;

private:
    std::shared_ptr<PeerConnectionFactoryWrapper> factory_;
    rtc::scoped_refptr<PeerConnectionInterface> pc_;

    struct EventHandler {
        Napi::FunctionReference ref;
        Napi::ThreadSafeFunction tsfn;
    };

    // set by 'onxxx' like event handler attribute, eg. onicecandidate.
    std::map<std::string, EventHandler> eventHandlers_;

    std::mutex eventMutex_;

    // Call event handler in this thread (instead of signaling thread) to avoid potential deadlock.
    // There is a bug in OpenHarmony (4.1 and before), see
    // https://gitee.com/openharmony/arkui_napi/commit/485e75b6f623bcacf9ab4cb4c82bf0f077fe1f6e
    std::unique_ptr<rtc::Thread> eventThread_;
};

class NapiPeerConnection : public Napi::ObjectWrap<NapiPeerConnection> {
public:
    static void Init(Napi::Env env, Napi::Object exports);

    static Napi::Value NewInstance(Napi::Env env, std::unique_ptr<NapiPeerConnectionWrapper> wrapper);

    explicit NapiPeerConnection(const Napi::CallbackInfo& info);
    ~NapiPeerConnection();

protected:
    Napi::Value GetCanTrickleIceCandidates(const Napi::CallbackInfo& info);
    Napi::Value GetSignalingState(const Napi::CallbackInfo& info);
    Napi::Value GetIceGatheringState(const Napi::CallbackInfo& info);
    Napi::Value GetIceConnectionState(const Napi::CallbackInfo& info);
    Napi::Value GetConnectionState(const Napi::CallbackInfo& info);
    Napi::Value GetLocalDescription(const Napi::CallbackInfo& info);
    Napi::Value GetRemoteDescription(const Napi::CallbackInfo& info);
    Napi::Value GetCurrentLocalDescription(const Napi::CallbackInfo& info);
    Napi::Value GetCurrentRemoteDescription(const Napi::CallbackInfo& info);
    Napi::Value GetPendingLocalDescription(const Napi::CallbackInfo& info);
    Napi::Value GetPendingRemoteDescription(const Napi::CallbackInfo& info);
    Napi::Value GetSctp(const Napi::CallbackInfo& info);

    Napi::Value GetEventHandler(const Napi::CallbackInfo& info);
    void SetEventHandler(const Napi::CallbackInfo& info, const Napi::Value& value);

    Napi::Value AddTrack(const Napi::CallbackInfo& info);
    Napi::Value RemoveTrack(const Napi::CallbackInfo& info);
    Napi::Value SetLocalDescription(const Napi::CallbackInfo& info);
    Napi::Value SetRemoteDescription(const Napi::CallbackInfo& info);
    Napi::Value CreateOffer(const Napi::CallbackInfo& info);
    Napi::Value CreateAnswer(const Napi::CallbackInfo& info);
    Napi::Value CreateDataChannel(const Napi::CallbackInfo& info);
    Napi::Value AddIceCandidate(const Napi::CallbackInfo& info);
    Napi::Value GetSenders(const Napi::CallbackInfo& info);
    Napi::Value GetReceivers(const Napi::CallbackInfo& info);
    Napi::Value GetTransceivers(const Napi::CallbackInfo& info);
    Napi::Value GetConfiguration(const Napi::CallbackInfo& info);
    Napi::Value SetConfiguration(const Napi::CallbackInfo& info);
    Napi::Value RestartIce(const Napi::CallbackInfo& info);
    Napi::Value AddTransceiver(const Napi::CallbackInfo& info);
    Napi::Value Close(const Napi::CallbackInfo& info);
    Napi::Value GetStats(const Napi::CallbackInfo& info);
    Napi::Value ToJson(const Napi::CallbackInfo& info);

    static Napi::Value GenerateCertificate(const Napi::CallbackInfo& info);

private:
    static Napi::FunctionReference constructor_;

    std::unique_ptr<NapiPeerConnectionWrapper> wrapper_;
    rtc::scoped_refptr<PeerConnectionInterface> pc_;

    Napi::ObjectReference sctpTransportRef_;
};

} // namespace webrtc

#endif // WEBRTC_PEER_CONNECTION_H
