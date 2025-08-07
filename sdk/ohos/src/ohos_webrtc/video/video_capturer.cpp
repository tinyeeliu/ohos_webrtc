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

#include "video_capturer.h"
#include "../utils/marcos.h"
 
namespace webrtc {

VideoCapturerBase::VideoCapturerBase() = default;
VideoCapturerBase::~VideoCapturerBase() = default;

void VideoCapturerBase::SetObserver(Observer* observer)
{
    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    observer_ = observer;
}

void VideoCapturerBase::NotifyCapturerStarted()
{
    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnCapturerStarted();
    }
}

void VideoCapturerBase::NotifyCapturerStopped()
{
    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnCapturerStopped();
    }
}

void VideoCapturerBase::NotifyCapturerError(int32_t code, const std::string& message)
{
    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnCapturerError(code, message);
    }
}

void VideoCapturerBase::NotifyFrameCaptured(rtc::scoped_refptr<VideoFrameBuffer> buffer, int64_t timestampUs, VideoRotation rotation)
{
    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    if (observer_) {
        observer_->OnFrameCaptured(buffer, timestampUs, rotation);
    }
}
 
} // namespace webrtc
 