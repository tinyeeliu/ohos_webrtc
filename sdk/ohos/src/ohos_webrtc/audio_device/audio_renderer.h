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

#ifndef WEBRTC_AUDIO_RENDERER_H
#define WEBRTC_AUDIO_RENDERER_H

#include "audio_device_module.h"

#include <ohaudio/native_audiorenderer.h>
#include <ohaudio/native_audiostreambuilder.h>

#include <set>
#include <memory>
#include <atomic>

#include "napi.h"

#include "api/sequence_checker.h"
#include "rtc_base/race_checker.h"
#include "modules/audio_device/fine_audio_buffer.h"

namespace webrtc {

class AudioRenderer : public AudioOutput {
public:
    static std::unique_ptr<AudioRenderer>
    Create(int32_t usage, int32_t sampleRate, bool useStereoOutput, bool useLowLatency);

    // Do not use this constructor directly, use 'Create' above.
    AudioRenderer(int32_t usage, int32_t sampleRate, bool useStereoOutput, bool useLowLatency);
    ~AudioRenderer() override;

    int32_t Init() override;
    int32_t Terminate() override;

    int32_t InitPlayout() override;
    bool PlayoutIsInitialized() const override;

    int32_t StartPlayout() override;
    int32_t StopPlayout() override;
    bool Playing() const override;

    void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) override;

    int32_t SpeakerMuteIsAvailable(bool* available) override;
    int32_t setSpeakerMute(bool mute) override;
    int32_t SpeakerMute(bool* enabled) const override;

    int32_t PlayoutDelay(uint16_t* delayMs) const override;
    int GetPlayoutUnderrunCount() override;

    void RegisterObserver(Observer* obs) override;
    void UnregisterObserver(Observer* obs) override;

protected:
    static int32_t OnWriteData1(OH_AudioRenderer* renderer, void* userData, void* buffer, int32_t length);
    static int32_t OnStreamEvent1(OH_AudioRenderer* renderer, void* userData, OH_AudioStream_Event event);
    static int32_t OnInterruptEvent1(
        OH_AudioRenderer* renderer, void* userData, OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint);
    static int32_t OnError1(OH_AudioRenderer* renderer, void* userData, OH_AudioStream_Result error);
    static void
    OnDeviceChangeCallback1(OH_AudioRenderer* renderer, void* userData, OH_AudioStream_DeviceChangeReason reason);

    int32_t OnWriteData(OH_AudioRenderer* renderer, void* buffer, int32_t length);
    int32_t OnStreamEvent(OH_AudioRenderer* renderer, OH_AudioStream_Event event);
    int32_t OnInterruptEvent(OH_AudioRenderer* renderer, OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint);
    int32_t OnError(OH_AudioRenderer* renderer, OH_AudioStream_Result error);
    void OnDeviceChangeCallback(OH_AudioRenderer* renderer, OH_AudioStream_DeviceChangeReason reason);

    OH_AudioStream_State GetCurrentState() const;
    double EstimateLatencyMillis() const;
    uint32_t GetUnderflowCount() const;

    void NotifyError(AudioErrorType error, const std::string& message);
    void NotifyStateChange(AudioStateType state);

private:
    SequenceChecker threadChecker_;
    rtc::RaceChecker dataRaceChecker_;

    const int32_t rendererUsage_;
    const int32_t sampleRate_;
    const int32_t channelCount_;
    const bool useLowLatency_;

    // Samples to be played are replaced by zeros if `speakerMute_` is set to true.
    // Can be used to ensure that the speaker is fully muted.
    std::atomic<bool> speakerMute_{false};

    bool initialized_{false};
    bool playing_{false};

    uint32_t underflowCount_{0};

    AudioDeviceBuffer* audioDeviceBuffer_{nullptr};
    std::unique_ptr<FineAudioBuffer> fineAudioBuffer_;

    OH_AudioRenderer* renderer_{nullptr};

    std::set<Observer*> observers_;
};

} // namespace webrtc

#endif // WEBRTC_AUDIO_RENDERER_H
