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

#include "modules/audio_device/ohos/ohaudio_player_wrapper.h"

#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"

#define LOG_ON_ERROR(op)                                                      \
    do {                                                                        \
        OH_AudioStream_Result result = (op);                                            \
        if (result != AUDIOSTREAM_SUCCESS) {                                                \
            RTC_LOG(LS_ERROR) << #op << ": " << "NOK"; \
        }                                                                         \
    } while (0)

#define RETURN_ON_ERROR(op, ...)                                              \
    do {                                                                        \
        OH_AudioStream_Result result = (op);                                            \
        if (result != AUDIOSTREAM_SUCCESS) {                                                \
            RTC_LOG(LS_ERROR) << #op << ": " << "NOK"; \
            return __VA_ARGS__;                                                     \
        }                                                                         \
    } while (0)

namespace webrtc {
namespace {

const char* DirectionToString(OH_AudioStream_Type direction)
{
    switch (direction) {
        case AUDIOSTREAM_TYPE_RENDERER:
            return "OUTPUT";
        case AUDIOSTREAM_TYPE_CAPTURER:
            return "INPUT";
        default:
            return "UNKNOWN";
    }
}

const char *StateToString(OH_AudioStream_State state)
{
    switch (state) {
        case AUDIOSTREAM_STATE_INVALID:
            return "INVALID";
        case AUDIOSTREAM_STATE_PREPARED:
            return "PREPARED";
        case AUDIOSTREAM_STATE_RUNNING:
            return "RUNNING";
        case AUDIOSTREAM_STATE_STOPPED:
            return "STOPPED";
        case AUDIOSTREAM_STATE_RELEASED:
            return "RELEASED";
        case AUDIOSTREAM_STATE_PAUSED:
            return "PAUSED";
        default:
            return "UNKNOWN";
    }
}

const char* PerformanceModeToString(OH_AudioStream_LatencyMode mode)
{
    switch (mode) {
        case AUDIOSTREAM_LATENCY_MODE_NORMAL:
            return "NORMAL";
        case AUDIOSTREAM_LATENCY_MODE_FAST:
            return "LOW_LATENCY";
        default:
            return "UNKNOWN";
    }
}

const char* FormatToString(OH_AudioStream_SampleFormat id)
{
    switch (id) {
        case AUDIOSTREAM_SAMPLE_U8:
            return "INVALu8ID";
        case AUDIOSTREAM_SAMPLE_S16LE:
            return "S16LE";
        case AUDIOSTREAM_SAMPLE_S24LE:
            return "S24LE";
        case AUDIOSTREAM_SAMPLE_S32LE:
            return "S32LE";
        default:
            return "UNKNOWN";
    }
}

static int32_t ErrorCallback(OH_AudioRenderer *stream, void* user_data, OH_AudioStream_Result error)
{
    RTC_DCHECK(user_data);
    OHAudioPlayerWrapper* ohaudio_wrapper = reinterpret_cast<OHAudioPlayerWrapper*>(user_data);
    RTC_LOG(LS_WARNING) << "ErrorCallback: "
                        << DirectionToString(ohaudio_wrapper->direction());
    RTC_DCHECK(ohaudio_wrapper->observer());
    return ohaudio_wrapper->observer()->OnErrorCallback(error);
}

static int32_t DataCallback(OH_AudioRenderer *stream, void* user_data, void* audio_data, int32_t bufferLen)
{
    RTC_DCHECK(user_data);
    // Some tests shows that this callback can sometimes be called with nullptr
    // 'audio_data' and zero 'bufferLen' (after output device changed for example).
    if (!audio_data || bufferLen == 0) {
        RTC_LOG(LS_WARNING) << "Invalid data buffer";
        return 0;
    }
    RTC_LOG(LS_INFO) << "bufferLen=" << bufferLen;
    OHAudioPlayerWrapper* ohaudio_wrapper = reinterpret_cast<OHAudioPlayerWrapper*>(user_data);
    RTC_DCHECK(ohaudio_wrapper->observer());
    return ohaudio_wrapper->observer()->OnDataCallback(audio_data, bufferLen);
}

class ScopedStreamBuilder {
public:
    ScopedStreamBuilder(OH_AudioStream_Type streamType)
    {
        LOG_ON_ERROR(OH_AudioStreamBuilder_Create(&builder_, streamType));
        RTC_DCHECK(builder_);
    }
    ~ScopedStreamBuilder()
    {
        if (builder_) {
            LOG_ON_ERROR(OH_AudioStreamBuilder_Destroy(builder_));
        }
    }

    OH_AudioStreamBuilder* get() const { return builder_; }

private:
    OH_AudioStreamBuilder* builder_ = nullptr;
};
}  // namespace

OHAudioPlayerWrapper::OHAudioPlayerWrapper(const AudioParameters& audio_parameters,
    OH_AudioStream_Type direction, OHAudioPlayerObserverInterface* observer)
    : audio_parameters_(audio_parameters), direction_(direction), observer_(observer)
{
    RTC_LOG(LS_INFO) << "ctor";
    RTC_DCHECK(observer_);
    ohaudio_thread_checker_.Detach();
    RTC_LOG(LS_INFO) << audio_parameters_.ToString();
}

OHAudioPlayerWrapper::~OHAudioPlayerWrapper()
{
    RTC_LOG(LS_INFO) << "dtor";
    RTC_DCHECK(thread_checker_.IsCurrent());
    RTC_DCHECK(!stream_);
}

bool OHAudioPlayerWrapper::Init()
{
    RTC_LOG(LS_INFO) << "Init";
    RTC_DCHECK(thread_checker_.IsCurrent());
    ScopedStreamBuilder builder(AUDIOSTREAM_TYPE_RENDERER);
    SetStreamConfiguration(builder.get());
    if (!OpenStream(builder.get())) {
        return false;
    }
    if (!VerifyStreamConfiguration()) {
        return false;
    }
    if (!OptimizeBuffers()) {
        return false;
    }
    LogStreamState();
    return true;
}

bool OHAudioPlayerWrapper::Start()
{
    RTC_LOG(LS_INFO) << "Start";
    RTC_DCHECK(thread_checker_.IsCurrent());
    OH_AudioStream_State current_state;
    OH_AudioRenderer_GetCurrentState(stream_, &current_state);
    if (current_state != AUDIOSTREAM_STATE_PREPARED) {
        RTC_LOG(LS_ERROR) << "Invalid state: "
                          << StateToString(current_state);
        return false;
    }
    RETURN_ON_ERROR(OH_AudioRenderer_Start(stream_), false);
    LogStreamState();
    return true;
}

bool OHAudioPlayerWrapper::Stop()
{
    RTC_LOG(LS_INFO) << "Stop: " << DirectionToString(direction());
    RTC_DCHECK(thread_checker_.IsCurrent());
    RETURN_ON_ERROR(OH_AudioRenderer_Stop(stream_), false);
    CloseStream();
    ohaudio_thread_checker_.Detach();
    return true;
}

double OHAudioPlayerWrapper::EstimateLatencyMillis() const
{
    RTC_DCHECK(stream_);
    double latency_millis = 0.0;
    static const int latency_time = 20;
    if (direction() == AUDIOSTREAM_TYPE_CAPTURER) {
        latency_millis = latency_time;
    } else {
        int64_t existing_frame_index;
        int64_t existing_frame_presentation_time;
        OH_AudioStream_Result result = OH_AudioRenderer_GetTimestamp(
            stream_, CLOCK_MONOTONIC, &existing_frame_index,
            &existing_frame_presentation_time);
        if (result == AUDIOSTREAM_SUCCESS) {
            int64_t next_frame_index = frames_written();
            int64_t frame_index_delta = next_frame_index - existing_frame_index;
            int64_t next_frame_write_time = rtc::TimeNanos();
            int64_t frame_time_delta = (frame_index_delta * rtc::kNumNanosecsPerSec) / sample_rate();
            int64_t next_frame_presentation_time = existing_frame_presentation_time + frame_time_delta;
            latency_millis = static_cast<double>(next_frame_presentation_time -
                                                 next_frame_write_time) /
                                                 rtc::kNumNanosecsPerMillisec;
        }
    }
    return latency_millis;
}

bool OHAudioPlayerWrapper::IncreaseOutputBufferSize()
{
    RTC_LOG(LS_INFO) << "IncreaseBufferSize";
    RTC_DCHECK(stream_);
    RTC_DCHECK(ohaudio_thread_checker_.IsCurrent());
    RTC_DCHECK_EQ(direction(), AUDIOSTREAM_TYPE_RENDERER);

    return true;
}

void OHAudioPlayerWrapper::ClearInputStream(void* audio_data, int32_t num_frames)
{
    RTC_LOG(LS_INFO) << "ClearInputStream";
    RTC_DCHECK(stream_);
    RTC_DCHECK(ohaudio_thread_checker_.IsCurrent());
    RTC_DCHECK_EQ(direction(), AUDIOSTREAM_TYPE_CAPTURER);
}

OHAudioPlayerObserverInterface* OHAudioPlayerWrapper::observer() const
{
    return observer_;
}

AudioParameters OHAudioPlayerWrapper::audio_parameters() const
{
    return audio_parameters_;
}

int32_t OHAudioPlayerWrapper::samples_per_frame() const
{
    RTC_DCHECK(stream_);
    int32_t channel_count;
    OH_AudioRenderer_GetChannelCount(stream_, &channel_count);
    return channel_count;
}

int32_t OHAudioPlayerWrapper::device_id() const
{
    RTC_DCHECK(stream_);
    return 0;
}

int32_t OHAudioPlayerWrapper::xrun_count() const
{
    RTC_DCHECK(stream_);
    return 0;
}

OH_AudioStream_SampleFormat OHAudioPlayerWrapper::format() const
{
    RTC_DCHECK(stream_);
    OH_AudioStream_SampleFormat sampleFormat;
    OH_AudioRenderer_GetSampleFormat(stream_, &sampleFormat);
    return sampleFormat;
}

int32_t OHAudioPlayerWrapper::sample_rate() const
{
    RTC_DCHECK(stream_);
    int32_t rate;
    OH_AudioRenderer_GetSamplingRate(stream_, &rate);
    return rate;
}

int32_t OHAudioPlayerWrapper::channel_count() const
{
    RTC_DCHECK(stream_);
    int32_t channelCount;
    OH_AudioRenderer_GetChannelCount(stream_, &channelCount);
    return channelCount;
}

OH_AudioStream_LatencyMode OHAudioPlayerWrapper::performance_mode() const
{
    RTC_DCHECK(stream_);
    OH_AudioStream_LatencyMode latencyMode;
    OH_AudioRenderer_GetLatencyMode(stream_, &latencyMode);
    return latencyMode;
}

OH_AudioStream_State OHAudioPlayerWrapper::stream_state() const
{
    RTC_DCHECK(stream_);
    OH_AudioStream_State current_state;
    OH_AudioRenderer_GetCurrentState(stream_, &current_state);
    return current_state;
}

int64_t OHAudioPlayerWrapper::frames_written() const
{
    RTC_DCHECK(stream_);
    int64_t frames;
    OH_AudioRenderer_GetFramesWritten(stream_, &frames);
    return frames;
}

int64_t OHAudioPlayerWrapper::frames_read() const
{
    RTC_DCHECK(stream_);
    return 0;
}

void OHAudioPlayerWrapper::SetStreamConfiguration(OH_AudioStreamBuilder* builder)
{
    RTC_LOG(LS_INFO) << "SetStreamConfiguration";
    RTC_DCHECK(builder);
    RTC_DCHECK(thread_checker_.IsCurrent());
    // Set renderer info to AUDIOSTREAM_USAGE_VOICE_COMMUNICATION seems more reasonable, or set to
    // AUDIOSTREAM_USAGE_VIDEO_COMMUNICATION if you prefer to use the speaker as default output device.
    // See also OH_AudioRenderer_SetDefaultOutputDevice.
    OH_AudioStreamBuilder_SetRendererInfo(builder, AUDIOSTREAM_USAGE_VOICE_COMMUNICATION);
    OH_AudioStreamBuilder_SetSamplingRate(builder, audio_parameters().sample_rate());
    OH_AudioStreamBuilder_SetChannelCount(builder, audio_parameters().channels());
    OH_AudioStreamBuilder_SetSampleFormat(builder, AUDIOSTREAM_SAMPLE_S16LE);
    OH_AudioStreamBuilder_SetLatencyMode(builder, (OH_AudioStream_LatencyMode)AUDIOSTREAM_LATENCY_MODE_FAST);
    OH_AudioRenderer_Callbacks callbacks;
    callbacks.OH_AudioRenderer_OnWriteData = DataCallback;
    callbacks.OH_AudioRenderer_OnError = ErrorCallback;
    callbacks.OH_AudioRenderer_OnInterruptEvent = nullptr;
    callbacks.OH_AudioRenderer_OnStreamEvent = nullptr;
    OH_AudioStreamBuilder_SetRendererCallback(builder, callbacks, this);
}

bool OHAudioPlayerWrapper::OpenStream(OH_AudioStreamBuilder* builder)
{
    RTC_LOG(LS_INFO) << "OpenStream";
    RTC_DCHECK(builder);
    OH_AudioRenderer* stream = nullptr;
    RETURN_ON_ERROR(OH_AudioStreamBuilder_GenerateRenderer(builder, &stream), false);
    stream_ = stream;
    LogStreamConfiguration();
    return true;
}

void OHAudioPlayerWrapper::CloseStream()
{
    RTC_LOG(LS_INFO) << "CloseStream";
    RTC_DCHECK(stream_);
    LOG_ON_ERROR(OH_AudioRenderer_Release(stream_));
    stream_ = nullptr;
}

void OHAudioPlayerWrapper::LogStreamConfiguration()
{
    RTC_DCHECK(stream_);
    char ss_buf[1024];
    rtc::SimpleStringBuilder ss(ss_buf);
    ss << "Stream Configuration: ";
    ss << "sample rate=" << sample_rate() << ", channels=" << channel_count();
    ss << ", format=" << FormatToString(format());
    ss << ", performance mode=" << PerformanceModeToString(performance_mode());
    ss << ", direction=" << DirectionToString(direction());
    RTC_LOG(LS_INFO) << ss.str();
}

void OHAudioPlayerWrapper::LogStreamState()
{
    RTC_LOG(LS_INFO) << "OHAudio stream state: "
                     << StateToString(stream_state());
}

bool OHAudioPlayerWrapper::VerifyStreamConfiguration()
{
    RTC_LOG(LS_INFO) << "VerifyStreamConfiguration";
    RTC_DCHECK(stream_);
    int32_t rate;
    OH_AudioRenderer_GetSamplingRate(stream_, &rate);
    if (rate != audio_parameters().sample_rate()) {
        RTC_LOG(LS_ERROR) << "Stream unable to use requested sample rate";
        return false;
    }
    int32_t channelCount;
    OH_AudioRenderer_GetChannelCount(stream_, &channelCount);
    if (channelCount != static_cast<int32_t>(audio_parameters().channels())) {
        RTC_LOG(LS_ERROR) << "Stream unable to use requested channel count";
        return false;
    }

    OH_AudioStream_SampleFormat sampleFormat;
    OH_AudioRenderer_GetSampleFormat(stream_, &sampleFormat);
    if (sampleFormat != AUDIOSTREAM_SAMPLE_S16LE) {
        RTC_LOG(LS_ERROR) << "Stream unable to use requested format";
        return false;
    }
    return true;
}

bool OHAudioPlayerWrapper::OptimizeBuffers()
{
    RTC_LOG(LS_INFO) << "OptimizeBuffers";
    RTC_DCHECK(stream_);
    return true;
}
}  // namespace webrtc
