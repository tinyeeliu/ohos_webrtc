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

#ifndef WEBRTC_AUDIO_CAPTURER_H
#define WEBRTC_AUDIO_CAPTURER_H

#include "audio_device_module.h"

#include <ohaudio/native_audiocapturer.h>
#include <ohaudio/native_audiostreambuilder.h>

#include <set>
#include <memory>
#include <atomic>

#include "napi.h"

#include "api/sequence_checker.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_device/fine_audio_buffer.h"

namespace webrtc {

class AudioCapturer : public AudioInput {
public:
    static std::unique_ptr<AudioCapturer>
    Create(int32_t source, int32_t format, int32_t sampleRate, bool useStereoInput, bool useLowLatency);

    // Do not use this constructor directly, use 'Create' above.
    AudioCapturer(int32_t source, int32_t format, int32_t sampleRate, bool useStereoInput, bool useLowLatency);
    ~AudioCapturer() override;

    int32_t Init() override;
    int32_t Terminate() override;

    int32_t InitRecording() override;
    bool RecordingIsInitialized() const override;

    int32_t StartRecording() override;
    int32_t StopRecording() override;
    bool Recording() const override;

    void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) override;

    int32_t MicrophoneMuteIsAvailable(bool* available) override;
    int32_t SetMicrophoneMute(bool mute) override;
    int32_t MicrophoneMute(bool* enabled) const override;

    void RegisterObserver(Observer* obs) override;
    void UnregisterObserver(Observer* obs) override;

protected:
    static int32_t OnReadData1(OH_AudioCapturer* stream, void* userData, void* buffer, int32_t lenth);
    static int32_t OnStreamEvent1(OH_AudioCapturer* capturer, void* userData, OH_AudioStream_Event event);
    static int32_t OnInterruptEvent1(
        OH_AudioCapturer* stream, void* userData, OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint);
    static int32_t OnError1(OH_AudioCapturer* stream, void* userData, OH_AudioStream_Result error);

    int32_t OnReadData(OH_AudioCapturer* stream, void* buffer, int32_t lenth);
    int32_t OnStreamEvent(OH_AudioCapturer* stream, OH_AudioStream_Event event);
    int32_t OnInterruptEvent(OH_AudioCapturer* stream, OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint);
    int32_t OnError(OH_AudioCapturer* stream, OH_AudioStream_Result error);

    OH_AudioStream_State GetCurrentState() const;

    void NotifyError(AudioErrorType error, const std::string& message);
    void NotifyStateChange(AudioStateType state);
    void NotifySamplesReady(int32_t sampleRate, int32_t format, int32_t channelCount, rtc::CopyOnWriteBuffer* data);

private:
    SequenceChecker threadChecker_;

    const int32_t audioSource_;
    const int32_t audioFormat_;
    const int32_t sampleRate_;
    const int32_t channelCount_;
    const bool useLowLatency_;

    // Delay estimate of the input
    std::atomic<int> captureDelayMs_{0};

    // Sets all recorded samples to zero if `microphoneMute_` is true, i.e., ensures that
    // the microphone is muted.
    std::atomic<bool> microphoneMute_{false};

    bool initialized_{false};
    bool recording_{false};

    AudioDeviceBuffer* audioDeviceBuffer_{nullptr};
    std::unique_ptr<FineAudioBuffer> fineAudioBuffer_;

    OH_AudioCapturer* capturer_{nullptr};

    std::set<Observer*> observers_;
};

} // namespace webrtc

#endif // WEBRTC_AUDIO_CAPTURER_H
