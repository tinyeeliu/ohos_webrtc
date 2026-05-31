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

#ifndef WEBRTC_FAKE_AUDIO_SOURCE_H
#define WEBRTC_FAKE_AUDIO_SOURCE_H

#include <atomic>
#include <memory>

#include "api/audio_options.h"
#include "api/media_stream_interface.h"
#include "api/notifier.h"
#include "api/scoped_refptr.h"
#include "rtc_base/thread.h"

#include "audio_input.h"

namespace webrtc {

class OhosAudioDeviceModule;

class OhosLocalAudioSource : public Notifier<AudioSourceInterface> {
public:
    // Creates an instance of OhosLocalAudioSource.
    static rtc::scoped_refptr<OhosLocalAudioSource> Create(
        cricket::AudioOptions audioOptions, std::shared_ptr<AudioInput> audioInput,
        rtc::Thread* signalingThread, rtc::scoped_refptr<OhosAudioDeviceModule> adm);

    SourceState state() const override
    {
        return state_.load();
    }

    bool remote() const override
    {
        return false;
    }

    const cricket::AudioOptions options() const override
    {
        return audioOptions_;
    }

    void AddSink(AudioTrackSinkInterface* sink) override {}
    void RemoveSink(AudioTrackSinkInterface* sink) override {}

    std::shared_ptr<AudioInput> GetAudioInput() const
    {
        return audioInput_;
    }

    std::string GetLabel() const;

    void SetMute(bool mute);

    void SetVolume(double volume) override;

    void Stop();
protected:
    OhosLocalAudioSource() = default;
    ~OhosLocalAudioSource() override;

    void Initialize(
        cricket::AudioOptions audioOptions, std::shared_ptr<AudioInput> audioInput,
        rtc::Thread* signalingThread, rtc::scoped_refptr<OhosAudioDeviceModule> adm);

private:
    void ReleaseAdmLiveSourceIfNeeded();

    cricket::AudioOptions audioOptions_;
    std::shared_ptr<AudioInput> audioInput_;
    rtc::Thread* signalingThread_{nullptr}; // PeerConnectionFactory signaling thread
    rtc::scoped_refptr<OhosAudioDeviceModule> adm_;
    std::atomic<bool> releasedFromAdm_{false};
    std::atomic<SourceState> state_{kLive};
};

} // namespace webrtc

#endif // WEBRTC_FAKE_AUDIO_SOURCE_H
