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

#ifndef WEBRTC_MEDIA_CONSTRAINTS_UTIL_H
#define WEBRTC_MEDIA_CONSTRAINTS_UTIL_H

#include "media_constraints.h"
#include "../camera/camera_device_info.h"

#include <string>

#include "api/audio_options.h"
#include "rtc_base/checks.h"

namespace webrtc {

struct CameraCaptureSettings {
    std::string deviceId;
    video::VideoProfile profile;
};

bool SelectSettingsForVideo(
    const std::vector<CameraDeviceInfo>& devices, const MediaTrackConstraints& constraints, int default_width,
    int default_height, double default_frame_rate, CameraCaptureSettings& setting, std::string& failed_constraint_name);

// Copy all relevant constraints into an AudioOptions object.
void CopyConstraintsIntoAudioOptions(const MediaTrackConstraints& constraints, cricket::AudioOptions& options);

void GetScreenCaptureConstraints(const MediaTrackConstraints& constraints, int& width, int& height);

} // namespace webrtc

#endif // WEBRTC_MEDIA_CONSTRAINTS_UTIL_H
