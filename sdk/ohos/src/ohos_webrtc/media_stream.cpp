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

#include "media_stream.h"
#include "media_stream_track.h"
#include "peer_connection_factory.h"

#include "api/rtc_error.h"
#include "rtc_base/logging.h"
#include "rtc_base/helpers.h"

namespace webrtc {

using namespace Napi;

const char kClassName[] = "MediaStream";

const char kAttributeNameId[] = "id";
const char kAttributeNameActive[] = "active";

const char kMethodNameAddTrack[] = "addTrack";
const char kMethodNameRemoveTrack[] = "removeTrack";
const char kMethodNameGetTrackById[] = "getTrackById";
const char kMethodNameGetTracks[] = "getTracks";
const char kMethodNameGetAudioTracks[] = "getAudioTracks";
const char kMethodNameGetVideoTracks[] = "getVideoTracks";
const char kMethodNameToJson[] = "toJSON";

FunctionReference NapiMediaStream::constructor_;

void NapiMediaStream::Init(Napi::Env env, Object exports)
{
    Function func = DefineClass(
        env, kClassName,
        {
            InstanceAccessor<&NapiMediaStream::GetId>(kAttributeNameId),
            InstanceAccessor<&NapiMediaStream::GetActive>(kAttributeNameActive),
            InstanceMethod<&NapiMediaStream::AddTrack>(kMethodNameAddTrack),
            InstanceMethod<&NapiMediaStream::RemoveTrack>(kMethodNameRemoveTrack),
            InstanceMethod<&NapiMediaStream::GetTrackById>(kMethodNameGetTrackById),
            InstanceMethod<&NapiMediaStream::GetTracks>(kMethodNameGetTracks),
            InstanceMethod<&NapiMediaStream::GetAudioTracks>(kMethodNameGetAudioTracks),
            InstanceMethod<&NapiMediaStream::GetVideoTracks>(kMethodNameGetVideoTracks),
            InstanceMethod<&NapiMediaStream::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
}

Object NapiMediaStream::NewInstance(Napi::Env env, rtc::scoped_refptr<MediaStreamInterface> stream)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!stream) {
        NAPI_THROW(Error::New(env, "Invalid argument"), Object::New(env));
    }

    auto external =
        External<MediaStreamInterface>::New(env, stream.release(), [](Napi::Env, MediaStreamInterface* stream) {
            auto status = stream->Release();
            RTC_DLOG(LS_VERBOSE) << "NapiMediaStream release status=" << status;
        });

    return constructor_.New({external});
}

NapiMediaStream::NapiMediaStream(const CallbackInfo& info) : ObjectWrap<NapiMediaStream>(info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() > 0 && info[0].IsExternal()) {
        auto stream = info[0].As<External<MediaStreamInterface>>().Data();
        stream_ = rtc::scoped_refptr<MediaStreamInterface>(stream);
    }

    AudioTrackVector audioTracks;
    VideoTrackVector videoTracks;
    if (info.Length() > 0 && info[0].IsObject()) {
        if (info[0].IsArray()) {
            auto jsTracks = info[0].As<Array>();
            for (uint32_t i = 0; i < jsTracks.Length(); i++) {
                Napi::Value jsTrackValue = jsTracks[i];
                auto napiTrack = NapiMediaStreamTrack::Unwrap(jsTrackValue.As<Object>());
                if (!napiTrack || !napiTrack->Get()) {
                    NAPI_THROW_VOID(Error::New(info.Env(), "Invalid argument"));
                }

                auto track = napiTrack->Get();
                if (track->kind() == MediaStreamTrackInterface::kAudioKind) {
                    auto audioTrack =
                        rtc::scoped_refptr<AudioTrackInterface>(static_cast<AudioTrackInterface*>(track.get()));
                    audioTracks.push_back(audioTrack);
                } else if (track->kind() == MediaStreamTrackInterface::kVideoKind) {
                    auto videoTrack =
                        rtc::scoped_refptr<VideoTrackInterface>(static_cast<VideoTrackInterface*>(track.get()));
                    videoTracks.push_back(videoTrack);
                } else {
                    RTC_LOG(LS_WARNING) << "Unknown type of media stream track: " << track->id();
                }
            }
        } else {
            auto napiStream = NapiMediaStream::Unwrap(info[0].As<Object>());
            if (!napiStream || !napiStream->Get()) {
                NAPI_THROW_VOID(Error::New(info.Env(), "Invalid argument"));
            }

            auto stream = napiStream->Get();
            audioTracks = stream->GetAudioTracks();
            videoTracks = stream->GetVideoTracks();
        }
    }

    if (!stream_) {
        auto pcFactory = PeerConnectionFactoryWrapper::GetDefault();
        if (!pcFactory || !pcFactory->GetFactory()) {
            NAPI_THROW_VOID(Error::New(info.Env(), "No default peer connection factory"));
        }

        stream_ = pcFactory->GetFactory()->CreateLocalMediaStream(rtc::CreateRandomUuid());
        if (!stream_) {
            NAPI_THROW_VOID(Error::New(info.Env(), "Failed to create media stream"));
        }
    }

    for (auto& track : audioTracks) {
        stream_->AddTrack(track);
    }

    for (auto& track : videoTracks) {
        stream_->AddTrack(track);
    }

    observer_.reset(new MediaStreamObserver(
        stream_.get(),
        [this](AudioTrackInterface* audio_track, MediaStreamInterface* media_stream) {
            OnAudioTrackAddedToStream(audio_track, media_stream);
        },
        [this](AudioTrackInterface* audio_track, MediaStreamInterface* media_stream) {
            OnAudioTrackRemovedFromStream(audio_track, media_stream);
        },
        [this](VideoTrackInterface* video_track, MediaStreamInterface* media_stream) {
            OnVideoTrackAddedToStream(video_track, media_stream);
        },
        [this](VideoTrackInterface* video_track, MediaStreamInterface* media_stream) {
            OnVideoTrackRemovedFromStream(video_track, media_stream);
        }));
}

NapiMediaStream::~NapiMediaStream()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
}

Napi::Value NapiMediaStream::GetId(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    return String::New(info.Env(), stream_->id());
}

Napi::Value NapiMediaStream::GetActive(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto active = false;
    for (const auto& track : stream_->GetAudioTracks()) {
        if (track->state() != MediaStreamTrackInterface::kEnded) {
            active = true;
        }
    }
    for (const auto& track : stream_->GetVideoTracks()) {
        if (track->state() != MediaStreamTrackInterface::kEnded) {
            active = true;
        }
    }

    return Boolean::New(info.Env(), active);
}

Napi::Value NapiMediaStream::AddTrack(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 1) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsObject()) {
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), info.Env().Undefined());
    }

    auto jsTrack = info[0].As<Object>();
    auto nativeTrack = NapiMediaStreamTrack::Unwrap(jsTrack);
    if (!nativeTrack) {
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), info.Env().Undefined());
    }

    auto track = nativeTrack->Get();
    if (!track) {
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), info.Env().Undefined());
    }

    bool success = false;
    if (track->kind() == MediaStreamTrackInterface::kAudioKind) {
        auto audioTrack = rtc::scoped_refptr<AudioTrackInterface>(static_cast<AudioTrackInterface*>(track.get()));
        success = stream_->AddTrack(audioTrack);
    } else if (track->kind() == MediaStreamTrackInterface::kVideoKind) {
        auto videoTrack = rtc::scoped_refptr<VideoTrackInterface>(static_cast<VideoTrackInterface*>(track.get()));
        success = stream_->AddTrack(videoTrack);
    } else {
        RTC_LOG(LS_WARNING) << "Unknown type of media stream track: " << track->id();
    }

    if (!success) {
        RTC_LOG(LS_ERROR) << "Failed to add track to media stream";
        NAPI_THROW(Error::New(info.Env(), "Unknown error"), info.Env().Undefined());
    }

    return info.Env().Undefined();
}

Napi::Value NapiMediaStream::RemoveTrack(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 1) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsObject()) {
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), info.Env().Undefined());
    }

    auto jsTrack = info[0].As<Object>();
    auto nativeTrack = NapiMediaStreamTrack::Unwrap(jsTrack);
    if (!nativeTrack) {
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), info.Env().Undefined());
    }

    auto track = nativeTrack->Get();
    if (!track) {
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), info.Env().Undefined());
    }

    bool success = false;
    if (track->kind() == MediaStreamTrackInterface::kAudioKind) {
        auto audioTrack = rtc::scoped_refptr<AudioTrackInterface>(static_cast<AudioTrackInterface*>(track.get()));
        success = stream_->RemoveTrack(audioTrack);
    } else if (track->kind() == MediaStreamTrackInterface::kVideoKind) {
        auto videoTrack = rtc::scoped_refptr<VideoTrackInterface>(static_cast<VideoTrackInterface*>(track.get()));
        success = stream_->RemoveTrack(videoTrack);
    } else {
        RTC_LOG(LS_WARNING) << "Unknown type of media stream track: " << track->id();
    }

    if (!success) {
        RTC_LOG(LS_ERROR) << "Failed to remove track from media stream";
        NAPI_THROW(Error::New(info.Env(), "Unknown error"), info.Env().Undefined());
    }

    return info.Env().Undefined();
}

Napi::Value NapiMediaStream::GetTrackById(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 1) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsString()) {
        NAPI_THROW(Error::New(info.Env(), "Invalid argument"), info.Env().Undefined());
    }

    auto trackId = info[0].As<String>().Utf8Value();
    auto audioTrack = stream_->FindAudioTrack(trackId);
    if (audioTrack) {
        return NapiMediaStreamTrack::NewInstance(info.Env(), MediaStreamTrackWrapper::Create(audioTrack));
    }

    auto videoTrack = stream_->FindVideoTrack(trackId);
    if (videoTrack) {
        return NapiMediaStreamTrack::NewInstance(info.Env(), MediaStreamTrackWrapper::Create(videoTrack));
    }

    RTC_LOG(LS_INFO) << "No track with id: " << trackId;

    return info.Env().Null();
}

Napi::Value NapiMediaStream::GetTracks(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto audioTracks = stream_->GetAudioTracks();
    auto videoTracks = stream_->GetVideoTracks();

    auto result = Array::New(info.Env(), audioTracks.size() + videoTracks.size());
    for (uint32_t i = 0; i < audioTracks.size(); i++) {
        result[i] = NapiMediaStreamTrack::NewInstance(info.Env(), MediaStreamTrackWrapper::Create(audioTracks[i]));
    }
    for (uint32_t i = 0; i < videoTracks.size(); i++) {
        result[audioTracks.size() + i] =
            NapiMediaStreamTrack::NewInstance(info.Env(), MediaStreamTrackWrapper::Create(videoTracks[i]));
    }

    return result;
}

Napi::Value NapiMediaStream::GetAudioTracks(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto audioTracks = stream_->GetAudioTracks();

    auto result = Array::New(info.Env(), audioTracks.size());
    for (uint32_t i = 0; i < audioTracks.size(); i++) {
        result[i] = NapiMediaStreamTrack::NewInstance(info.Env(), MediaStreamTrackWrapper::Create(audioTracks[i]));
    }

    return result;
}

Napi::Value NapiMediaStream::GetVideoTracks(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto videoTracks = stream_->GetVideoTracks();

    auto result = Array::New(info.Env(), videoTracks.size());
    for (uint32_t i = 0; i < videoTracks.size(); i++) {
        result[i] = NapiMediaStreamTrack::NewInstance(info.Env(), MediaStreamTrackWrapper::Create(videoTracks[i]));
    }

    return result;
}

Value NapiMediaStream::ToJson(const CallbackInfo& info)
{
    auto json = Object::New(info.Env());
    json.Set(kAttributeNameId, GetId(info));
    json.Set(kAttributeNameActive, GetActive(info));
#ifndef NDEBUG
    json.Set("__native_class__", String::New(info.Env(), "NapiMediaStream"));
#endif

    return json;
}

void NapiMediaStream::OnAudioTrackAddedToStream(AudioTrackInterface* track, MediaStreamInterface* stream)
{
    (void)stream;
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " track: " << track->id();
}

void NapiMediaStream::OnVideoTrackAddedToStream(VideoTrackInterface* track, MediaStreamInterface* stream)
{
    (void)stream;
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " track: " << track->id();
}

void NapiMediaStream::OnAudioTrackRemovedFromStream(AudioTrackInterface* track, MediaStreamInterface* stream)
{
    (void)stream;
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " track: " << track->id();
}

void NapiMediaStream::OnVideoTrackRemovedFromStream(VideoTrackInterface* track, MediaStreamInterface* stream)
{
    (void)stream;
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " track: " << track->id();
}

} // namespace webrtc
