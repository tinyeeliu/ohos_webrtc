/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_FAKE_AUDIO_SOURCE_H
#define WEBRTC_FAKE_AUDIO_SOURCE_H

#include <memory>

#include "api/audio_options.h"
#include "api/media_stream_interface.h"
#include "api/notifier.h"
#include "api/scoped_refptr.h"
#include "rtc_base/thread.h"

#include "audio_input.h"

namespace webrtc {

class OhosLocalAudioSource : public Notifier<AudioSourceInterface> {
public:
    // Creates an instance of OhosLocalAudioSource.
    static rtc::scoped_refptr<OhosLocalAudioSource>
    Create(cricket::AudioOptions audioOptions, std::shared_ptr<AudioInput> audioInput);

    SourceState state() const override
    {
        return kLive;
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

protected:
    OhosLocalAudioSource() = default;
    ~OhosLocalAudioSource() override = default;

    void Initialize(cricket::AudioOptions audioOptions, std::shared_ptr<AudioInput> audioInput);

private:
    cricket::AudioOptions audioOptions_;
    std::shared_ptr<AudioInput> audioInput_;
};

} // namespace webrtc

#endif // WEBRTC_FAKE_AUDIO_SOURCE_H
