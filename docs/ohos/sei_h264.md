# H.264 SEI 收发与验证

此功能需要从当前源码重新编译 `libohos_webrtc.so`，同步使用配套类型声明。仅替换 ArkTS 文件不能增加 Native 能力。实现位于 `sdk/ohos/src/ohos_webrtc/sei`，通过 WebRTC 已有 FrameTransformer 在编码后/RTP 打包前注入，在 RTP 组帧后/解码前解析。

## 支持范围与接口约定

- 支持 H.264 Annex B 码流中的 `user_data_unregistered`（SEI payloadType=5）。发送要求 `packetization-mode=1`，以支持 SEI NAL 的 RTP 分片；模式 0 的发送 ready 保持 false，避免大 SEI 使整帧打包失败。接收解析不限制该模式。发送内容为标准 16 字节 UUID + 业务字节，SDK 完成长度编码、转义和 NAL 插入，保留原有 AUD/SPS/PPS/SEI 和视频内容。
- 业务负载采用 `ArrayBuffer`，发送长度为 1–4096 字节；单个 sender 最多等待 64 条。每个新的编码帧 RTP 时间戳消耗一条，同时间戳的 simulcast 层复制这条消息（缓存最近 16 个时间戳）。`injectedMessages` 按实际编码输出帧计数。
- 默认 UUID 为 `5f7d3c20-9b61-4e8a-a2f0-6c8d174b9301`，也可逐次传入规范的 36 字符 UUID。接收端回调全部符合长度限制的 type=5 消息，包括其他 UUID；应用负责筛选。
- `writeSei()` 同步复制输入内存。成功表示**入队**，不表示已经注入或送达。队列满、未就绪、关闭、长度或 UUID 非法时抛出异常。
- `ready` 在实际看到 H.264 Annex B 视频帧后变为 true。需先调用 `getEncoderWrapper()` 安装转换器，再等待 ready；格式不支持时保持 false。编码器暂停可能停止消耗队列；编码格式改变/转换器卸载/连接关闭时清空待发数据。
- `clearSei()` 清除待发消息及缓存，不撤回已经交给 RTP 的帧。
- 回调包含 `uuid`、`data`、`payloadType`、`codec`、`ssrc`、`rtpTimestamp`。`data` 不含 UUID。RTP 时间戳为 90 kHz 编码帧时间戳，不是毫秒/墙钟时间；收到回调不表示该帧已经显示。
- 每个 N-API receiver 包装对象可注册一个 `onsei`，不同包装对象共享同一 Native 转换器，可各自监听（最多 16 个并存注册）。赋值 null 取消该监听及尚未执行的通知。注册期间 Native 保持包装对象存活，`pc.close()` 同步取消所有通知并释放待发数据。
- ArkTS 通知队列每个监听器最多 64 条，队列满时丢弃通知、不阻塞视频。后续事件的 `droppedMessages` 为该监听器累计通知丢弃数。应用回调异常会被隔离并记录日志。
- 解析器跳过其他 SEI 消息类型及不合法/超限的 SEI，继续转发原视频。单个接收编码帧最多通知 64 条消息，单条业务负载最多 4096 字节。
- SEI 随视频丢包/丢帧而可能丢失，不提供送达确认或业务去重。普通 SRTP 通话无需额外加解密处理；本次没有接入自定义 FrameEncryptor/FrameDecryptor 的组合路径，也不提供 H.265 SEI。

## ArkTS 使用

```typescript
import webrtc from '@ohos/webrtc';

// pc、factory 和本地 videoTrack 由应用创建；让静态能力查询使用同一工厂。
webrtc.PeerConnectionFactory.setDefault(factory);
const h264 = webrtc.RTCRtpSender.getCapabilities('video')?.codecs.filter(
  (codec: webrtc.RTCRtpCodec) => codec.mimeType.toLowerCase() === 'video/h264' &&
    (codec.sdpFmtpLine ?? '').split(';').some((part: string) => part.trim() === 'packetization-mode=1')) ?? [];
if (h264.length === 0) { throw new Error('当前设备/构建不支持 H.264'); }

const transceiver = pc.addTransceiver(videoTrack, { direction: 'sendonly' });
transceiver.setCodecPreferences(h264); // Offer/Answer 之前调用；示例只允许 H.264。
const encoder = transceiver.sender.getEncoderWrapper();

// 然后完成正常 SDP、ICE 交换。在实际开流后用有超时的轮询等待 encoder.ready。
// 不要同时对同一个 videoTrack 再调用一次 addTrack。
if (encoder.ready) {
  const bytes = new Uint8Array([0, 0, 1, 0, 255, 128]);
  encoder.writeSei(bytes.buffer);
  // 或指定业务 UUID：encoder.writeSei(bytes.buffer, '00112233-4455-6677-8899-aabbccddeeff');
}

remotePc.ontrack = (event: webrtc.RTCTrackEvent) => {
  if (event.track.kind !== 'video') { return; }
  event.receiver.onsei = (sei: webrtc.RTCSeiEvent) => {
    const bytes = new Uint8Array(sei.data);
    console.info(`SEI uuid=${sei.uuid}, bytes=${bytes.length}, ssrc=${sei.ssrc}, rtp=${sei.rtpTimestamp}`);
  };
}; // 在远端 setRemoteDescription/开流之前注册 ontrack。

// 可单独取消某个 receiver 的监听：receiver.onsei = null;
// 结束时 pc.close()/remotePc.close() 会取消各自连接的 SEI 状态。
```

`EncoderWrapper` 是 sender 的 SEI 控制对象，通过 `getEncoderWrapper()` 获取，不支持直接 new。重复获取包装对象共享同一条发送队列。对音频 sender 获取该对象、对音频 receiver 设置 onsei 均报错。

示例使用只保留 H.264 的能力列表，未配置 RTX 等辅助格式；正式业务可在保持 H.264 约束的同时按协商规则保留匹配的辅助编码。

## Native 构建和集成

按 `docs/ohos/webrtc_build.md` 准备依赖及 Native SDK。示例为 Linux shell，在仓库根目录执行，SDK 路径必须指向 native 目录：

```sh
gn gen out/arm64 --args='is_clang=true target_os="ohos" target_cpu="arm64" ohos_extra_ldflags="-static-libstdc++" is_official_build=true ohos_sdk_native_root="/path/to/sdk/native"'
ninja -C out/arm64 libohos_webrtc
```

将生成的 `libohos_webrtc.so` 复制到 `sdk/ohos/har_hap/ohos_webrtc/libs/arm64-v8a/`，再使用 DevEco 构建 HAR/HAP。其他 ABI 必须分别构建并放到对应目录。若不静态链接 C++ 运行库，按示例 README 携带匹配的 `libc++_shared.so`。

需要同步的类型声明位于 `sdk/ohos/api/libohos_webrtc/webrtc.d.ts` 和 `sdk/ohos/har_hap/ohos_webrtc/src/main/libohos_webrtc/webrtc.d.ts`。新增接口注册、源文件和依赖已加入 `napi_module.cpp`、`sdk/ohos/BUILD.gn`。

## 验证

### 1. 主机独立格式测试

只需要 Python 和本机可运行的 C++17 编译器，不依赖 OHOS 或 WebRTC 三方库：

```sh
python sdk/ohos/tests/run_h264_sei_tests.py --cxx /usr/bin/g++
# 支持时可附加 --sanitize 运行 ASan/UBSan。
```

测试生产代码 `h264_sei.cpp`，包含手写标准样本、NAL 顺序、转义、中文/二进制、长度扩展边界、最大长度、多个消息、500 次随机往返和 10000 个不合法输入。

主机也可执行帧转换器测试，传入完整 Abseil、GoogleTest 源码根目录（后者包含 `include/` 和 `src/`）：

```sh
python sdk/ohos/tests/run_h264_sei_tests.py --cxx /usr/bin/g++ --transformers --abseil /path/to/abseil-cpp --gtest /path/to/googletest
```

### 2. Native 帧转换器测试

完整源码及依赖环境中启用测试后：

```sh
gn gen out/sei-test --args='is_clang=true target_os="ohos" target_cpu="arm64" rtc_include_tests=true ohos_sdk_native_root="/path/to/sdk/native"'
ninja -C out/sei-test sei_unittests
```

把生成的测试可执行文件及其运行库推送到目标设备执行 `sei_unittests`。此处是 OHOS 交叉编译产物，不能直接在 Linux/Windows 主机执行。覆盖队列容量、就绪状态、FIFO、simulcast 时间戳复制、多监听器、视频透传和关闭。

### 3. 本机真实 RTP 手动验证

安装新 `.so` 构建的示例 HAP，从首页进入 **SEI测试**：

1. 页面上方展示本次测试的 SEI 数据说明。点击右上角“开始回环”并允许相机权限，等待发送按钮可用并出现接收视频。同一按钮在启动后显示“停止回环”，点击后结束测试并恢复为“开始回环”。
2. 输入中文/英文文本并发送。页面附加二进制转义测试字节和序号；入队后立即改写原 ArrayBuffer，用于检查 Native 已复制数据。
   接收视频底部会叠加回调中解析出的文本、序号和 RTP 时间戳，由测试人员对照发送内容观察。修改输入框但不发送时，叠加文字应保持不变；再次发送后才更新。这里显示最近收到的一条 SEI，不代表与当前显示帧严格同步，也不会把文字写入视频像素。
3. “发送一条”仅发送一次；“连续发送”立即发送一条，此后每 200 毫秒发送一条，按钮变为“停止发送”，再次点击结束连续发送。停止回环或退出页面也会结束连续发送。对照输入文本观察接收内容，并观察序号和 RTP 时间戳。
4. 接收窗口按实际视频帧的宽高比调整，旋转设备时检查窗口跟随旋转后的帧比例变化，画面无拉伸。停止、重新启动、退出页面，检查叠加文字已清空、无旧回调、无崩溃。

该页面在一台设备上建立两个 PeerConnection，交换 SDP/ICE 并通过真实 RTP 收发视频；SEI 不通过信令或 DataChannel 传递。

### 4. 设备侧 Hypium 测试

新增 `entry/src/ohosTest/ets/test/Sei.test.ets`，已注册到 List.test。运行前授予测试应用相机权限，并使用支持 H.264 的真机/构建；不具备能力时该收发测试会失败，不能作为通过记录。

测试两份 receiver 包装对象同时监听、1/239/240/4096 字节往返、输入内存复制、取消单个监听、自定义 UUID、非法参数和关闭后拒绝写入。完整设备测试可能需要为测试运行器配置足够的异步超时。

### 5. 双端和异常验证

在已有双端通话中按上述 ArkTS 代码绑定发送和接收接口，确认两端协商为 H.264；核对真实网络下 UUID 和业务数据逐字节一致。补测软/硬编码、轨道替换、分辨率变化、前后台、重连、多 sender 隔离、弱网和长时间运行。受控无丢包验证与弱网丢失统计应分别记录。

普通网络抓包为加密 SRTP，不能直接搜索业务明文；需要查看 SEI 原始 NAL 时，在测试构建的 RTP 打包前或接收组帧后导出编码帧，用独立解析器交叉检查。

## 本次本地检查记录（2026-10-27）

- 主机执行独立格式测试通过：手写样本、边界/转义、500 次随机往返、10000 个异常输入。
- 主机编译并执行上述 4 个帧转换器 GoogleTest 用例通过，包含 packetization-mode=0 拒绝、队列、同时间戳多层复制和解析后视频透传。
- 使用已安装的 DevEco OHOS clang 15.0.4，对新增/修改的 8 个 Native `.cpp` 完成 arm64 交叉语法检查。缺少仓库完整三方依赖时，检查使用临时 Abseil 头文件；这不代表正式 GN 构建或二进制链接成功。
- 使用 DevEco 自带 ETS/TypeScript 解析器（启用 UI 组件解析），对 7 个相关 `.ets`/`.d.ts` 文件完成语法检查，新增 SEI 接口在两份声明中一致。
- 尚未执行：完整 GN/Ninja `.so` 链接、DevEco 全量 ArkTS 类型检查与 HAR/HAP 打包、设备侧 Hypium 测试和真机收发。以上步骤需要在配套环境完成，不能以本地主机检查替代。
