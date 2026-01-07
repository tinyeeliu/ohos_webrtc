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

#include <window_manager/oh_display_manager.h>
#include "api/video/video_rotation.h"

namespace ohos {

class DisplayChangeListener {
public:
    static DisplayChangeListener& GetInstance()
    {
        static DisplayChangeListener instance;
        return instance;
    }
    webrtc::VideoRotation GetRealRotation(const webrtc::VideoRotation& cameraCaptureRotation);

private:
    DisplayChangeListener();
    ~DisplayChangeListener();
};

}
#define DISPLAY ohos::DisplayChangeListener::GetInstance()
