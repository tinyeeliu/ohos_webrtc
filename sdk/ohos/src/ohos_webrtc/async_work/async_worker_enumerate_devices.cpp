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

#include <algorithm>

#include "rtc_base/logging.h"

namespace webrtc {

namespace {

// {min,max} range object shared by width/height/frameRate.
Napi::Object MakeRange(Napi::Env env, double min, double max)
{
    auto range = Napi::Object::New(env);
    range.Set("min", Napi::Number::New(env, min));
    range.Set("max", Napi::Number::New(env, max));
    return range;
}

// Aggregates all camera preview profiles of a device into
// MediaTrackCapabilities-style ranges (#4). frameRate follows the raw range
// reported by the enumerator; {0, UINT_MAX} means "unrestricted".
Napi::Object MakeVideoInputCapabilities(Napi::Env env, const std::vector<video::VideoProfile>& profiles)
{
    uint32_t minWidth = 0;
    uint32_t maxWidth = 0;
    uint32_t minHeight = 0;
    uint32_t maxHeight = 0;
    uint32_t minFps = 0;
    uint32_t maxFps = 0;
    bool first = true;
    for (const auto& profile : profiles) {
        if (first) {
            minWidth = profile.resolution.width;
            maxWidth = profile.resolution.width;
            minHeight = profile.resolution.height;
            maxHeight = profile.resolution.height;
            minFps = profile.frameRateRange.min;
            maxFps = profile.frameRateRange.max;
            first = false;
            continue;
        }
        minWidth = std::min(minWidth, profile.resolution.width);
        maxWidth = std::max(maxWidth, profile.resolution.width);
        minHeight = std::min(minHeight, profile.resolution.height);
        maxHeight = std::max(maxHeight, profile.resolution.height);
        minFps = std::min(minFps, profile.frameRateRange.min);
        maxFps = std::max(maxFps, profile.frameRateRange.max);
    }

    auto caps = Napi::Object::New(env);
    caps.Set("width", MakeRange(env, minWidth, maxWidth));
    caps.Set("height", MakeRange(env, minHeight, maxHeight));
    caps.Set("frameRate", MakeRange(env, minFps, maxFps));
    return caps;
}

// Audio devices expose no capability data source on OHOS yet; both input and
// output report that honestly instead of an empty capability object.
Napi::Value AudioCapabilitiesNotAvailable(const Napi::CallbackInfo& info)
{
    NAPI_THROW(
        Napi::Error::New(info.Env(), "Not implemented: audio device capabilities are not available"),
        info.Env().Undefined());
}

} // namespace

using namespace Napi;

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

    for (uint32_t index = 0; index < cameraDevices_.size(); index++) {
        auto obj = Object::New(Env());
        obj.Set("deviceId", String::New(Env(), cameraDevices_[index].deviceId));
        obj.Set("groupId", String::New(Env(), cameraDevices_[index].groupId));
        obj.Set("label", String::New(Env(), cameraDevices_[index].label));
        obj.Set("kind", String::New(Env(), "videoinput"));
        obj.Set("getCapabilities", Function::New(
            Env(),
            [capabilities = Napi::ObjectReference::New(MakeVideoInputCapabilities(
                Env(), cameraDevices_[index].profiles))](const CallbackInfo& info) {
                return capabilities.Value();
            }));
        result.Set(index, obj);
    }

    for (uint32_t index = 0; index < audioDevices_.size(); index++) {
        auto obj = Object::New(Env());
        obj.Set("deviceId", String::New(Env(), audioDevices_[index].deviceId));
        obj.Set("groupId", String::New(Env(), audioDevices_[index].groupId));
        obj.Set("label", String::New(Env(), audioDevices_[index].label));
        if (audioDevices_[index].role == AudioDeviceRole::Input) {
            obj.Set("kind", String::New(Env(), "audioinput"));
            obj.Set("getCapabilities", Function::New(Env(), AudioCapabilitiesNotAvailable));
        } else if (audioDevices_[index].role == AudioDeviceRole::Output) {
            obj.Set("kind", String::New(Env(), "audiooutput"));
            obj.Set("getCapabilities", Function::New(Env(), AudioCapabilitiesNotAvailable));
        } else {
            RTC_LOG(LS_WARNING) << "Invalid audio device role";
        }
        result.Set(cameraDevices_.size() + index, obj);
    }

    deferred_.Resolve(result);
}

void AsyncWorkerEnumerateDevices::OnError(const Napi::Error& e)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    deferred_.Reject(e.Value());
}

} // namespace webrtc
