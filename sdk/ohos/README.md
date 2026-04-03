# <center>ohos_webrtc</center>

## 简介

webrtc 是适用于 OpenHarmony/HarmonyOS 的实时通信库，提供 ArkTS 接口封装，并在 C++ 层适配了视频采集、渲染及编解码等模块。大部分接口遵循 [WebRTC 规范](https://www.w3.org/TR/webrtc/)，同时额外提供 PeerConnectionFactory 和 AudioDeviceModule 等接口。

**图1** SDK 架构图

![architecture](./img/webrtc-arch.png)



## 下载安装

```bash
ohpm install @ohos/webrtc
```

OpenHarmony ohpm 环境配置等更多内容，请参考[如何安装 OpenHarmony ohpm 包](https://gitcode.com/openharmony-tpc/docs/blob/master/OpenHarmony_har_usage.md)。

> **注意**：若引入库后运行时出现 `does not provide an export name 'NativeVideoRenderer'` 报错，请确认已正确编译并放置 `libohos_webrtc.so` 动态库，参考 [har_hap 示例工程](./har_hap/README.md) 的准备步骤。

## 约束与限制

### 兼容性

在下述版本验证通过：

- DevEco Studio: NEXT Beta2(5.0.3.900)，SDK: API12 Release(5.0.0.71)，ROM：5.0.0.71

> **OHOS SDK 要求 API 12 及以上。**

### 权限要求

打开 `entry/src/main/module.json5`，根据实际功能需求添加以下权限：

```json
"requestPermissions": [
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

以下示例展示基本的音视频通话建立流程：

```typescript
import webrtc, { VideoRenderController } from '@ohos/webrtc';

// 1. 创建 PeerConnectionFactory 和 MediaDevices 实例
const pcf = new webrtc.PeerConnectionFactory();
const mediaDevices = new webrtc.MediaDevices();

// 2. 获取本地音视频媒体流
const localStream: webrtc.MediaStream = await mediaDevices.getUserMedia({
  audio: true,
  video: { width: 1280, height: 720, frameRate: 30 }
});

// 3. 创建 RTCPeerConnection 并添加本地媒体轨道
const pc: webrtc.RTCPeerConnection = pcf.createPeerConnection({
  iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
});
localStream.getTracks().forEach(track => pc.addTrack(track, localStream));

// 4. 监听 ICE 候选者，通过信令通道发送给对端
pc.onicecandidate = (event: webrtc.RTCPeerConnectionIceEvent) => {
  if (event.candidate) {
    // 通过信令通道将 ICE 候选者发送给对端
    sendToRemote({ type: 'ice', ice: event.candidate });
  }
};

// 5. 监听远端轨道，绑定到渲染器
const remoteController: VideoRenderController = new VideoRenderController();
pc.ontrack = (event: webrtc.RTCTrackEvent) => {
  if (event.track.kind === 'video') {
    remoteController.setVideoTrack(event.track);
  }
};

// 6. 创建 SDP Offer 并发送给对端
const offer: webrtc.RTCSessionDescriptionInit = await pc.createOffer();
await pc.setLocalDescription(offer);
sendToRemote({ type: 'offer', sdp: offer });
```

更多使用示例请查看 [`har_hap`](./har_hap) 目录下的示例工程，动态库编译方法请参阅[编译文档](../../docs/ohos/webrtc_build.md)。

## 使用说明

### 1. 初始化 PeerConnectionFactory

`PeerConnectionFactory` 是核心工厂类，负责创建 `RTCPeerConnection`、音视频源和轨道。支持自定义音频设备模块和编解码器工厂：

```typescript
import webrtc from '@ohos/webrtc';

// 自定义音频设备模块（可选）
const adm = new webrtc.AudioDeviceModule({
  useLowLatency: true,                      // 启用低延迟模式
  useHardwareAcousticEchoCanceler: true      // 启用硬件回声消除
});

// 使用自定义配置创建工厂
const pcf = new webrtc.PeerConnectionFactory({
  adm,
  videoEncoderFactory: new webrtc.HardwareVideoEncoderFactory(true),  // 启用 H264 High Profile
  videoDecoderFactory: new webrtc.HardwareVideoDecoderFactory()
});

// 将此工厂设为默认，后续可通过 PeerConnectionFactory.getDefault() 获取
webrtc.PeerConnectionFactory.setDefault(pcf);
```

### 2. 获取摄像头和麦克风

通过 `MediaDevices.getUserMedia()` 获取本地音视频流，可通过 `facingMode` 参数切换前后置摄像头：

```typescript
import webrtc from '@ohos/webrtc';

const mediaDevices = new webrtc.MediaDevices();

// 获取前置摄像头流
const frontStream: webrtc.MediaStream = await mediaDevices.getUserMedia({
  audio: true,
  video: { facingMode: 'user', width: 1280, height: 720, frameRate: 30 }
});

// 切换为后置摄像头
const backStream: webrtc.MediaStream = await mediaDevices.getUserMedia({
  video: { facingMode: 'environment', width: 1280, height: 720 }
});
```

> **注意**：切换摄像头时，需先调用旧视频轨道的 `stop()` 方法释放摄像头资源，再重新调用 `getUserMedia()` 获取新轨道，并通过 `RTCRtpSender.replaceTrack()` 替换已发送的轨道。

### 3. 视频渲染

使用 `VideoRenderController`（封装了 `NativeVideoRenderer`）结合 XComponent 组件渲染视频：

```typescript
import webrtc, { VideoRenderController } from '@ohos/webrtc';

@Component
struct VideoView {
  private controller: VideoRenderController = new VideoRenderController();
  videoTrack: webrtc.MediaStreamTrack | null = null;

  build() {
    XComponent({
      id: 'remoteVideo',
      type: 'surface',
      controller: this.controller
    })
    .width(320)
    .height(240)
    .onLoad(() => {
      // Surface 就绪后绑定视频轨道
      if (this.videoTrack) {
        this.controller.setVideoTrack(this.videoTrack);
      }
    })
  }
}
```

> **注意**：必须等待 XComponent 的 `onSurfaceCreated` 回调触发（即 Surface 就绪）后才能调用 `setVideoTrack()`，过早调用会导致画面无法显示。

### 4. 屏幕共享

通过 `MediaDevices.getDisplayMedia()` 获取屏幕捕获流：

```typescript
import webrtc from '@ohos/webrtc';

const mediaDevices = new webrtc.MediaDevices();

// 捕获整个屏幕（含系统音频）
const screenStream: webrtc.MediaStream = await mediaDevices.getDisplayMedia({
  video: { ohosScreenCaptureMode: 'home-screen' } as webrtc.MediaTrackConstraints,
  ohosSystemAudio: true
} as webrtc.DisplayMediaStreamOptions);
```

> **注意**：屏幕录制需要 `ohos.permission.CAPTURE_SCREEN` 权限。在一次点击事件中多次触发屏幕录制，可能导致权限申请弹窗不再弹出，建议每次录制前检查当前捕获状态后再发起请求。

### 5. 音频控制

通过 `AudioDeviceModule` 控制静音及听筒/外放切换：

```typescript
import webrtc from '@ohos/webrtc';

// 切换外放：rendererUsage 设为 STREAM_USAGE_MUSIC(1) 启用外放
// 切换听筒：rendererUsage 设为 STREAM_USAGE_VOICE_COMMUNICATION(6，默认值)
const adm = new webrtc.AudioDeviceModule({
  rendererUsage: 1  // 1 = STREAM_USAGE_MUSIC（外放）
});
const pcf = new webrtc.PeerConnectionFactory({ adm });

// 静音麦克风
adm.setMicrophoneMute(true);

// 恢复麦克风
adm.setMicrophoneMute(false);

// 静音扬声器
adm.setSpeakerMute(true);
```

### 6. 数据通道

通过 `RTCDataChannel` 在对等端之间传输任意数据：

```typescript
import webrtc from '@ohos/webrtc';

// 发送方：创建有序数据通道
const sendChannel: webrtc.RTCDataChannel = pc.createDataChannel('chat', { ordered: true });
sendChannel.onopen = () => {
  sendChannel.send('Hello!');
};

// 接收方：监听 ondatachannel 事件
pc.ondatachannel = (event: webrtc.RTCDataChannelEvent) => {
  const recvChannel = event.channel;
  recvChannel.onmessage = (e: webrtc.MessageEvent<string | ArrayBuffer>) => {
    console.info('收到消息：' + e.data);
  };
};
```

## 接口说明

完整接口说明请参阅[接口文档](./api/libwebrtc.md)，接口定义见 `api/libohos_webrtc/webrtc.d.ts`。

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

使用过程中发现任何问题都可以提 [Issue](https://gitcode.com/openharmony-sig/ohos_webrtc/issues)，也非常欢迎发 [PR](https://gitcode.com/openharmony-sig/ohos_webrtc/pulls) 共建。

## 开源协议

本项目基于 [BSD License](../../LICENSE)，请自由地享受和参与开源。
