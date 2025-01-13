/*
# Copyright (c) 2023 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#include "modules/audio_device/ohos/ohaudio_player.h"

#include <memory>

#include "api/array_view.h"
#include "api/task_queue/task_queue_base.h"
#include "modules/audio_device/fine_audio_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

OHAudioPlayer::OHAudioPlayer(const AudioParameters& audio_parameters)
    : main_thread_(TaskQueueBase::Current()),
    ohaudio_(audio_parameters, AUDIOSTREAM_TYPE_RENDERER, this)
{
    RTC_LOG(LS_INFO) << "ctor";
}

OHAudioPlayer::~OHAudioPlayer()
{
    RTC_LOG(LS_INFO) << "dtor";
    RTC_DCHECK_RUN_ON(&main_thread_checker_);
    Terminate();
    RTC_LOG(LS_INFO) << "#detected underruns: " << underrun_count_;
}

int OHAudioPlayer::Init()
{
    RTC_LOG(LS_INFO) << "Init";
    RTC_DCHECK_RUN_ON(&main_thread_checker_);
    if (ohaudio_.audio_parameters().channels() == default_channels) {
        RTC_DLOG(LS_WARNING) << "Stereo mode is enabled";
    }
    return 0;
}

int OHAudioPlayer::Terminate()
{
    RTC_LOG(LS_INFO) << "Terminate";
    RTC_DCHECK_RUN_ON(&main_thread_checker_);
    StopPlayout();
    return 0;
}

int OHAudioPlayer::InitPlayout()
{
    RTC_LOG(LS_INFO) << "InitPlayout";
    RTC_DCHECK_RUN_ON(&main_thread_checker_);
    RTC_DCHECK(!initialized_);
    RTC_DCHECK(!playing_);
    if (!ohaudio_.Init()) {
        return -1;
    }
    initialized_ = true;
    return 0;
}

bool OHAudioPlayer::PlayoutIsInitialized() const
{
    RTC_DCHECK_RUN_ON(&main_thread_checker_);
    return initialized_;
}

int OHAudioPlayer::StartPlayout()
{
    RTC_LOG(LS_INFO) << "StartPlayout";
    RTC_DCHECK_RUN_ON(&main_thread_checker_);
    RTC_DCHECK(!playing_);
    if (!initialized_) {
        RTC_DLOG(LS_WARNING)
            << "Playout can not start since InitPlayout must succeed first";
        return 0;
    }
    if (fine_audio_buffer_) {
        fine_audio_buffer_->ResetPlayout();
    }
    if (!ohaudio_.Start()) {
        return -1;
    }
    underrun_count_ = ohaudio_.xrun_count();
    first_data_callback_ = true;
    playing_ = true;
    return 0;
}

int OHAudioPlayer::StopPlayout()
{
    RTC_LOG(LS_INFO) << "StopPlayout";
    RTC_DCHECK_RUN_ON(&main_thread_checker_);
    if (!initialized_ || !playing_) {
        return 0;
    }
    if (!ohaudio_.Stop()) {
        RTC_LOG(LS_ERROR) << "StopPlayout failed";
        return -1;
    }
    initialized_ = false;
    playing_ = false;
    return 0;
}

bool OHAudioPlayer::Playing() const
{
    RTC_DCHECK_RUN_ON(&main_thread_checker_);
    return playing_;
}

void OHAudioPlayer::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer)
{
    RTC_DLOG(LS_INFO) << "AttachAudioBuffer";
    RTC_DCHECK_RUN_ON(&main_thread_checker_);
    audio_device_buffer_ = audioBuffer;
    const AudioParameters audio_parameters = ohaudio_.audio_parameters();
    audio_device_buffer_->SetPlayoutSampleRate(audio_parameters.sample_rate());
    audio_device_buffer_->SetPlayoutChannels(audio_parameters.channels());
    RTC_CHECK(audio_device_buffer_);
    fine_audio_buffer_ = std::make_unique<FineAudioBuffer>(audio_device_buffer_);
}

int OHAudioPlayer::SpeakerVolumeIsAvailable(bool &available)
{
    available=false;
    return 0;
}

int32_t OHAudioPlayer::OnErrorCallback(OH_AudioStream_Result error)
{
    return 0;
}

int32_t OHAudioPlayer::OnDataCallback(void* audio_data, int32_t num_frames)
{
    static const float half_sec = 0.5;
    // Some tests shows that this callback can sometimes be called on different
    // threads (after output device changed for example), so a thread checker
    // here may cause problem, and use race checker instead.
    RTC_DCHECK_RUNS_SERIALIZED(&race_checker_ohaudio_);
    if (first_data_callback_) {
        RTC_LOG(LS_INFO) << "--- First output data callback: device id="
                         << ohaudio_.device_id();
        first_data_callback_ = false;
    }
    RTC_LOG(LS_INFO) << "num_frames=" << num_frames;
    const int32_t underrun_count = ohaudio_.xrun_count();
    if (underrun_count > underrun_count_) {
        RTC_LOG(LS_ERROR) << "Underrun detected: " << underrun_count;
        underrun_count_ = underrun_count;
        ohaudio_.IncreaseOutputBufferSize();
    }
    latency_millis_ = ohaudio_.EstimateLatencyMillis();
    fine_audio_buffer_->GetPlayoutData(
        rtc::MakeArrayView(static_cast<int16_t*>(audio_data),
            num_frames / sizeof(int16_t)),
            static_cast<int>(latency_millis_ + half_sec));

    return 0;
}

void OHAudioPlayer::HandleStreamDisconnected()
{
    RTC_DCHECK_RUN_ON(&main_thread_checker_);
    RTC_DLOG(LS_INFO) << "HandleStreamDisconnected";
    if (!initialized_ || !playing_) {
        return;
    }
    StopPlayout();
    InitPlayout();
    StartPlayout();
}
}  // namespace webrtc
