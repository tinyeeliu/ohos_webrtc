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

#include "rtp_transceiver.h"
#include "rtp_receiver.h"
#include "rtp_sender.h"
#include "media_stream.h"
#include "rtp_parameters.h"
#include "peer_connection.h"

#include <atomic>
#include <thread>

#include "rtc_base/logging.h"
#include "api/rtp_transceiver_direction.h"

namespace webrtc {

// [WDIAG] NewInstance 并发诊断:检测 H2(FunctionReference 并发竞态)。
static std::atomic<int> g_transceiverNewInFlight{0};

using namespace Napi;

const char kClassName[] = "RTCRtpTransceiver";

const char kAttributeNameTrack[] = "mid";
const char kAttributeNameSender[] = "sender";
const char kAttributeNameReceiver[] = "receiver";
const char kAttributeNameDirection[] = "direction";
const char kAttributeNameCurrentDirection[] = "currentDirection";
const char kAttributeNameStreams[] = "streams";
const char kAttributeNameSendEncodings[] = "sendEncodings";
const char kAttributeNameActive[] = "active";
const char kAttributeNameMaxBitrate[] = "maxBitrate";
const char kAttributeNameMaxFramerate[] = "maxFramerate";
const char kAttributeNameScaleResolutionDownBy[] = "scaleResolutionDownBy";

const char kMethodNameStop[] = "stop";
const char kMethodNameSetCodecPreferences[] = "setCodecPreferences";
const char kMethodNameToJson[] = "toJSON";

const char kEnumRtpTransceiverDirectionInactive[] = "inactive";
const char kEnumRtpTransceiverDirectionRecvOnly[] = "recvonly";
const char kEnumRtpTransceiverDirectionSendOnly[] = "sendonly";
const char kEnumRtpTransceiverDirectionSendRecv[] = "sendrecv";
const char kEnumRtpTransceiverDirectionStopped[] = "stopped";

thread_local FunctionReference NapiRtpTransceiver::constructor_;
static constexpr int indexTwo = 2;
static constexpr int callbackInfoLen = 3;

void NapiRtpTransceiver::Init(Napi::Env env, Napi::Object exports)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    Function func = DefineClass(
        env, kClassName,
        {
            InstanceAccessor<&NapiRtpTransceiver::GetMid>(kAttributeNameTrack),
            InstanceAccessor<&NapiRtpTransceiver::GetSender>(kAttributeNameSender),
            InstanceAccessor<&NapiRtpTransceiver::GetReceiver>(kAttributeNameReceiver),
            InstanceAccessor<&NapiRtpTransceiver::GetDirection, &NapiRtpTransceiver::SetDirection>(
                kAttributeNameDirection),
            InstanceAccessor<&NapiRtpTransceiver::GetCurrentDirection>(kAttributeNameCurrentDirection),
            InstanceMethod<&NapiRtpTransceiver::Stop>(kMethodNameStop),
            InstanceMethod<&NapiRtpTransceiver::SetCodecPreferences>(kMethodNameSetCodecPreferences),
            InstanceMethod<&NapiRtpTransceiver::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
    constructor_.SuppressDestruct();
}

Napi::Object NapiRtpTransceiver::NewInstance(
    std::shared_ptr<PeerConnectionFactoryWrapper> factory, rtc::scoped_refptr<PeerConnectionInterface> pc,
    rtc::scoped_refptr<RtpTransceiverInterface> transceiver)
{
    // [WDIAG] H2 并发 + H3 env错配 诊断
    int cur = ++g_transceiverNewInFlight;
    RTC_LOG(LS_INFO) << "[WDIAG] Transceiver::NewInstance enter inFlight=" << cur
                     << " ctorEnv=" << (void*)(napi_env)constructor_.Env()
                     << " tid=" << std::this_thread::get_id();

    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto env = constructor_.Env();
    if (!factory || !pc || !transceiver) {
        RTC_LOG(LS_ERROR) << "[WDIAG] Transceiver::NewInstance null arg, inFlight=" << g_transceiverNewInFlight;
        --g_transceiverNewInFlight;
        NAPI_THROW(Error::New(env, "Invalid argument"), Object());
    }

    Napi::HandleScope scope(env);
    auto obj = constructor_.New(
        {External<std::shared_ptr<PeerConnectionFactoryWrapper>>::New(env, &factory),
         External<rtc::scoped_refptr<PeerConnectionInterface>>::New(env, &pc),
         External<rtc::scoped_refptr<RtpTransceiverInterface>>::New(env, &transceiver)});
    RTC_LOG(LS_INFO) << "[WDIAG] Transceiver::NewInstance exit inFlight=" << (--g_transceiverNewInFlight)
                     << " obj=" << (void*)(napi_value)obj;
    return obj;
}

NapiRtpTransceiver::NapiRtpTransceiver(const Napi::CallbackInfo& info) : Napi::ObjectWrap<NapiRtpTransceiver>(info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    // Create from native with 3 paramters, and SHOULD NOT create from ArkTS
    if (info.Length() != callbackInfoLen || !info[0].IsExternal() || !info[1].IsExternal() ||
        !info[indexTwo].IsExternal()) {
        NAPI_THROW_VOID(Napi::Error::New(info.Env(), "Invalid Operation"));
    }

    factory_ = *info[0].As<External<std::shared_ptr<PeerConnectionFactoryWrapper>>>().Data();
    pc_ = *info[1].As<External<rtc::scoped_refptr<PeerConnectionInterface>>>().Data();
    rtpTransceiver_ = *info[indexTwo].As<External<rtc::scoped_refptr<RtpTransceiverInterface>>>().Data();
}

NapiRtpTransceiver::~NapiRtpTransceiver()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

rtc::scoped_refptr<RtpTransceiverInterface> NapiRtpTransceiver::Get() const
{
    return rtpTransceiver_;
}

Napi::Value NapiRtpTransceiver::GetMid(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto mid = rtpTransceiver_->mid();
    if (!mid) {
        return info.Env().Null();
    }

    return String::New(info.Env(), *mid);
}

Napi::Value NapiRtpTransceiver::GetSender(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    return NapiRtpSender::NewInstance(factory_, pc_, rtpTransceiver_->sender());
}

Napi::Value NapiRtpTransceiver::GetReceiver(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    return NapiRtpReceiver::NewInstance(factory_, pc_, rtpTransceiver_->receiver());
}

Napi::Value NapiRtpTransceiver::GetDirection(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto direction = rtpTransceiver_->direction();
    switch (direction) {
        case RtpTransceiverDirection::kInactive:
            return String::New(info.Env(), kEnumRtpTransceiverDirectionInactive);
        case RtpTransceiverDirection::kRecvOnly:
            return String::New(info.Env(), kEnumRtpTransceiverDirectionRecvOnly);
        case RtpTransceiverDirection::kSendOnly:
            return String::New(info.Env(), kEnumRtpTransceiverDirectionSendOnly);
        case RtpTransceiverDirection::kSendRecv:
            return String::New(info.Env(), kEnumRtpTransceiverDirectionSendRecv);
        case RtpTransceiverDirection::kStopped:
            return String::New(info.Env(), kEnumRtpTransceiverDirectionStopped);
        default:
            RTC_LOG(LS_ERROR) << "Invalid direction: " << direction;
            break;
    }

    NAPI_THROW(Error::New(info.Env(), "Invalid direction"), info.Env().Null());
}

void NapiRtpTransceiver::SetDirection(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!value.IsString()) {
        NAPI_THROW_VOID(Error::New(info.Env(), "The argument is not string"));
    }

    auto jsDirection = value.As<String>().Utf8Value();
    auto newDirection = RtpTransceiverDirection::kInactive;

    if (jsDirection == kEnumRtpTransceiverDirectionInactive) {
        newDirection = RtpTransceiverDirection::kInactive;
    } else if (jsDirection == kEnumRtpTransceiverDirectionRecvOnly) {
        newDirection = RtpTransceiverDirection::kRecvOnly;
    } else if (jsDirection == kEnumRtpTransceiverDirectionSendOnly) {
        newDirection = RtpTransceiverDirection::kSendOnly;
    } else if (jsDirection == kEnumRtpTransceiverDirectionSendRecv) {
        newDirection = RtpTransceiverDirection::kSendRecv;
    } else if (jsDirection == kEnumRtpTransceiverDirectionStopped) {
        newDirection = RtpTransceiverDirection::kStopped;
    }

    // SetDirectionWithError must be called on the signaling thread.
    RTCError error;
    pc_->signaling_thread()->BlockingCall([this, newDirection, &error] {
        error = rtpTransceiver_->SetDirectionWithError(newDirection);
    });
    if (!error.ok()) {
        RTC_LOG(LS_ERROR) << "Failed to set direction: " << error.type() << ", " << error.message();
        NAPI_THROW_VOID(Error::New(info.Env(), "Failed to set direction"));
    }
}

Napi::Value NapiRtpTransceiver::GetCurrentDirection(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!rtpTransceiver_->current_direction()) {
        return info.Env().Null();
    }

    switch (rtpTransceiver_->current_direction().value()) {
        case RtpTransceiverDirection::kInactive:
            return String::New(info.Env(), kEnumRtpTransceiverDirectionInactive);
        case RtpTransceiverDirection::kRecvOnly:
            return String::New(info.Env(), kEnumRtpTransceiverDirectionRecvOnly);
        case RtpTransceiverDirection::kSendOnly:
            return String::New(info.Env(), kEnumRtpTransceiverDirectionSendOnly);
        case RtpTransceiverDirection::kSendRecv:
            return String::New(info.Env(), kEnumRtpTransceiverDirectionSendRecv);
        case RtpTransceiverDirection::kStopped:
            return String::New(info.Env(), kEnumRtpTransceiverDirectionStopped);
        default:
            RTC_LOG(LS_WARNING) << "Invalid value: " << rtpTransceiver_->current_direction().value();
            break;
    }

    return info.Env().Null();
}

Napi::Value NapiRtpTransceiver::Stop(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    // StopStandard must be called on the signaling thread.
    RTCError error;
    pc_->signaling_thread()->BlockingCall([this, &error] {
        error = rtpTransceiver_->StopStandard();
    });
    if (!error.ok()) {
        RTC_LOG(LS_ERROR) << "Failed to stop: " << error.type() << ", " << error.message();
        NAPI_THROW(Error::New(info.Env(), "Failed to stop"), info.Env().Undefined());
    }

    return info.Env().Undefined();
}

Napi::Value NapiRtpTransceiver::SetCodecPreferences(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 1) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsArray()) {
        NAPI_THROW(Error::New(info.Env(), "First argument is not array"), info.Env().Undefined());
    }

    auto jsCodecs = info[0].As<Array>();
    auto codecs = std::vector<RtpCodecCapability>();
    for (uint32_t i = 0; i < jsCodecs.Length(); i++) {
        Napi::Value jsCodec = jsCodecs[i];
        RtpCodecCapability codec;
        NapiRtpCodecCapability::JsToNative(jsCodec.As<Object>(), codec);
        if (info.Env().IsExceptionPending()) {
            NAPI_THROW(info.Env().GetAndClearPendingException(), info.Env().Undefined());
        }
        codecs.push_back(codec);
    }

    auto error = rtpTransceiver_->SetCodecPreferences(codecs);
    if (!error.ok()) {
        RTC_LOG(LS_ERROR) << "Failed to set codec preferences: " << error.type() << ", " << error.message();
        NAPI_THROW(Error::New(info.Env(), "Failed to set codec preferences"), info.Env().Undefined());
    }

    return info.Env().Undefined();
}

Napi::Value NapiRtpTransceiver::ToJson(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", "NapiRtpTransceiver");
#endif

    return json;
}

void PopulateTransceiverInit(const Object& obj, webrtc::RtpTransceiverInit& init)
{
    // Handle direction
    if (obj.Has(kAttributeNameDirection)) {
        std::string direction = obj.Get(kAttributeNameDirection).As<String>();
        if (direction == kEnumRtpTransceiverDirectionInactive) {
            init.direction = webrtc::RtpTransceiverDirection::kInactive;
        } else if (direction == kEnumRtpTransceiverDirectionRecvOnly) {
            init.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
        } else if (direction == kEnumRtpTransceiverDirectionSendOnly) {
            init.direction = webrtc::RtpTransceiverDirection::kSendOnly;
        } else if (direction == kEnumRtpTransceiverDirectionSendRecv) {
            init.direction = webrtc::RtpTransceiverDirection::kSendRecv;
        } else if (direction == kEnumRtpTransceiverDirectionStopped) {
            init.direction = webrtc::RtpTransceiverDirection::kStopped;
        }
    }

    // Handle streams
    if (obj.Has(kAttributeNameStreams)) {
        Array streamsArray = obj.Get(kAttributeNameStreams).As<Array>();
        for (uint32_t i = 0; i < streamsArray.Length(); i++) {
            Value streamValue = streamsArray.Get(i);
            if (streamValue.IsObject()) {
                auto napiStream = NapiMediaStream::Unwrap(streamValue.As<Object>());
                // 判空:Unwrap 可能返回 nullptr(对象不是合法 MediaStream 包装),
                // Get() 可能返回空 ref(对象已失效)。漏判会导致 stream->id() NULL 解引用
                // (struct 偏移 @0x30)崩溃。参照 media_stream.cpp:111-114 的判空范式。
                if (!napiStream) {
                    RTC_LOG(LS_WARNING) << "PopulateTransceiverInit: streams[" << i
                                        << "] is not a valid MediaStream, skipped";
                    continue;
                }
                auto stream = napiStream->Get();
                if (!stream) {
                    RTC_LOG(LS_WARNING) << "PopulateTransceiverInit: streams[" << i
                                        << "] has null native stream, skipped";
                    continue;
                }
                init.stream_ids.push_back(stream->id());
            }
        }
    }

    // Handle sendEncodings
    if (obj.Has(kAttributeNameSendEncodings)) {
        Array encodingsArray = obj.Get(kAttributeNameSendEncodings).As<Array>();
        for (uint32_t i = 0; i < encodingsArray.Length(); i++) {
            Value encodingValue = encodingsArray.Get(i);
            if (encodingValue.IsObject()) {
                Object encodingObj = encodingValue.As<Object>();
                webrtc::RtpEncodingParameters encodingParams;
                NapiRtpEncodingParameters::JsToNative(encodingObj, encodingParams);
                init.send_encodings.push_back(encodingParams);
            }
        }
    }
}

} // namespace webrtc
