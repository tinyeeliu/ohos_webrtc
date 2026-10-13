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

#include "posture_observer.h"

#include <cmath>

#include "rtc_base/logging.h"

namespace ohos {

namespace {

// Axis indices of accelerometer samples (oh_sensor_type.h: data[0..2] = x/y/z).
constexpr uint32_t K_AXIS_INDEX_X = 0;
constexpr uint32_t K_AXIS_INDEX_Y = 1;
constexpr uint32_t K_AXIS_INDEX_Z = 2;
// Component count of one accelerometer sample.
constexpr uint32_t K_ACCELEROMETER_DATA_COUNT = 3;

} // namespace

void PostureObserver::RegisterObserver(Observer* observer)
{
    if (observer == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    observers_.push_back(observer);
    if (!subscribed_ && SubscribeLocked()) {
        subscribed_ = true;
    }
}

void PostureObserver::UnregisterObserver(Observer* observer)
{
    if (observer == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    observers_.remove(observer);
    if (subscribed_ && observers_.empty()) {
        UnsubscribeLocked();
        subscribed_ = false;
    }
}

bool PostureObserver::SubscribeLocked()
{
    if (subscriptionId_ == nullptr) {
        subscriptionId_ = OH_Sensor_CreateSubscriptionId();
    }
    if (subscriptionAttribute_ == nullptr) {
        subscriptionAttribute_ = OH_Sensor_CreateSubscriptionAttribute();
    }
    if (subscriber_ == nullptr) {
        subscriber_ = OH_Sensor_CreateSubscriber();
    }
    if (subscriptionId_ == nullptr || subscriptionAttribute_ == nullptr || subscriber_ == nullptr) {
        return false;
    }
    if (OH_SensorSubscriptionId_SetType(subscriptionId_, SENSOR_TYPE_ACCELEROMETER) != SENSOR_SUCCESS) {
        return false;
    }
    if (OH_SensorSubscriptionAttribute_SetSamplingInterval(subscriptionAttribute_, kSamplingIntervalNs)
        != SENSOR_SUCCESS) {
        return false;
    }
    if (OH_SensorSubscriber_SetCallback(subscriber_, &PostureObserver::OnSensorEvent) != SENSOR_SUCCESS) {
        return false;
    }

    // Fails with SENSOR_PERMISSION_DENIED when the host app does not declare
    // ohos.permission.ACCELEROMETER; the family then stays UNKNOWN and the
    // consumer degrades to no compensation (unchanged legacy behavior).
    Sensor_Result result = OH_Sensor_Subscribe(subscriptionId_, subscriptionAttribute_, subscriber_);
    if (result != SENSOR_SUCCESS) {
        RTC_LOG(LS_INFO) << "PostureObserver: accelerometer subscribe failed, result=" << result
                         << ", declare ohos.permission.ACCELEROMETER to enable posture compensation";
        return false;
    }
    return true;
}

void PostureObserver::UnsubscribeLocked()
{
    if (subscriptionId_ != nullptr && subscriber_ != nullptr) {
        OH_Sensor_Unsubscribe(subscriptionId_, subscriber_);
    }
    // Subscription handles are kept for reuse; the singleton never destroys them.
}

void PostureObserver::OnSensorEvent(Sensor_Event* event)
{
    float* data = nullptr;
    uint32_t length = 0;
    if (OH_SensorEvent_GetData(event, &data, &length) != SENSOR_SUCCESS || data == nullptr
        || length < K_ACCELEROMETER_DATA_COUNT) {
        return;
    }
    GetInstance().HandleSensorData(data[K_AXIS_INDEX_X], data[K_AXIS_INDEX_Y], data[K_AXIS_INDEX_Z]);
}

void PostureObserver::HandleSensorData(float x, float y, float z)
{
    // Flat on a table the gravity follows the screen normal and the in-plane
    // components are noise; the sample does not vote.
    float horizontalMagnitude = std::sqrt(x * x + y * y);
    if (std::fabs(z) > horizontalMagnitude) {
        return;
    }

    // The dominant-axis test uses absolute values only, so it does not depend
    // on axis sign conventions: gravity on Y (device long axis) => portrait
    // family, on X (device short axis) => landscape family.
    float absX = std::fabs(x);
    float absY = std::fabs(y);

    PostureFamily rawFamily = PostureFamily::UNKNOWN;
    if (absY > kFamilyRatioThreshold * absX) {
        rawFamily = PostureFamily::PORTRAIT;
    } else if (absX > kFamilyRatioThreshold * absY) {
        rawFamily = PostureFamily::LANDSCAPE;
    }
    if (rawFamily == PostureFamily::UNKNOWN) {
        return; // Diagonal hysteresis band: the sample does not vote.
    }

    if (rawFamily == pendingFamily_) {
        pendingCount_++;
    } else {
        pendingFamily_ = rawFamily;
        pendingCount_ = 1;
    }

    // Debounce: commit and notify only after kFamilyConfirmCount consecutive
    // agreeing samples.
    if (pendingCount_ >= kFamilyConfirmCount && rawFamily != currentFamily_.load(std::memory_order_relaxed)) {
        currentFamily_.store(rawFamily, std::memory_order_release);
        NotifyPostureFamilyChanged(rawFamily);
    }
}

void PostureObserver::NotifyPostureFamilyChanged(PostureFamily family)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (Observer* observer : observers_) {
        observer->OnPostureFamilyChanged(family);
    }
}

} // namespace ohos
