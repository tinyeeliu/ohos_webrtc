/**
 * Copyright (c) 2026 Archermind Technology (Nanjing) Co. Ltd.
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

#include "delegated_audio_processing.h"

#include "modules/audio_processing/include/aec_dump.h"
#include "rtc_base/ref_counted_object.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace webrtc {

namespace {

constexpr float kInt16Scale = 32768.0f;

inline int16_t FloatToInt16(float sample)
{
    return static_cast<int16_t>(std::clamp(sample, -1.0f, 1.0f) * kInt16Scale);
}

} // namespace

rtc::scoped_refptr<DelegatedAudioProcessing> DelegatedAudioProcessing::Create(Slot captureSlot, Slot renderSlot)
{
    auto internal = AudioProcessingBuilder().Create();
    if (!internal) {
        return nullptr;
    }
    return rtc::scoped_refptr<DelegatedAudioProcessing>(
        new rtc::RefCountedObject<DelegatedAudioProcessing>(
            internal, std::move(captureSlot), std::move(renderSlot)));
}

DelegatedAudioProcessing::DelegatedAudioProcessing(
    rtc::scoped_refptr<AudioProcessing> internal, Slot captureSlot, Slot renderSlot)
    : internal_(std::move(internal)), captureSlot_(std::move(captureSlot)), renderSlot_(std::move(renderSlot))
{
}

DelegatedAudioProcessing::~DelegatedAudioProcessing()
{
    // internal_ scoped_refptr releases the built-in APM; slots are released by
    // their shared_ptr holders
}

int DelegatedAudioProcessing::Initialize()
{
    return internal_->Initialize();
}

int DelegatedAudioProcessing::Initialize(const ProcessingConfig& processing_config)
{
    return internal_->Initialize(processing_config);
}

void DelegatedAudioProcessing::ApplyConfig(const AudioProcessing::Config& config)
{
    internal_->ApplyConfig(config);
}

int DelegatedAudioProcessing::proc_sample_rate_hz() const
{
    return internal_->proc_sample_rate_hz();
}

int DelegatedAudioProcessing::proc_split_sample_rate_hz() const
{
    return internal_->proc_split_sample_rate_hz();
}

size_t DelegatedAudioProcessing::num_input_channels() const
{
    return internal_->num_input_channels();
}

size_t DelegatedAudioProcessing::num_proc_channels() const
{
    return internal_->num_proc_channels();
}

size_t DelegatedAudioProcessing::num_output_channels() const
{
    return internal_->num_output_channels();
}

size_t DelegatedAudioProcessing::num_reverse_channels() const
{
    return internal_->num_reverse_channels();
}

void DelegatedAudioProcessing::set_output_will_be_muted(bool muted)
{
    internal_->set_output_will_be_muted(muted);
}

void DelegatedAudioProcessing::SetRuntimeSetting(RuntimeSetting setting)
{
    internal_->SetRuntimeSetting(setting);
}

bool DelegatedAudioProcessing::PostRuntimeSetting(RuntimeSetting setting)
{
    return internal_->PostRuntimeSetting(setting);
}

void DelegatedAudioProcessing::RunCaptureSlot(const int16_t* src, const StreamConfig& config, int16_t* dest)
{
    if (!captureSlot_) {
        return;
    }

    const size_t numFrames = static_cast<size_t>(config.sample_rate_hz() / 100);
    const size_t channels = config.num_channels();
    const size_t total = numFrames * channels;
    if (total == 0) {
        return;
    }

    captureScratch_.resize(total);
    for (size_t i = 0; i < total; i++) {
        captureScratch_[i] = src[i] / kInt16Scale;
    }

    if (captureSlot_(captureScratch_.data(), config.sample_rate_hz(), static_cast<int>(channels), numFrames) == 0) {
        for (size_t i = 0; i < total; i++) {
            dest[i] = FloatToInt16(captureScratch_[i]);
        }
    } else if (dest != src) {
        memcpy(dest, src, total * sizeof(int16_t));
    }
}

void DelegatedAudioProcessing::RunRenderSlot(const int16_t* src, const StreamConfig& config, int16_t* dest)
{
    if (!renderSlot_) {
        return;
    }

    const size_t numFrames = static_cast<size_t>(config.sample_rate_hz() / 100);
    const size_t channels = config.num_channels();
    const size_t total = numFrames * channels;
    if (total == 0) {
        return;
    }

    renderScratch_.resize(total);
    for (size_t i = 0; i < total; i++) {
        renderScratch_[i] = src[i] / kInt16Scale;
    }

    if (renderSlot_(renderScratch_.data(), config.sample_rate_hz(), static_cast<int>(channels), numFrames) == 0) {
        for (size_t i = 0; i < total; i++) {
            dest[i] = FloatToInt16(renderScratch_[i]);
        }
    } else if (dest != src) {
        memcpy(dest, src, total * sizeof(int16_t));
    }
}

void DelegatedAudioProcessing::RunCaptureSlotFloat(
    const float* const* src, const StreamConfig& config, float* const* dest)
{
    if (!captureSlot_) {
        return;
    }

    const size_t numFrames = static_cast<size_t>(config.sample_rate_hz() / 100);
    const size_t channels = config.num_channels();
    const size_t total = numFrames * channels;
    if (total == 0) {
        return;
    }

    captureFloatScratch_.resize(total);
    for (size_t ch = 0; ch < channels; ch++) {
        for (size_t i = 0; i < numFrames; i++) {
            captureFloatScratch_[i * channels + ch] = src[ch][i];
        }
    }

    if (captureSlot_(captureFloatScratch_.data(), config.sample_rate_hz(), static_cast<int>(channels), numFrames) ==
        0) {
        for (size_t ch = 0; ch < channels; ch++) {
            for (size_t i = 0; i < numFrames; i++) {
                dest[ch][i] = captureFloatScratch_[i * channels + ch];
            }
        }
    } else {
        for (size_t ch = 0; ch < channels; ch++) {
            if (dest[ch] != src[ch]) {
                memcpy(dest[ch], src[ch], numFrames * sizeof(float));
            }
        }
    }
}

void DelegatedAudioProcessing::RunRenderSlotFloat(
    const float* const* src, const StreamConfig& config, float* const* dest)
{
    if (!renderSlot_) {
        return;
    }

    const size_t numFrames = static_cast<size_t>(config.sample_rate_hz() / 100);
    const size_t channels = config.num_channels();
    const size_t total = numFrames * channels;
    if (total == 0) {
        return;
    }

    renderFloatScratch_.resize(total);
    for (size_t ch = 0; ch < channels; ch++) {
        for (size_t i = 0; i < numFrames; i++) {
            renderFloatScratch_[i * channels + ch] = src[ch][i];
        }
    }

    if (renderSlot_(renderFloatScratch_.data(), config.sample_rate_hz(), static_cast<int>(channels), numFrames) ==
        0) {
        for (size_t ch = 0; ch < channels; ch++) {
            for (size_t i = 0; i < numFrames; i++) {
                dest[ch][i] = renderFloatScratch_[i * channels + ch];
            }
        }
    } else {
        for (size_t ch = 0; ch < channels; ch++) {
            if (dest[ch] != src[ch]) {
                memcpy(dest[ch], src[ch], numFrames * sizeof(float));
            }
        }
    }
}

int DelegatedAudioProcessing::ProcessStream(const int16_t* const src,
    const StreamConfig& input_config, const StreamConfig& output_config, int16_t* const dest)
{
    RunCaptureSlot(src, input_config, dest);
    return internal_->ProcessStream(dest, input_config, output_config, dest);
}

int DelegatedAudioProcessing::ProcessStream(const float* const* src,
    const StreamConfig& input_config, const StreamConfig& output_config, float* const* dest)
{
    RunCaptureSlotFloat(src, input_config, dest);
    return internal_->ProcessStream(dest, input_config, output_config, dest);
}

int DelegatedAudioProcessing::ProcessReverseStream(const int16_t* const src,
    const StreamConfig& input_config, const StreamConfig& output_config, int16_t* const dest)
{
    RunRenderSlot(src, input_config, dest);
    return internal_->ProcessReverseStream(dest, input_config, output_config, dest);
}

int DelegatedAudioProcessing::ProcessReverseStream(const float* const* src,
    const StreamConfig& input_config, const StreamConfig& output_config, float* const* dest)
{
    RunRenderSlotFloat(src, input_config, dest);
    return internal_->ProcessReverseStream(dest, input_config, output_config, dest);
}

int DelegatedAudioProcessing::AnalyzeReverseStream(const float* const* data, const StreamConfig& reverse_config)
{
    return internal_->AnalyzeReverseStream(data, reverse_config);
}

bool DelegatedAudioProcessing::GetLinearAecOutput(rtc::ArrayView<std::array<float, 160>> linear_output) const
{
    return internal_->GetLinearAecOutput(linear_output);
}

void DelegatedAudioProcessing::set_stream_analog_level(int level)
{
    internal_->set_stream_analog_level(level);
}

int DelegatedAudioProcessing::recommended_stream_analog_level() const
{
    return internal_->recommended_stream_analog_level();
}

int DelegatedAudioProcessing::set_stream_delay_ms(int delay)
{
    return internal_->set_stream_delay_ms(delay);
}

int DelegatedAudioProcessing::stream_delay_ms() const
{
    return internal_->stream_delay_ms();
}

void DelegatedAudioProcessing::set_stream_key_pressed(bool key_pressed)
{
    internal_->set_stream_key_pressed(key_pressed);
}

bool DelegatedAudioProcessing::CreateAndAttachAecDump(
    absl::string_view file_name, int64_t max_log_size_bytes, rtc::TaskQueue* worker_queue)
{
    return internal_->CreateAndAttachAecDump(file_name, max_log_size_bytes, worker_queue);
}

bool DelegatedAudioProcessing::CreateAndAttachAecDump(
    FILE* handle, int64_t max_log_size_bytes, rtc::TaskQueue* worker_queue)
{
    return internal_->CreateAndAttachAecDump(handle, max_log_size_bytes, worker_queue);
}

void DelegatedAudioProcessing::AttachAecDump(std::unique_ptr<AecDump> aec_dump)
{
    internal_->AttachAecDump(std::move(aec_dump));
}

void DelegatedAudioProcessing::DetachAecDump()
{
    internal_->DetachAecDump();
}

AudioProcessingStats DelegatedAudioProcessing::GetStatistics()
{
    return internal_->GetStatistics();
}

AudioProcessingStats DelegatedAudioProcessing::GetStatistics(bool has_remote_tracks)
{
    return internal_->GetStatistics(has_remote_tracks);
}

AudioProcessing::Config DelegatedAudioProcessing::GetConfig() const
{
    return internal_->GetConfig();
}

} // namespace webrtc
