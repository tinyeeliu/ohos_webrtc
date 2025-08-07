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

#ifndef WEBRTC_AUDIO_SOURCE_H
#define WEBRTC_AUDIO_SOURCE_H

#include "napi.h"

#include "api/media_stream_interface.h"

#include "video/video_track_source.h"
#include "audio_device/ohos_local_audio_source.h"
#include "event/event_target.h"
#include "utils/marcos.h"

namespace webrtc {

class OhosLocalAudioSource;

class NapiMediaSource {
public:
    NAPI_ATTRIBUTE_NAME_DECLARE(State, state);
    NAPI_METHOD_NAME_DECLARE(Release, release);
    NAPI_METHOD_NAME_DECLARE(ToJson, toJSON);
    NAPI_ENUM_NAME_DECLARE(SourceStateInitializing, initializing);
    NAPI_ENUM_NAME_DECLARE(SourceStateLive, live);
    NAPI_ENUM_NAME_DECLARE(SourceStateEnded, ended);
    NAPI_ENUM_NAME_DECLARE(SourceStateMuted, muted);

    virtual ~NapiMediaSource() = 0;
};

class NapiAudioSource : public Napi::ObjectWrap<NapiAudioSource>, public NapiMediaSource {
public:
    NAPI_CLASS_NAME_DECLARE(AudioSource);
    NAPI_METHOD_NAME_DECLARE(SetVolume, setVolume);

    static void Init(Napi::Env env, Napi::Object exports);

    static Napi::Object NewInstance(Napi::Env env, rtc::scoped_refptr<OhosLocalAudioSource> source);

    ~NapiAudioSource() override;

    rtc::scoped_refptr<OhosLocalAudioSource> Get() const;

protected:
    friend class ObjectWrap;

    static Napi::FunctionReference& Constructor();
    explicit NapiAudioSource(const Napi::CallbackInfo& info);

    Napi::Value GetState(const Napi::CallbackInfo& info);
    Napi::Value Release(const Napi::CallbackInfo& info);
    Napi::Value SetVolume(const Napi::CallbackInfo& info);
    Napi::Value ToJson(const Napi::CallbackInfo& info);

private:
    rtc::scoped_refptr<OhosLocalAudioSource> source_;
};

class NapiVideoSource : public NapiEventTarget<NapiVideoSource>, public NapiMediaSource, public VideoCapturer::Observer {
public:
    NAPI_CLASS_NAME_DECLARE(VideoSource);
    NAPI_ATTRIBUTE_NAME_DECLARE(OnCapturerStarted, oncapturerstarted);
    NAPI_ATTRIBUTE_NAME_DECLARE(OnCapturerStopped, oncapturerstopped);
    NAPI_ATTRIBUTE_NAME_DECLARE(OnError, onerror);
    NAPI_EVENT_NAME_DECLARE(CapturerStarted, capturerstarted);
    NAPI_EVENT_NAME_DECLARE(CapturerStopped, capturerstopped);
    NAPI_EVENT_NAME_DECLARE(Error, error);
    NAPI_METHOD_NAME_DECLARE(StartCapture, startCapture);
    NAPI_METHOD_NAME_DECLARE(StopCapture, stopCapture);

    static void Init(Napi::Env env, Napi::Object exports);

    static Napi::Object NewInstance(Napi::Env env, rtc::scoped_refptr<OhosVideoTrackSource> source);

    ~NapiVideoSource() override;

    rtc::scoped_refptr<OhosVideoTrackSource> Get() const
    {
        return source_;
    }

protected:
    friend class ObjectWrap;

    static Napi::FunctionReference& Constructor();
    explicit NapiVideoSource(const Napi::CallbackInfo& info);

    Napi::Value GetState(const Napi::CallbackInfo& info);
    Napi::Value Release(const Napi::CallbackInfo& info);
    Napi::Value GetEventHandler(const Napi::CallbackInfo& info);
    void SetEventHandler(const Napi::CallbackInfo& info, const Napi::Value& value);
    Napi::Value StartCapture(const Napi::CallbackInfo& info);
    Napi::Value StopCapture(const Napi::CallbackInfo& info);
    Napi::Value ToJson(const Napi::CallbackInfo& info);

protected:
    // VideoCapturer::Observer
    void OnCapturerStarted() override;
    void OnCapturerStopped() override;
    void OnCapturerError(int32_t code, const std::string& message) override;

private:
    rtc::scoped_refptr<OhosVideoTrackSource> source_;
    rtc::Thread* signalThread_{};

    struct EventHandler {
        Napi::FunctionReference ref;
        Napi::ThreadSafeFunction tsfn;
    };

    mutable std::mutex eventMutex_;
    std::map<std::string, EventHandler> eventHandlers_;
};

} // namespace webrtc

#endif // WEBRTC_AUDIO_SOURCE_H
