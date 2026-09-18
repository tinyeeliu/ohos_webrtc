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

// 设备物理姿态族: 依据加速度传感器重力主轴判定, 与 display rotation 相互独立,
// 窗口方向锁定(display rotation 冻结)时仍随物理姿态变化(issue #200)。
enum class PostureFamily {
    UNKNOWN, // 传感器数据未就绪(未声明权限/无传感器/服务异常), 无法判定
    PORTRAIT, // 重力沿设备长轴(Y 轴)方向: 竖握/倒竖握
    LANDSCAPE, // 重力沿设备短轴(X 轴)方向: 左横握/右横握
};

// 加速度传感器姿态族观察器(进程内单例)。
// 注意: 订阅加速度传感器需宿主应用声明 ohos.permission.ACCELEROMETER 权限
// (normal 级, module.json5 声明即生效); 未声明时订阅失败, 姿态族保持 UNKNOWN,
// 使用方(相机旋转补偿)据此优雅降级为不补偿。
class PostureObserver {
public:
    class Observer {
    public:
        virtual ~Observer() = default;

        // 姿态族变化时回调(传感器服务线程, 勿阻塞或做耗时操作)。
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

    // 采样间隔 100ms: 姿态为人体速度变化, 10Hz 足以及时跟踪。
    static constexpr int64_t kSamplingIntervalNs = 100000000;
    // 姿态族切换阈值(迟滞): 主轴分量需达到次轴分量的该倍数, 避免临界角反复切换。
    static constexpr float kFamilyRatioThreshold = 1.5f;
    // 姿态族提交防抖: 连续 kFamilyConfirmCount 个样本一致才提交(约 500ms)。
    // 用于过滤自动旋转时"传感器先于 display 旋转感知到新姿态"造成的瞬态错配,
    // 避免旋转动画期间补偿闪跳; 窗口锁定场景错配为持久状态, 确认延迟无感知。
    static constexpr uint32_t kFamilyConfirmCount = 5;

    std::mutex mutex_;
    std::list<Observer*> observers_;
    std::atomic<PostureFamily> currentFamily_{PostureFamily::UNKNOWN};
    // 防抖投票状态, 仅传感器回调线程访问。
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
