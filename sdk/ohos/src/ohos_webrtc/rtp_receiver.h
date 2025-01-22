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

#ifndef WEBRTC_RTP_RECEIVER_H
#define WEBRTC_RTP_RECEIVER_H

#include "async_work/async_worker_get_stats.h"

#include "napi.h"

#include "api/rtp_receiver_interface.h"

namespace webrtc {

class NapiPeerConnectionWrapper;

class NapiRtpReceiver : public Napi::ObjectWrap<NapiRtpReceiver> {
public:
    static void Init(Napi::Env env, Napi::Object exports);

    static Napi::Object
    NewInstance(Napi::Env env, rtc::scoped_refptr<RtpReceiverInterface> receiver, NapiPeerConnectionWrapper* pcWrapper);

    explicit NapiRtpReceiver(const Napi::CallbackInfo& info);

    rtc::scoped_refptr<RtpReceiverInterface> Get() const;

protected:
    static Napi::Value GetCapabilities(const Napi::CallbackInfo& info);

    Napi::Value GetTrack(const Napi::CallbackInfo& info);
    Napi::Value GetTransport(const Napi::CallbackInfo& info);
    Napi::Value GetId(const Napi::CallbackInfo& info); // 新增：暴露 id 属性的 getter

    Napi::Value GetParameters(const Napi::CallbackInfo& info);
    Napi::Value GetStats(const Napi::CallbackInfo& info);
    Napi::Value GetContributingSources(const Napi::CallbackInfo& info);
    Napi::Value GetSynchronizationSources(const Napi::CallbackInfo& info);
    Napi::Value ToJson(const Napi::CallbackInfo& info);

private:
    static Napi::FunctionReference constructor_;

    rtc::scoped_refptr<RtpReceiverInterface> rtpReceiver_;
    NapiPeerConnectionWrapper* pcWrapper_{};
};

} // namespace webrtc

#endif // WEBRTC_RTP_RECEIVER_H
