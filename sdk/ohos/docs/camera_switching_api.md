# WebRTC OHOS 摄像头切换API使用指南

## 1. API概述

WebRTC在OHOS平台上提供了摄像头切换功能，允许应用程序在运行时切换前后置摄像头。API设计参考了Android平台的实现，提供了灵活的切换方式和完整的事件通知机制。

## 2. API结构

### 2.1 核心方法

#### `switchCamera()`
- **功能**：自动切换到下一个可用摄像头
- **参数**：无
- **返回值**：无
- **说明**：如果设备上只有一个摄像头，调用此方法会触发错误事件

#### `switchCamera(cameraId)`
- **功能**：切换到指定ID的摄像头
- **参数**：
  - `cameraId`：字符串，目标摄像头的ID
- **返回值**：无
- **说明**：如果指定的摄像头ID不存在，调用此方法会触发错误事件

### 2.2 事件

#### `oncameraswitchdone`
- **触发时机**：摄像头切换成功后
- **事件对象**：
  ```javascript
  {
    type: "CameraSwitchDoneEvent",
    newCameraId: "string"  // 切换后的摄像头ID
  }
  ```

#### `oncameraswitcherror`
- **触发时机**：摄像头切换失败时
- **事件对象**：
  ```javascript
  {
    type: "CameraSwitchErrorEvent",
    errorMessage: "string"  // 错误信息
  }
  ```

## 3. 使用步骤

### 3.1 获取视频源

首先，通过`getUserMedia`获取媒体流，然后从媒体流中获取视频轨道，最后从视频轨道中获取视频源：

```javascript
navigator.mediaDevices.getUserMedia({ video: true })
  .then(stream => {
    // 获取第一个视频轨道
    const videoTrack = stream.getVideoTracks()[0];
    // 获取视频源
    const videoSource = videoTrack.getSource();
    
    // 保存视频源引用，用于后续切换摄像头
    this.videoSource = videoSource;
  })
  .catch(error => {
    console.error("获取摄像头失败:", error);
  });
```

### 3.2 监听切换事件

```javascript
// 监听切换成功事件
videoSource.oncameraswitchdone = (event) => {
  console.log("摄像头切换成功，新摄像头ID:", event.newCameraId);
};

// 监听切换错误事件
videoSource.oncameraswitcherror = (event) => {
  console.error("摄像头切换失败:", event.errorMessage);
};
```

### 3.3 调用切换方法

#### 自动切换摄像头

```javascript
// 自动切换到下一个摄像头
try {
  this.videoSource.switchCamera();
} catch (error) {
  console.error("调用切换摄像头方法失败:", error);
}
```

#### 指定ID切换摄像头

```javascript
// 获取所有摄像头设备
navigator.mediaDevices.enumerateDevices()
  .then(devices => {
    // 筛选出所有视频输入设备
    const videoDevices = devices.filter(device => device.kind === "videoinput");
    
    // 找到后置摄像头（假设第二个设备是后置摄像头）
    if (videoDevices.length > 1) {
      const backCameraId = videoDevices[1].deviceId;
      // 切换到后置摄像头
      this.videoSource.switchCamera(backCameraId);
    }
  })
  .catch(error => {
    console.error("获取设备列表失败:", error);
  });
```

## 4. 完整示例

```javascript
// 初始化摄像头
async function initCamera() {
  try {
    // 获取媒体流
    const stream = await navigator.mediaDevices.getUserMedia({ video: true });
    
    // 将流绑定到video元素
    const videoElement = document.getElementById("localVideo");
    videoElement.srcObject = stream;
    
    // 获取视频轨道和视频源
    const videoTrack = stream.getVideoTracks()[0];
    const videoSource = videoTrack.getSource();
    
    // 保存视频源引用
    window.videoSource = videoSource;
    
    // 设置事件监听
    setupCameraEvents(videoSource);
    
    console.log("摄像头初始化成功");
  } catch (error) {
    console.error("初始化摄像头失败:", error);
  }
}

// 设置摄像头事件监听
function setupCameraEvents(videoSource) {
  // 监听切换成功事件
  videoSource.oncameraswitchdone = (event) => {
    console.log("摄像头切换成功，新摄像头ID:", event.newCameraId);
    alert("摄像头切换成功");
  };
  
  // 监听切换错误事件
  videoSource.oncameraswitcherror = (event) => {
    console.error("摄像头切换失败:", event.errorMessage);
    alert("摄像头切换失败: " + event.errorMessage);
  };
}

// 切换摄像头按钮点击事件
function onSwitchCamera() {
  if (window.videoSource) {
    try {
      window.videoSource.switchCamera();
      console.log("切换摄像头请求已发送");
    } catch (error) {
      console.error("调用切换摄像头方法失败:", error);
      alert("切换摄像头失败: " + error.message);
    }
  } else {
    alert("摄像头未初始化");
  }
}

// 页面加载完成后初始化摄像头
document.addEventListener("DOMContentLoaded", () => {
  initCamera();
  
  // 绑定切换按钮事件
  const switchButton = document.getElementById("switchCameraBtn");
  if (switchButton) {
    switchButton.addEventListener("click", onSwitchCamera);
  }
});
```

## 5. 注意事项

1. **权限要求**：应用必须具有摄像头访问权限
2. **设备支持**：设备必须至少有两个摄像头才能进行切换
3. **状态检查**：在调用切换方法前，确保摄像头已经成功启动
4. **错误处理**：务必监听`oncameraswitcherror`事件，以便处理切换失败的情况
5. **异步操作**：摄像头切换是异步操作，切换结果通过事件通知

## 6. API调用流程

```
JS应用 → NapiVideoSource::SwitchCamera() → OhosVideoTrackSource::SwitchCamera() → CameraCapturer::SwitchCamera() → OHOS Camera API
```

## 7. 常见问题

### 7.1 调用switchCamera()后没有反应

**可能原因**：
- 设备上只有一个摄像头
- 摄像头未成功启动
- 没有正确设置事件监听

**解决方案**：
- 检查设备是否有多个摄像头
- 确保摄像头已经成功初始化和启动
- 确保正确设置了`oncameraswitchdone`和`oncameraswitcherror`事件监听

### 7.2 切换摄像头时画面卡顿

**可能原因**：
- 切换过程中需要重新初始化摄像头
- 设备性能限制

**解决方案**：
- 在切换过程中显示加载提示
- 优化应用程序的资源使用

## 8. 版本兼容性

此API从WebRTC OHOS SDK版本1.0.0开始支持。

## 9. 相关API

- `navigator.mediaDevices.getUserMedia()`：获取媒体流
- `navigator.mediaDevices.enumerateDevices()`：获取设备列表
- `MediaStreamTrack.getSource()`：获取媒体轨道的源

## 10. 联系与反馈

如果您在使用过程中遇到问题或有改进建议，欢迎通过以下方式反馈：

- 提交GitHub Issue
- 发送邮件至：webrtc-ohos@example.com

## 11. 更新日志

- 2025-12-19：初始版本，支持自动切换和指定ID切换

## 12. 许可证

此文档和相关API实现遵循Apache License 2.0许可证。
