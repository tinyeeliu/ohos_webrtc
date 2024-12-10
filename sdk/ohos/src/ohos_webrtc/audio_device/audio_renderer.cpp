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

#include "audio_renderer.h"
#include "audio_common.h"

#include "rtc_base/logging.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

std::unique_ptr<AudioRenderer>
AudioRenderer::Create(int32_t usage, int32_t sampleRate, bool useStereoOutput, bool useLowLatency)
{
    return std::make_unique<AudioRenderer>(usage, sampleRate, useStereoOutput, useLowLatency);
}

int32_t AudioRenderer::OnWriteData1(OH_AudioRenderer* renderer, void* userData, void* buffer, int32_t length)
{
    auto self = (AudioRenderer*)userData;
    return self->OnWriteData(renderer, buffer, length);
}

int32_t AudioRenderer::OnStreamEvent1(OH_AudioRenderer* renderer, void* userData, OH_AudioStream_Event event)
{
    auto self = (AudioRenderer*)userData;
    return self->OnStreamEvent(renderer, event);
}

int32_t AudioRenderer::OnInterruptEvent1(
    OH_AudioRenderer* renderer, void* userData, OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint)
{
    auto self = (AudioRenderer*)userData;
    return self->OnInterruptEvent(renderer, type, hint);
}

int32_t AudioRenderer::OnError1(OH_AudioRenderer* renderer, void* userData, OH_AudioStream_Result error)
{
    auto self = (AudioRenderer*)userData;
    return self->OnError(renderer, error);
}

AudioRenderer::AudioRenderer(int32_t usage, int32_t sampleRate, bool useStereoOutput, bool useLowLatency)
    : rendererUsage_(usage),
      sampleRate_(sampleRate),
      channelCount_(useStereoOutput ? kAudioChannelCount_Stereo : kAudioChannelCount_Mono),
      useLowLatency_(useLowLatency)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    threadChecker_.Detach();
}

AudioRenderer::~AudioRenderer()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RTC_DCHECK(threadChecker_.IsCurrent());
    Terminate();
}

int32_t AudioRenderer::Init()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RTC_DCHECK(threadChecker_.IsCurrent());
    return 0;
}

int32_t AudioRenderer::Terminate()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RTC_DCHECK(threadChecker_.IsCurrent());
    StopPlayout();
    threadChecker_.Detach();

    return 0;
}

int32_t AudioRenderer::InitPlayout()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RTC_DCHECK(threadChecker_.IsCurrent());
    if (initialized_) {
        // Already initialized.
        return 0;
    }
    RTC_DCHECK(!playing_);

    OH_AudioStreamBuilder* builder = nullptr;
    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER),
        NotifyError(AudioErrorType::INIT, "System error"), -1);

    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_SetRendererInfo(builder, static_cast<OH_AudioStream_Usage>(rendererUsage_)),
        NotifyError(AudioErrorType::INIT, "failed to set renderer info"), -1);
    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_SetSamplingRate(builder, sampleRate_),
        NotifyError(AudioErrorType::INIT, "failed to set sample rate"), -1);
    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_SetChannelCount(builder, channelCount_),
        NotifyError(AudioErrorType::INIT, "failed to set channel count"), -1);
    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_SetSampleFormat(builder, AUDIOSTREAM_SAMPLE_S16LE),
        NotifyError(AudioErrorType::INIT, "failed to set sample format"), -1);
    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_SetLatencyMode(
            builder, useLowLatency_ ? AUDIOSTREAM_LATENCY_MODE_FAST : AUDIOSTREAM_LATENCY_MODE_NORMAL),
        NotifyError(AudioErrorType::INIT, "failed to set latency mode"), -1);

    OH_AudioRenderer_Callbacks callbacks;
    callbacks.OH_AudioRenderer_OnWriteData = OnWriteData1;
    callbacks.OH_AudioRenderer_OnStreamEvent = OnStreamEvent1;
    callbacks.OH_AudioRenderer_OnInterruptEvent = OnInterruptEvent1;
    callbacks.OH_AudioRenderer_OnError = OnError1;
    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_SetRendererCallback(builder, callbacks, this),
        NotifyError(AudioErrorType::INIT, "failed to set renderer callback"), -1);

    OH_AudioRenderer* stream = nullptr;
    OH_RESULT_CHECK(
        OH_AudioStreamBuilder_GenerateRenderer(builder, &stream),
        NotifyError(AudioErrorType::INIT, "failed to generate renderer"), -1);

    // check configuration
    int32_t rate;
    OH_RESULT_CHECK(
        OH_AudioRenderer_GetSamplingRate(stream, &rate),
        NotifyError(AudioErrorType::INIT, "failed to get sampling rate"), -1);
    if (rate != sampleRate_) {
        RTC_LOG(LS_ERROR) << "Stream unable to use requested sample rate";
        NotifyError(AudioErrorType::INIT, "unmatched sampling rate");
        return -1;
    }

    int32_t channelCount;
    OH_RESULT_CHECK(
        OH_AudioRenderer_GetChannelCount(stream, &channelCount),
        NotifyError(AudioErrorType::INIT, "failed to get channel count"), -1);
    if (channelCount != static_cast<int32_t>(channelCount_)) {
        RTC_LOG(LS_ERROR) << "Stream unable to use requested channel count";
        NotifyError(AudioErrorType::INIT, "unmatched channel count");
        return -1;
    }

    OH_AudioStream_SampleFormat sampleFormat;
    OH_RESULT_CHECK(
        OH_AudioRenderer_GetSampleFormat(stream, &sampleFormat),
        NotifyError(AudioErrorType::INIT, "failed to get sample format"), -1);
    if (sampleFormat != AUDIOSTREAM_SAMPLE_S16LE) {
        RTC_LOG(LS_ERROR) << "Stream unable to use requested format";
        NotifyError(AudioErrorType::INIT, "unmatched sample format");
        return -1;
    }

    renderer_ = stream;
    initialized_ = true;

    RTC_DLOG(LS_VERBOSE) << "current state: " << StateToString(GetCurrentState());

    return 0;
}

bool AudioRenderer::PlayoutIsInitialized() const
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    return initialized_;
}

int32_t AudioRenderer::StartPlayout()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RTC_DCHECK(threadChecker_.IsCurrent());
    if (playing_) {
        // Already playing.
        return 0;
    }

    if (!initialized_) {
        RTC_DLOG(LS_WARNING) << "Playout can not start since InitPlayout must succeed first";
        return 0;
    }

    if (fineAudioBuffer_) {
        fineAudioBuffer_->ResetPlayout();
    }

    OH_AudioStream_State state = GetCurrentState();
    if (state != AUDIOSTREAM_STATE_PREPARED) {
        RTC_LOG(LS_ERROR) << "Invalid state: " << StateToString(state);
        NotifyError(AudioErrorType::START_STATE_MISMATCH, std::string("Invalid state: ") + StateToString(state));
        return -1;
    }

    OH_RESULT_CHECK(
        OH_AudioRenderer_Start(renderer_), NotifyError(AudioErrorType::START_EXCEPTION, "System error"), -1);
    RTC_DLOG(LS_VERBOSE) << "Current state: " << StateToString(GetCurrentState());

    playing_ = true;

    NotifyStateChange(AudioStateType::START);

    return 0;
}

int32_t AudioRenderer::StopPlayout()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RTC_DCHECK(threadChecker_.IsCurrent());
    if (!initialized_ || !playing_) {
        return 0;
    }

    OH_RESULT_CHECK(OH_AudioRenderer_Stop(renderer_), -1);
    OH_RESULT_CHECK(OH_AudioRenderer_Release(renderer_), -1);

    initialized_ = false;
    playing_ = false;

    NotifyStateChange(AudioStateType::STOP);

    return 0;
}

bool AudioRenderer::Playing() const
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    return playing_;
}

void AudioRenderer::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RTC_DCHECK(threadChecker_.IsCurrent());
    audioDeviceBuffer_ = audioBuffer;
    RTC_CHECK(audioDeviceBuffer_);
    audioDeviceBuffer_->SetPlayoutSampleRate(sampleRate_);
    audioDeviceBuffer_->SetPlayoutChannels(channelCount_);

    fineAudioBuffer_ = std::make_unique<FineAudioBuffer>(audioDeviceBuffer_);
}

int32_t AudioRenderer::SpeakerMuteIsAvailable(bool* available)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    *available = true;
    return 0;
}

int32_t AudioRenderer::setSpeakerMute(bool mute)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    speakerMute_ = mute;
    return 0;
}

int32_t AudioRenderer::SpeakerMute(bool* enabled) const
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    *enabled = speakerMute_;
    return 0;
}

int32_t AudioRenderer::PlayoutDelay(uint16_t* delayMs) const
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    *delayMs = rendererDelayMs_;
    return 0;
}

int AudioRenderer::GetPlayoutUnderrunCount()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    return 0;
}

void AudioRenderer::RegisterObserver(Observer* obs)
{
    if (!obs) {
        return;
    }

    observers_.insert(obs);
}

void AudioRenderer::UnregisterObserver(Observer* obs)
{
    if (!obs) {
        return;
    }

    observers_.erase(obs);
}

int32_t AudioRenderer::OnWriteData(OH_AudioRenderer* renderer, void* buffer, int32_t length)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " length=" << length;

    (void)renderer;

    int64_t position;
    int64_t timestamp;
    OH_RESULT_CHECK(OH_AudioRenderer_GetTimestamp(renderer_, CLOCK_MONOTONIC, &position, &timestamp), 0);

    double latencyMillis = static_cast<double>(timestamp - rtc::TimeNanos()) / rtc::kNumNanosecsPerMillisec;
    RTC_DLOG(LS_VERBOSE) << "latencyMillis=" << latencyMillis;
    rendererDelayMs_ = static_cast<int>(std::ceil(latencyMillis));

    fineAudioBuffer_->GetPlayoutData(
        rtc::MakeArrayView(static_cast<int16_t*>(buffer), length / sizeof(int16_t)), rendererDelayMs_);

    if (speakerMute_) {
        std::memset(buffer, 0, length);
    }

    return 0;
}

int32_t AudioRenderer::OnStreamEvent(OH_AudioRenderer* renderer, OH_AudioStream_Event event)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " event=" << event;

    (void)renderer;

    return 0;
}

int32_t AudioRenderer::OnInterruptEvent(
    OH_AudioRenderer* renderer, OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " type=" << type;

    (void)renderer;
    (void)hint;

    return 0;
}

int32_t AudioRenderer::OnError(OH_AudioRenderer* renderer, OH_AudioStream_Result error)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " error=" << error;

    (void)renderer;

    NotifyError(AudioErrorType::GENERAL, "System error");

    return 0;
}

OH_AudioStream_State AudioRenderer::GetCurrentState() const
{
    OH_AudioStream_State state = AUDIOSTREAM_STATE_INVALID;
    if (renderer_) {
        OH_AudioRenderer_GetCurrentState(renderer_, &state);
    }

    return state;
}

void AudioRenderer::NotifyError(AudioErrorType error, const std::string& message)
{
    for (auto& obs : observers_) {
        obs->OnAudioOutputError(error, message);
    }
}

void AudioRenderer::NotifyStateChange(AudioStateType state)
{
    for (auto& obs : observers_) {
        obs->OnAudioOutputStateChange(state);
    }
}

} // namespace webrtc
