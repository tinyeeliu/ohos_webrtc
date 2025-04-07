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

#ifndef WEBRTC_HELPER_DISPLAY_MANAGER_H
#define WEBRTC_HELPER_DISPLAY_MANAGER_H

#include "pointer_wrapper.h"
#include "error.h"

#include <list>

#include <window_manager/oh_display_info.h>
#include <window_manager/oh_display_manager.h>

namespace ohos {

class DisplayManager {
public:
    class ChangeObserver {
    public:
        virtual ~ChangeObserver() = default;
        virtual void OnDisplayChange(uint64_t displayId) = 0;
    };

    static void RegisterChangeCallback(ChangeObserver* observer);
    static void UnregisterChangeCallback(ChangeObserver* observer);

    static uint64_t GetDefaultDisplayId();
    static int32_t GetDefaultDisplayWidth();
    static int32_t GetDefaultDisplayHeight();
    static NativeDisplayManager_Rotation GetDefaultDisplayRotation();
    static NativeDisplayManager_Orientation GetDefaultDisplayOrientation();

protected:
    static void DisplayChangeCallback(uint64_t displayId);
    static bool RegisterDisplayChangeListener();
    static bool UnregisterDisplayChangeListener();

private:
    static uint32_t listenerIndex;
    static std::list<ChangeObserver*> changeObservers_;
    static std::mutex mutex_;
};

inline uint64_t DisplayManager::GetDefaultDisplayId()
{
    uint64_t displayId = 0;
    NativeDisplayManager_ErrorCode errCode = OH_NativeDisplayManager_GetDefaultDisplayId(&displayId);
    if (errCode != DISPLAY_MANAGER_OK) {
        NATIVE_THROW_IF_FAILED(errCode == DISPLAY_MANAGER_OK, errCode, "NativeDisplayManager",
            "Failed to get default display id", displayId);
    }
    return displayId;
}

inline int32_t DisplayManager::GetDefaultDisplayWidth()
{
    int32_t width = 0;
    NativeDisplayManager_ErrorCode errCode = OH_NativeDisplayManager_GetDefaultDisplayWidth(&width);
    if (errCode != DISPLAY_MANAGER_OK) {
        NATIVE_THROW_IF_FAILED(errCode == DISPLAY_MANAGER_OK, errCode, "NativeDisplayManager",
            "Failed to get default display width", width);
    }
    return width;
}

inline int32_t DisplayManager::GetDefaultDisplayHeight()
{
    int32_t height = 0;
    NativeDisplayManager_ErrorCode errCode = OH_NativeDisplayManager_GetDefaultDisplayHeight(&height);
    if (errCode != DISPLAY_MANAGER_OK) {
        NATIVE_THROW_IF_FAILED(errCode == DISPLAY_MANAGER_OK, -1, "NativeDisplayManager",
            "Failed to get default display height", DISPLAY_MANAGER_ROTATION_0);
    }
    return height;
}

inline NativeDisplayManager_Rotation DisplayManager::GetDefaultDisplayRotation()
{
    NativeDisplayManager_Rotation displayRotation;
    NativeDisplayManager_ErrorCode errCode = OH_NativeDisplayManager_GetDefaultDisplayRotation(&displayRotation);
    if (errCode != DISPLAY_MANAGER_OK) {
        NATIVE_THROW_IF_FAILED(errCode == DISPLAY_MANAGER_OK, -1, "NativeDisplayManager",
            "Failed to get default display rotation", DISPLAY_MANAGER_ROTATION_0);
    }
    return displayRotation;
}

inline NativeDisplayManager_Orientation DisplayManager::GetDefaultDisplayOrientation()
{
    NativeDisplayManager_Orientation displayOrientation = DISPLAY_MANAGER_UNKNOWN;
    NativeDisplayManager_ErrorCode errCode = OH_NativeDisplayManager_GetDefaultDisplayOrientation(&displayOrientation);
    if (errCode != DISPLAY_MANAGER_OK) {
        NATIVE_THROW_IF_FAILED(errCode == DISPLAY_MANAGER_OK, -1, "NativeDisplayManager",
            "Failed to get default display orientation", displayOrientation);
    }
    return displayOrientation;
}

} // namespace ohos

#endif // WEBRTC_HELPER_DISPLAY_MANAGER_H
