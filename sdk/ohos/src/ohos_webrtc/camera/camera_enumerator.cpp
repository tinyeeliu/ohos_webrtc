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

#include "camera_enumerator.h"
#include "../user_media/media_constraints.h"
#include "../helper/camera.h"

#include <sys/stat.h>
#include <vector>

namespace webrtc {

namespace {

// Merge output-capability profiles into the device entry: preview profiles become
// selectable track profiles; video profiles provide the real fps range for getCapabilities.
void FillDeviceProfiles(ohos::CameraOutputCapability& capability, CameraDeviceInfo& device)
{
    for (uint32_t j = 0; j < capability.PreviewProfileSize(); j++) {
        Camera_Profile* profile = capability.GetPreviewProfile(j);
        RTC_DLOG(LS_VERBOSE) << "preview format: " << profile->format;
        RTC_DLOG(LS_VERBOSE) << "preview size: " << profile->size.width << "x" << profile->size.height;

        video::VideoProfile p;
        p.format = NativeCameraFormatToPixelFormat(profile->format);
        p.resolution = video::Resolution{profile->size.width, profile->size.height};
        p.frameRateRange = video::FrameRateRange{0, UINT_MAX}; // equivalently to ignore 'frameRate' constraints
        device.profiles.push_back(p);
    }

    // Real device fps capability for getCapabilities; Camera_VideoProfile is flat
    // (format/size/range), see ohcamera camera_manager.h
    for (uint32_t j = 0; j < capability.VideoProfileSize(); j++) {
        Camera_VideoProfile* fpsProfile = capability.GetVideoProfile(j);
        if (!fpsProfile) {
            continue;
        }
        RTC_DLOG(LS_VERBOSE) << "video profile size: " << fpsProfile->size.width << "x"
                             << fpsProfile->size.height << ", fps " << fpsProfile->range.min << "-"
                             << fpsProfile->range.max;
        if (device.frameRateRange.min == 0 || fpsProfile->range.min < device.frameRateRange.min) {
            device.frameRateRange.min = fpsProfile->range.min;
        }
        if (fpsProfile->range.max > device.frameRateRange.max) {
            device.frameRateRange.max = fpsProfile->range.max;
        }
    }
}

} // namespace

std::vector<CameraDeviceInfo> CameraEnumerator::GetDevices()
{
    std::vector<CameraDeviceInfo> result;

    auto cameras = ohos::CameraManager::GetInstance().GetSupportedCameras();
    RTC_LOG(LS_INFO) << "CameraEnumerator::GetDevices() found " << cameras.Size() << " cameras";
    for (std::size_t i = 0; i < cameras.Size(); i++) {
        auto camera = cameras[i];
        RTC_LOG(LS_INFO) << "camera id: " << camera->cameraId;
        RTC_LOG(LS_INFO) << "camera type: " << camera->cameraType;
        RTC_LOG(LS_INFO) << "camera position: " << camera->cameraPosition;
        RTC_LOG(LS_INFO) << "camera connection type: " << camera->connectionType;

        CameraDeviceInfo device;
        device.deviceId = camera->cameraId;
        device.groupId = "default";
        device.label = CameraConnectionTypeToString(camera->connectionType) + " " +
                       CameraPositionToString(camera->cameraPosition) + " (" + device.deviceId + ")";
        switch (camera->cameraPosition) {
            case CAMERA_POSITION_FRONT:
                device.facingMode = FacingMode::kUser;
                RTC_LOG(LS_INFO) << "Camera " << camera->cameraId << " set to FacingMode::kUser";
                break;
            case CAMERA_POSITION_BACK:
                device.facingMode = FacingMode::kEnvironment;
                RTC_LOG(LS_INFO) << "Camera " << camera->cameraId << " set to FacingMode::kEnvironment";
                break;
            default:
                device.facingMode = FacingMode::kNone;
                RTC_LOG(LS_INFO) << "Camera " << camera->cameraId << " set to FacingMode::kNone";
                break;
        }

        auto capability = ohos::CameraManager::GetInstance().GetSupportedCameraOutputCapability(camera);
        FillDeviceProfiles(capability, device);

        result.push_back(device);
    }

    return result;
}

} // namespace webrtc
