# <center>ohos_webrtc</center>

## 简介

本目录下提供 WebRTC 的 ArkTS 接口封装，并在 C++ 层适配了视频的采集、渲染及编解码等模块。其中大部分接口的设计遵循 [WebRTC 规范](https://www.w3.org/TR/webrtc/)，另外也根据实际需要，额外提供了 PeerConnectionFactory 和 AudioDeviceModule 等接口。

**图1** SDK 架构图

![architecture](./img/webrtc-arch.png)


## 下载安装

```bash
ohpm install @ohos/webrtc
```

OpenHarmony ohpm 环境配置等更多内容，请参考[如何安装 OpenHarmony ohpm 包](https://gitcode.com/openharmony-tpc/docs/blob/master/OpenHarmony_har_usage.md)。

## 约束与限制

### 兼容性

在下述版本验证通过：

- DevEco Studio: NEXT Beta2(5.0.3.900)，SDK: API12 Release(5.0.0.71)，ROM：5.0.0.71；

**OHOS SDK 要求 12 及以上**。

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
  if (event instanceof webrtc.RTCPeerConnectionIceEvent && event.candidate) {
    // 通过信令通道将候选者发送给对端
  }
};

// 创建并设置本地 Offer 描述
let offer = await peerConnection.createOffer();
await peerConnection.setLocalDescription(offer);
// 通过信令通道将 offer 发送给对端
```

更多使用示例请查看 `har_hap` 目录下的示例工程，动态库的编译方法请参阅[编译文档](../../docs/ohos/webrtc_build.md)。

实现代码在 `src/ohos_webrtc` 目录下，其中 NAPI 接口的使用借助了开源项目 [node-addon-api](https://github.com/nodejs/node-addon-api)。

## 接口说明

完整接口说明请参阅[接口文档](./api/libwebrtc.md)，接口定义见 `api/libohos_webrtc/webrtc.d.ts`。

## 关于混淆

代码混淆，请查看[代码混淆简介](https://developer.huawei.com/consumer/cn/doc/harmonyos-guides/source-obfuscation)。

如果希望在代码混淆过程中不会被混淆，需要在混淆规则配置文件 `obfuscation-rules.txt` 中添加相应的排除规则：

```
-keep
./oh_modules/@ohos/webrtc
```

## 目录结构

SDK 目录结构及说明如下：

```shell
sdk/ohos/
├── BUILD.gn            # src 编译脚本
├── api                 # 存放接口文件
│   ├── ets
│   │   ├── log
│   │   │   └── Logging.ets             # 日志
│   │   └── xcomponent
│   │       └── VideoRenderController.ets # 视频渲染
│   ├── libohos_webrtc
│   │   ├── index.d.ts
│   │   └── webrtc.d.ts                 # 接口定义
│   └── libwebrtc.md                    # 接口文档
├── har_hap             # DevEco Studio 工程目录，包含一个 HAR 模块和一个示例 HAP 模块
└── src
    ├── node-addon-api  # 三方库，辅助 NAPI 开发
    └── ohos_webrtc     # 实现源码
        ├── async_work      # 用于处理异步任务的辅助类
        ├── audio_device    # 音频相关模块
        ├── camera          # 相机相关模块
        ├── desktop_capture # 桌面采集
        ├── helper          # NDK 接口辅助类
        ├── logging         # 日志接口适配
        ├── render          # 视频渲染相关模块
        ├── user_media      # 媒体约束相关的辅助类
        ├── utils           # 工具
        ├── video           # 视频相关模块
        ├── video_codec     # 视频编解码相关模块
        ├── peer_connection.cpp  # RTCPeerConnection 实现
        ├── peer_connection.h    # RTCPeerConnection 头文件
        └── ...              # 其他接口实现
```

## 贡献代码

使用过程中发现任何问题都可以提 [Issue](https://gitcode.com/openharmony-tpc/ohos_webrtc/issues) ，当然，也非常欢迎发 [PR](https://gitcode.com/openharmony-tpc/ohos_webrtc/pulls) 共建。

## 开源协议

本项目基于 [BSD License](../../LICENSE)，请自由地享受和参与开源。
