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

#ifndef WEBRTC_RENDER_NATIVE_WINDOW_RENDERER_H
#define WEBRTC_RENDER_NATIVE_WINDOW_RENDERER_H

#include "../helper/native_window.h"

#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"

namespace webrtc {
namespace adapter {

class NativeWindowRenderer : public rtc::VideoSinkInterface<VideoFrame> {
public:
    virtual ~NativeWindowRenderer();

    uint64_t GetSurfaceId() const;

protected:
    explicit NativeWindowRenderer(ohos::NativeWindow window);

protected:
    ohos::NativeWindow window_;
};

} // namespace adapter
} // namespace webrtc

#endif // WEBRTC_RENDER_NATIVE_WINDOW_RENDERER_H
