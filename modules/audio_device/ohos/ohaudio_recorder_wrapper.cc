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

#include "modules/audio_device/ohos/ohaudio_recorder_wrapper.h"

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

static int32_t ErrorCallback(OH_AudioCapturer* stream, void* user_data, OH_AudioStream_Result error)
{
    RTC_DCHECK(user_data);
    OHAudioRecorderWrapper* ohaudio_wrapper = reinterpret_cast<OHAudioRecorderWrapper*>(user_data);
    RTC_LOG(LS_WARNING) << "ErrorCallback: "
                        << DirectionToString(ohaudio_wrapper->direction());
    RTC_DCHECK(ohaudio_wrapper->observer());
    return ohaudio_wrapper->observer()->OnErrorCallback(error);
}

static int32_t DataCallback(OH_AudioCapturer* stream, void* user_data, void* audio_data, int32_t bufferLen)
{
    RTC_DCHECK(user_data);
    RTC_DCHECK(audio_data);
    RTC_LOG(LS_INFO) << "bufferLen=" << bufferLen;
    OHAudioRecorderWrapper* ohaudio_wrapper = reinterpret_cast<OHAudioRecorderWrapper*>(user_data);
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

OHAudioRecorderWrapper::OHAudioRecorderWrapper(const AudioParameters& audio_parameters,
    OH_AudioStream_Type direction, OHAudioRecorderObserverInterface* observer)
    : audio_parameters_(audio_parameters), direction_(direction), observer_(observer)
{
    RTC_LOG(LS_INFO) << "ctor";
    RTC_DCHECK(observer_);
    ohaudio_thread_checker_.Detach();
    RTC_LOG(LS_INFO) << audio_parameters_.ToString();
}

OHAudioRecorderWrapper::~OHAudioRecorderWrapper()
{
    RTC_LOG(LS_INFO) << "dtor";
    RTC_DCHECK(thread_checker_.IsCurrent());
    RTC_DCHECK(!stream_);
}

bool OHAudioRecorderWrapper::Init()
{
    RTC_LOG(LS_INFO) << "Init";
    RTC_DCHECK(thread_checker_.IsCurrent());
    ScopedStreamBuilder builder(AUDIOSTREAM_TYPE_CAPTURER);
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

bool OHAudioRecorderWrapper::Start()
{
    RTC_LOG(LS_INFO) << "Start";
    RTC_DCHECK(thread_checker_.IsCurrent());
    OH_AudioStream_State current_state;
    OH_AudioCapturer_GetCurrentState(stream_, &current_state);
    if (current_state != AUDIOSTREAM_STATE_PREPARED) {
        RTC_LOG(LS_ERROR) << "Invalid state: "
                          << StateToString(current_state);
        return false;
    }
    RETURN_ON_ERROR(OH_AudioCapturer_Start(stream_), false);
    LogStreamState();
    return true;
}

bool OHAudioRecorderWrapper::Stop()
{
    RTC_LOG(LS_INFO) << "Stop: " << DirectionToString(direction());
    RTC_DCHECK(thread_checker_.IsCurrent());
    RETURN_ON_ERROR(OH_AudioCapturer_Stop(stream_), false);
    CloseStream();
    ohaudio_thread_checker_.Detach();
    return true;
}

double OHAudioRecorderWrapper::EstimateLatencyMillis() const
{
    RTC_DCHECK(stream_);
    double latency_millis = 0.0;
    if (direction() == AUDIOSTREAM_TYPE_CAPTURER) {
        latency_millis = static_cast<double>(frames_per_burst()) / sample_rate() *
                         rtc::kNumMillisecsPerSec;
    }
    return latency_millis;
}

bool OHAudioRecorderWrapper::IncreaseOutputBufferSize()
{
    RTC_LOG(LS_INFO) << "IncreaseBufferSize";
    RTC_DCHECK(stream_);
    RTC_DCHECK(ohaudio_thread_checker_.IsCurrent());
    RTC_DCHECK_EQ(direction(), AUDIOSTREAM_TYPE_RENDERER);

    return true;
}

void OHAudioRecorderWrapper::ClearInputStream(void* audio_data, int32_t num_frames)
{
    RTC_LOG(LS_INFO) << "ClearInputStream";
    RTC_DCHECK(stream_);
    RTC_DCHECK(ohaudio_thread_checker_.IsCurrent());
    RTC_DCHECK_EQ(direction(), AUDIOSTREAM_TYPE_CAPTURER);
    OH_AudioCapturer_Flush(stream_);
}

OHAudioRecorderObserverInterface* OHAudioRecorderWrapper::observer() const
{
    return observer_;
}

AudioParameters OHAudioRecorderWrapper::audio_parameters() const
{
    return audio_parameters_;
}

int32_t OHAudioRecorderWrapper::samples_per_frame() const
{
    RTC_DCHECK(stream_);
    int32_t channel_count;
    OH_AudioCapturer_GetChannelCount(stream_, &channel_count);
    return channel_count;
}

int32_t OHAudioRecorderWrapper::device_id() const
{
    RTC_DCHECK(stream_);
    return 0;
}

int32_t OHAudioRecorderWrapper::xrun_count() const
{
    RTC_DCHECK(stream_);
    return 0;
}

OH_AudioStream_SampleFormat OHAudioRecorderWrapper::format() const
{
    RTC_DCHECK(stream_);
    OH_AudioStream_SampleFormat sampleFormat;
    OH_AudioCapturer_GetSampleFormat(stream_, &sampleFormat);
    return sampleFormat;
}

int32_t OHAudioRecorderWrapper::sample_rate() const
{
    RTC_DCHECK(stream_);
    int32_t rate;
    OH_AudioCapturer_GetSamplingRate(stream_, &rate);
    return rate;
}

int32_t OHAudioRecorderWrapper::channel_count() const
{
    RTC_DCHECK(stream_);
    int32_t channelCount;
    OH_AudioCapturer_GetChannelCount(stream_, &channelCount);
    return channelCount;
}

OH_AudioStream_LatencyMode OHAudioRecorderWrapper::performance_mode() const
{
    RTC_DCHECK(stream_);
    OH_AudioStream_LatencyMode latencyMode;
    OH_AudioCapturer_GetLatencyMode(stream_, &latencyMode);
    return latencyMode;
}

OH_AudioStream_State OHAudioRecorderWrapper::stream_state() const
{
    RTC_DCHECK(stream_);
    OH_AudioStream_State current_state;
    OH_AudioCapturer_GetCurrentState(stream_, &current_state);
    return current_state;
}

int64_t OHAudioRecorderWrapper::frames_written() const
{
    RTC_DCHECK(stream_);
    return 0;
}

int64_t OHAudioRecorderWrapper::frames_read() const
{
    RTC_DCHECK(stream_);
    int64_t frames;
    OH_AudioCapturer_GetFramesRead(stream_, &frames);
    return frames;
}

void OHAudioRecorderWrapper::SetStreamConfiguration(OH_AudioStreamBuilder* builder)
{
    RTC_LOG(LS_INFO) << "SetStreamConfiguration";
    RTC_DCHECK(builder);
    RTC_DCHECK(thread_checker_.IsCurrent());
    // Use default capturer info (which is AUDIOSTREAM_SOURCE_TYPE_MIC) seems not work well after input device changed,
    // such as from mic to bluetooth earphone, so use AUDIOSTREAM_SOURCE_TYPE_VOICE_COMMUNICATION instead.
    OH_AudioStreamBuilder_SetCapturerInfo(builder, AUDIOSTREAM_SOURCE_TYPE_VOICE_COMMUNICATION);
    OH_AudioStreamBuilder_SetSamplingRate(builder, audio_parameters().sample_rate());
    OH_AudioStreamBuilder_SetChannelCount(builder, audio_parameters().channels());
    OH_AudioStreamBuilder_SetSampleFormat(builder, AUDIOSTREAM_SAMPLE_S16LE);
    OH_AudioStreamBuilder_SetLatencyMode(builder, AUDIOSTREAM_LATENCY_MODE_NORMAL);

    OH_AudioCapturer_Callbacks callbacks;
    callbacks.OH_AudioCapturer_OnReadData = DataCallback;
    callbacks.OH_AudioCapturer_OnError = ErrorCallback;
    callbacks.OH_AudioCapturer_OnInterruptEvent = nullptr;
    callbacks.OH_AudioCapturer_OnStreamEvent = nullptr;
    OH_AudioStreamBuilder_SetCapturerCallback(builder, callbacks, this);
}

bool OHAudioRecorderWrapper::OpenStream(OH_AudioStreamBuilder* builder)
{
    RTC_LOG(LS_INFO) << "OpenStream";
    RTC_DCHECK(builder);
    OH_AudioCapturer* stream = nullptr;
    RETURN_ON_ERROR(OH_AudioStreamBuilder_GenerateCapturer(builder, &stream), false);
    stream_ = stream;
    LogStreamConfiguration();
    return true;
}

void OHAudioRecorderWrapper::CloseStream()
{
    RTC_LOG(LS_INFO) << "CloseStream";
    RTC_DCHECK(stream_);
    LOG_ON_ERROR(OH_AudioCapturer_Release(stream_));
    stream_ = nullptr;
}

void OHAudioRecorderWrapper::LogStreamConfiguration()
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

void OHAudioRecorderWrapper::LogStreamState()
{
    RTC_LOG(LS_INFO) << "OHAudio stream state: "
                     << StateToString(stream_state());
}

bool OHAudioRecorderWrapper::VerifyStreamConfiguration()
{
    RTC_LOG(LS_INFO) << "VerifyStreamConfiguration";
    RTC_DCHECK(stream_);
    int32_t rate;
    OH_AudioCapturer_GetSamplingRate(stream_, &rate);
    if (rate != audio_parameters().sample_rate()) {
        RTC_LOG(LS_ERROR) << "Stream unable to use requested sample rate";
        return false;
    }

    int32_t channelCount;
    OH_AudioCapturer_GetChannelCount(stream_, &channelCount);
    if (channelCount != static_cast<int32_t>(audio_parameters().channels())) {
        RTC_LOG(LS_ERROR) << "Stream unable to use requested channel count";
        return false;
    }

    OH_AudioStream_SampleFormat sampleFormat;
    OH_AudioCapturer_GetSampleFormat(stream_, &sampleFormat);
    if (sampleFormat != AUDIOSTREAM_SAMPLE_S16LE) {
        RTC_LOG(LS_ERROR) << "Stream unable to use requested format";
        return false;
    }

    return true;
}

bool OHAudioRecorderWrapper::OptimizeBuffers()
{
    RTC_LOG(LS_INFO) << "OptimizeBuffers";
    RTC_DCHECK(stream_);

    return true;
}
}  // namespace webrtc
