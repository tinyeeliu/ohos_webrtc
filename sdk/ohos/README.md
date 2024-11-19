本目录下提供WebRTC的ArkTS接口封装，并在C++层适配了视频的采集、渲染及编解码等模块。其中大部分接口的设计遵循[WebRTC规范](https://www.w3.org/TR/webrtc/)，另外也根据实际需要，并参考Android平台，额外提供PeerConnectionFactory和AudioDeviceModule等接口。

接口说明及使用方法请参阅[文档](./api/libwebrtc.md)，实现代码在src/ohos_webrtc目录下，其中NAPI接口的使用借助了开源项目[node-addon-api](https://github.com/nodejs/node-addon-api)。

通过[BUILD.gn](./BUILD.gn)可编译得到动态库libohos_webrtc.so，之后可用于构建HAR包以供其它应用模块使用，完整的HAR及示例工程在har目录下。动态库的编译方法请参阅[文档](/docs/ohos/webrtc_build.md)。

**OHOS SDK要求12及以上**。