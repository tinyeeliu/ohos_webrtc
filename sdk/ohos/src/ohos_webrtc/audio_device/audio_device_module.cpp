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

#include "audio_device_module.h"
#include "audio_common.h"
#include "audio_capturer.h"
#include "audio_renderer.h"

#include <memory>
#include <utility>

#include "api/sequence_checker.h"
#include "api/make_ref_counted.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "rtc_base/logging.h"

namespace webrtc {

using namespace Napi;

const char kClassName[] = "AudioDeviceModule";

const char kAttributeNameOnCapturerError[] = "oncapturererror";
const char kAttributeNameOnCapturerStateChange[] = "oncapturerstatechange";
const char kAttributeNameOnCapturerSamplesReady[] = "oncapturersamplesready";
const char kAttributeNameOnRendererError[] = "onrenderererror";
const char kAttributeNameOnRendererStateChange[] = "onrendererstatechange";

const char kMethodNameSetSpeakerMute[] = "setSpeakerMute";
const char kMethodNameSetMicrophoneMute[] = "setMicrophoneMute";
const char kMethodNameSetNoiseSuppressorEnabled[] = "setNoiseSuppressorEnabled";
const char kMethodNameIsBuiltInAcousticEchoCancelerSupported[] = "isBuiltInAcousticEchoCancelerSupported";
const char kMethodNameIsBuiltInNoiseSuppressorSupported[] = "isBuiltInNoiseSuppressorSupported";
const char kMethodNameToJson[] = "toJSON";

const char kEventNameCapturerError[] = "capturererror";
const char kEventNameCapturerStateChange[] = "capturerstatechange";
const char kEventNameCapturerSamplesReady[] = "capturersamplesready";
const char kEventNameRendererError[] = "renderererror";
const char kEventNameRendererStateChange[] = "rendererstatechange";

const char kAttributeNameAudioSource[] = "audioSource";
const char kAttributeNameAudioFormat[] = "audioFormat";
const char kAttributeNameInputSampleRate[] = "inputSampleRate";
const char kAttributeNameUseStereoInput[] = "useStereoInput";
const char kAttributeNameOutputSampleRate[] = "outputSampleRate";
const char kAttributeNameUseStereoOutput[] = "useStereoOutput";
const char kAttributeNameRendererUsage[] = "rendererUsage";
const char kAttributeNameUseLowLatency[] = "useLowLatency";
const char kAttributeNameUseHardwareAcousticEchoCanceler[] = "useHardwareAcousticEchoCanceler";
const char kAttributeNameUseHardwareNoiseSuppressor[] = "useHardwareNoiseSuppressor";

const char kEnumAudioErrorTypeInit[] = "init";
const char kEnumAudioErrorTypeStartException[] = "start-exception";
const char kEnumAudioErrorTypeStartStateMismatch[] = "start-state-mismatch";
const char kEnumAudioErrorTypeGeneral[] = "general";

const char kEnumAudioStateStart[] = "start";
const char kEnumAudioStateStop[] = "stop";

const char* AudioErrorTypeToString(AudioErrorType type)
{
    switch (type) {
        case AudioErrorType::INIT:
            return kEnumAudioErrorTypeInit;
        case AudioErrorType::START_EXCEPTION:
            return kEnumAudioErrorTypeStartException;
        case AudioErrorType::START_STATE_MISMATCH:
            return kEnumAudioErrorTypeStartStateMismatch;
        case AudioErrorType::GENERAL:
            return kEnumAudioErrorTypeGeneral;
        default:
            break;
    }

    return "unknown";
}

const char* AudioStateToString(AudioStateType state)
{
    switch (state) {
        case AudioStateType::START:
            return kEnumAudioStateStart;
        case AudioStateType::STOP:
            return kEnumAudioStateStop;
        default:
            break;
    }

    return "unknown";
}

class OhosAudioDeviceModule : public AudioDeviceModule {
public:
    enum class InitStatus { OK = 0, PLAYOUT_ERROR = 1, RECORDING_ERROR = 2, OTHER_ERROR = 3, NUM_STATUSES = 4 };

    OhosAudioDeviceModule(
        AudioDeviceModule::AudioLayer audioLayer, bool isStereoPlayoutSupported, bool isStereoRecordSupported,
        std::unique_ptr<AudioInput> audioInput, std::unique_ptr<AudioOutput> audioOutput)
        : audioLayer_(audioLayer), isStereoPlayoutSupported_(isStereoPlayoutSupported),
          isStereoRecordSupported_(isStereoRecordSupported), taskQueueFactory_(CreateDefaultTaskQueueFactory()),
          input_(std::move(audioInput)), output_(std::move(audioOutput)), initialized_(false)
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        RTC_DCHECK(input_);
        RTC_DCHECK(output_);

        threadChecker_.Detach();
    }

    ~OhosAudioDeviceModule() override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    }

    int32_t ActiveAudioLayer(AudioDeviceModule::AudioLayer* audioLayer) const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        *audioLayer = audioLayer_;
        return 0;
    }

    int32_t RegisterAudioCallback(AudioTransport* audioCallback) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return audioDeviceBuffer_->RegisterAudioCallback(audioCallback);
    }

    int32_t Init() override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        RTC_DCHECK(threadChecker_.IsCurrent());

        if (initialized_) {
            return 0;
        }

        audioDeviceBuffer_ = std::make_unique<AudioDeviceBuffer>(taskQueueFactory_.get());
        AttachAudioBuffer();

        InitStatus status;
        if (output_->Init() != 0) {
            status = InitStatus::PLAYOUT_ERROR;
        } else if (input_->Init() != 0) {
            output_->Terminate();
            status = InitStatus::RECORDING_ERROR;
        } else {
            initialized_ = true;
            status = InitStatus::OK;
        }

        if (status != InitStatus::OK) {
            RTC_LOG(LS_ERROR) << "Audio device initialization failed.";
            return -1;
        }

        return 0;
    }

    int32_t Terminate() override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        if (!initialized_) {
            return 0;
        }

        RTC_DCHECK(threadChecker_.IsCurrent());

        int32_t err = input_->Terminate();
        err |= output_->Terminate();
        if (err != 0) {
            RTC_LOG(LS_ERROR) << "error: " << err;
        }

        initialized_ = false;
        threadChecker_.Detach();
        audioDeviceBuffer_.reset(nullptr);

        return err;
    }

    bool Initialized() const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << ":" << initialized_;

        return initialized_;
    }

    int16_t PlayoutDevices() override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return 1;
    }

    int16_t RecordingDevices() override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return 1;
    }

    int32_t PlayoutDeviceName(uint16_t index, char name[kAdmMaxDeviceNameSize], char guid[kAdmMaxGuidSize]) override
    {
        (void)index;
        (void)name;
        (void)guid;
        RTC_CHECK_NOTREACHED();
    }

    int32_t RecordingDeviceName(uint16_t index, char name[kAdmMaxDeviceNameSize], char guid[kAdmMaxGuidSize]) override
    {
        (void)index;
        (void)name;
        (void)guid;
        RTC_CHECK_NOTREACHED();
    }

    int32_t SetPlayoutDevice(uint16_t index) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << "(" << index << ")";

        // no effect
        return 0;
    }

    int32_t SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType device) override
    {
        (void)device;
        RTC_CHECK_NOTREACHED();
    }

    int32_t SetRecordingDevice(uint16_t index) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << "(" << index << ")";

        // no effect
        return 0;
    }

    int32_t SetRecordingDevice(AudioDeviceModule::WindowsDeviceType device) override
    {
        (void)device;
        RTC_CHECK_NOTREACHED();
    }

    int32_t PlayoutIsAvailable(bool* available) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        *available = true;
        return 0;
    }

    int32_t InitPlayout() override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
        if (!initialized_) {
            return -1;
        }

        if (PlayoutIsInitialized()) {
            return 0;
        }

        int32_t result = output_->InitPlayout();
        RTC_DLOG(LS_VERBOSE) << "output: " << result;

        return result;
    }

    bool PlayoutIsInitialized() const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return output_->PlayoutIsInitialized();
    }

    int32_t RecordingIsAvailable(bool* available) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        *available = true;
        return 0;
    }

    int32_t InitRecording() override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        if (!initialized_) {
            return -1;
        }

        if (RecordingIsInitialized()) {
            return 0;
        }

        int32_t result = input_->InitRecording();
        RTC_DLOG(LS_VERBOSE) << "output: " << result;

        return result;
    }

    bool RecordingIsInitialized() const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return input_->RecordingIsInitialized();
    }

    int32_t StartPlayout() override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        if (!initialized_) {
            return -1;
        }

        if (Playing()) {
            return 0;
        }

        int32_t result = output_->StartPlayout();
        RTC_DLOG(LS_VERBOSE) << "output: " << result;

        if (result == 0) {
            // Only start playing the audio device buffer if starting the audio output succeeded.
            audioDeviceBuffer_->StartPlayout();
        }

        return result;
    }

    int32_t StopPlayout() override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        if (!initialized_) {
            return -1;
        }

        if (!Playing()) {
            return 0;
        }

        audioDeviceBuffer_->StopPlayout();
        int32_t result = output_->StopPlayout();
        RTC_DLOG(LS_VERBOSE) << "output: " << result;

        return result;
    }

    bool Playing() const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return output_->Playing();
    }

    int32_t StartRecording() override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        if (!initialized_) {
            return -1;
        }

        if (Recording()) {
            return 0;
        }

        int32_t result = input_->StartRecording();
        RTC_DLOG(LS_VERBOSE) << "output: " << result;

        if (result == 0) {
            // Only start recording the audio device buffer if starting the audio input succeeded.
            audioDeviceBuffer_->StartRecording();
        }

        return result;
    }

    int32_t StopRecording() override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        if (!initialized_) {
            return -1;
        }

        if (!Recording()) {
            return 0;
        }

        audioDeviceBuffer_->StopRecording();
        int32_t result = input_->StopRecording();
        RTC_DLOG(LS_VERBOSE) << "output: " << result;

        return result;
    }

    bool Recording() const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return input_->Recording();
    }

    int32_t InitSpeaker() override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return initialized_ ? 0 : -1;
    }

    bool SpeakerIsInitialized() const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return initialized_;
    }

    int32_t InitMicrophone() override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return initialized_ ? 0 : -1;
    }

    bool MicrophoneIsInitialized() const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return initialized_;
    }

    int32_t SpeakerVolumeIsAvailable(bool* available) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        *available = false;
        return 0;
    }

    int32_t SetSpeakerVolume(uint32_t volume) override
    {
        (void)volume;
        RTC_CHECK_NOTREACHED();
    }

    int32_t SpeakerVolume(uint32_t* output_volume) const override
    {
        (void)output_volume;
        RTC_CHECK_NOTREACHED();
    }

    int32_t MaxSpeakerVolume(uint32_t* output_max_volume) const override
    {
        (void)output_max_volume;
        RTC_CHECK_NOTREACHED();
    }

    int32_t MinSpeakerVolume(uint32_t* output_min_volume) const override
    {
        (void)output_min_volume;
        RTC_CHECK_NOTREACHED();
    }

    int32_t MicrophoneVolumeIsAvailable(bool* available) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        *available = false;
        return -1;
    }

    int32_t SetMicrophoneVolume(uint32_t volume) override
    {
        (void)volume;
        RTC_CHECK_NOTREACHED();
    }

    int32_t MicrophoneVolume(uint32_t* volume) const override
    {
        (void)volume;
        RTC_CHECK_NOTREACHED();
    }

    int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const override
    {
        (void)maxVolume;
        RTC_CHECK_NOTREACHED();
    }

    int32_t MinMicrophoneVolume(uint32_t* minVolume) const override
    {
        (void)minVolume;
        RTC_CHECK_NOTREACHED();
    }

    int32_t SpeakerMuteIsAvailable(bool* available) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return output_->SpeakerMuteIsAvailable(available);
    }

    int32_t SetSpeakerMute(bool enable) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << "(" << enable << ")";

        return output_->setSpeakerMute(enable);
    }

    int32_t SpeakerMute(bool* enabled) const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return output_->SpeakerMute(enabled);
    }

    int32_t MicrophoneMuteIsAvailable(bool* available) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return input_->MicrophoneMuteIsAvailable(available);
    }

    int32_t SetMicrophoneMute(bool enable) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << "(" << enable << ")";

        return input_->SetMicrophoneMute(enable);
    }

    int32_t MicrophoneMute(bool* enabled) const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return input_->MicrophoneMute(enabled);
    }

    int32_t StereoPlayoutIsAvailable(bool* available) const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        *available = isStereoPlayoutSupported_;
        RTC_DLOG(LS_VERBOSE) << "output: " << *available;

        return 0;
    }

    int32_t SetStereoPlayout(bool enable) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << "(" << enable << ")";

        bool available = isStereoPlayoutSupported_;
        if (enable != available) {
            RTC_LOG(LS_WARNING) << "changing stereo playout not supported";
            return -1;
        }

        return 0;
    }

    int32_t StereoPlayout(bool* enabled) const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        *enabled = isStereoPlayoutSupported_;
        RTC_DLOG(LS_VERBOSE) << "output: " << *enabled;

        return 0;
    }

    int32_t StereoRecordingIsAvailable(bool* available) const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        *available = isStereoRecordSupported_;
        RTC_DLOG(LS_VERBOSE) << "output: " << *available;

        return 0;
    }

    int32_t SetStereoRecording(bool enable) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << "(" << enable << ")";

        bool available = isStereoRecordSupported_;
        if (enable != available) {
            RTC_LOG(LS_WARNING) << "changing stereo recording not supported";
            return -1;
        }

        return 0;
    }

    int32_t StereoRecording(bool* enabled) const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        *enabled = isStereoRecordSupported_;
        RTC_DLOG(LS_VERBOSE) << "output: " << *enabled;

        return 0;
    }

    int32_t PlayoutDelay(uint16_t* delayMs) const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return output_->PlayoutDelay(delayMs);
    }

    bool BuiltInAECIsAvailable() const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return false;
    }

    bool BuiltInAGCIsAvailable() const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return false;
    }

    bool BuiltInNSIsAvailable() const override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        return false;
    }

    int32_t EnableBuiltInAEC(bool enable) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << "(" << enable << ")";
        RTC_LOG_F(LS_ERROR) << "Not supported on this platform";

        return -1;
    }

    int32_t EnableBuiltInAGC(bool enable) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << "(" << enable << ")";
        RTC_LOG_F(LS_ERROR) << "Not supported on this platform";

        return -1;
    }

    int32_t EnableBuiltInNS(bool enable) override
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << "(" << enable << ")";
        RTC_LOG_F(LS_ERROR) << "Not supported on this platform";

        return -1;
    }

    int32_t GetPlayoutUnderrunCount() const override
    {
        if (!initialized_) {
            return -1;
        }

        return output_->GetPlayoutUnderrunCount();
    }

    absl::optional<Stats> GetStats() const override
    {
        if (!initialized_) {
            return absl::nullopt;
        }

        return output_->GetStats();
    }

    int32_t AttachAudioBuffer()
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        output_->AttachAudioBuffer(audioDeviceBuffer_.get());
        input_->AttachAudioBuffer(audioDeviceBuffer_.get());

        return 0;
    }

private:
    SequenceChecker threadChecker_;

    const AudioDeviceModule::AudioLayer audioLayer_;
    const bool isStereoPlayoutSupported_;
    const bool isStereoRecordSupported_;
    const std::unique_ptr<TaskQueueFactory> taskQueueFactory_;
    const std::unique_ptr<AudioInput> input_;
    const std::unique_ptr<AudioOutput> output_;
    std::unique_ptr<AudioDeviceBuffer> audioDeviceBuffer_;

    bool initialized_;
};

FunctionReference NapiAudioDeviceModule::constructor_;

void NapiAudioDeviceModule::Init(Napi::Env env, Napi::Object exports)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Function func = DefineClass(
        env, kClassName,
        {
            InstanceAccessor<&NapiAudioDeviceModule::GetEventHandler, &NapiAudioDeviceModule::SetEventHandler>(
                kAttributeNameOnCapturerError, napi_default, (void*)kEventNameCapturerError),
            InstanceAccessor<&NapiAudioDeviceModule::GetEventHandler, &NapiAudioDeviceModule::SetEventHandler>(
                kAttributeNameOnCapturerStateChange, napi_default, (void*)kEventNameCapturerStateChange),
            InstanceAccessor<&NapiAudioDeviceModule::GetEventHandler, &NapiAudioDeviceModule::SetEventHandler>(
                kAttributeNameOnCapturerSamplesReady, napi_default, (void*)kEventNameCapturerSamplesReady),
            InstanceAccessor<&NapiAudioDeviceModule::GetEventHandler, &NapiAudioDeviceModule::SetEventHandler>(
                kAttributeNameOnRendererError, napi_default, (void*)kEventNameRendererError),
            InstanceAccessor<&NapiAudioDeviceModule::GetEventHandler, &NapiAudioDeviceModule::SetEventHandler>(
                kAttributeNameOnRendererStateChange, napi_default, (void*)kEventNameRendererStateChange),
            InstanceMethod<&NapiAudioDeviceModule::SetSpeakerMute>(kMethodNameSetSpeakerMute),
            InstanceMethod<&NapiAudioDeviceModule::SetMicrophoneMute>(kMethodNameSetMicrophoneMute),
            InstanceMethod<&NapiAudioDeviceModule::SetNoiseSuppressorEnabled>(kMethodNameSetNoiseSuppressorEnabled),
            InstanceMethod<&NapiAudioDeviceModule::ToJson>(kMethodNameToJson),
            StaticMethod<&NapiAudioDeviceModule::isBuiltInAcousticEchoCancelerSupported>(
                kMethodNameIsBuiltInAcousticEchoCancelerSupported),
            StaticMethod<&NapiAudioDeviceModule::isBuiltInNoiseSuppressorSupported>(
                kMethodNameIsBuiltInNoiseSuppressorSupported),
        });
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
}

NapiAudioDeviceModule::NapiAudioDeviceModule(const Napi::CallbackInfo& info) : ObjectWrap<NapiAudioDeviceModule>(info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    int32_t audioSource = AUDIOSTREAM_SOURCE_TYPE_VOICE_COMMUNICATION;
    int32_t audioFormat = AUDIOSTREAM_SAMPLE_S16LE;
    int32_t inputSampleRate = 48000;
    int32_t outputSampleRate = 48000;
    int32_t rendererUsage = AUDIOSTREAM_USAGE_VOICE_COMMUNICATION;
    bool useStereoInput = false;
    bool useStereoOutput = false;
    bool useLowLatency = false;
    bool useHardwareAcousticEchoCanceler = false;
    bool useHardwareNoiseSuppressor = false;

    if (info.Length() > 0) {
        auto options = ((Napi::Value)info[0]).As<Object>();
        if (options.Has(kAttributeNameAudioSource)) {
            audioSource = options.Get(kAttributeNameAudioSource).As<Number>().Int32Value();
        }
        if (options.Has(kAttributeNameAudioFormat)) {
            audioFormat = options.Get(kAttributeNameAudioFormat).As<Number>().Int32Value();
        }
        if (options.Has(kAttributeNameInputSampleRate)) {
            inputSampleRate = options.Get(kAttributeNameInputSampleRate).As<Number>().Int32Value();
        }
        if (options.Has(kAttributeNameUseStereoInput)) {
            useStereoInput = options.Get(kAttributeNameUseStereoInput).As<Boolean>().Value();
        }
        if (options.Has(kAttributeNameOutputSampleRate)) {
            outputSampleRate = options.Get(kAttributeNameOutputSampleRate).As<Number>().Int32Value();
        }
        if (options.Has(kAttributeNameUseStereoOutput)) {
            useStereoOutput = options.Get(kAttributeNameUseStereoOutput).As<Boolean>().Value();
        }
        if (options.Has(kAttributeNameRendererUsage)) {
            rendererUsage = options.Get(kAttributeNameRendererUsage).As<Number>().Int32Value();
        }
        if (options.Has(kAttributeNameUseLowLatency)) {
            useLowLatency = options.Get(kAttributeNameUseLowLatency).As<Boolean>().Value();
        }
        if (options.Has(kAttributeNameUseHardwareAcousticEchoCanceler)) {
            useHardwareAcousticEchoCanceler =
                options.Get(kAttributeNameUseHardwareAcousticEchoCanceler).As<Boolean>().Value();
        }
        if (options.Has(kAttributeNameUseHardwareNoiseSuppressor)) {
            useHardwareNoiseSuppressor = options.Get(kAttributeNameUseHardwareNoiseSuppressor).As<Boolean>().Value();
        }
    }

    AudioParameters output_parameters;
    output_parameters.reset(outputSampleRate, 1);

    auto audioInput = AudioCapturer::Create(audioSource, audioFormat, inputSampleRate, useStereoInput, useLowLatency);
    audioInput->RegisterObserver(this);

    auto audioOutput = AudioRenderer::Create(rendererUsage, outputSampleRate, useStereoOutput, useLowLatency);
    audioOutput->RegisterObserver(this);

    adm_ = rtc::make_ref_counted<OhosAudioDeviceModule>(
        AudioDeviceModule::kPlatformDefaultAudio, useStereoOutput, useStereoInput, std::move(audioInput),
        std::move(audioOutput));
    if (!adm_) {
        NAPI_THROW_VOID(Error::New(info.Env(), "failed to create audio device module"));
    }
}

NapiAudioDeviceModule::~NapiAudioDeviceModule()
{
    RTC_DLOG(LS_INFO) << __FUNCTION__;

    for (auto& handler : eventHandlers_) {
        handler.second.tsfn.Release();
    }
}

Napi::Value NapiAudioDeviceModule::GetEventHandler(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    const auto type = (const char*)info.Data();

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = eventHandlers_.find(type);
    if (it != eventHandlers_.end()) {
        return it->second.ref.Value();
    }

    return info.Env().Null();
}

void NapiAudioDeviceModule::SetEventHandler(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!value.IsFunction()) {
        NAPI_THROW_VOID(TypeError::New(info.Env(), "First argument is not Function"));
    }

    const auto type = (const char*)info.Data();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = eventHandlers_.find(type);
        if (it != eventHandlers_.end()) {
            it->second.tsfn.Release();
            eventHandlers_.erase(it);
        }
    }

    Function cb = value.As<Function>();

    Reference<Napi::Value>* context = new Reference<Napi::Value>;
    *context = Persistent(info.This());

    EventHandler handler;
    handler.ref = Persistent(cb);
    handler.tsfn = ThreadSafeFunction::New(
        info.Env(), cb, type, 0, 1, context, [](Napi::Env env, Reference<Napi::Value>* ctx) {
            (void)env;
            ctx->Reset();
            delete ctx;
        });

    std::lock_guard<std::mutex> lock(mutex_);
    eventHandlers_[type] = std::move(handler);
}

Napi::Value NapiAudioDeviceModule::SetSpeakerMute(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    adm_->SetSpeakerMute(info[0].As<Boolean>().Value());
    return info.Env().Undefined();
}

Napi::Value NapiAudioDeviceModule::SetMicrophoneMute(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    adm_->SetMicrophoneMute(info[0].As<Boolean>().Value());
    return info.Env().Undefined();
}

Napi::Value NapiAudioDeviceModule::SetNoiseSuppressorEnabled(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    // Noise suppressor is not supported.
    return Boolean::New(info.Env(), false);
}

Napi::Value NapiAudioDeviceModule::ToJson(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto result = Object::New(info.Env());
#ifndef NDEBUG
    result.Set("__native_class__", String::New(info.Env(), "NapiAudioDeviceModule"));
#endif

    return result;
}

Napi::Value NapiAudioDeviceModule::isBuiltInAcousticEchoCancelerSupported(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    return Boolean::New(info.Env(), false);
}

Napi::Value NapiAudioDeviceModule::isBuiltInNoiseSuppressorSupported(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    return Boolean::New(info.Env(), false);
}

void NapiAudioDeviceModule::OnAudioInputError(AudioErrorType type, const std::string& message)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " error=" << type << ", " << message;

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = eventHandlers_.find(kEventNameCapturerError);
    if (it == eventHandlers_.end()) {
        return;
    }

    auto& tsfn = it->second.tsfn;
    Reference<Napi::Value>* context = tsfn.GetContext();
    napi_status status = tsfn.NonBlockingCall([context, type, message](Napi::Env env, Napi::Function jsCallback) {
        RTC_DLOG(LS_VERBOSE) << "error=" << type << ", " << message;
        auto jsError = Object::New(env);
        jsError.Set("type", String::New(env, AudioErrorTypeToString(type)));
        jsError.Set("message", String::New(env, message));

        auto jsEvent = Object::New(env);
        jsEvent.Set("error", jsError);
        jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
    });
    if (status != napi_ok) {
        RTC_LOG(LS_ERROR) << " tsfn call error: " << status;
    }
}

void NapiAudioDeviceModule::OnAudioInputStateChange(AudioStateType newState)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = eventHandlers_.find(kEventNameCapturerStateChange);
    if (it == eventHandlers_.end()) {
        return;
    }

    auto& tsfn = it->second.tsfn;
    Reference<Napi::Value>* context = tsfn.GetContext();
    napi_status status = tsfn.NonBlockingCall([context, newState](Napi::Env env, Napi::Function jsCallback) {
        auto jsEvent = Object::New(env);
        jsEvent.Set("state", String::New(env, AudioStateToString(newState)));
        jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
    });
    if (status != napi_ok) {
        RTC_LOG(LS_ERROR) << " tsfn call error: " << status;
    }
}

void NapiAudioDeviceModule::OnAudioInputSamplesReady(
    int32_t sampleRate, int32_t format, int32_t channelCount, rtc::CopyOnWriteBuffer* data)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = eventHandlers_.find(kEventNameCapturerSamplesReady);
    if (it == eventHandlers_.end()) {
        return;
    }

    auto& tsfn = it->second.tsfn;
    Reference<Napi::Value>* context = tsfn.GetContext();
    napi_status status = tsfn.NonBlockingCall(
        [context, sampleRate, format, channelCount, data](Napi::Env env, Napi::Function jsCallback) {
            auto arrayBuffer = ArrayBuffer::New(
                env, static_cast<void*>(data->MutableData()), data->size(),
                [](Napi::Env /*env*/, void* /*data*/, rtc::CopyOnWriteBuffer* hint) {
                    RTC_DLOG(LS_VERBOSE) << "release rtc::CopyOnWriteBuffer";
                    delete hint;
                },
                data);

            auto jsAudioSamples = Object::New(env);
            jsAudioSamples.Set("sampleRate", Number::New(env, sampleRate));
            jsAudioSamples.Set("audioFormat", Number::New(env, format));
            jsAudioSamples.Set("channelCount", Number::New(env, channelCount));
            jsAudioSamples.Set("data", arrayBuffer);

            auto jsEvent = Object::New(env);
            jsEvent.Set("samples", jsAudioSamples);
            jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
        });
    if (status != napi_ok) {
        RTC_LOG(LS_ERROR) << " tsfn call error: " << status;
    }
}

void NapiAudioDeviceModule::OnAudioOutputError(AudioErrorType type, const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = eventHandlers_.find(kEventNameRendererError);
    if (it == eventHandlers_.end()) {
        return;
    }

    auto& tsfn = it->second.tsfn;
    Reference<Napi::Value>* context = tsfn.GetContext();
    napi_status status = tsfn.NonBlockingCall([context, type, message](Napi::Env env, Napi::Function jsCallback) {
        auto jsError = Object::New(env);
        jsError.Set("type", String::New(env, AudioErrorTypeToString(type)));
        jsError.Set("message", String::New(env, message));

        auto jsEvent = Object::New(env);
        jsEvent.Set("error", jsError);
        jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
    });
    if (status != napi_ok) {
        RTC_LOG(LS_ERROR) << " tsfn call error: " << status;
    }
}

void NapiAudioDeviceModule::OnAudioOutputStateChange(AudioStateType newState)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = eventHandlers_.find(kEventNameRendererStateChange);
    if (it == eventHandlers_.end()) {
        return;
    }

    auto& tsfn = it->second.tsfn;
    Reference<Napi::Value>* context = tsfn.GetContext();
    napi_status status = tsfn.NonBlockingCall([context, newState](Napi::Env env, Napi::Function jsCallback) {
        auto jsEvent = Object::New(env);
        jsEvent.Set("state", String::New(env, AudioStateToString(newState)));
        jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
    });
    if (status != napi_ok) {
        RTC_LOG(LS_ERROR) << " tsfn call error: " << status;
    }
}

} // namespace webrtc
