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

#include "async_worker_get_display_media.h"
#include "../peer_connection_factory.h"
#include "../media_stream.h"
#include "../media_stream_track.h"
#include "../desktop_capture/desktop_capturer.h"
#include "../video/video_track_source.h"
#include "../user_media/media_constraints_util.h"
#include "../render/egl_env.h"

#include "rtc_base/logging.h"
#include "rtc_base/helpers.h"

namespace webrtc {

AsyncWorkerGetDisplayMedia* AsyncWorkerGetDisplayMedia::Create(Napi::Env env, const char* resourceName)
{
    auto asyncWorker = new AsyncWorkerGetDisplayMedia(env, resourceName);
    return asyncWorker;
}

AsyncWorkerGetDisplayMedia::AsyncWorkerGetDisplayMedia(Napi::Env env, const char* resourceName)
    : AsyncWorker(env, resourceName), deferred_(Napi::Promise::Deferred::New(env))
{
}

void AsyncWorkerGetDisplayMedia::Start(MediaTrackConstraints audio, MediaTrackConstraints video)
{
    audioConstraints_ = std::move(audio);
    videoConstraints_ = std::move(video);
    Queue();
}

void AsyncWorkerGetDisplayMedia::Execute()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto factoryWrapper = PeerConnectionFactoryWrapper::GetDefault();
    if (!factoryWrapper) {
        SetError("Internal error");
        return;
    }
    auto factory = factoryWrapper->GetFactory();
    if (!factory) {
        SetError("Internal error");
        return;
    }

    stream_ = factory->CreateLocalMediaStream(rtc::CreateRandomUuid());
    if (!stream_) {
        SetError("Failed to create media stream");
        return;
    }

    if (!audioConstraints_.IsNull()) {
        cricket::AudioOptions options;
        CopyConstraintsIntoAudioOptions(audioConstraints_, options);

        auto audioSource = factoryWrapper->GetFactory()->CreateAudioSource(options);
        if (!audioSource) {
            SetError("Failed to create audio source");
            return;
        }
        auto audioTrack = factoryWrapper->GetFactory()->CreateAudioTrack(rtc::CreateRandomUuid(), audioSource.get());
        if (!audioTrack) {
            SetError("Failed to create audio track");
            return;
        }

        stream_->AddTrack(audioTrack);
    }

    if (!videoConstraints_.IsNull()) {
        // default size
        int width = 720;
        int height = 1080;
        GetScreenCaptureConstraints(videoConstraints_, width, height);

        video::VideoProfile profile;
        profile.resolution.width = width;
        profile.resolution.height = height;
        profile.format = video::PixelFormat::RGBA;
        auto capturer = DesktopCapturer::Create(profile);
        if (!capturer) {
            SetError("Failed to create camera capturer");
            return;
        }

        auto videoSource = OhosVideoTrackSource::Create(
            std::move(capturer), factoryWrapper->GetSignalingThread(), EglEnv::GetDefault().GetContext());
        if (!videoSource) {
            SetError("Failed to create video source");
            return;
        }

        auto videoTrack = factoryWrapper->GetFactory()->CreateVideoTrack(videoSource, rtc::CreateRandomUuid());
        if (!videoTrack) {
            SetError("Failed to create video track");
            return;
        }

        stream_->AddTrack(videoTrack);
    }
}

void AsyncWorkerGetDisplayMedia::OnOK()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto mediaStream = NapiMediaStream::NewInstance(Env(), stream_);
    if (Env().IsExceptionPending()) {
        deferred_.Reject(Env().GetAndClearPendingException().Value());
        return;
    }

    deferred_.Resolve(mediaStream);
}

void AsyncWorkerGetDisplayMedia::OnError(const Napi::Error& e)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    deferred_.Reject(e.Value());
}

} // namespace webrtc
