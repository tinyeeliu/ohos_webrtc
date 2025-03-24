/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "ohos_local_audio_source.h"

#include <thread>

#include "api/audio/audio_frame.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

rtc::scoped_refptr<OhosLocalAudioSource>
OhosLocalAudioSource::Create(cricket::AudioOptions audioOptions, std::shared_ptr<AudioInput> audioInput)
{
    RTC_DCHECK(audioInput);
    if (!audioInput) {
        RTC_LOG(LS_WARNING) << "Invalid parameter";
        return nullptr;
    }

    auto source = rtc::make_ref_counted<OhosLocalAudioSource>();
    source->Initialize(std::move(audioOptions), std::move(audioInput));

    return source;
}

void OhosLocalAudioSource::Initialize(cricket::AudioOptions audioOptions, std::shared_ptr<AudioInput> audioInput)
{
    audioOptions_ = std::move(audioOptions);
    audioInput_ = std::move(audioInput);
}

std::string OhosLocalAudioSource::GetLabel() const
{
    return audioInput_->GetLabel();
}

void OhosLocalAudioSource::SetMute(bool mute)
{
    audioInput_->SetMute(mute);
}

} // namespace webrtc
