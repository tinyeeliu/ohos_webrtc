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

#include "display_change_listener.h"
#include <atomic>
#include "rtc_base/logging.h"
#include <map>

namespace {
using namespace webrtc;
const std::map<int32_t, std::map<NativeDisplayManager_Orientation, VideoRotation>> rotationMap = {
    {VideoRotation::kVideoRotation_270, {
        {DISPLAY_MANAGER_PORTRAIT, VideoRotation::kVideoRotation_270},
        {DISPLAY_MANAGER_LANDSCAPE, VideoRotation::kVideoRotation_180},
        {DISPLAY_MANAGER_PORTRAIT_INVERTED, VideoRotation::kVideoRotation_90},
        {DISPLAY_MANAGER_LANDSCAPE_INVERTED, VideoRotation::kVideoRotation_0}
    }},
    {VideoRotation::kVideoRotation_180, {
        {DISPLAY_MANAGER_PORTRAIT, VideoRotation::kVideoRotation_180},
        {DISPLAY_MANAGER_LANDSCAPE, VideoRotation::kVideoRotation_270},
        {DISPLAY_MANAGER_PORTRAIT_INVERTED, VideoRotation::kVideoRotation_0},
        {DISPLAY_MANAGER_LANDSCAPE_INVERTED, VideoRotation::kVideoRotation_90}
    }},
    {VideoRotation::kVideoRotation_90, {
        {DISPLAY_MANAGER_PORTRAIT, VideoRotation::kVideoRotation_90},
        {DISPLAY_MANAGER_LANDSCAPE, VideoRotation::kVideoRotation_0},
        {DISPLAY_MANAGER_PORTRAIT_INVERTED, VideoRotation::kVideoRotation_270},
        {DISPLAY_MANAGER_LANDSCAPE_INVERTED, VideoRotation::kVideoRotation_180}
    }},
    {VideoRotation::kVideoRotation_0, {
        {DISPLAY_MANAGER_PORTRAIT, VideoRotation::kVideoRotation_0},
        {DISPLAY_MANAGER_LANDSCAPE, VideoRotation::kVideoRotation_90},
        {DISPLAY_MANAGER_PORTRAIT_INVERTED, VideoRotation::kVideoRotation_180},
        {DISPLAY_MANAGER_LANDSCAPE_INVERTED, VideoRotation::kVideoRotation_270}
    }}
};

std::atomic<VideoRotation> kRealRotation = VideoRotation::kVideoRotation_0;
std::atomic<VideoRotation> kCameraCaptureRotation = VideoRotation::kVideoRotation_0;
uint32_t kDisplayChangeListenerID = 0;

void OnDisplayChange(uint64_t displayId)
{
    NativeDisplayManager_Orientation displayOrientation;
    OH_NativeDisplayManager_GetDefaultDisplayOrientation(&displayOrientation);
    RTC_DLOG(LS_INFO) << __FUNCTION__ << " " << displayId << ", " << displayOrientation;
    auto cameraIt = rotationMap.find(kCameraCaptureRotation);
    if (cameraIt != rotationMap.end()) {
        auto displayIt = cameraIt->second.find(displayOrientation);
        if (displayIt != cameraIt->second.end()) {
            kRealRotation = displayIt->second;
        }
    }
}
}

namespace ohos {
DisplayChangeListener::DisplayChangeListener()
{
    OnDisplayChange(0);
    OH_NativeDisplayManager_RegisterDisplayChangeListener(OnDisplayChange, &kDisplayChangeListenerID);
}

DisplayChangeListener::~DisplayChangeListener()
{
    OH_NativeDisplayManager_UnregisterDisplayChangeListener(kDisplayChangeListenerID);
}

VideoRotation DisplayChangeListener::GetRealRotation(const VideoRotation& cameraCaptureRotation)
{
    if (kCameraCaptureRotation == cameraCaptureRotation) {
        return kRealRotation;
    }

    kCameraCaptureRotation = cameraCaptureRotation;
    OnDisplayChange(0);
    return kRealRotation;
}
}

