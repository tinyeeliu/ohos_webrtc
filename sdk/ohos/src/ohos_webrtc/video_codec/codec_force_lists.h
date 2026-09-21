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

#ifndef WEBRTC_VIDEO_CODEC_CODEC_FORCE_LISTS_H
#define WEBRTC_VIDEO_CODEC_CODEC_FORCE_LISTS_H

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <vector>

#include "api/video_codecs/sdp_video_format.h"

namespace webrtc {
namespace adapter {

// Per-codec force software / force hardware lists (#9), so that specific
// codecs can be pinned to software (or hardware) when a device's hardware
// codec misbehaves. Codec names are matched case-insensitively
// ("h264" == "H264").
class CodecForceLists {
public:
    CodecForceLists() = default;

    CodecForceLists(std::vector<std::string> forceSoftware, std::vector<std::string> forceHardware)
    {
        for (auto& name : forceSoftware) {
            forceSoftware_.insert(Normalize(name));
        }
        for (auto& name : forceHardware) {
            forceHardware_.insert(Normalize(name));
        }
    }

    bool IsForceSoftware(const SdpVideoFormat& format) const
    {
        return forceSoftware_.count(Normalize(format.name)) > 0;
    }

    bool IsForceHardware(const SdpVideoFormat& format) const
    {
        return forceHardware_.count(Normalize(format.name)) > 0;
    }

    bool IsEmpty() const
    {
        return forceSoftware_.empty() && forceHardware_.empty();
    }

private:
    static std::string Normalize(std::string name)
    {
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        return name;
    }

    std::set<std::string> forceSoftware_;
    std::set<std::string> forceHardware_;
};

} // namespace adapter
} // namespace webrtc

#endif // WEBRTC_VIDEO_CODEC_CODEC_FORCE_LISTS_H
