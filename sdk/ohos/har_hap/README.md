# DevEco Studio 示例工程

## 简介

此目录为OpenHarmony应用示例工程（基于DevEco Studio 5.0.0 Release），其中ohos_webrtc模块使用编译生成的`libohos_webrtc.so`及相关接口文件构建HAR包，entry模块依赖ohos_webrtc并提供应用示例。

## 准备

参考[文档](../../../docs/ohos/webrtc_build.md)编译生成`libohos_webrtc.so`动态库。

`arm64-v8a`版本放置在`ohos_webrtc/libs/arm64-v8a`目录下，可使用如下参数编译：
``` shell
gn gen out/arm64 --args='is_clang=true target_os="ohos" target_cpu="arm64" ohos_extra_ldflags="-static-libstdc++" is_official_build=true ohos_sdk_native_root="path/to/ohos/sdk"'
ninja -C out/arm64
```

`armeabi-v7a`版本放置在`ohos_webrtc/libs/armeabi-v7a`目录下，可使用如下参数编译：
``` shell
gn gen out/arm32 --args='is_clang=true target_os="ohos" target_cpu="arm" arm_float_abi="softfp" ohos_extra_ldflags="-static-libstdc++" is_official_build=true ohos_sdk_native_root="path/to/ohos/sdk"'
ninja -C out/arm32
```

**注意**
> 如果不指定`ohos_extra_ldflags="-static-libstdc++"`参数，则需要将sdk中的`libc++_shared.so`一起放到对应目录下。

## 运行调试

准备好两台运行 OpenHarmony 5.0 （或以上版本）的设备（也可以仅使用一台设备，然后与浏览器端连接，浏览器端测试地址：https://webrtc.youzhi.life/）。确保设备或浏览器上可正常使用摄像头和音频设备，并连接好网络。

使用 DevEco Studio 打开本工程，连接设备后点击运行。之后在应用界面的文本框输入对端的ID，点击 Connect 按钮。

**说明**
> 本工程所使用的信令服务器（wss://youzhi.life:8443），及浏览器端测试网页（https://webrtc.youzhi.life）仅用于测试，请勿作其它用途。