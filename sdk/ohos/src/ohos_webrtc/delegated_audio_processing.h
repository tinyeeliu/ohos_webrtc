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

#ifndef WEBRTC_DELEGATED_AUDIO_PROCESSING_H
#define WEBRTC_DELEGATED_AUDIO_PROCESSING_H

#include "modules/audio_processing/include/audio_processing.h"

#include <functional>
#include <vector>

namespace rtc {
template <typename T>
class RefCountedObject;
} // namespace rtc

namespace webrtc {

// AudioProcessing wrapper delegating to an internal (built-in) APM, handing
// ~10ms interleaved float frames to JS slots before/after the internal chain.
// Slots run on the JS thread via tsfn; the audio thread waits bounded time for
// the rewritten frame and falls back to the original data on timeout.
class DelegatedAudioProcessing : public AudioProcessing {
public:
    // data: interleaved float samples in [-1, 1], numFrames * channels values.
    // Return 0 when the data was rewritten in place, non-zero to keep original.
    using Slot = std::function<int(float* data, int sampleRate, int channels, size_t numFrames)>;

    static rtc::scoped_refptr<DelegatedAudioProcessing> Create(Slot captureSlot, Slot renderSlot);

    ~DelegatedAudioProcessing() override;

    // AudioProcessing implementation
    int Initialize() override;
    int Initialize(const ProcessingConfig& processing_config) override;
    void ApplyConfig(const AudioProcessing::Config& config) override;
    int proc_sample_rate_hz() const override;
    int proc_split_sample_rate_hz() const override;
    size_t num_input_channels() const override;
    size_t num_proc_channels() const override;
    size_t num_output_channels() const override;
    size_t num_reverse_channels() const override;
    void set_output_will_be_muted(bool muted) override;
    void SetRuntimeSetting(RuntimeSetting setting) override;
    bool PostRuntimeSetting(RuntimeSetting setting) override;
    int ProcessStream(const int16_t* const src,
                      const StreamConfig& input_config,
                      const StreamConfig& output_config,
                      int16_t* const dest) override;
    int ProcessStream(const float* const* src,
                      const StreamConfig& input_config,
                      const StreamConfig& output_config,
                      float* const* dest) override;
    int ProcessReverseStream(const int16_t* const src,
                             const StreamConfig& input_config,
                             const StreamConfig& output_config,
                             int16_t* const dest) override;
    int ProcessReverseStream(const float* const* src,
                             const StreamConfig& input_config,
                             const StreamConfig& output_config,
                             float* const* dest) override;
    int AnalyzeReverseStream(const float* const* data, const StreamConfig& reverse_config) override;
    bool GetLinearAecOutput(rtc::ArrayView<std::array<float, 160>> linear_output) const override;
    void set_stream_analog_level(int level) override;
    int recommended_stream_analog_level() const override;
    int set_stream_delay_ms(int delay) override;
    int stream_delay_ms() const override;
    void set_stream_key_pressed(bool key_pressed) override;
    bool CreateAndAttachAecDump(absl::string_view file_name,
                                int64_t max_log_size_bytes,
                                rtc::TaskQueue* worker_queue) override;
    bool CreateAndAttachAecDump(FILE* handle, int64_t max_log_size_bytes, rtc::TaskQueue* worker_queue) override;
    void AttachAecDump(std::unique_ptr<AecDump> aec_dump) override;
    void DetachAecDump() override;
    AudioProcessingStats GetStatistics() override;
    AudioProcessingStats GetStatistics(bool has_remote_tracks) override;
    AudioProcessing::Config GetConfig() const override;

private:
    friend class rtc::RefCountedObject<DelegatedAudioProcessing>;
    DelegatedAudioProcessing(rtc::scoped_refptr<AudioProcessing> internal, Slot captureSlot, Slot renderSlot);

    // Run the JS slot over an interleaved int16 frame; keeps original data on failure
    void RunCaptureSlot(const int16_t* src, const StreamConfig& config, int16_t* dest);
    void RunRenderSlot(const int16_t* src, const StreamConfig& config, int16_t* dest);
    void RunCaptureSlotFloat(const float* const* src, const StreamConfig& config, float* const* dest);
    void RunRenderSlotFloat(const float* const* src, const StreamConfig& config, float* const* dest);

    rtc::scoped_refptr<AudioProcessing> internal_;
    Slot captureSlot_;
    Slot renderSlot_;

    // Reused per-call scratch buffers; Process(Stream|ReverseStream) each run on
    // their own single thread
    std::vector<float> captureScratch_;
    std::vector<float> renderScratch_;
    std::vector<float> captureFloatScratch_;
    std::vector<float> renderFloatScratch_;
};

} // namespace webrtc

#endif // WEBRTC_DELEGATED_AUDIO_PROCESSING_H
