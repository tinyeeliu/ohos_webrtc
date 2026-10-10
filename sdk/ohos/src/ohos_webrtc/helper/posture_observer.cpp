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

// 加速度传感器数据轴下标(oh_sensor_type.h: data[0]/[1]/[2] 分别为 x/y/z 轴分量)。
constexpr uint32_t kAxisIndexX = 0;
constexpr uint32_t kAxisIndexY = 1;
constexpr uint32_t kAxisIndexZ = 2;
// 加速度传感器数据分量个数。
constexpr uint32_t kAccelerometerDataCount = 3;

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

    // 宿主应用未声明 ohos.permission.ACCELEROMETER 时返回 SENSOR_PERMISSION_DENIED,
    // 姿态族保持 UNKNOWN, 使用方据此退化为不补偿(与既往版本行为一致)。
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
    // 订阅句柄保留复用, 单例生命周期内不销毁。
}

void PostureObserver::OnSensorEvent(Sensor_Event* event)
{
    float* data = nullptr;
    uint32_t length = 0;
    if (OH_SensorEvent_GetData(event, &data, &length) != SENSOR_SUCCESS || data == nullptr
        || length < kAccelerometerDataCount) {
        return;
    }
    GetInstance().HandleSensorData(data[kAxisIndexX], data[kAxisIndexY], data[kAxisIndexZ]);
}

void PostureObserver::HandleSensorData(float x, float y, float z)
{
    // 平放(重力沿屏幕法线方向)时水平两轴分量接近噪声, 样本无效不参与投票。
    float horizontalMagnitude = std::sqrt(x * x + y * y);
    if (std::fabs(z) > horizontalMagnitude) {
        return;
    }

    // 主轴判定仅依据分量绝对值, 与加速度计坐标轴符号约定无关:
    // 重力沿 Y 轴(设备长轴) => 竖握族; 沿 X 轴(设备短轴) => 横握族。
    float absX = std::fabs(x);
    float absY = std::fabs(y);

    PostureFamily rawFamily = PostureFamily::UNKNOWN;
    if (absY > kFamilyRatioThreshold * absX) {
        rawFamily = PostureFamily::PORTRAIT;
    } else if (absX > kFamilyRatioThreshold * absY) {
        rawFamily = PostureFamily::LANDSCAPE;
    }
    if (rawFamily == PostureFamily::UNKNOWN) {
        return; // 临界迟滞区: 样本不参与投票。
    }

    if (rawFamily == pendingFamily_) {
        pendingCount_++;
    } else {
        pendingFamily_ = rawFamily;
        pendingCount_ = 1;
    }

    // 防抖: 连续 kFamilyConfirmCount 个样本一致才提交并通知。
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
