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

#include "peer_connection_factory.h"

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/scoped_refptr.h"
#include "rtc_base/physical_socket_server.h"

#include "configuration.h"
#include "media_track_constraints.h"
#include "media_source.h"
#include "media_stream_track.h"
#include "peer_connection.h"
#include "audio_device/audio_device_module.h"
#include "camera/camera_enumerator.h"
#include "camera/camera_capturer.h"
#include "desktop_capture/desktop_capturer.h"
#include "video/video_track_source.h"
#include "user_media/media_constraints_util.h"
#include "video_encoder_factory.h"
#include "video_decoder_factory.h"
#include "audio_processing_factory.h"
#include "utils/marcos.h"

namespace webrtc {

using namespace Napi;

const char kClassName[] = "PeerConnectionFactory";

const char kMethodNameSetDefault[] = "setDefault";
const char kMethodNameCreatePeerConnection[] = "createPeerConnection";
const char kMethodNameCreateAudioSource[] = "createAudioSource";
const char kMethodNameCreateAudioTrack[] = "createAudioTrack";
const char kMethodNameCreateVideoSource[] = "createVideoSource";
const char kMethodNameCreateVideoTrack[] = "createVideoTrack";
const char kMethodNameStartAecDump[] = "startAecDump";
const char kMethodNameStopAecDump[] = "stopAecDump";
const char kMethodNameToJson[] = "toJSON";

class NapiPeerConnectionFactoryOptions {
public:
    NAPI_ATTRIBUTE_NAME_DECLARE(Adm, adm);
    NAPI_ATTRIBUTE_NAME_DECLARE(VideoEncoderFactory, videoEncoderFactory);
    NAPI_ATTRIBUTE_NAME_DECLARE(VideoDecoderFactory, videoDecoderFactory);
    NAPI_ATTRIBUTE_NAME_DECLARE(AudioProcessing, audioProcessing);
};

std::mutex PeerConnectionFactoryWrapper::mutex_;
std::shared_ptr<PeerConnectionFactoryWrapper> PeerConnectionFactoryWrapper::defaultFactory_;

std::shared_ptr<PeerConnectionFactoryWrapper> PeerConnectionFactoryWrapper::GetDefault()
{
    UNUSED std::lock_guard<std::mutex> lock(mutex_);
    if (!defaultFactory_) {
        defaultFactory_ = std::shared_ptr<PeerConnectionFactoryWrapper>(new PeerConnectionFactoryWrapper());
        if (!defaultFactory_->Init(nullptr, nullptr, nullptr, nullptr)) {
            defaultFactory_.reset();
        }
    }

    return defaultFactory_;
}

void PeerConnectionFactoryWrapper::SetDefault(std::shared_ptr<PeerConnectionFactoryWrapper> wrapper)
{
    UNUSED std::lock_guard<std::mutex> lock(mutex_);
    defaultFactory_ = wrapper;
}

std::shared_ptr<PeerConnectionFactoryWrapper> PeerConnectionFactoryWrapper::Create(
    rtc::scoped_refptr<AudioDeviceModule> adm, std::unique_ptr<VideoEncoderFactory> videoEncoderFactory,
    std::unique_ptr<VideoDecoderFactory> videoDecoderFactory, rtc::scoped_refptr<AudioProcessing> audioProcessing)
{
    auto wrapper = std::shared_ptr<PeerConnectionFactoryWrapper>(new PeerConnectionFactoryWrapper());
    if (!wrapper->Init(adm, std::move(videoEncoderFactory), std::move(videoDecoderFactory), audioProcessing)) {
        return nullptr;
    }
    return wrapper;
}

bool PeerConnectionFactoryWrapper::Init(
    rtc::scoped_refptr<AudioDeviceModule> adm, std::unique_ptr<VideoEncoderFactory> videoEncoderFactory,
    std::unique_ptr<VideoDecoderFactory> videoDecoderFactory, rtc::scoped_refptr<AudioProcessing> audioProcessing)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    rtc::ThreadManager::Instance()->WrapCurrentThread();

    auto socketServer = std::make_unique<rtc::PhysicalSocketServer>();
    auto networkThread = std::make_unique<rtc::Thread>(socketServer.get());
    networkThread->SetName("network_thread", nullptr);
    if (!networkThread->Start()) {
        RTC_LOG(LS_ERROR) << "Failed to start network thread";
        return false;
    }

    auto workerThread = rtc::Thread::Create();
    workerThread->SetName("worker_thread", nullptr);
    if (!workerThread->Start()) {
        RTC_LOG(LS_ERROR) << "Failed to start worker thread";
        return false;
    }

    auto signalingThread = rtc::Thread::Create();
    signalingThread->SetName("signaling_thread", NULL);
    if (!signalingThread->Start()) {
        RTC_LOG(LS_ERROR) << "Failed to start signaling thread";
        return false;
    }

    pcFactory_ = CreatePeerConnectionFactory(
        networkThread.get(), workerThread.get(), signalingThread.get(), adm, CreateBuiltinAudioEncoderFactory(),
        CreateBuiltinAudioDecoderFactory(),
        videoEncoderFactory ? std::move(videoEncoderFactory) : CreateDefaultVideoEncoderFactory(),
        videoDecoderFactory ? std::move(videoDecoderFactory) : CreateDefaultVideoDecoderFactory(),
        nullptr /* audio_mixer */, audioProcessing);

    if (!pcFactory_) {
        RTC_LOG(LS_ERROR) << "Failed to create PeerConnectionFactory";
        return false;
    }

    socketServer_ = std::move(socketServer);
    networkThread_ = std::move(networkThread);
    workerThread_ = std::move(workerThread);
    signalingThread_ = std::move(signalingThread);

    return true;
}

FunctionReference NapiPeerConnectionFactory::constructor_;

NapiPeerConnectionFactory::NapiPeerConnectionFactory(const CallbackInfo& info)
    : ObjectWrap<NapiPeerConnectionFactory>(info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    rtc::scoped_refptr<AudioDeviceModule> adm;
    std::unique_ptr<VideoEncoderFactory> videoEncoderFactory;
    std::unique_ptr<VideoDecoderFactory> videoDecoderFactory;
    rtc::scoped_refptr<AudioProcessing> audioProcessing;
    if (info.Length() > 0 && info[0].IsObject()) {
        auto jsOptions = info[0].As<Object>();
        if (jsOptions.Has(NapiPeerConnectionFactoryOptions::kAttributeNameAdm)) {
            auto jsAdm = jsOptions.Get(NapiPeerConnectionFactoryOptions::kAttributeNameAdm).As<Object>();
            auto napiAdm = NapiAudioDeviceModule::Unwrap(jsAdm);
            adm = napiAdm->Get();
        }
        if (jsOptions.Has(NapiPeerConnectionFactoryOptions::kAttributeNameVideoEncoderFactory)) {
            auto jsVideoEncoderFactory =
                jsOptions.Get(NapiPeerConnectionFactoryOptions::kAttributeNameVideoEncoderFactory).As<Object>();
            videoEncoderFactory = CreateVideoEncoderFactory(jsVideoEncoderFactory);
        }
        if (jsOptions.Has(NapiPeerConnectionFactoryOptions::kAttributeNameVideoDecoderFactory)) {
            auto jsVideoDecoderFactory =
                jsOptions.Get(NapiPeerConnectionFactoryOptions::kAttributeNameVideoDecoderFactory).As<Object>();
            videoDecoderFactory = CreateVideoDecoderFactory(jsVideoDecoderFactory);
        }
        if (jsOptions.Has(NapiPeerConnectionFactoryOptions::kAttributeNameAudioProcessing)) {
            auto jsAudioProcessing =
                jsOptions.Get(NapiPeerConnectionFactoryOptions::kAttributeNameAudioProcessing).As<Object>();
            auto napiAudioProcessing = NapiAudioProcessing::Unwrap(jsAudioProcessing);
            audioProcessing = napiAudioProcessing->Get();
        }
    }

    wrapper_ = PeerConnectionFactoryWrapper::Create(
        adm, std::move(videoEncoderFactory), std::move(videoDecoderFactory), audioProcessing);
};

void NapiPeerConnectionFactory::Init(Napi::Env env, Object exports)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    Function func = DefineClass(
        env, kClassName,
        {
            StaticMethod<&NapiPeerConnectionFactory::SetDefault>(kMethodNameSetDefault),
            InstanceMethod<&NapiPeerConnectionFactory::CreatePeerConnection>(kMethodNameCreatePeerConnection),
            InstanceMethod<&NapiPeerConnectionFactory::CreateAudioSource>(kMethodNameCreateAudioSource),
            InstanceMethod<&NapiPeerConnectionFactory::CreateAudioTrack>(kMethodNameCreateAudioTrack),
            InstanceMethod<&NapiPeerConnectionFactory::CreateVideoSource>(kMethodNameCreateVideoSource),
            InstanceMethod<&NapiPeerConnectionFactory::CreateVideoTrack>(kMethodNameCreateVideoTrack),
            InstanceMethod<&NapiPeerConnectionFactory::StartAecDump>(kMethodNameStartAecDump),
            InstanceMethod<&NapiPeerConnectionFactory::StopAecDump>(kMethodNameStopAecDump),
            InstanceMethod<&NapiPeerConnectionFactory::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
}

Napi::Value NapiPeerConnectionFactory::SetDefault(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() == 0) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsObject()) {
        NAPI_THROW(TypeError::New(info.Env(), "First argument is not Object"), info.Env().Undefined());
    }

    auto nativeFactory = NapiPeerConnectionFactory::Unwrap(info[0].As<Object>());
    if (!nativeFactory) {
        NAPI_THROW(TypeError::New(info.Env(), "Invalid argument"), info.Env().Undefined());
    }

    PeerConnectionFactoryWrapper::SetDefault(nativeFactory->GetWrapper());

    return info.Env().Undefined();
}

Napi::Value NapiPeerConnectionFactory::CreatePeerConnection(const CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() == 0) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsObject()) {
        NAPI_THROW(TypeError::New(info.Env(), "First argument is not Object"), info.Env().Undefined());
    }

    PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    JsToNativeConfiguration(info[0].As<Object>(), config);

    auto observer = NapiPeerConnectionWrapper::Create(wrapper_, config);
    return NapiPeerConnection::NewInstance(info.Env(), std::move(observer));
}

Napi::Value NapiPeerConnectionFactory::CreateAudioSource(const CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    cricket::AudioOptions options;
    if (info.Length() > 0) {
        if (!info[0].IsObject()) {
            NAPI_THROW(TypeError::New(info.Env(), "The first argument must be an object"), info.Env().Undefined());
        }

        MediaTrackConstraints audioConstraints;
        NapiMediaConstraints::JsToNative(info[0], audioConstraints);
        if (info.Env().IsExceptionPending()) {
            NAPI_THROW(info.Env().GetAndClearPendingException(), info.Env().Undefined());
        }
        CopyConstraintsIntoAudioOptions(audioConstraints, options);
    }

    if (options.echo_cancellation) {
        RTC_DLOG(LS_VERBOSE) << "echo_cancellation = " << options.echo_cancellation.value();
    }
    auto source = GetFactory()->CreateAudioSource(options);
    if (!source) {
        NAPI_THROW(Error::New(info.Env(), "Failed to create audio source"), info.Env().Undefined());
    }

    return NapiAudioSource::NewInstance(info.Env(), source);
}

Napi::Value NapiPeerConnectionFactory::CreateAudioTrack(const CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 2) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsString()) {
        NAPI_THROW(TypeError::New(info.Env(), "First argument is not String"), info.Env().Undefined());
    }

    if (!info[1].IsObject()) {
        NAPI_THROW(TypeError::New(info.Env(), "Second argument is not Object"), info.Env().Undefined());
    }

    auto id = info[0].As<String>().Utf8Value();
    RTC_DLOG(LS_VERBOSE) << "id=" << id;

    auto source = NapiAudioSource::Unwrap(info[1].As<Object>());
    if (!source) {
        NAPI_THROW(TypeError::New(info.Env(), "Second argument is not AudioSource"), info.Env().Undefined());
    }

    auto track = GetFactory()->CreateAudioTrack(id, source->Get());
    if (!track) {
        NAPI_THROW(TypeError::New(info.Env(), "Failed to create audio track"), info.Env().Undefined());
    }

    return NapiMediaStreamTrack::NewInstance(info.Env(), MediaStreamTrackWrapper::Create(track));
}

Napi::Value NapiPeerConnectionFactory::CreateVideoSource(const CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    MediaTrackConstraints video;
    bool isScreencast = false;
    if (info.Length() > 0) {
        if (!info[0].IsObject()) {
            NAPI_THROW(TypeError::New(info.Env(), "The first argument must be an object"), info.Env().Undefined());
        }

        NapiMediaConstraints::JsToNative(info[0], video);
        if (info.Env().IsExceptionPending()) {
            NAPI_THROW(info.Env().GetAndClearPendingException(), info.Env().Undefined());
        }
    } else {
        video.Initialize();
    }

    if (info.Length() > 1) {
        if (!info[1].IsBoolean()) {
            NAPI_THROW(TypeError::New(info.Env(), "The first argument must be boolean"), info.Env().Undefined());
        }

        isScreencast = info[1].As<Boolean>().Value();
    }

    std::unique_ptr<VideoCapturer> videoCapturer;
    if (isScreencast) {
        // default size
        int width = 720;
        int height = 1080;
        GetScreenCaptureConstraints(video, width, height);

        video::VideoProfile profile;
        profile.resolution.width = width;
        profile.resolution.height = height;
        profile.format = video::PixelFormat::RGBA;
        videoCapturer = DesktopCapturer::Create(profile);
        if (!videoCapturer) {
            NAPI_THROW(Error::New(info.Env(), "Failed to create desktop capturer"), info.Env().Undefined());
        }
    } else {
        CameraCaptureSettings selectedSetting;
        std::string failedConstraintName;
        if (SelectSettingsForVideo(
                CameraEnumerator::GetDevices(), video, 640, 480, 30, selectedSetting, failedConstraintName))
        {
            RTC_DLOG(LS_VERBOSE) << "Selected camera device: " << selectedSetting.deviceId
                                 << ", resolution = " << selectedSetting.profile.resolution.width << "x"
                                 << selectedSetting.profile.resolution.height
                                 << ", format = " << selectedSetting.profile.format
                                 << ", framerate = " << selectedSetting.profile.frameRateRange.min << "-"
                                 << selectedSetting.profile.frameRateRange.max;
        } else {
            RTC_LOG(LS_ERROR) << "Failed to select settings for video, unsatisfied constraint: "
                              << failedConstraintName;
            NAPI_THROW(
                Error::New(info.Env(), std::string("Unsatisfied constraint: ") + failedConstraintName),
                info.Env().Undefined());
        }

        videoCapturer = CameraCapturer::Create(selectedSetting.deviceId, selectedSetting.profile);
        if (!videoCapturer) {
            NAPI_THROW(Error::New(info.Env(), "Failed to create camera capturer"), info.Env().Undefined());
        }
    }

    auto source = OhosVideoTrackSource::Create(
        std::move(videoCapturer), wrapper_->GetSignalingThread(), EglEnv::GetDefault().GetContext());
    if (!source) {
        NAPI_THROW(Error::New(info.Env(), "Failed to create video source"), info.Env().Undefined());
    }

    return NapiVideoSource::NewInstance(info.Env(), source);
}

Napi::Value NapiPeerConnectionFactory::CreateVideoTrack(const CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() < 2) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (!info[0].IsString()) {
        NAPI_THROW(TypeError::New(info.Env(), "First argument is not String"), info.Env().Undefined());
    }

    if (!info[1].IsObject()) {
        NAPI_THROW(TypeError::New(info.Env(), "Second argument is not Object"), info.Env().Undefined());
    }

    auto id = info[0].As<String>().Utf8Value();
    RTC_DLOG(LS_VERBOSE) << "id=" << id;

    auto source = NapiVideoSource::Unwrap(info[1].As<Object>());
    if (!source) {
        NAPI_THROW(TypeError::New(info.Env(), "Second argument is not VideoSource"), info.Env().Undefined());
    }

    auto track = GetFactory()->CreateVideoTrack(rtc::scoped_refptr<VideoTrackSourceInterface>(source->Get()), id);
    if (!track) {
        NAPI_THROW(TypeError::New(info.Env(), "Failed to create video track"), info.Env().Undefined());
    }

    return NapiMediaStreamTrack::NewInstance(info.Env(), MediaStreamTrackWrapper::Create(track));
}

// startAecDump(fd : number, max_size_bytes : number) : boolean;
Napi::Value NapiPeerConnectionFactory::StartAecDump(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto fd = info[0].As<Number>().Int32Value();
    auto max_size_bytes = info[1].As<Number>().Int32Value();

    FILE* file = fdopen(fd, "wb");
    if (!file) {
        close(fd);
        return Boolean::New(info.Env(), false);
    }

    return Boolean::New(info.Env(), GetFactory()->StartAecDump(file, max_size_bytes));
}

// stopAecDump(): void;
Napi::Value NapiPeerConnectionFactory::StopAecDump(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    GetFactory()->StopAecDump();

    return info.Env().Undefined();
}

Napi::Value NapiPeerConnectionFactory::ToJson(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", String::New(info.Env(), "NapiPeerConnectionFactory"));
#endif

    return json;
}

} // namespace webrtc
