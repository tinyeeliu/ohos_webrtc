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

#include "modules/audio_device/ohos/ohaudio_recorder.h"

#include <memory>
#include <cerrno>
#include "api/array_view.h"
#include "modules/audio_device/fine_audio_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

OHAudioRecorder::OHAudioRecorder(const AudioParameters& audio_parameters)
    : main_thread_(TaskQueueBase::Current()),
      ohaudio_(audio_parameters, AUDIOSTREAM_TYPE_CAPTURER, this)
{
    RTC_LOG(LS_INFO) << "ctor";
    thread_checker_ohaudio_.Detach();
}

OHAudioRecorder::~OHAudioRecorder()
{
    RTC_LOG(LS_INFO) << "dtor";
    RTC_DCHECK(thread_checker_.IsCurrent());
    Terminate();
    RTC_LOG(LS_INFO) << "detected owerflows: " << overflow_count_;
}

int OHAudioRecorder::Init()
{
    RTC_LOG(LS_INFO) << "Init";
    RTC_DCHECK(thread_checker_.IsCurrent());
    if (ohaudio_.audio_parameters().channels() == default_channels) {
        RTC_DLOG(LS_WARNING) << "Stereo mode is enabled";
    }
    return 0;
}

int OHAudioRecorder::Terminate()
{
    RTC_LOG(LS_INFO) << "Terminate";
    RTC_DCHECK(thread_checker_.IsCurrent());
    StopRecording();
    return 0;
}

int OHAudioRecorder::InitRecording()
{
    RTC_LOG(LS_INFO) << "InitRecording";
    RTC_DCHECK(thread_checker_.IsCurrent());
    RTC_DCHECK(!initialized_);
    RTC_DCHECK(!recording_);
    if (!ohaudio_.Init()) {
        RTC_LOG(LS_ERROR) << "OHAUDIO INIT FAILED! errcode = " << errno;
        return -1;
    }
    initialized_ = true;
    return 0;
}

bool OHAudioRecorder::RecordingIsInitialized() const
{
    RTC_LOG(LS_INFO) << "initialized_ = " << initialized_;
    return initialized_;
}

int OHAudioRecorder::StartRecording()
{
    RTC_LOG(LS_INFO) << "StartRecording";
    RTC_DCHECK(thread_checker_.IsCurrent());
    RTC_DCHECK(initialized_);
    RTC_DCHECK(!recording_);
    if (fine_audio_buffer_) {
        fine_audio_buffer_->ResetPlayout();
    }
    if (!ohaudio_.Start()) {
        return -1;
    }
    overflow_count_ = ohaudio_.xrun_count();
    first_data_callback_ = true;
    recording_ = true;
    return 0;
}

int OHAudioRecorder::StopRecording()
{
    RTC_LOG(LS_INFO) << "StopRecording";
    RTC_DCHECK(thread_checker_.IsCurrent());
    if (!initialized_ || !recording_) {
        return 0;
    }
    if (!ohaudio_.Stop()) {
        return -1;
    }
    thread_checker_ohaudio_.Detach();
    initialized_ = false;
    recording_ = false;
    return 0;
}

bool OHAudioRecorder::Recording() const
{
    return recording_;
}

void OHAudioRecorder::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer)
{
    RTC_LOG(LS_INFO) << "AttachAudioBuffer";
    RTC_DCHECK(thread_checker_.IsCurrent());
    audio_device_buffer_ = audioBuffer;
    const AudioParameters audio_parameters = ohaudio_.audio_parameters();
    audio_device_buffer_->SetRecordingSampleRate(audio_parameters.sample_rate());
    audio_device_buffer_->SetRecordingChannels(audio_parameters.channels());
    RTC_CHECK(audio_device_buffer_);
    fine_audio_buffer_ = std::make_unique<FineAudioBuffer>(audio_device_buffer_);
}

bool OHAudioRecorder::IsAcousticEchoCancelerSupported() const
{
    return false;
}

bool OHAudioRecorder::IsNoiseSuppressorSupported() const
{
    return false;
}

int OHAudioRecorder::EnableBuiltInAEC(bool enable)
{
    RTC_LOG(LS_INFO) << "EnableBuiltInAEC: " << enable;
    RTC_LOG(LS_ERROR) << "Not implemented";
    return -1;
}

int OHAudioRecorder::EnableBuiltInNS(bool enable)
{
    RTC_LOG(LS_INFO) << "EnableBuiltInNS: " << enable;
    RTC_LOG(LS_ERROR) << "Not implemented";
    return -1;
}

int32_t OHAudioRecorder::OnErrorCallback(OH_AudioStream_Result error)
{
    return 0;
}

int32_t OHAudioRecorder::OnDataCallback(void* audio_data, int32_t num_frames)
{
    RTC_LOG(LS_INFO) << "num_frames=" <<num_frames;
    static const int audio_data_1s = 1000;
    static const float half_sec = 0.5;
    if (first_data_callback_) {
        RTC_LOG(LS_INFO) << "--- First input data callback: device id="
                         << ohaudio_.device_id();
        ohaudio_.ClearInputStream(audio_data, num_frames);
        first_data_callback_ = false;
    }

    const int32_t overflow_count = ohaudio_.xrun_count();
    if (overflow_count > overflow_count_) {
        RTC_LOG(LS_ERROR) << "Overflow detected: " << overflow_count;
        overflow_count_ = overflow_count;
    }

    latency_millis_ = ohaudio_.EstimateLatencyMillis();
    if (ohaudio_.frames_read() % (audio_data_1s * ohaudio_.frames_per_burst()) == 0) {
        RTC_DLOG(LS_INFO) << "input latency: " << latency_millis_
                          << ", num_frames: " << num_frames;
    }
    fine_audio_buffer_->DeliverRecordedData(
        rtc::MakeArrayView(static_cast<const int16_t*>(audio_data),
                           num_frames / sizeof(int16_t)),
        static_cast<int>(latency_millis_ + half_sec));

    return 0;
}

void OHAudioRecorder::HandleStreamDisconnected()
{
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RTC_LOG(LS_INFO) << "HandleStreamDisconnected";
    if (!initialized_ || !recording_) {
        return;
    }
    StopRecording();
    InitRecording();
    StartRecording();
}
}  // namespace webrtc
