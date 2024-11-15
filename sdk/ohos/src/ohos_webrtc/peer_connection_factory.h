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

#ifndef WEBRTC_PEER_CONNECTION_FACTORY_H
#define WEBRTC_PEER_CONNECTION_FACTORY_H

#include "audio_device/audio_device_module.h"

#include <memory>
#include <mutex>

#include "napi.h"

#include "api/peer_connection_interface.h"
#include "rtc_base/socket_server.h"

namespace webrtc {

class PeerConnectionFactoryWrapper : public std::enable_shared_from_this<PeerConnectionFactoryWrapper> {
public:
    static std::shared_ptr<PeerConnectionFactoryWrapper> GetDefault();
    
    static void SetDefault(std::shared_ptr<PeerConnectionFactoryWrapper> wrapper);

    static std::shared_ptr<PeerConnectionFactoryWrapper> Create(
        rtc::scoped_refptr<AudioDeviceModule> adm = nullptr,
        std::unique_ptr<VideoEncoderFactory> videoEncoderFactory = nullptr,
        std::unique_ptr<VideoDecoderFactory> videoDecoderFactory = nullptr,
        rtc::scoped_refptr<AudioProcessing> audioProcessing = nullptr);

    PeerConnectionFactoryInterface* GetFactory()
    {
        return pcFactory_.get();
    }

    rtc::SocketServer* GetSocketServer()
    {
        return socketServer_.get();
    }

    rtc::Thread* GetNetworkThread()
    {
        return networkThread_.get();
    }

    rtc::Thread* GetSignalingThread()
    {
        return signalingThread_.get();
    }

    rtc::Thread* GetWorkerThread()
    {
        return workerThread_.get();
    }

protected:
    PeerConnectionFactoryWrapper() = default;

    bool Init(
        rtc::scoped_refptr<AudioDeviceModule> adm, std::unique_ptr<VideoEncoderFactory> videoEncoderFactory,
        std::unique_ptr<VideoDecoderFactory> videoDecoderFactory, rtc::scoped_refptr<AudioProcessing> audioProcessing);

private:
    static std::mutex mutex_;
    static std::shared_ptr<PeerConnectionFactoryWrapper> defaultFactory_;

    std::unique_ptr<rtc::SocketServer> socketServer_;
    std::unique_ptr<rtc::Thread> networkThread_;
    std::unique_ptr<rtc::Thread> workerThread_;
    std::unique_ptr<rtc::Thread> signalingThread_;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pcFactory_;
};

class NapiPeerConnectionFactory : public Napi::ObjectWrap<NapiPeerConnectionFactory> {
public:
    static void Init(Napi::Env env, Napi::Object exports);

    NapiPeerConnectionFactory(const Napi::CallbackInfo& info);

    std::shared_ptr<PeerConnectionFactoryWrapper> GetWrapper() const
    {
        return wrapper_;
    }

protected:
    static Napi::Value SetDefault(const Napi::CallbackInfo& info);
    
    Napi::Value CreatePeerConnection(const Napi::CallbackInfo& info);
    Napi::Value CreateAudioSource(const Napi::CallbackInfo& info);
    Napi::Value CreateAudioTrack(const Napi::CallbackInfo& info);
    Napi::Value CreateVideoSource(const Napi::CallbackInfo& info);
    Napi::Value CreateVideoTrack(const Napi::CallbackInfo& info);
    Napi::Value StartAecDump(const Napi::CallbackInfo& info);
    Napi::Value StopAecDump(const Napi::CallbackInfo& info);

    Napi::Value ToJson(const Napi::CallbackInfo& info);

private:
    static Napi::FunctionReference constructor_;

    PeerConnectionFactoryInterface* GetFactory() const
    {
        return wrapper_->GetFactory();
    }

    std::shared_ptr<PeerConnectionFactoryWrapper> wrapper_;
};

} // namespace webrtc

#endif // WEBRTC_PEER_CONNECTION_FACTORY_H
