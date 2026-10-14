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

#ifndef WEBRTC_HELPER_POSTURE_OBSERVER_H
#define WEBRTC_HELPER_POSTURE_OBSERVER_H

#include <atomic>
#include <list>
#include <mutex>

#include <sensors/oh_sensor.h>
#include <sensors/oh_sensor_type.h>

namespace ohos {

// Physical posture of the device, derived from the dominant gravity axis of
// the accelerometer. Independent of display rotation: it still follows the
// physical hold while the window orientation is locked.
enum class PostureFamily {
    UNKNOWN, // Sensor data unavailable (permission not declared, no sensor, service error).
    PORTRAIT, // Gravity along the device long axis (Y): upright or upside-down hold.
    LANDSCAPE, // Gravity along the device short axis (X): left or right landscape hold.
};

// Posture family observer backed by the accelerometer (process-wide singleton).
// Subscribing requires the host app to declare ohos.permission.ACCELEROMETER
// (normal protection level, granted on declaration in module.json5). Without it
// the subscription fails, the family stays UNKNOWN, and consumers (camera
// rotation compensation) gracefully degrade to doing nothing.
class PostureObserver {
public:
    // Contract for OnPostureFamilyChanged:
    // - Runs on the sensor service thread; keep it light, never block.
    // - Never call RegisterObserver/UnregisterObserver from the callback:
    //   the registry mutex is non-recursive and notification holds it.
    // - Never destroy the observer from the callback.
    // - The current family is not reported on registration; query
    //   GetPostureFamily() once after registering if the initial value matters.
    // - UnregisterObserver must be called before the observer is destroyed.
    class Observer {
    public:
        virtual ~Observer() = default;

        virtual void OnPostureFamilyChanged(PostureFamily family) = 0;
    };

    static PostureObserver& GetInstance();

    void RegisterObserver(Observer* observer);
    void UnregisterObserver(Observer* observer);

    PostureFamily GetPostureFamily() const;

private:
    PostureObserver() = default;
    ~PostureObserver() = default;
    PostureObserver(const PostureObserver&) = delete;
    PostureObserver& operator=(const PostureObserver&) = delete;

    bool SubscribeLocked();
    void UnsubscribeLocked();
    void HandleSensorData(float x, float y, float z);
    void NotifyPostureFamilyChanged(PostureFamily family);

    static void OnSensorEvent(Sensor_Event* event);

    // 100 ms sampling interval: posture changes at human speed; 10 Hz tracks it in time.
    static constexpr int64_t kSamplingIntervalNs = 100000000;
    // Family switch threshold with hysteresis: the dominant axis must exceed
    // the other by this ratio, so the family never toggles around the diagonal.
    static constexpr float kFamilyRatioThreshold = 1.5f;
    // Family commit debounce: kFamilyConfirmCount consecutive agreeing samples
    // (~500 ms) are required before a family change is committed. During
    // auto-rotation the sensor reports the new posture before the display
    // rotation catches up; the debounce filters that transient mismatch so
    // compensation never flickers mid-animation. With a locked window the
    // mismatch is persistent and the confirmation delay is imperceptible.
    static constexpr uint32_t kFamilyConfirmCount = 5;

    std::mutex mutex_;
    std::list<Observer*> observers_;
    std::atomic<PostureFamily> currentFamily_{PostureFamily::UNKNOWN};
    // Debounce voting state, accessed only on the sensor callback thread.
    PostureFamily pendingFamily_{PostureFamily::UNKNOWN};
    uint32_t pendingCount_{0};
    bool subscribed_{false};
    Sensor_SubscriptionId* subscriptionId_{nullptr};
    Sensor_SubscriptionAttribute* subscriptionAttribute_{nullptr};
    Sensor_Subscriber* subscriber_{nullptr};
};

inline PostureObserver& PostureObserver::GetInstance()
{
    static PostureObserver instance;
    return instance;
}

inline PostureFamily PostureObserver::GetPostureFamily() const
{
    return currentFamily_.load(std::memory_order_acquire);
}

} // namespace ohos

#endif // WEBRTC_HELPER_POSTURE_OBSERVER_H
