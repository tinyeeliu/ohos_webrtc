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

#ifndef WEBRTC_UTILS_THREAD_UTILS_H
#define WEBRTC_UTILS_THREAD_UTILS_H

#include <chrono>
#include <future>
#include <memory>

#include "rtc_base/logging.h"
#include "rtc_base/thread.h"

namespace webrtc {

constexpr auto kThreadStopDeadline = std::chrono::seconds(2);

inline void StopThreadWithDeadline(std::unique_ptr<rtc::Thread>& thread, const char* tag)
{
    if (!thread) {
        return;
    }
    auto done = std::make_shared<std::promise<void>>();
    auto future = done->get_future();
    thread->PostTask([done]() { done->set_value(); });
    if (future.wait_for(kThreadStopDeadline) == std::future_status::ready) {
        thread->Stop();
    } else {
        RTC_LOG(LS_ERROR) << tag << ": cleanup timeout, leak worker thread instead of blocking";
        thread.release();
    }
}

} // namespace webrtc

#endif // WEBRTC_UTILS_THREAD_UTILS_H
