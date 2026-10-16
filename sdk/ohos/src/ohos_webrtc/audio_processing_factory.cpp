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

#include "audio_processing_factory.h"
#include "delegated_audio_processing.h"

#include "rtc_base/logging.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

namespace webrtc {

using namespace Napi;

namespace {

constexpr int kSlotWaitTimeoutMs = 8;

inline void CopyFloats(const void* src, void* dest, size_t count)
{
    std::copy(static_cast<const float*>(src), static_cast<const float*>(src) + count, static_cast<float*>(dest));
}

// JS-slot adapter: the audio thread copies a frame in and waits bounded time for
// the JS callback to rewrite it. On timeout/queue failure the original data is
// kept (slot reports failure) so processing degrades but never blocks audio.
class JsAudioSlot : public std::enable_shared_from_this<JsAudioSlot> {
public:
    explicit JsAudioSlot(Napi::Function userCallback)
    {
        tsfn_ = ThreadSafeFunction::New(userCallback.Env(), userCallback, "audioProcessingSlot", 0, 1);
    }

    ~JsAudioSlot()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            released_ = true;
            cv_.notify_all();
        }
        tsfn_.Release();
    }

    int operator()(float* data, int sampleRate, int channels, size_t numFrames)
    {
        const size_t total = numFrames * channels;
        uint64_t seq = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (released_ || total == 0) {
                return -1;
            }
            shared_.assign(data, data + total);
            sampleRate_ = sampleRate;
            channels_ = channels;
            seq = ++seq_;
            doneSeq_ = seq - 1;
        }

        napi_status status = tsfn_.NonBlockingCall(
            [this, self = shared_from_this(), seq](Napi::Env env, Napi::Function jsCallback) {
                RunUserCallback(env, jsCallback, seq);
            });
        if (status != napi_ok) {
            RTC_LOG(LS_WARNING) << "audio processing slot tsfn call failed: " << status;
            return -1;
        }

        std::unique_lock<std::mutex> lock(mutex_);
        bool done = cv_.wait_for(lock, std::chrono::milliseconds(kSlotWaitTimeoutMs), [this, seq] {
            return doneSeq_ >= seq;
        });
        if (!done) {
            RTC_LOG(LS_WARNING) << "audio processing slot timeout, keep original frame";
            return -1;
        }
        CopyFloats(shared_.data(), data, total);
        return 0;
    }

private:
    // Runs on the JS thread: builds the frame object, invokes the user callback
    // (which rewrites data in place) and copies the samples back.
    void RunUserCallback(Napi::Env env, Napi::Function jsCallback, uint64_t seq)
    {
        std::vector<float> local;
        int sampleRate = 0;
        int channels = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (released_ || seq != seq_) {
                return; // superseded by a newer frame or torn down
            }
            local = shared_;
            sampleRate = sampleRate_;
            channels = channels_;
        }

        auto arrayBuffer = ArrayBuffer::New(env, local.size() * sizeof(float));
        CopyFloats(local.data(), arrayBuffer.Data(), local.size());
        auto frame = Object::New(env);
        frame.Set("sampleRate", Number::New(env, sampleRate));
        frame.Set("channels", Number::New(env, channels));
        frame.Set("data", Float32Array::New(env, local.size(), arrayBuffer, 0));

        jsCallback.Call({frame});

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!released_ && seq == seq_) {
                CopyFloats(arrayBuffer.Data(), shared_.data(), shared_.size());
                doneSeq_ = seq;
            }
            cv_.notify_all();
        }
    }

    // Frame metadata of the in-flight frame; guarded by mutex_
    int sampleRate_ = 0;
    int channels_ = 0;

    ThreadSafeFunction tsfn_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<float> shared_;
    uint64_t seq_ = 0;
    uint64_t doneSeq_ = 0;
    bool released_ = false;
};

} // namespace

thread_local FunctionReference NapiAudioProcessing::constructor_;

void NapiAudioProcessing::Init(Napi::Env env, Napi::Object exports)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Function func = DefineClass(
        env, kClassName,
        {
            InstanceMethod<&NapiAudioProcessing::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
    constructor_.SuppressDestruct();
}

Napi::Object NapiAudioProcessing::NewInstance(Napi::Env env, rtc::scoped_refptr<AudioProcessing> audioProcessing)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (!audioProcessing) {
        NAPI_THROW(Error::New(env, "Invalid argument"), Object());
    }

    auto external = External<AudioProcessing>::New(
        env, audioProcessing.release(), [](Napi::Env /*env*/, AudioProcessing* apm) { apm->Release(); });

    return constructor_.New({external});
}

NapiAudioProcessing::NapiAudioProcessing(const Napi::CallbackInfo& info) : ObjectWrap<NapiAudioProcessing>(info)
{
    if (info.Length() == 0 || !info[0].IsExternal()) {
        NAPI_THROW_VOID(Error::New(info.Env(), "Invalid argument"));
    }

    audioProcessing_ = info[0].As<External<AudioProcessing>>().Data();
}

rtc::scoped_refptr<AudioProcessing> NapiAudioProcessing::Get() const
{
    return audioProcessing_;
}

Napi::Value NapiAudioProcessing::ToJson(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", "NapiAudioProcessing");
#endif

    return json;
}

thread_local FunctionReference NapiAudioProcessingFactory::constructor_;

void NapiAudioProcessingFactory::Init(Napi::Env env, Napi::Object exports)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Function func = DefineClass(
        env, kClassName,
        {
            InstanceMethod<&NapiAudioProcessingFactory::Create>(kMethodNameCreate),
            InstanceMethod<&NapiAudioProcessingFactory::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
    constructor_.SuppressDestruct();
}

NapiAudioProcessingFactory::NapiAudioProcessingFactory(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<NapiAudioProcessingFactory>(info)
{
}

namespace {

Napi::Object MakeBuiltinAudioProcessing(Napi::Env env)
{
    AudioProcessing::Config apmConfig;
    return NapiAudioProcessing::NewInstance(env, AudioProcessingBuilder().SetConfig(apmConfig).Create());
}

// Parses an optional JS processing slot; empty Function when absent or null.
// Throws TypeError when present but not a function (fail-fast contract).
Napi::Function ParseProcessingSlot(const Napi::CallbackInfo& info, const Napi::Object& jsOptions, const char* name)
{
    if (!jsOptions.Has(name)) {
        return Napi::Function();
    }
    auto value = jsOptions.Get(name);
    if (value.IsNull() || value.IsUndefined()) {
        return Napi::Function();
    }
    if (!value.IsFunction()) {
        std::string message = std::string(name) + " must be a function";
        NAPI_THROW(TypeError::New(info.Env(), message.c_str()), Napi::Function());
    }
    return value.As<Napi::Function>();
}

// Wraps a JS slot into the C++ slot signature expected by DelegatedAudioProcessing
DelegatedAudioProcessing::Slot MakeDelegatedSlot(Napi::Function jsSlot)
{
    auto adapter = std::make_shared<JsAudioSlot>(jsSlot);
    return [adapter](float* data, int sampleRate, int channels, size_t numFrames) {
        return (*adapter)(data, sampleRate, channels, numFrames);
    };
}

} // namespace

Napi::Value NapiAudioProcessingFactory::Create(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    auto env = info.Env();

    // create(): built-in APM. create(options): delegated APM with JS slots.
    bool hasOptions = info.Length() > 0 && !info[0].IsNull() && !info[0].IsUndefined();
    if (!hasOptions) {
        return MakeBuiltinAudioProcessing(env);
    }

    if (!info[0].IsObject()) {
        NAPI_THROW(TypeError::New(env, "The first argument must be an AudioProcessingOptions object"),
                   env.Undefined());
    }

    auto jsOptions = info[0].As<Object>();
    auto jsCaptureSlot = ParseProcessingSlot(info, jsOptions, "capturePostProcessing");
    auto jsRenderSlot = ParseProcessingSlot(info, jsOptions, "renderPreProcessing");
    if (!jsCaptureSlot && !jsRenderSlot) {
        return MakeBuiltinAudioProcessing(env);
    }

    DelegatedAudioProcessing::Slot captureSlot;
    if (jsCaptureSlot) {
        captureSlot = MakeDelegatedSlot(jsCaptureSlot);
    }
    DelegatedAudioProcessing::Slot renderSlot;
    if (jsRenderSlot) {
        renderSlot = MakeDelegatedSlot(jsRenderSlot);
    }

    auto delegated = DelegatedAudioProcessing::Create(std::move(captureSlot), std::move(renderSlot));
    if (!delegated) {
        NAPI_THROW(Error::New(env, "Failed to create the audio processing instance"), env.Undefined());
    }

    return NapiAudioProcessing::NewInstance(env, delegated);
}

Napi::Value NapiAudioProcessingFactory::ToJson(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", "NapiAudioProcessingFactory");
#endif

    return json;
}

} // namespace webrtc
