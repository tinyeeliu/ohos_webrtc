# DevEco Studio 示例工程

## 简介

此目录为 OpenHarmony/HarmonyOS 应用示例工程，其中 ohos_webrtc 模块使用编译生成的 `libohos_webrtc.so` 及相关接口文件构建 HAR 包，entry 模块依赖 ohos_webrtc 并提供应用示例。当前本地 `build-profile.json5` 的目标和兼容 SDK 均为 `6.0.1(21)`，请使用配套的 DevEco Studio/SDK，并配置自己的调试签名。

## 准备

参考[文档](../../../docs/ohos/webrtc_build.md)编译生成`libohos_webrtc.so`动态库。

多设备预览要求 `NativeVideoRenderer.cameraPreviewApiVersion >= 1`；当前 Native 源码为版本 2，增加采集帧计数。请整库重编并替换对应 ABI 的二进制，同时使用配套 ArkTS 文件。版本 1 仍可预览，实验室仅在版本 2 及以上启用 8 秒采样的停帧监测。缺失预览能力时会话报告 `incompatible`。

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

`x86_64`版本（供模拟器调试使用）放置在`ohos_webrtc/libs/x86_64`目录下。与 arm64 共用同一套 `sdk/ohos` 源码，可使用如下命令编译：
``` shell
gn gen out/x86_64 --args='is_clang=true target_os="ohos" target_cpu="x86_64" ohos_extra_ldflags="-static-libstdc++" is_official_build=true ohos_sdk_native_root="path/to/ohos/sdk"'
ninja -C out/x86_64
```

**x86_64 模拟器注意**：

- 模拟器不支持硬件编解码。请使用默认 `PeerConnectionFactory`，其内部同时包含软件编解码能力，可在硬件编解码不可用时自动改用软件编解码；若仅使用 `HardwareVideoEncoderFactory` / `HardwareVideoDecoderFactory`，模拟器上因缺少硬件编解码支持，将出现编解码协商失败。
- 自 API 18 起，模拟器支持调用本地计算机的摄像头。请确保本地计算机上存在可用的摄像头（不支持通过 USB 连接的摄像头）；示例工程中，若未获取到可用摄像头，将回退为仅音频通信模式。

**注意**
> 如果不指定`ohos_extra_ldflags="-static-libstdc++"`参数，则需要将sdk中的`libc++_shared.so`一起放到对应目录下。

## 运行调试

准备好两台满足当前工程 `compatibleSdkVersion` 的设备（也可以仅使用一台设备，然后与浏览器测试端连接）。确保设备或浏览器上可正常使用摄像头和音频设备，并连接好网络。

使用 DevEco Studio 打开本工程，连接设备后点击运行。原有通话示例通过首页“switch to working page”进入，在文本框输入对端 ID 后点击 Connect。多设备适配通过首页“多设备相机实验室（A–F）”进入。

首页“SEI测试”用于本机回环，点击“开始回环”后测试；双端测试则在两台设备的“switch to working page”建立视频通话后操作视频下方的 SEI 区域。SEI 接口及发送条件见 [EncoderWrapper](../api/libwebrtc.md#encoderwrapper)。

填写“SEI文本”和 8 个两位十六进制字节（00–FF），点击“发送一条”或“连续发送”（每 200 毫秒一条），在接收区域核对文本、序号和二进制数据。未收到数据时只显示接收标题；断开或离开页面时停止连续发送。显示内容来自最近一次回调，不保证与当前视频帧严格同步。

Demo 使用默认 UUID，数据格式为“4 字节大端序号 + UTF-8 文本 + 8 字节二进制数据”。这是示例的业务约定，应用可自行定义。接收端需提取 SEI 并按相同格式解析后展示，普通浏览器测试页未提供该展示功能。

**说明**
> 本工程所使用的信令服务器（wss://youzhi.life:8443），及浏览器端测试网页（https://webrtc.youzhi.life）仅用于测试，请勿作其它用途。

## 多设备相机实验室（A–F）

允许相机权限后，页面通过库侧 `CameraPreviewSession` 管理采集。库接入方法见 [SDK README](../README_zh.md#多设备相机预览)。

| 项目 | 操作与核对内容 |
| --- | --- |
| A 断点布局 | 调整窗口大小或折叠设备，核对 sm（宽度 <600vp）上下布局、md（600–839vp）及 lg（≥840vp）左右布局；自动旋转策略按窗口短边是否达到 600vp 判断 |
| B 相机设备 | 切换前后置；折叠/展开后检查设备列表刷新、镜头可用性及用户偏好保持 |
| C 预览比例 | 切换全屏、16:9、4:3、1:1，确认等比裁切、物体不变形 |
| D 预览方向验证 | 四向持握后保存预览截图并检查方向和裁剪；手动启动本机双 PeerConnection 回环，检查编码/解码后画面 |
| E 悬停页面 | 半折叠时核对预览与操作区分居折痕两侧、避让区域不可操作；切换预览侧并检查横竖屏及展开后的布局 |
| F 本地与对端视频 | 两端填入相同信令服务及不同的 2–4 位本机 ID，注册后由一端输入对端 ID 发起呼叫；核对旋转、折叠、换镜头和前后台恢复后的画面 |

D 项截图来自合成后的取景画面；本机回环无需外部信令或 STUN/TURN，可验证本机编解码链路。对照带“上/左/右”和文字的标记人工判断方向与镜像，收到帧或显示尺寸不代表验证通过。

F 项仅测试视频，使用 STUN，未配置 TURN；受限网络下可能无法建立连接。双端画面需要在实际设备上核对，本机回环结果不能替代双端验证。页面会在轨道变化时更新通话发送轨道。

建议覆盖普通手机、折叠/三折叠设备及多显示器迁移，并测试窗口大小不变时的显示器切换、前后台恢复和采集停帧。停帧监测只检查采集计数增长，不检测画面内容冻结或渲染停滞。当前示例不提供原始帧导出或图像/扫码识别。

## 自动检查

在仓库根目录运行现有 Node.js 测试。需要可解析的 `typescript` 包，或通过 `TYPESCRIPT_PATH` 指定本机 DevEco 的 `typescript.js`：

```powershell
$env:TYPESCRIPT_PATH = '<DevEco 安装目录>/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript/lib/typescript.js'
node --test sdk/ohos/tests/camera_preview_session.test.cjs sdk/ohos/tests/demo_multidevice.test.cjs sdk/ohos/tests/preview_loopback.test.cjs sdk/ohos/tests/preview_peer_call.test.cjs
```

测试覆盖会话恢复、旧接口兼容、布局、回环与双端信令逻辑，使用平台替身，不会启动设备相机或连接真实信令服务。设备侧约束测试位于 [`VideoSource.test.ets`](entry/src/ohosTest/ets/test/VideoSource.test.ets)。

设备侧 [Sei.test.ets](entry/src/ohosTest/ets/test/Sei.test.ets) 覆盖 SEI 实际收发、输入内存复制、多个监听器及关闭行为，需支持 H.264 的设备和相机权限。双端还需验证分辨率变化、前后台、重连、弱网及未适配 SEI 的对端。

ArkTS 编译、HAP 打包、完整 API 12 SDK 实编及配套新 `.so` 的真机验证应分别记录；自动测试通过不能替代后两项。
