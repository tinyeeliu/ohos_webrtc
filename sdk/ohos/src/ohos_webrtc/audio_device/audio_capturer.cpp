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

#include "audio_capturer.h"
#include "audio_common.h"

#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"
#include <bits/alltypes.h>
#include <cmath>

namespace webrtc {

using namespace Napi;

std::unique_ptr<AudioCapturer>
AudioCapturer::Create(int source, int format, int sampleRate, bool useStereoInput, bool useLowLatency)
{
    return std::unique_ptr<AudioCapturer>(new AudioCapturer(source, format, sampleRate, useStereoInput, useLowLatency));
}

int32_t AudioCapturer::OnReadData1(OH_AudioCapturer* stream, void* userData, void* buffer, int32_t length)
{
    AudioCapturer* self = reinterpret_cast<AudioCapturer*>(userData);
    return self->OnReadData(stream, buffer, length);
}

int32_t AudioCapturer::OnStreamEvent1(OH_AudioCapturer* stream, void* userData, OH_AudioStream_Event event)
{
    AudioCapturer* self = reinterpret_cast<AudioCapturer*>(userData);
    return self->OnStreamEvent(stream, event);
}

int32_t AudioCapturer::OnInterruptEvent1(
    OH_AudioCapturer* stream, void* userData, OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint)
{
    AudioCapturer* self = reinterpret_cast<AudioCapturer*>(userData);
    return self->OnInterruptEvent(stream, type, hint);
}

int32_t AudioCapturer::OnError1(OH_AudioCapturer* stream, void* userData, OH_AudioStream_Result error)
{
    AudioCapturer* self = reinterpret_cast<AudioCapturer*>(userData);
    return self->OnError(stream, error);
}

AudioCapturer::AudioCapturer(int source, int format, int sampleRate, bool useStereoInput, bool useLowLatency)
    : audioSource_(source), audioFormat_(format), sampleRate_(sampleRate), channelCount_(useStereoInput ? 2 : 1),
      useLowLatency_(useLowLatency)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    // Detach from this thread since construction is allowed to happen on a different thread.
    threadChecker_.Detach();
}

AudioCapturer::~AudioCapturer()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RTC_DCHECK(threadChecker_.IsCurrent());
    Terminate();
}

int32_t AudioCapturer::Init()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RTC_DCHECK(threadChecker_.IsCurrent());
    return 0;
}

int32_t AudioCapturer::Terminate()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RTC_DCHECK(threadChecker_.IsCurrent());
    StopRecording();
    threadChecker_.Detach();

    return 0;
}

int32_t AudioCapturer::InitRecording()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RTC_DCHECK(threadChecker_.IsCurrent());
    if (initialized_) {
        // Already initialized.
        return 0;
    }
    RTC_DCHECK(!recording_);

    OH_AudioStreamBuilder* builder = nullptr;
    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_CAPTURER),
        NotifyError(AudioErrorType::INIT, "system error"), -1);

    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_SetCapturerInfo(builder, static_cast<OH_AudioStream_SourceType>(audioSource_)),
        NotifyError(AudioErrorType::INIT, "failed to set capturer info"), -1);
    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_SetSamplingRate(builder, sampleRate_),
        NotifyError(AudioErrorType::INIT, "failed to set sample rate"), -1);
    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_SetChannelCount(builder, channelCount_),
        NotifyError(AudioErrorType::INIT, "failed to set channel count"), -1);
    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_SetSampleFormat(builder, (OH_AudioStream_SampleFormat)audioFormat_),
        NotifyError(AudioErrorType::INIT, "failed to set sample format"), -1);
    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_SetLatencyMode(
            builder, useLowLatency_ ? AUDIOSTREAM_LATENCY_MODE_FAST : AUDIOSTREAM_LATENCY_MODE_NORMAL),
        NotifyError(AudioErrorType::INIT, "failed to set latency mode"), -1);

    OH_AudioCapturer_Callbacks callbacks;
    callbacks.OH_AudioCapturer_OnReadData = OnReadData1;
    callbacks.OH_AudioCapturer_OnStreamEvent = OnStreamEvent1;
    callbacks.OH_AudioCapturer_OnInterruptEvent = OnInterruptEvent1;
    callbacks.OH_AudioCapturer_OnError = OnError1;
    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_SetCapturerCallback(builder, callbacks, this),
        NotifyError(AudioErrorType::INIT, "failed to set capture callback"), -1);

    OH_AudioCapturer* stream = nullptr;
    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_GenerateCapturer(builder, &stream), NotifyError(AudioErrorType::INIT, "system error"),
        -1);

    // check configuration
    int32_t rate;
    OH_RESULT_CHECK(
        OH_AudioCapturer_GetSamplingRate(stream, &rate), NotifyError(AudioErrorType::INIT, "failed to get sample rate"),
        -1);
    if (rate != sampleRate_) {
        RTC_LOG(LS_ERROR) << "Stream unable to use requested sample rate";
        NotifyError(AudioErrorType::INIT, "unmatched sample rate");
        return -1;
    }

    int32_t channelCount;
    OH_RESULT_CHECK(
        OH_AudioCapturer_GetChannelCount(stream, &channelCount),
        NotifyError(AudioErrorType::INIT, "failed to get channel count"), -1);
    if (channelCount != static_cast<int32_t>(channelCount_)) {
        RTC_LOG(LS_ERROR) << "Stream unable to use requested channel count";
        NotifyError(AudioErrorType::INIT, "unmatched channel count");
        return -1;
    }

    OH_AudioStream_SampleFormat sampleFormat;
    OH_RESULT_CHECK(
        OH_AudioCapturer_GetSampleFormat(stream, &sampleFormat),
        NotifyError(AudioErrorType::INIT, "failed to get sample format"), -1);
    if (sampleFormat != AUDIOSTREAM_SAMPLE_S16LE) {
        RTC_LOG(LS_ERROR) << "Stream unable to use requested format";
        NotifyError(AudioErrorType::INIT, "unmatched sample format");
        return -1;
    }

    capturer_ = stream;
    initialized_ = true;

    RTC_DLOG(LS_VERBOSE) << "current state: " << StateToString(GetCurrentState());

    return 0;
}

bool AudioCapturer::RecordingIsInitialized() const
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " initialized_ = " << initialized_;
    return initialized_;
}

int32_t AudioCapturer::StartRecording()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RTC_DCHECK(threadChecker_.IsCurrent());
    if (recording_) {
        // Already recording.
        return 0;
    }

    if (!initialized_) {
        RTC_DLOG(LS_WARNING) << "Recording can not start since InitRecording must succeed first";
        return 0;
    }

    if (fineAudioBuffer_) {
        fineAudioBuffer_->ResetPlayout();
    }

    OH_AudioStream_State state = GetCurrentState();
    if (state != AUDIOSTREAM_STATE_PREPARED) {
        RTC_LOG(LS_ERROR) << "Invalid state: " << StateToString(state);
        NotifyError(AudioErrorType::START_STATE_MISMATCH, std::string("invalid state: ") + StateToString(state));
        return -1;
    }

    OH_RESULT_CHECK(
        OH_AudioCapturer_Start(capturer_), NotifyError(AudioErrorType::START_EXCEPTION, "system error"), -1);
    RTC_DLOG(LS_VERBOSE) << "current state: " << StateToString(GetCurrentState());
    recording_ = true;

    NotifyStateChange(AudioStateType::START);

    return 0;
}

int32_t AudioCapturer::StopRecording()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RTC_DCHECK(threadChecker_.IsCurrent());
    if (!initialized_ || !recording_) {
        return 0;
    }

    OH_RESULT_CHECK(OH_AudioCapturer_Stop(capturer_), -1);
    OH_RESULT_CHECK(OH_AudioCapturer_Release(capturer_), -1);

    initialized_ = false;
    recording_ = false;

    NotifyStateChange(AudioStateType::STOP);

    return 0;
}

bool AudioCapturer::Recording() const
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " recording_ = " << recording_;
    return recording_;
}

void AudioCapturer::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " audioBuffer = " << audioBuffer;

    RTC_DCHECK(threadChecker_.IsCurrent());
    audioDeviceBuffer_ = audioBuffer;
    audioDeviceBuffer_->SetRecordingSampleRate(sampleRate_);
    audioDeviceBuffer_->SetRecordingChannels(channelCount_);
    fineAudioBuffer_ = std::make_unique<FineAudioBuffer>(audioDeviceBuffer_);
}

int32_t AudioCapturer::MicrophoneMuteIsAvailable(bool* available)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    *available = true;
    return 0;
}

int32_t AudioCapturer::SetMicrophoneMute(bool mute)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    microphoneMute_ = mute;
    return 0;
}

int32_t AudioCapturer::MicrophoneMute(bool* enabled) const
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    *enabled = microphoneMute_;
    return 0;
}

void AudioCapturer::RegisterObserver(Observer* obs)
{
    if (!obs) {
        return;
    }

    observers_.insert(obs);
}

void AudioCapturer::UnregisterObserver(Observer* obs)
{
    if (!obs) {
        return;
    }

    observers_.erase(obs);
}

int32_t AudioCapturer::OnReadData(OH_AudioCapturer* stream, void* buffer, int32_t length)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " bufferLen=" << length;

    (void)stream;

    int64_t position;
    int64_t timestamp;
    OH_RESULT_CHECK(OH_AudioCapturer_GetTimestamp(capturer_, CLOCK_MONOTONIC, &position, &timestamp), 0);

    double latencyMillis = static_cast<double>(rtc::TimeNanos() - timestamp) / rtc::kNumNanosecsPerMillisec;
    RTC_DLOG(LS_VERBOSE) << "latencyMillis=" << latencyMillis;
    captureDelayMs_ = static_cast<int>(std::ceil(latencyMillis));

    if (microphoneMute_) {
        std::memset(buffer, 0, length);
    }

    fineAudioBuffer_->DeliverRecordedData(
        rtc::MakeArrayView(static_cast<const int16_t*>(buffer), length / sizeof(int16_t)), captureDelayMs_);

    auto data = new rtc::CopyOnWriteBuffer((uint8_t*)buffer, length);
    NotifySamplesReady(sampleRate_, audioFormat_, channelCount_, data);

    return -1;
}

int32_t AudioCapturer::OnStreamEvent(OH_AudioCapturer* stream, OH_AudioStream_Event event)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " event=" << event;

    (void)stream;

    return 0;
}

int32_t
AudioCapturer::OnInterruptEvent(OH_AudioCapturer* stream, OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " type=" << type;

    (void)stream;
    (void)hint;

    return 0;
}

int32_t AudioCapturer::OnError(OH_AudioCapturer* stream, OH_AudioStream_Result error)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " error=" << error;

    (void)stream;

    NotifyError(AudioErrorType::GENERAL, "system error");

    return 0;
}

OH_AudioStream_State AudioCapturer::GetCurrentState() const
{
    OH_AudioStream_State state = AUDIOSTREAM_STATE_INVALID;
    if (capturer_) {
        OH_AudioCapturer_GetCurrentState(capturer_, &state);
    }

    return state;
}

void AudioCapturer::NotifyError(AudioErrorType error, const std::string& message)
{
    for (auto& obs : observers_) {
        obs->OnAudioInputError(error, message);
    }
}

void AudioCapturer::NotifyStateChange(AudioStateType state)
{
    for (auto& obs : observers_) {
        obs->OnAudioInputStateChange(state);
    }
}

void AudioCapturer::NotifySamplesReady(
    int32_t sampleRate, int32_t format, int32_t channelCount, rtc::CopyOnWriteBuffer* data)
{
    for (auto& obs : observers_) {
        obs->OnAudioInputSamplesReady(sampleRate, format, channelCount, data);
    }
}

} // namespace webrtc
