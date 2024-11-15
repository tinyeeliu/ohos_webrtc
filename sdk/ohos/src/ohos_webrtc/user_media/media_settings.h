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

#ifndef WEBRTC_MEDIA_SETTINGS_H
#define WEBRTC_MEDIA_SETTINGS_H

#include "../camera/camera_device_info.h"

#include <string>
#include <optional>

namespace webrtc {

struct MediaTrackSettings {

    MediaTrackSettings();
    
    std::optional<int32_t> width;
    std::optional<int32_t> height;
    std::optional<double> aspectRatio;
    std::optional<double> frameRate;
    std::optional<FacingMode> facingMode;
    std::optional<std::string> resizeMode;
    std::optional<int32_t> sampleRate;
    std::optional<int32_t> sampleSize;
    std::optional<bool> echoCancellation;
    std::optional<bool> autoGainControl;
    std::optional<bool> noiseSuppression;
    std::optional<double> latency;
    std::optional<int32_t> channelCount;
    std::optional<std::string> deviceId;
    std::optional<std::string> groupId;
};

} // namespace webrtc

#endif // WEBRTC_MEDIA_SETTINGS_H
