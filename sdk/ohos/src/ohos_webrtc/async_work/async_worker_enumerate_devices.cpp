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

#include "async_worker_enumerate_devices.h"

#include "rtc_base/logging.h"

#include <algorithm>
#include <cstdint>

namespace webrtc {

using namespace Napi;

namespace {

// Real hardware capability ranges from profiles collected during enumeration;
// empty Function when the device reported no profiles
Napi::Function MakeCameraCapabilitiesFunction(Napi::Env env, const CameraDeviceInfo& device)
{
    int32_t minWidth = INT32_MAX;
    int32_t maxWidth = 0;
    int32_t minHeight = INT32_MAX;
    int32_t maxHeight = 0;
    for (const auto& profile : device.profiles) {
        auto profileWidth = static_cast<int32_t>(profile.resolution.width);
        auto profileHeight = static_cast<int32_t>(profile.resolution.height);
        minWidth = std::min(minWidth, profileWidth);
        maxWidth = std::max(maxWidth, profileWidth);
        minHeight = std::min(minHeight, profileHeight);
        maxHeight = std::max(maxHeight, profileHeight);
    }
    if (maxWidth <= 0 || maxHeight <= 0) {
        return Napi::Function();
    }

    uint32_t minFps = device.frameRateRange.min;
    uint32_t maxFps = device.frameRateRange.max;
    auto makeRange = [](Env rangeEnv, double min, double max) {
        auto range = Object::New(rangeEnv);
        range.Set("min", Number::New(rangeEnv, min));
        range.Set("max", Number::New(rangeEnv, max));
        return range;
    };
    return Function::New(
        env, [minWidth, maxWidth, minHeight, maxHeight, minFps, maxFps, makeRange](const CallbackInfo& info) {
            auto capabilities = Object::New(info.Env());
            capabilities.Set("width", makeRange(info.Env(), minWidth, maxWidth));
            capabilities.Set("height", makeRange(info.Env(), minHeight, maxHeight));
            if (minFps > 0 || maxFps > 0) {
                capabilities.Set("frameRate", makeRange(info.Env(), minFps, maxFps));
            }
            return capabilities;
        });
}

} // namespace

AsyncWorkerEnumerateDevices* AsyncWorkerEnumerateDevices::Create(Napi::Env env, const char* resourceName)
{
    auto asyncWorker = new AsyncWorkerEnumerateDevices(env, resourceName);
    return asyncWorker;
}

AsyncWorkerEnumerateDevices::AsyncWorkerEnumerateDevices(Napi::Env env, const char* resourceName)
    : AsyncWorker(env, resourceName), deferred_(Napi::Promise::Deferred::New(env))
{
}

void AsyncWorkerEnumerateDevices::Execute()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    cameraDevices_ = CameraEnumerator::GetDevices();
    audioDevices_ = AudioDeviceEnumerator::GetDevices();
}

void AsyncWorkerEnumerateDevices::OnOK()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto result = Array::New(Env(), cameraDevices_.size() + audioDevices_.size());
    FillCameraDevices(result);
    FillAudioDevices(result);

    deferred_.Resolve(result);
}

void AsyncWorkerEnumerateDevices::FillCameraDevices(Napi::Array& result)
{
    for (uint32_t index = 0; index < cameraDevices_.size(); index++) {
        auto& device = cameraDevices_[index];
        auto obj = Object::New(Env());
        obj.Set("deviceId", String::New(Env(), device.deviceId));
        obj.Set("groupId", String::New(Env(), device.groupId));
        obj.Set("label", String::New(Env(), device.label));
        obj.Set("kind", String::New(Env(), "videoinput"));
        auto capabilities = MakeCameraCapabilitiesFunction(Env(), device);
        if (!capabilities.IsEmpty()) {
            obj.Set("getCapabilities", capabilities);
        }
        result.Set(index, obj);
    }
}

void AsyncWorkerEnumerateDevices::FillAudioDevices(Napi::Array& result)
{
    for (uint32_t index = 0; index < audioDevices_.size(); index++) {
        auto obj = Object::New(Env());
        obj.Set("deviceId", String::New(Env(), audioDevices_[index].deviceId));
        obj.Set("groupId", String::New(Env(), audioDevices_[index].groupId));
        obj.Set("label", String::New(Env(), audioDevices_[index].label));
        if (audioDevices_[index].role == AudioDeviceRole::Input) {
            obj.Set("kind", String::New(Env(), "audioinput"));
        } else if (audioDevices_[index].role == AudioDeviceRole::Output) {
            obj.Set("kind", String::New(Env(), "audiooutput"));
        } else {
            RTC_LOG(LS_WARNING) << "Invalid audio device role";
        }
        result.Set(cameraDevices_.size() + index, obj);
    }
}

void AsyncWorkerEnumerateDevices::OnError(const Napi::Error& e)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    deferred_.Reject(e.Value());
}

} // namespace webrtc
