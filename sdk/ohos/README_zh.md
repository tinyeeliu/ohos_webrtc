# <center>ohos_webrtc</center>

本项目基于 [WebRTC](https://webrtc.googlesource.com/src) 开发。

## 简介

本目录下提供 WebRTC 的 ArkTS 接口封装，并在 C++ 层适配了视频的采集、渲染及编解码等模块。其中大部分接口的设计遵循 [WebRTC 规范](https://www.w3.org/TR/webrtc/)，另外也根据实际需要，额外提供了 PeerConnectionFactory 和 AudioDeviceModule 等接口。

**图1** SDK 架构图

![architecture](./img/webrtc-arch.png)

## 下载安装

```bash
ohpm install @ohos/webrtc
```

OpenHarmony ohpm 环境配置等更多内容，请参考[如何安装 OpenHarmony ohpm 包](https://gitcode.com/openharmony-tpc/docs/blob/master/OpenHarmony_har_usage.md)。

若需在 NDK C++ 工程中直接集成 `libwebrtc.a`，请参阅[北向应用集成二进制文件](../../docs/ohos/webrtc_adapter_app.md)。

## 约束与限制

### 兼容性

OpenHarmony API 版本 ≥ 15。

### 权限要求

打开 `entry/src/main/module.json5`，根据实际功能需求添加以下权限：

> **说明**：HAR 模块已预声明 `ohos.permission.INTERNET`、`ohos.permission.CAMERA`、`ohos.permission.MICROPHONE` 三个权限。`ohos.permission.CAPTURE_SCREEN` 仅在使用屏幕采集（`getDisplayMedia`）时需要，且必须由宿主应用在自己的 `module.json5` 中声明；HAR 未预声明此权限，因为它是系统授予型权限，仅应用可申请。

```json
"requestPermissions": [
  {
    "name": "ohos.permission.INTERNET",
    "reason": "$string:internet_reason",
    "usedScene": {
      "abilities": ["EntryAbility"],
      "when": "inuse"
    }
  },
  {
    "name": "ohos.permission.MICROPHONE",
    "reason": "$string:microphone_reason",
    "usedScene": {
      "abilities": ["EntryAbility"],
      "when": "inuse"
    }
  },
  {
    "name": "ohos.permission.CAMERA",
    "reason": "$string:camera_reason",
    "usedScene": {
      "abilities": ["EntryAbility"],
      "when": "inuse"
    }
  },
  {
    "name": "ohos.permission.CAPTURE_SCREEN",
    "reason": "$string:screen_capture_reason",
    "usedScene": {
      "abilities": ["EntryAbility"],
      "when": "inuse"
    }
  }
]
```

## 使用示例

以下示例展示了基本的音视频通话建立流程：

```typescript
import webrtc from '@ohos/webrtc';

// 创建 PeerConnectionFactory 实例
let peerConnectionFactory = new webrtc.PeerConnectionFactory();

// 创建本地音频源和音频轨道
let audioSource = peerConnectionFactory.createAudioSource();
let audioTrack = peerConnectionFactory.createAudioTrack('audio', audioSource);

// 创建本地视频源和视频轨道（分辨率 640×480，帧率 15~30fps）
let videoSource = peerConnectionFactory.createVideoSource({
  width: 640,
  height: 480,
  frameRate: { min: 15, max: 30 }
});
let videoTrack = peerConnectionFactory.createVideoTrack('video', videoSource);

// 创建 RTCPeerConnection
let peerConnection = peerConnectionFactory.createPeerConnection({
  iceServers: [{ urls: 'stun:stun.example.com' }]
});

// 添加媒体轨道到连接
peerConnection.addTrack(audioTrack);
peerConnection.addTrack(videoTrack);

// 监听 ICE 候选者
peerConnection.onicecandidate = (event) => {
  if (event.candidate) {
    // 通过信令通道将候选者发送给对端
  }
};

// 创建并设置本地 Offer 描述
let offer = await peerConnection.createOffer();
await peerConnection.setLocalDescription(offer);
// 通过信令通道将 SDP offer 发送给远程端点
```

更多使用示例请查看 `har_hap` 目录下的示例工程，动态库的编译方法请参阅[编译文档](../../docs/ohos/webrtc_build.md)，编译过程中遇到问题请参阅[编译常见问题](../../docs/ohos/webrtc_adapter_questions.md)。

实现代码在 `src/ohos_webrtc` 目录下，其中 NAPI 接口的使用借助了开源项目 [node-addon-api](https://github.com/nodejs/node-addon-api)。

## 上游升级 API 变更及鸿蒙适配情况

- 上游库：WebRTC m120（[https://webrtc.googlesource.com/src](https://webrtc.googlesource.com/src)）
- 当前库版本：基于 WebRTC m120 版本
- OpenHarmony API 版本：>= 15

### API 变更

| 变更类型 | 项目 | 说明 |
| --- | --- | --- |
| 本地新增 | CameraPreviewSession | 统一管理手机、折叠/三折叠设备及多显示器场景中的相机选择、前后台恢复、有限重试和宿主窗口方向同步 |
| 本地新增 | VideoRenderController | 封装 NativeVideoRenderer，用于视频渲染控制 |
| 本地新增 | VideoSource.getCaptureFrameCount() | 用于停帧监测（能力版本 2+） |
| 本地新增 | VideoSource.applyDisplayRotation() | 宿主提供的旋转角度控制 |
| 适配 | 视频采集模块 | 适配 OHOS Camera API |
| 适配 | 视频渲染模块 | 适配 ArkUI XComponent/Surface |
| 适配 | 音频模块 | 适配 OHOS OHAudio API |
| 适配 | 视频编解码模块 | 适配 OHOS MediaCodec |
| 适配 | 网络模块 | 适配 OHOS 网络协议栈 |

### 鸿蒙适配情况

基于 WebRTC m120；C++ 层适配视频采集、渲染及编解码模块；ArkTS 接口绑定通过 NAPI（node-addon-api）实现；大部分接口设计遵循 W3C WebRTC 规范；根据实际需要额外提供 PeerConnectionFactory、AudioDeviceModule 等接口。

## 接口说明

完整接口定义见 `api/libohos_webrtc/webrtc.d.ts`，完整说明请参阅[接口文档](./api/libwebrtc.md)。

### 全量 API 清单

| 分类 | API 名称 | 类型 | 说明 |
| --- | --- | --- | --- |
| PeerConnectionFactory | constructor | 构造函数 | 创建 PeerConnectionFactory 实例 |
| PeerConnectionFactory | createAudioSource | 方法 | 创建本地音频源 |
| PeerConnectionFactory | createAudioTrack | 方法 | 基于音频源创建音频轨道 |
| PeerConnectionFactory | createVideoSource | 方法 | 根据指定分辨率和帧率创建本地视频源 |
| PeerConnectionFactory | createVideoTrack | 方法 | 基于视频源创建视频轨道 |
| PeerConnectionFactory | createPeerConnection | 方法 | 根据给定配置创建 RTCPeerConnection |
| RTCPeerConnection | addTrack | 方法 | 向连接添加媒体轨道 |
| RTCPeerConnection | removeTrack | 方法 | 从连接移除媒体轨道 |
| RTCPeerConnection | createOffer | 方法 | 创建 SDP offer |
| RTCPeerConnection | createAnswer | 方法 | 创建 SDP answer |
| RTCPeerConnection | setLocalDescription | 方法 | 设置本地会话描述 |
| RTCPeerConnection | setRemoteDescription | 方法 | 设置远端会话描述 |
| RTCPeerConnection | addIceCandidate | 方法 | 添加远端发送的 ICE 候选者 |
| RTCPeerConnection | close | 方法 | 关闭连接并释放资源 |
| RTCPeerConnection | onicecandidate | 事件 | ICE 候选者可用时触发 |
| RTCPeerConnection | ontrack | 事件 | 收到对端轨道时触发 |
| RTCPeerConnection | oniceconnectionstatechange | 事件 | ICE 连接状态变化时触发 |
| MediaStreamTrack | kind | 属性 | 轨道类型（audio/video） |
| MediaStreamTrack | id | 属性 | 轨道唯一标识 |
| MediaStreamTrack | enabled | 属性 | 轨道是否启用 |
| MediaStreamTrack | stop | 方法 | 停止并释放轨道 |
| VideoSource | applyDisplayRotation | 方法 | 应用宿主角度（0/90/180/270；-1 恢复默认显示器自动旋转） |
| VideoSource | getCaptureFrameCount | 方法 | 返回采集帧计数，用于停帧监测（能力版本 2+） |
| CameraPreviewSession | constructor | 构造函数 | 创建多设备场景相机预览会话 |
| CameraPreviewSession | attachWindow | 方法 | 挂载宿主窗口以同步方向 |
| CameraPreviewSession | setActive | 方法 | 启用或停用采集 |
| CameraPreviewSession | setCameraPreference | 方法 | 设置前后置相机偏好 |
| CameraPreviewSession | retry | 方法 | 显式重试相机采集 |
| CameraPreviewSession | release | 方法 | 释放会话及其资源 |
| VideoRenderController | constructor | 构造函数 | 创建封装 NativeVideoRenderer 的渲染控制器 |
| VideoRenderController | onFrameSizeChanged | 回调 | 通知旋转后实际帧宽高，用于布局 |
| MediaDevices | getUserMedia | 方法 | 从本地设备获取音视频流 |
| MediaDevices | getDisplayMedia | 方法 | 获取屏幕采集流 |
| 接口 | RTCConfiguration | 接口 | createPeerConnection 的配置 |
| 接口 | RTCIceServer | 接口 | ICE 服务器配置 |
| 接口 | RTCSessionDescription | 接口 | SDP 会话描述 |
| AudioDeviceModule | - | 模块 | 音频设备模块，用于音频输入输出管理 |

## 多设备相机预览

当前本地增量提供 `CameraPreviewSession`，统一管理手机、折叠/三折叠设备及多显示器场景中的相机选择、前后台恢复、有限重试和宿主窗口方向同步。发布状态见 CHANGELOG。

### 接入与生命周期

先申请相机权限，再创建会话；将同一个 `VideoRenderController` 挂到 XComponent。以下 `context`、`factory` 和 `hostWindow` 由宿主提供：

```typescript
import { CameraPreviewSession, VideoRenderController } from '@ohos/webrtc';

const controller = new VideoRenderController();
const session = new CameraPreviewSession(context, factory, controller, {
  front: true,
  allowFallback: true,
  autoRecover: true,
  maxRetries: 3
});
session.attachWindow(hostWindow);
session.setActive(true);

// 页面隐藏：session.setActive(false)
// 页面恢复：session.setActive(true)
// 切换后置偏好：session.setCameraPreference(false)
// 显式重试：session.retry()
// 页面销毁：session.release()
```

- 默认请求 1280×720、后置相机、AspectFit；尺寸为采集请求，显示布局应使用 `controller.onFrameSizeChanged` 返回的旋转后实际宽高。
- 非空 `deviceId` 严格匹配，缺失时报告错误；未指定 ID 时按前后置偏好选择，`allowFallback` 决定是否允许回退到当前列表首项。
- 库监听窗口显隐、显示器变化和相机/折叠事件。页面仍需通过 `setActive` 表达是否使用相机，并负责权限、布局和状态提示。
- 恢复或切换镜头可能重建轨道。用于通话时，应通过 `onTrackChanged` 更新发送端绑定，并处理轨道为 `undefined` 的情况。
- `frameStallTimeoutMs` 默认 0（关闭），可设置为 1000–60000 毫秒，通过采集帧计数检测停帧并触发有限重试；它不检测静止画面或下游渲染停滞。

### 配套 Native 库

| 能力版本 | 会话行为 |
| --- | --- |
| `NativeVideoRenderer.cameraPreviewApiVersion` 缺失或小于 1 | 报告 `incompatible`，不创建视频源、不自动重试 |
| 1 | 支持增强预览、窗口角度同步及 Surface 尺寸适配；保持停帧监测关闭 |
| 2 及以上 | 增加 `VideoSource.getCaptureFrameCount()`；可显式启用停帧监测 |

当前 Native 源码声明能力版本 2。使用停帧监测需从当前源码整库重编并替换对应 ABI 的 `libohos_webrtc.so`；仅更新 ArkTS 文件不足以提供新增能力。显式请求监测但缺少帧计数方法时报告 `incompatible`。

低层 `VideoSource` 接口继续可用。`applyDisplayRotation(0/90/180/270)` 启用宿主角度模式，`-1` 恢复默认显示器自动模式；`CameraPreviewSession` 负责同步角度，无需页面重复补偿。控制器负责 Surface 尺寸与等比渲染，Native 按运行时能力处理安装角和 HAL 旋转回退。

### 示例与验证

从示例首页进入“多设备相机实验室（A–F）”，检查断点布局、相机设备、预览比例、预览方向、悬停页面和本地/对端视频。操作方法及自动测试命令见[示例工程 README](./har_hap/README.md#多设备相机实验室af)。

预览截图用于检查合成画面的方向与裁剪；本机回环用于检查编码/解码链路，F 项用于双端视频验证。仍需在目标设备上核对四向持握、前后置切换、折叠/展开、前后台恢复、显示器迁移及停帧恢复。当前增量不包含图像或扫码识别引擎。

## 关于混淆

有关代码混淆的详细信息，请查看[代码混淆简介](https://developer.huawei.com/consumer/cn/doc/harmonyos-guides/source-obfuscation)。

若希望 `@ohos/webrtc` 在代码混淆过程中不被混淆，需在混淆规则配置文件 `obfuscation-rules.txt` 中添加以下排除规则：

```
-keep
./oh_modules/@ohos/webrtc
```

## 目录结构

SDK 目录结构及说明如下：

```shell
sdk/ohos/
├── BUILD.gn            # src 编译脚本
├── api                 # 接口文件
│   ├── ets
│   │   ├── log
│   │   │   └── Logging.ets             # 日志工具
│   │   └── xcomponent
│   │       ├── CameraPreviewSession.ets  # 多设备相机预览与恢复
│   │       └── VideoRenderController.ets # 视频渲染控制器（封装 NativeVideoRenderer）
│   ├── libohos_webrtc
│   │   ├── index.d.ts                  # 接口入口（re-export webrtc.d.ts）
│   │   └── webrtc.d.ts                 # 完整接口类型定义
│   └── libwebrtc.md                    # 接口说明文档
├── har_hap             # DevEco Studio 示例工程（含 HAR 模块和示例 HAP 模块）
└── src
    ├── node-addon-api  # 辅助 NAPI 开发的三方库（node-addon-api）
    └── ohos_webrtc     # C++ 实现源码
        ├── async_work      # 异步任务辅助类
        ├── audio_device    # 音频相关模块
        ├── camera          # 相机相关模块
        ├── desktop_capture # 桌面采集
        ├── helper          # NDK 接口辅助类
        ├── logging         # 日志接口适配
        ├── render          # 视频渲染相关模块
        ├── user_media      # 媒体约束辅助类
        ├── utils           # 工具类
        ├── video           # 视频相关模块
        ├── video_codec     # 视频编解码相关模块
        ├── peer_connection.cpp  # RTCPeerConnection 实现
        └── peer_connection.h    # RTCPeerConnection 头文件
```

## 贡献代码

使用过程中发现任何问题都可以提 [Issue](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/issues)，也非常欢迎发 [PR](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pulls) 共建。

## 开源协议

本项目基于 [BSD License](../../LICENSE)，请自由地享受和参与开源。
