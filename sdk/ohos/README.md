# WebRTC SDK for OHOS

## 简介

本目录下提供WebRTC的ArkTS接口封装，并在C++层适配了视频的采集、渲染及编解码等模块。其中大部分接口的设计遵循[WebRTC规范](https://www.w3.org/TR/webrtc/)，另外也根据实际需要，并参考Android平台，额外提供PeerConnectionFactory和AudioDeviceModule等接口。

**图1** SDK架构图

![architecture](./img/webrtc-arch.png)

## 目录

SDK目录结构及说明如下：

```shell
sdk/ohos/
├── BUILD.gn	# src 编译脚本
├── api				# 存放接口文件
│   ├── ets
│   │   ├── log
│   │   │   └── Logging.ets	# 日志
│   │   └── xcomponent
│   │       └── VideoRenderController.ets # 视频渲染
│   ├── libohos_webrtc
│   │   ├── index.d.ts
│   │   └── webrtc.d.ts	# 接口定义
│   └── libwebrtc.md		# 接口文档
├── har_hap	# Deveco Studio 工程目录，包含一个 HAR 模块，和一个示例 HAP 模块
└── src
    ├── node-addon-api	# 三方库，辅助 NAPI 开发
    └── ohos_webrtc	# 实现源码
        ├── async_work	# 一些用于处理异步任务的辅助类
        ├── audio_device	# 音频相关模块
        ├── camera	# 相机相关模块
        ├── desktop_capture	# 桌面采集
        ├── helper	# 一些 NDK 接口辅助类
        ├── logging	# 日志接口适配
        ├── render	# 视频渲染相关模块
        ├── user_media	# 媒体约束相关的辅助类，如根据指定的分辨率筛选合适的相机配置参数
        ├── utils	# 工具
        ├── video	# 视频相关模块
        ├── video_codec # 视频编解码相关模块
        ├── peer_connection.cpp	# RTPPeerConnection 实现
        ├── peer_connection.h		# RTPPeerConnection 实现
        |—— ...	# 其它接口实现
```

##  下载安装
```
ohpm install @ohos/webrtc
```

## 其它

接口说明及使用方法请参阅[文档](./api/libwebrtc.md)，实现代码在src/ohos_webrtc目录下，其中NAPI接口的使用借助了开源项目[node-addon-api](https://github.com/nodejs/node-addon-api)。

通过[BUILD.gn](./BUILD.gn)可编译得到动态库`libohos_webrtc.so`，之后可用于构建HAR包以供其它应用模块使用，完整的HAR及示例工程在`har_hap`目录下。动态库的编译方法请参阅[文档](../../docs/ohos/webrtc_build.md)。

**OHOS SDK要求12及以上**。
