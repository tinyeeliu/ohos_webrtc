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

#ifndef WEBRTC_ICE_CANDIDATE_H
#define WEBRTC_ICE_CANDIDATE_H

#include <vector>

#include "api/data_channel_interface.h"
#include "api/jsep.h"
#include "api/jsep_ice_candidate.h"
#include "api/peer_connection_interface.h"
#include "api/rtp_parameters.h"
#include "rtc_base/ssl_identity.h"

#include "napi.h"

namespace webrtc {

class NapiIceCandidate : public Napi::ObjectWrap<NapiIceCandidate> {
public:
    static void Init(Napi::Env env, Napi::Object exports);

    static Napi::Object NewInstance(const Napi::CallbackInfo& info, IceCandidateInterface* candidate);

    NapiIceCandidate(const Napi::CallbackInfo& info);

protected:
    Napi::Value GetCandidate(const Napi::CallbackInfo& info);
    Napi::Value GetSdpMid(const Napi::CallbackInfo& info);
    Napi::Value GetSdpMLineIndex(const Napi::CallbackInfo& info);
    Napi::Value GetUsernameFragment(const Napi::CallbackInfo& info);

    Napi::Value ToJson(const Napi::CallbackInfo& info);

private:
    static Napi::FunctionReference constructor_;

    std::string sdp;
    std::string sdpMid;
    int sdpMLineIndex{0};
    cricket::Candidate candidate_;
    //    IceCandidateInterface* candidate_{};
};

cricket::Candidate JsToNativeCandidate(Napi::Env env, const Napi::Value& jsCandidate);

Napi::Object NativeToJsCandidate(
    Napi::Env env, const std::string& sdpMid, int sdpMLineIndex, const std::string& sdp,
    const cricket::Candidate& candidate);

Napi::Object NativeToJsCandidate(Napi::Env env, const cricket::Candidate& candidate);

Napi::Object NativeToJsIceCandidate(Napi::Env env, const IceCandidateInterface& candidate);

} // namespace webrtc

#endif // WEBRTC_ICE_CANDIDATE_H
