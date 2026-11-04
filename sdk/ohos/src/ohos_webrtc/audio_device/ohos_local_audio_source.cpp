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

#include "ohos_local_audio_source.h"

#include "ohos_audio_device_module.h"

#include "api/audio/audio_frame.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

rtc::scoped_refptr<OhosLocalAudioSource> OhosLocalAudioSource::Create(
    cricket::AudioOptions audioOptions, std::shared_ptr<AudioInput> audioInput,
    rtc::Thread* signalingThread, rtc::scoped_refptr<OhosAudioDeviceModule> adm)
{
    RTC_DCHECK(audioInput);
    RTC_DCHECK(signalingThread);
    RTC_DCHECK(adm);
    if (!audioInput || !signalingThread || !adm) {
        RTC_LOG(LS_WARNING) << "Invalid parameter";
        return nullptr;
    }

    auto source = rtc::make_ref_counted<OhosLocalAudioSource>();
    source->Initialize(std::move(audioOptions), std::move(audioInput), signalingThread, std::move(adm));

    return source;
}

void OhosLocalAudioSource::Initialize(
    cricket::AudioOptions audioOptions, std::shared_ptr<AudioInput> audioInput,
    rtc::Thread* signalingThread, rtc::scoped_refptr<OhosAudioDeviceModule> adm)
{
    audioOptions_ = std::move(audioOptions);
    audioInput_ = std::move(audioInput);
    signalingThread_ = signalingThread;
    adm_ = std::move(adm);
    if (audioInput_) {
        audioInput_->RegisterObserver(this);
    }
}

OhosLocalAudioSource::~OhosLocalAudioSource()
{
    if (audioInput_) {
        audioInput_->UnregisterObserver(this);
    }
    // Source destroyed without track.stop(): still release ADM live-source refcount.
    if (state_.load() == kLive) {
        ReleaseAdmLiveSourceIfNeeded();
    }
}

std::string OhosLocalAudioSource::GetLabel() const
{
    return audioInput_->GetLabel();
}

void OhosLocalAudioSource::SetMute(bool mute)
{
    audioInput_->SetMute(mute);
}

void OhosLocalAudioSource::SetVolume(double volume)
{
    audioInput_->SetVolume(volume);
}

void OhosLocalAudioSource::ApplyConstraints(const cricket::AudioOptions& options)
{
    RTC_LOG(LS_INFO) << __FUNCTION__ << " request: echo_cancellation="
                     << (options.echo_cancellation.has_value()
                                ? (options.echo_cancellation.value() ? "true" : "false")
                                : "unset")
                     << ", auto_gain_control="
                     << (options.auto_gain_control.has_value()
                                ? (options.auto_gain_control.value() ? "true" : "false")
                                : "unset")
                     << ", noise_suppression="
                     << (options.noise_suppression.has_value()
                                ? (options.noise_suppression.value() ? "true" : "false")
                                : "unset");

    if (options.echo_cancellation.has_value()) {
        audioOptions_.echo_cancellation = options.echo_cancellation;
    }
    if (options.auto_gain_control.has_value()) {
        audioOptions_.auto_gain_control = options.auto_gain_control;
    }
    if (options.noise_suppression.has_value()) {
        audioOptions_.noise_suppression = options.noise_suppression;
    }

    // Distinguish "requested" from "effectively configured": unset options keep the
    // engine default; explicit values (including false) are passed through verbatim.
    RTC_LOG(LS_INFO) << __FUNCTION__ << " effective: echo_cancellation="
                     << (audioOptions_.echo_cancellation.has_value()
                                ? (audioOptions_.echo_cancellation.value() ? "true" : "false")
                                : "unset")
                     << ", auto_gain_control="
                     << (audioOptions_.auto_gain_control.has_value()
                                ? (audioOptions_.auto_gain_control.value() ? "true" : "false")
                                : "unset")
                     << ", noise_suppression="
                     << (audioOptions_.noise_suppression.has_value()
                                ? (audioOptions_.noise_suppression.value() ? "true" : "false")
                                : "unset");
}

void OhosLocalAudioSource::ReleaseAdmLiveSourceIfNeeded()
{
    if (releasedFromAdm_.exchange(true)) {
        return;
    }
    if (adm_) {
        // May be called from signaling thread; ADM posts StopRecording to worker internally.
        adm_->ReleaseLiveAudioSource();
    }
}

void OhosLocalAudioSource::Stop()
{
    if (state_.exchange(MediaSourceInterface::kEnded) == MediaSourceInterface::kEnded) {
        return;
    }

    // Observers (AudioTrack / AudioRtpSender) require signaling thread; never call FireOnChanged
    // from NAPI/worker. Run the whole stop sequence on signalingThread_ via PostTask if needed.
    auto doStop = [self = rtc::scoped_refptr<OhosLocalAudioSource>(this)]() {
        // 1) Notify track ended and stop RTP (ClearSend).
        self->FireOnChanged();
        // 2) Decrement ADM live-source count; last source triggers StopRecording on worker thread.
        self->ReleaseAdmLiveSourceIfNeeded();
    };

    if (rtc::Thread::Current() == signalingThread_) {
        doStop();
    } else {
        signalingThread_->PostTask(std::move(doStop));
    }
}

void OhosLocalAudioSource::OnAudioInputError(AudioInput* input, AudioErrorType type, const std::string& message)
{
    (void)input;
    RTC_LOG(LS_ERROR) << __FUNCTION__ << " type=" << type << ", message=" << message;
}

void OhosLocalAudioSource::OnAudioInputStateChange(AudioInput* input, AudioStateType newState)
{
    (void)input;
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " newState=" << newState;
}

void OhosLocalAudioSource::OnAudioInputDataReady(
    AudioInput* input, void* buffer, int32_t length, int64_t timestampUs, int64_t deleyUs)
{
    (void)input;
    (void)buffer;
    (void)length;
    (void)timestampUs;
    (void)deleyUs;
}

void OhosLocalAudioSource::OnAudioInputInterrupted(AudioInput* input, bool interrupted)
{
    (void)input;
    RTC_LOG(LS_INFO) << __FUNCTION__ << " interrupted=" << interrupted;

    // Arrives on the audio callback thread: migrate state and notify on the signaling
    // thread, mirroring Stop(). Keep a ref so the source outlives the posted task.
    if (rtc::Thread::Current() == signalingThread_) {
        ApplyInterruptedState(interrupted);
    } else {
        signalingThread_->PostTask(
            [self = rtc::scoped_refptr<OhosLocalAudioSource>(this), interrupted] {
                self->ApplyInterruptedState(interrupted);
            });
    }
}

void OhosLocalAudioSource::ApplyInterruptedState(bool interrupted)
{
    // Terminal state wins: a late interrupt/resume must not revive or mutate an ended source.
    if (state_.load() == MediaSourceInterface::kEnded) {
        return;
    }

    auto newState = interrupted ? MediaSourceInterface::kMuted : MediaSourceInterface::kLive;
    if (state_.exchange(newState) == newState) {
        return;
    }

    RTC_LOG(LS_INFO) << __FUNCTION__ << " state -> " << (interrupted ? "muted" : "live");
    FireOnChanged();
}

} // namespace webrtc
