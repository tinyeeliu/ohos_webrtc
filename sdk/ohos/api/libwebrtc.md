# ohos_webrtc

## 多设备预览扩展的使用边界

`libohos_webrtc/index.d.ts` 通过 `export * from './webrtc'` 导出声明；原生模块也注册了对应方法。
因此，下面两个扩展方法均对 SDK 使用方可见，不能因为当前只有 SDK 控制器调用就视为私有 API。
以下调用方结论来自本仓库的生产代码，不代表仓库外的应用没有直接使用这些接口。

| 接口 | 本仓库中的调用方 | 使用边界与修改原因 |
| --- | --- | --- |
| `VideoSource.applyDisplayRotation(degrees)` | 示例应用 `MultiDeviceRtcLabPage` 的窗口适配逻辑 | 对外的 OHOS 扩展。同步所在窗口当前已生效的显示角度，让采集层更新预览旋转。 |
| `NativeVideoRenderer.setSurfaceSize(width, height)` | SDK `VideoRenderController` | 对外可见的底层渲染扩展。控制器用它同步 Surface 显示尺寸；直接管理渲染器的应用也可以调用。 |
| `NativeVideoRenderer.onFrameResolutionChanged` | 原生渲染器触发，SDK 控制器订阅；另有测试注册 | 原有公开事件，不是新增方法。保留 `(ev: Event) => any` 签名，帧字段通过 `FrameResolutionChangedEvent` 读取。 |
| `VideoRenderController.onFrameSizeChanged` | 示例应用订阅 | 控制器对应用提供的通知，返回旋转后的显示帧宽高，用于等比布局。 |

调用链：应用同步显示角度 → VideoSource 原生绑定 → 采集线程 → CameraCapturer；
Surface 生命周期/页面尺寸变化 → VideoRenderController → NativeVideoRenderer → 原生渲染线程。
反向通知链：原生帧尺寸变化 → 原有 Event 回调 → 控制器内部解析 → `onFrameSizeChanged`。
C++ 层的转发与实现属于内部调用，但 ArkTS 可见的绑定入口不是内部私有接口。

`webrtc.d.ts` 导出 `FrameResolutionChangedEvent`（继承 `Event`），描述原生已有的 `type`、`width`、`height`、`rotation` 字段。
`onFrameResolutionChanged` 的回调参数仍声明为 `Event`；订阅者需要读取帧字段时，将事件断言为 `FrameResolutionChangedEvent`，`VideoRenderController` 即按此方式读取并校验。
此类型不改变原生事件载荷，也不要求现有订阅者修改回调参数类型。

两个扩展方法需搭配实现它们的 `.so`。更新声明不会使旧二进制自动支持新方法。
控制器对缺少 `setSurfaceSize` 的旧库会记录错误并跳过调用，不代表旧库具备相同的尺寸适配能力。
这两个方法在接口中声明为可选成员（`applyDisplayRotation?`、`setSurfaceSize?`），调用前需判断是否存在；手工实现这些接口的适配层或测试替身可以不实现，但不实现时不具备对应的预览适配能力。
本次只澄清边界并保留事件原签名，没有隐藏或删除已经暴露的两个扩展方法。

## RTCErrorDetailType

表示 WebRTC 中可能出现的错误详细类型。

| 名称                             | 类型     | 说明          |
|--------------------------------|--------|-------------|
| data-channel-failure           | string | 数据通道发生了故障   |
| dtls-failure                   | string | DTLS 安全传输失败 |
| fingerprint-failure            | string | 安全证书指纹校验失败  |
| hardware-encoder-error         | string | 硬件编码器发生错误   |
| hardware-encoder-not-available | string | 硬件编码器不可用    |
| sctp-failure                   | string | SCTP 传输失败   |
| sdp-syntax-error               | string | SDP 语法错误    |

## RTCIceProtocol

用于表示 ICE 协议的类型别名。

| 名称  | 类型     | 说明           |
|-----|--------|--------------|
| tcp | string | 传输控制协议（TCP）  |
| udp | string | 用户数据报协议（UDP） |

## RTCIceCandidateType

表示 ICE 候选者类型的类型别名。

| 名称    | 类型     | 说明      |
|-------|--------|---------|
| host  | string | 主机本地地址  |
| prflx | string | 反射地址    |
| relay | string | 中继服务器   |
| srflx | string | 服务器反射地址 |

## RTCIceTcpCandidateType

表示 ICE TCP 候选者类型的类型别名。

| 名称      | 类型     | 说明           |
|---------|--------|--------------|
| active  | string | 主动连接到远程对等方   |
| passive | string | 等待远程对等方连接    |
| so      | string | 尝试与对等端同时打开连接 |

## RTCIceGathererState

用于表示 ICE 收集器的状态。

| 名称        | 类型     | 说明                 |
|-----------|--------|--------------------|
| new       | string | ICE 收集器处于初始状态      |
| gathering | string | ICE 收集器正在收集候选者     |
| complete  | string | ICE 收集器已经完成了候选者的收集 |

## RTCIceTransportState

表示 WebRTC 中 ICE 传输的不同状态。

| 名称           | 类型     | 说明              |
|--------------|--------|-----------------|
| new          | string | ICE 传输处于初始状态    |
| checking     | string | ICE 传输正在检查连接    |
| connected    | string | ICE 传输已连接但还没有完成 |
| completed    | string | ICE 传输已完成所有连接检查 |
| disconnected | string | ICE 传输已断开连接     |
| failed       | string | ICE 传输连接失败      |
| closed       | string | ICE 传输已关闭       |

## RTCIceRole

表示 ICE 代理在连接协商过程中的角色。

| 名称          | 类型     | 说明            |
|-------------|--------|---------------|
| unknown     | string | ICE 角色未知      |
| controlling | string | ICE 代理处于控制角色  |
| controlled  | string | ICE 代理处于被控制角色 |

## RTCIceComponent

表示 ICE 候选者的组件类型。

| 名称   | 类型     | 说明                   |
|------|--------|----------------------|
| rtp  | string | 用于传输 RTP 数据的 ICE 组件  |
| rtcp | string | 用于传输 RTCP 数据的 ICE 组件 |

## RTCSdpType

描述 SDP 会话描述类型的枚举类型。

| 名称       | 类型     | 说明                                   |
|----------|--------|--------------------------------------|
| offer    | string | 表示一个 SDP 描述，用于向另一端发起媒体会话的邀请          |
| answer   | string | 表示一个 SDP 描述，用于响应来自远端的媒体会话邀请          |
| pranswer | string | 表示一个 SDP 描述，用于发送给远端作为对 Offer 的临时响应   |
| rollback | string | 表示一个 SDP 描述，用于指示取消当前的会话描述状态，回滚到之前的状态 |

## BinaryType

表示 WebRTC 中可能的二进制数据类型。

| 名称          | 类型     | 说明                  |
|-------------|--------|---------------------|
| blob        | string | 处理文件或其它类型的大量二进制数据   |
| arraybuffer | string | 通用的、固定长度的原始二进制数据缓冲区 |

## DataChannelState

表示数据通道的状态。

| 名称         | 类型     | 说明                   |
|------------|--------|----------------------|
| closed     | string | 表示数据通道已关闭            |
| closing    | string | 表示数据通道正在关闭中          |
| connecting | string | 表示数据通道正在连接中          |
| open       | string | 表示数据通道已经打开并且可以进行数据传输 |

## RTCDtlsTransportState

表示 DTLS 传输状态的可能值。

| 名称         | 类型     | 说明                                                     |
|------------|--------|--------------------------------------------------------|
| new        | string | 此状态表示传输对象处于初始状态，连接尚未建立，适用于初始配置和资源分配                    |
| connecting | string | 此状态表示正在进行 DTLS 握手过程，用于建立安全通道。在此阶段，可以监控握手进度，并处理潜在的连接问题  |
| connected  | string | 此状态表示传输已准备好发送和接收加密数据。开发者可以在此状态下开始安全的数据传输               |
| closed     | string | 此状态表示连接已正常关闭。可以在此状态下进行资源清理和释放，并通知应用层连接已终止              |
| failed     | string | 此状态表示传输因错误无法建立或维持连接。开发者需要在此状态下处理错误，可能需要重新尝试连接或通知用户连接失败 |

## RTCIceGatheringState

表示 ICE 收集的状态。

| 名称        | 类型     | 说明                  |
|-----------|--------|---------------------|
| new       | string | 对等连接刚刚创建，尚未进行任何网络连接 |
| gathering | string | ICE 代理正在收集候选的连接     |
| complete  | string | ICE 代理完成了候选的收集      |

## RTCIceConnectionState

表示 ICE 连接状态。

| 名称           | 类型     | 说明                                                         |
|--------------|--------|------------------------------------------------------------|
| new          | string | ICE 代理正在搜集地址或者等待远程候选可用                                     |
| checking     | string | ICE 代理已收到至少一个远程候选，并进行校验，无论此时是否有可用连接。同时可能在继续收集候选            |
| connected    | string | ICE 代理至少对每个候选发现了一个可用的连接，此时仍然会继续测试远程候选以便发现更优的连接。同时可能在继续收集候选 |
| completed    | string | ICE 代理已经发现了可用的连接，不再测试远程候选                                  |
| failed       | string | ICE 候选测试了所有远程候选没有发现匹配的候选                                   |
| disconnected | string | 测试不再活跃                                                     |
| closed       | string | ICE 代理关闭，不再应答任何请求                                          |

## RTCSignalingState

表示与对等端连接过程中的信令处理状态。

| 名称                   | 类型     | 说明                                             |
|----------------------|--------|------------------------------------------------|
| stable               | string | 没有正在进行的 offer/answer 的交换                       |
| have-local-offer     | string | 成功应用了类型为 offer 的本地描述                           |
| have-remote-offer    | string | 成功应用了类型为 offer 的远程描述                           |
| have-local-pranswer  | string | 成功应用了类型为 offer 的远程描述，并且成功应用了类型为 pranswer 的本地描述 |
| have-remote-pranswer | string | 成功应用了类型为 offer 的本地描述，并且成功应用了类型为 pranswer 的远程描述 |
| closed               | string | RTCPeerConnection 已关闭                          |

## RTCPeerConnectionState

表示当前所有被使用的 ICE 连接的状态。

| 名称           | 类型     | 说明                                                                                                   |
|--------------|--------|------------------------------------------------------------------------------------------------------|
| new          | string | 表示至少有一个 ICE 连接处于 new 状态，并且没有连接处于以下状态： connecting、checking、failed、disconnected，或者这些连接都处于 closed 状态    |
| connecting   | string | 表示至少有一个 ICE 连接处于正在建立连接的状态；也就是说，它们的 iceConnectionState 值为 checking 或 connected，并且没有连接处于 failed 状态     |
| connected    | string | 表示每一个 ICE 连接要么正在使用（connected 或 completed 状态），要么已被关闭（closed 状态）；并且，至少有一个连接处于 connected 或 completed 状态 |
| disconnected | string | 表示至少有一个 ICE 连接处于 disconnected 状态，并且没有连接处于 failed、connecting 或 checking 状态                            |
| failed       | string | 表示至少有一个 ICE 连接处于 failed 的状态                                                                          |
| closed       | string | RTCPeerConnection 已关闭                                                                                |


## RTCBundlePolicy

表示与捆绑相关的 ICE 收集策略。

| 名称         | 类型     | 说明                                                                   |
|------------|--------|----------------------------------------------------------------------|
| balanced   | string | ICE 代理为每种使用的媒体类型（视频、音频和数据）收集 ICE 选项。如果远程对等端不支持捆绑，在单独的传输上仅协商一个音频和视频轨道 |
| max-compat | string | ICE 代理为每个轨道收集 ICE 候选者。如果远程对等端不支持捆绑，在单独的传输上协商所有的媒体轨道                  |
| max-bundle | string | ICE 代理只收集一个轨道的 ICE 候选者。如果远程对等端不支持捆绑，仅协商一个媒体轨道                        |


## RTCRtcpMuxPolicy

表示与RTCP多路复用相关的 ICE 收集策略。

| 名称      | 类型     | 说明                                                                   |
|---------|--------|----------------------------------------------------------------------|
| require | string | 仅在 RTP 候选者上为 RTP 和和 RTCP 多路复用收集 ICE 候选。如果远程对等端不能使用 RTCP 多路复用，会话协商将失败 |

## RTCIceTransportPolicy

ICE 候选策略，WebRTC 使用该策略向应用程序显示允许的候选者，只有这些候选者将用于连接检查。

| 名称    | 类型     | 说明                                |
|-------|--------|-----------------------------------|
| all   | string | ICE 代理可以使用任何类型的候选者                |
| relay | string | ICE 代理只使用媒体中继候选者，如经过 TURN 服务器的候选者 |

## RTCRtpTransceiverDirection

可能的字符串值，每个值表示一种传输方向的状态。

| 名称       | 类型     | 说明                                     |
|----------|--------|----------------------------------------|
| inactive | string | 表示既不发送也不接收媒体数据，传输器处于非活动状态。             |
| recvonly | string | 表示只接收媒体数据，不发送数据，这通常用于希望仅监听远端媒体流的情况。    |
| sendonly | string | 表示只发送媒体数据，不接收数据，这通常用于希望仅向远端发送媒体流的情况。   |
| sendrecv | string | 表示既发送又接收媒体数据，这是最常见的双向通信模式。             |
| stopped  | string | 表示传输器已停止，不再发送或接收媒体数据，此状态表示传输器的生命周期已结束。 |

## RTCSctpTransportState

描述 SCTP 传输的状态的枚举类型。

| 名称         | 类型     | 说明                   |
|------------|--------|----------------------|
| connecting | string | 表示 SCTP 传输正在建立连接的过程中 |
| connected  | string | 表示 SCTP 传输已成功建立连接    |
| closed     | string | 表示 SCTP 传输已关闭        |

## RTCStatsType

用于指定 WebRTC 统计信息（RTCStats）的类型。

| 名称                  | 类型     | 说明                      |
|---------------------|--------|-------------------------|
| candidate-pair      | string | 表示 ICE 候选对的统计信息         |
| certificate         | string | 表示与安全证书相关的统计信息          |
| codec               | string | 表示编解码器的统计信息             |
| data-channel        | string | 表示数据通道的统计信息             |
| inbound-rtp         | string | 表示入站 RTP 流的统计信息         |
| local-candidate     | string | 表示本地 ICE 候选者的统计信息       |
| media-playout       | string | 表示媒体播放的统计信息             |
| media-source        | string | 表示媒体源的统计信息              |
| outbound-rtp        | string | 表示出站 RTP 流的统计信息         |
| peer-connection     | string | 表示 PeerConnection 的统计信息 |
| remote-candidate    | string | 表示远端 ICE 候选者的统计信息       |
| remote-inbound-rtp  | string | 表示远端 ICE 候选者的统计信息       |
| remote-outbound-rtp | string | 表示远端的出站 RTP 流的统计信息。     |
| transport           | string | 表示传输层的统计信息              |

## RTCStatsIceCandidatePairState

描述 ICE 候选对状态的枚举类型。

| 名称          | 类型     | 说明             |
|-------------|--------|----------------|
| failed      | string | ICE 候选对连接失败    |
| frozen      | string | ICE 候选对连接被冻结   |
| in-progress | string | ICE 候选对连接正在进行中 |
| succeeded   | string | ICE 候选对连接成功建立  |
| waiting     | string | ICE 候选对处于等待状态  |

## MediaStreamTrackState

媒体流轨道的状态。

| 名称    | 类型     | 说明                 |
|-------|--------|--------------------|
| live  | string | 轨道处于活动状态，正在发送媒体数据。 |
| ended | string | 轨道已经结束，不再发送媒体数据。   |

## MediaDeviceKind

媒体设备类型

| 名称          | 类型     | 说明           |
|-------------|--------|--------------|
| audioinput  | string | 音频输入设备，如麦克风。 |
| audiooutput | string | 音频输出设备，如扬声器。 |
| videoinput  | string | 视频输入设备，如相机。  |

## HighResTimeStamp

表示高分辨率时间戳。

| 名称               | 类型     | 说明             |
|------------------|--------|----------------|
| HighResTimeStamp | number | 表示从某个固定起点开始的时间 |

## EpochTimeStamp 

时间戳

| 名称             | 类型     | 说明        |
|----------------|--------|-----------|
| EpochTimeStamp | number | 用于存储时间戳信息 |

## ConstrainBoolean

用于表示 boolean 或 ConstrainBooleanParameters 类型。

| 名称             | 类型                                  | 说明                                                         |
| ---------------- | ------------------------------------- | ------------------------------------------------------------ |
| ConstrainBoolean | boolean \| ConstrainBooleanParameters | boolean：表示可以直接接受 true 或 false 的布尔值。ConstrainBooleanParameters：表示一个接口或类型，通常用于更复杂的约束条件，用于描述对布尔值的约束条件，例如期望的值或确切的值。 |

## ConstrainULong

用于表示 number（unsigned long） 或 ConstrainULongRange 类型。

| 名称           | 类型                          | 说明                                                         |
| -------------- | ----------------------------- | ------------------------------------------------------------ |
| ConstrainULong | number \| ConstrainULongRange | number：表示可以直接接受一个数值。ConstrainULongRange：表示一个接口或类型，通常用于更复杂的约束条件。 |

## ConstrainDouble

用于表示 number（double） 或 ConstrainDoubleRange 类型。

| 名称            | 类型                            | 说明                   |
| --------------- | ------------------------------- |----------------------|
| ConstrainDouble | number \| ConstrainDoubleRange | number：表示可以直接接受一个数值。ConstrainDoubleRange：表示一个接口或类型，通常用于更复杂的约束条件。 |

## ConstrainString

用于表示 string 或 string[] 或 ConstrainStringParameters 类型。

| 名称            | 类型                                            | 说明                                                         |
| --------------- | ----------------------------------------------- | ------------------------------------------------------------ |
| ConstrainString | string \| string[] \| ConstrainStringParameters | string：表示可以直接接受一个字符串。string[]：表示可以接受一个字符串数组，用于描述多个可能的字符串值。ConstrainStringParameters：表示一个接口或类型，通常用于更复杂的约束条件，可以包含 exact和 ideal等属性，用于描述对字符串值的约束条件，例如期望的值或确切的值。 |

## AlgorithmIdentifier

用于表示 string 或 Algorithm 类型。

| 名称            | 类型                                            | 说明                                                         |
| --------------- | ----------------------------------------------- | ------------------------------------------------------------ |
| AlgorithmIdentifier | string \| Algorithm | string：表示可以直接接受一个字符串。Algorithm：表示一个接口或类型，用于更详细的描述一个算法 |

## RTCErrorInit

用于初始化RTCError。

### 属性

| 名称          | 类型               | 可读 | 可写 | 必填 | 说明                             |
| ------------- | ------------------ | ---- | ---- | ---- | -------------------------------- |
| errorDetail   | RTCErrorDetailType | 是   | 否   | 是   | 提供关于错误类型的明确描述  |
| sdpLineNumber | number             | 是   | 否   | 否   | 指示出错的SDP行号 |
| sctpCauseCode | number             | 是   | 否   | 否   | 提供了导致连接问题的具体错误代码  |
| receivedAlert | number             | 是   | 否   | 否   | 指示接收到的警报代码  |
| sentAlert     | number             | 是   | 否   | 否   | 指示发送的警报代码  |

## RTCError

继承自Error，用于描述WebRTC相关的错误信息。

### 属性

| 名称          | 类型               | 可读  | 可写 | 说明                             |
| ------------- | ------------------ | ---- | ---- | -------------------------------- |
| errorDetail   | RTCErrorDetailType | 是   | 否   | 提供关于错误类型的明确描述 |
| sdpLineNumber | number             | 是   | 否   | 指示出错的SDP行号  |
| sctpCauseCode | number             | 是   | 否   | 提供了导致连接问题的具体错误代码 |
| receivedAlert | number             | 是   | 否   | 指示接收到的警报代码 |
| sentAlert     | number             | 是   | 否   | 指示发送的警报代码 |

## Event 

表示一个事件（当前无任何内容，由特定事件类型继承）。

## EventTarget 

表示当某件事情发送时，该对象会分发相关事件（当前无任何内容）。

## RTCErrorEvent

继承自 Event，提供了关于发生的错误的详细描述。

### 属性

| 名称  | 类型     | 可读 | 可写 | 说明         |
| ----- | -------- | ---- | ---- | ------------ |
| error | RTCError | 是   | 否   | 错误详细信息 |

## RTCTrackEvent

继承自 Event，向 PeerConnection 中添加媒体轨道时会发送该事件。

### 属性

| 名称          | 类型                                         | 可读  | 可写  | 说明              |
|-------------|--------------------------------------------|-----|-----|-----------------|
| receiver    | [RTCRtpReceiver](#rtcrtpreceiver)          | 是   | 否   | 表示与事件相关联的接收器。   |
| track       | [MediaStreamTrack](#mediastreamtrack)      | 是   | 否   | 表示与事件相关联的媒体流轨道。 |
| streams     | ReadonlyArray<[MediaStream](#mediastream)> | 是   | 否   | 表示与事件相关的媒体流数组。  |
| transceiver | [RTCRtpTransceiver](#rtcrtptransceiver)    | 是   | 否   | 表示与事件相关联的收发器。   |

## RTCPeerConnectionIceEvent

表示 PeerConnection 中 ICE 候选者相关的事件。

### 属性

| 名称      | 类型            | 可读  | 可写 | 说明                               |
| --------- | --------------- | ---- | ---- | ---------------------------------- |
| candidate | RTCIceCandidate | 是   | 否   | ICE 候选者 |

## RTCPeerConnectionIceErrorEvent

表示 ICE 错误的详细信息，随`icecandidateerror`事件发布。

###  属性

| 名称      | 类型   | 可读 | 可写 | 说明                                                         |
| --------- | ------ | ---- | ---- | ------------------------------------------------------------ |
| address   | string | 是   | 否   | 与 STUN 或 TURN 服务器通信的本地IP地址 |
| port      | number | 是   | 否   | 一个无符号整数值，与 STUN 或 TURN 服务器进行通信的端口号 |
| url       | string | 是   | 否   | 指示发生错误的 STUN 或 TURN 服务器的 URL 的字符串。  |
| errorCode | number | 是   | 否   | 一个无符号整数值，表示 STUN 或 TURN 服务器返回的错误代码 |
| errorText | string | 是   | 否   | 包含 STUN 或 TURN 服务器返回的 STUN 原因文本的字符串 |

## RTCDataChannelEvent 

代表与特定 RTCDataChannel 相关的事件。

### 属性

| 名称    | 类型           | 可读 | 可写 | 说明                        |
| ------- | -------------- | ---- | ---- | --------------------------- |
| channel | RTCDataChannel | 是   | 否   | 与该事件关联的RTCDataChannel对象 |

## MediaStreamTrackEvent 

当 MediaStream 中添加或移除媒体轨道时会触发该事件。

### 属性

| 名称  | 类型             | 可读 | 可写 | 说明                         |
| ----- | ---------------- | ---- | ---- | ---------------------------- |
| track | MediaStreamTrack | 是   | 否   | 表示触发事件的音频或视频轨道 |

## RTCDTMFToneChangeEvent

在通过 WebRTC 发送 DTMF（双音多频）信号时触发的事件。

### 属性

| 名称 | 类型   | 可读 | 可写 | 说明                           |
| ---- | ------ | ---- | ---- | ------------------------------ |
| tone | string | 是   | 否   | 触发事件时发送的 DTMF 信号音调 |

## MessageEvent\<T\>

用于定义事件对象，特别是包含消息数据的事件对象。

### 属性

| 名称 | 类型 | 可读 | 可写 | 说明               |
| ---- | ---- | ---- | ---- | ------------------ |
| data | T    | 是   | 否   | 存储事件携带的数据 |

## VideoCapturerStartedEvent

视频源开始采集时触发的事件，继承自 Event。

### 属性

| 名称    | 类型    | 可读 | 可写 | 说明                                       |
| ------- | ------- | ---- | ---- | ------------------------------------------ |
| success | boolean | 是   | 否   | 视频采集启动是否成功，true 表示成功，false 表示失败 |

## ULongRange

用于表示一个数值范围，其值的类型为无符号长整型值（unsigned long）。

### 属性

| 名称 | 类型   | 必填 | 说明             |
| ---- | ------ | ---- | ---------------- |
| max  | number | 否   | 表示范围的最大值 |
| min  | number | 否   | 表示范围的最小值 |

## DoubleRange 

用于表示一个数值范围，其值的类型为双精度浮点数（double）。

### 属性

| 名称 | 类型   | 必填 | 说明             |
| ---- | ------ | ---- | ---------------- |
| max  | number | 否   | 表示范围的最大值 |
| min  | number | 否   | 表示范围的最小值 |

## ConstrainDoubleRange

用于表示一个受限制的数值范围，其值的类型为双精度浮点数（double）。

### 属性

| 名称  | 类型   | 必填 | 说明             |
| ----- | ------ | ---- | ---------------- |
| exact | number | 否   | 表示属性的精确值 |
| ideal | number | 否   | 表示属性的理想值 |

## ConstrainULongRange

用于表示一个受限制的数值范围，其值的类型为无符号长整型值（unsigned long）。

### 属性

| 名称  | 类型   | 必填 | 说明             |
| ----- | ------ | ---- | ---------------- |
| exact | number | 否   | 表示属性的精确值 |
| ideal | number | 否   | 表示属性的理想值 |

## ConstrainBooleanParameters

用于在约束条件中定义布尔类型参数的精确值和理想值。

### 属性

| 名称  | 类型    | 必填 | 说明             |
| ----- | ------- | ---- | ---------------- |
| exact | boolean | 否   | 表示属性的精确值 |
| ideal | boolean | 否   | 表示属性的理想值 |

## ConstrainStringParameters

用于在约束条件中定义字符串类型参数的精确值和理想值。

### 属性

| 名称  | 类型               | 必填 | 说明                                           |
| ----- | ------------------ | ---- | ---------------------------------------------- |
| exact | string \| string[] | 否   | 表示属性的精确值，可以是单个字符串或字符串数组 |
| ideal | string \| string[] | 否   | 表示属性的理想值，可以是单个字符串或字符串数组 |

## Algorithm

用于描述一个算法。

| 名称 | 类型   | 必填 | 说明             |
| ---- | ------ | ---- | ---------------- |
| name | string | 是   | 已注册的算法名称 |

## RTCIceServer 

定义了连接单个ICE服务器（如 STUN 或 TURN 服务器）所需的参数。

### 属性

| 名称       | 类型                 | 必填 | 说明                                                  |
| ---------- | -------------------- | ---- | ----------------------------------------------------- |
| urls       | string  \|  string[] | 是   | 单个字符串或字符串数组，每个字符串表示一个服务器 URL 。 |
| username   | string               | 否   | 如果是一个 TURN 服务器，则该属性表示鉴权所需的用户名。  |
| credential | string               | 否   | 登录 TURN 服务器所需的证明，通常为密码。 |

## RTCConfiguration 

WebRTC 连接的配置选项

### 属性

| 名称                 | 类型                  | 必填 | 说明               |
| -------------------- | --------------------- | ---- | ------------------ |
| iceServers           | RTCIceServer[]        | 否   | ICE 服务器配置数组 |
| iceTransportPolicy   | RTCIceTransportPolicy | 否   | ICE 传输策略       |
| bundlePolicy         | RTCBundlePolicy       | 否   | Bundle 策略        |
| rtcpMuxPolicy        | RTCRtcpMuxPolicy      | 否   | RTCP 复用策略      |
| certificates         | RTCCertificate[]      | 否   | 证书数组           |
| iceCandidatePoolSize | number                | 否   | ICE 候选池大小     |

## RTCOfferAnswerOptions 

在创建 offer 和 answer 时提供了额外的配置选项（当前为空）。

## RTCOfferOptions 

在创建 WebRTC offer 时提供了额外的配置选项。

### 属性

| 名称       | 类型    | 必填 | 说明 |
| ---------- | ------- | ---- | ---- |
| iceRestart | boolean | 否   | 要在活动连接上重新启动 ICE，请将其设置为true。这将导致返回的 offer 与已经存在的凭证不同。如果你应用返回的 offer，则 ICE 将重新启动。指定false以保留相同的凭据，因此不重新启动 ICE。默认值为false  |

## RTCAnswerOptions 

在创建 WebRTC answer 时提供了配置选项（当前为空）。

## RTCPeerConnection

用于在本地和远程对等端之间建立和管理点对点连接。它提供了一系列方法和事件，用于创建、维护、监视和关闭连接。

### 属性

| 名称                                   | 类型                   | 可读 | 可写 | 说明                                                         |
| -------------------------------------- | ---------------------- | ---- | ---- | ------------------------------------------------------------ |
| canTrickleIceCandidates    | boolean                | 是   | 否   | 表示远程对等端是否支持 trickle ICE                           |
| signalingState             | RTCSignalingState      | 是   | 否   | 返回一个字符串，表示与对等端连接过程中的信令处理状态         |
| iceGatheringState          | RTCIceGatheringState   | 是   | 否   | 表示 ICE 收集的状态                                          |
| iceConnectionState         | RTCIceConnectionState  | 是   | 否   | 表示 ICE 连接状态                                            |
| connectionState            | RTCPeerConnectionState | 是   | 否   | 表示当前所有被使用的 ICE 连接的状态                          |
| localDescription           | RTCSessionDescription  | 是   | 否   | 返回一个 RTCSessionDescription ，代表连接的本地描述。如果本地描述还没有被设置，返回 null |
| remoteDescription          | RTCSessionDescription  | 是   | 否   | 返回一个 RTCSessionDescription 对象，代表连接的远程描述，包括配置和媒体信息。如果当前尚未设置，将返回 null |
| currentLocalDescription    | RTCSessionDescription  | 是   | 否   | 返回一个 RTCSessionDescription 对象，该对象描述自上次 RTCPeerConnection 完成协商并连接到远程对等端后，最近一次成功协商的本地描述，同时也可能包括自创建 Offer 或 Answer 以来由 ICE 代理生成的任何本地候选者 |
| currentRemoteDescription   | RTCSessionDescription  | 是   | 否   | 返回一个 RTCSessionDescription 对象，该对象描述自上次 RTCPeerConnection 完成协商并连接到远程对等端后，最近一次成功协商的远程描述。同时也可能包括自创建 Offer 或 Answer 以来通过 addIceCandidate() 提供的任何本地候选者 |
| pendingLocalDescription    | RTCSessionDescription  | 是   | 否   | 返回一个 RTCSessionDescription 对象，表示正在协商过程中的本地描述 |
| pendingRemoteDescription   | RTCSessionDescription  | 是   | 否   | 返回一个 RTCSessionDescription 对象，表示正在协商过程中的远程描述 |
| onnegotiationneeded        | EventHandler \| null   | 是   | 是   | 该事件处理器的事件类型为 negotiationneeded                   |
| onicecandidate             | EventHandler \| null   | 是   | 是   | 该事件处理器的事件类型为 icecandidate                        |
| onicecandidateerror        | EventHandler \| null   | 是   | 是   | 该事件处理器的事件类型为 icecandidateerror                   |
| oniceconnectionstatechange | EventHandler \| null   | 是   | 是   | 该事件处理器的事件类型为 iceconnectionstatechange            |
| onicegatheringstatechange  | EventHandler \| null   | 是   | 是   | 该事件处理器的事件类型为 icegatheringstatechange             |
| onsignalingstatechange     | EventHandler \| null   | 是   | 是   | 该事件处理器的事件类型为 signalingstatechange                |
| onconnectionstatechange    | EventHandler \| null   | 是   | 是   | 该事件处理器的事件类型为 connectionstatechange               |
| ontrack                    | EventHandler \| null   | 是   | 是   | 该事件处理器的事件类型为 track                               |
| ondatachannel              | EventHandler \| null   | 是   | 是   | 该事件处理器的事件类型为 datachannel                         |

### 事件

| 名称                               | 接口                    | 说明                                                         |
| ---------------------------------- | ---------------------- | ------------------------------------------------------------ |
| negotiationneeded      | Event                  | 当需要通过信令通道协商连接时，向 RTCPeerConnection 发送该事件。如连接的初始设置期间，以及通信环境更改需要重新配置连接时 |
| icecandidate           | RTCPeerConnectionIceEvent | 当收集到新的 ICE 候选者时触发 |
| icecandidateerror      | RTCPeerConnectionIceErrorEvent | 当收集 ICE 候选者过程中发送错误时触发 |
| iceconnectionstatechange | Event                  | 当 RTCPeerConnection 的 ICE 连接状态改变时触发 |
| icegatheringstatechange | Event                  | 当 RTCPeerConnection 的 ICE 收集状态改变时触发 |
| onsignalingstatechange | Event                  | 当 RTCPeerConnection 的信令状态改变时触发 |
| connectionstatechange  | Event                  | 当 RTCPeerConnection 的连接状态改变时触发 |
| track                  | RTCTrackEvent          | 当已经为特定的 RTCRtpReceiver 协商了新的传入媒体，并且该 RTCRtpReceiver 的轨道已添加到任意相关的远程 MediaStream 中时触发 |
| datachannel            | RTCDataChannelEvent    | 当远程对等端创建了一个 RTCDataChannel 时触发 |

### addTrack

addTrack(track: MediaStreamTrack, ...streams: MediaStream[]): RTCRtpSender;

用于将媒体轨道添加到 RTCPeerConnection 连接中。

参数：

| 名称    | 类型             | 必填 | 说明                       |
| ------- | ---------------- | ---- | -------------------------- |
| track   | MediaStreamTrack | 是   | 表示要发送的媒体轨道       |
| streams | MediaStream[]    | 否   | 用来管理和控制媒体轨道的组 |

返回值：

| 类型                            | 说明                                 |
|-------------------------------|------------------------------------|
| [RTCRtpSender](#rtcrtpsender) | 表示已添加轨道的发送器对象，用于控制和管理发送到远程对等端的数据流。 |

### removeTrack

removeTrack(sender: RTCRtpSender): void;

从连接中移除一个 RTCRtpSender ，从而停止将特定的媒体流轨道发送到远程对等端。

参数：

| 名称   | 类型         | 必填 | 说明                   |
| ------ | ------------ | ---- | ---------------------- |
| sender | [RTCRtpSender](#rtcrtpsender) | 是   | 表示要移除的轨道发送器 |

### setLocalDescription

setLocalDescription(description?: RTCSessionDescription): Promise<void>;

设置与 RTCPeerConnection 连接关联的本地描述。此描述指定连接本地端的属性，包括媒体格式。

参数：

| 名称        | 类型                  | 必填 | 说明                                            |
| ----------- | --------------------- | ---- | ----------------------------------------------- |
| description | RTCSessionDescription | 否   | 本地描述, 通常调用 createOffer 或 createAnswer 获取 |

返回值：

| 类型          | 说明                                                         |
| ------------- | ------------------------------------------------------------ |
| Promise<void> | 返回一个 Promise 对象，表示设置本地描述的操作是否成功。成功时 Promise 解析，失败时抛出错误。 |

### setRemoteDescription

setRemoteDescription(description: RTCSessionDescription): Promise<void>;

设置远程对等端的 Offer 或 Answer 。该描述指定连接另一端的属性，包括媒体格式。

参数：

| 名称        | 类型                  | 必填 | 说明                     |
| ----------- | --------------------- | ---- | ------------------------ |
| description | RTCSessionDescription | 是   | 远程描述，通常从对端获取 |

返回值：

| 类型          | 说明                                                         |
| ------------- | ------------------------------------------------------------ |
| Promise<void> | 返回一个 Promise 对象，表示设置远程描述的操作是否成功。成功时 Promise 解析，失败时抛出错误。 |

### createOffer

createOffer(options?: RTCOfferOptions): Promise<RTCSessionDescription>;

创建 Offer，作为要约以用于发起新的 RTCPeerConnection 连接。

参数：

| 名称    | 类型            | 必填 | 说明                          |
| :------ | --------------- | ---- | ----------------------------- |
| options | RTCOfferOptions | 否   | 创建 Offer 时的参数 |

返回值：

| 类型                           | 说明                                                         |
| ------------------------------ | ------------------------------------------------------------ |
| Promise<RTCSessionDescription> | 返回一个 Promise 对象，当 offer 生成成功时，Promise 被解析为一个包含了 offer 的 RTCSessionDescription 对象。 |

### createAnswer

createAnswer(options?: RTCAnswerOptions): Promise<RTCSessionDescription>;

在 Offer/Answer 协商过程中，创建一个 Answer ，作为从对端收到的 Offer 的应答。

参数：

| 名称    | 类型             | 必填 | 说明              |
| ------- | ---------------- | ---- | ----------------- |
| options | RTCAnswerOptions | 否   | 创建Offer时的参数 |

返回值：

| 类型                           | 说明                                                         |
| ------------------------------ | ------------------------------------------------------------ |
| Promise<RTCSessionDescription> | 当成功创建本地 answer 时，Promise 被解析为一个 `RTCSessionDescription` 对象，该对象包含了本地端生成的 answer。 |

### createDataChannel

createDataChannel(label: string, dataChannelDict?: RTCDataChannelInit): RTCDataChannel;

创建一个与远程对等连接的新通道，该通道可以传输任何类型的数据。例如图像、文件传输、文本聊天、游戏更新包等。

参数：

| 名称            | 类型               | 必填 | 说明                     |
| --------------- | ------------------ | ---- | ------------------------ |
| label           | string             | 是   | 通道名                   |
| dataChannelDict | RTCDataChannelInit | 否   | 提供了数据通道的配置选项 |

返回值：

| 类型           | 说明                                             |
| -------------- | ------------------------------------------------ |
| RTCDataChannel | 返回一个 RTCDataChannel 对象，表示创建的数据通道。 |

### addIceCandidate

addIceCandidate(candidate?: RTCIceCandidateInit): Promise<void>;

用于将 ICE 候选者添加到 RTCPeerConnection 中。

参数：

| 名称      | 类型                | 必填 | 说明                          |
| --------- | ------------------- | ---- | ----------------------------- |
| candidate | RTCIceCandidateInit | 否   | 包含要添加的 ICE 候选者的信息 |

返回值：

| 类型             | 说明                                                          |
|----------------|-------------------------------------------------------------|
| Promise\<void> | 返回一个 Promise 对象，表示添加 ICE 候选者的操作是否成功。成功时 Promise 解析，失败时抛出错误。 |

### getSenders

getSenders(): RTCRtpSender[];

获取当前连接中所有的 RTCRtpSender 对象数组。

返回值：

| 类型                              | 说明                      |
|---------------------------------|-------------------------|
| [RTCRtpSender](#rtcrtpsender)[] | 返回一个数组，包含当前连接中的所有发送器对象。 |

### getReceivers

getReceivers(): RTCRtpReceiver[];

获取当前 RTCPeerConnection 中所有的 RTCRtpReceiver 对象数组。

返回值：

| 类型                                     | 说明                      |
|----------------------------------------|-------------------------|
| [RTCRtpReceiver](#rtcrtptransceiver)[] | 返回一个数组，包含当前连接中的所有接收器对象。 |

### getTransceivers

getTransceivers(): RTCRtpTransceiver[];

获取当前 RTCPeerConnection 中所有的 RTCRtpTransceiver 对象数组。

返回值：

| 类型                                        | 说明                      |
|-------------------------------------------|-------------------------|
| [RTCRtpTransceiver](#rtcrtptransceiver)[] | 返回一个数组，包含当前连接中的所有传输器对象。 |

### getConfiguration

getConfiguration(): RTCConfiguration;

获取当前 RTCPeerConnection 的配置信息。

返回值：

| 类型             | 说明                                 |
| ---------------- | ------------------------------------ |
| RTCConfiguration | 返回一个包含当前连接配置信息的对象。 |

### restartIce

restartIce(): void;

用于触发 RTCPeerConnection 实例的 ICE 重启过程。

### setConfiguration

setConfiguration(configuration?: RTCConfiguration): void;

用于设置 RTCPeerConnection 实例的配置信息，包括 ICE 服务器、传输策略和安全证书等。

参数：

| 名称          | 类型             | 必填 | 说明                     |
| ------------- | ---------------- | ---- | ------------------------ |
| configuration | RTCConfiguration | 否   | 包含要设置的新配置信息。 |

### addTransceiver

addTransceiver(trackOrKind: MediaStreamTrack | string, init?: RTCRtpTransceiverInit): RTCRtpTransceiver;

用于向 RTCPeerConnection 实例中添加一个新的 RTCRtpTransceiver，用于管理音频或视频流的传输。

参数：

| 名称        | 类型                                                         | 必填 | 说明                                                         |
| ----------- | ------------------------------------------------------------ | ---- | ------------------------------------------------------------ |
| trackOrKind | [MediaStreamTrack](#mediastreamtrack) \| string              | 是   | 可以是 MediaStreamTrack 对象，表示要传输的媒体类型。如果是字符串，指示要添加的传输器的类型，取值为"audio"或"video"。 |
| init        | [RTCRtpTransceiverInit](#rtcrtptransceiverInit) \| undefined | 否   | 用于配置传输器的初始设置                                     |

返回值：

| 类型                     | 说明                                          |
|------------------------| --------------------------------------------- |
| [RTCRtpTransceiver](#rtcrtptransceiver) | 返回新创建的传输器 (RTCRtpTransceiver) 对象。 |

### close

close(): void;

关闭 RTCPeerConnection 实例，释放与对等连接相关的所有资源，并终止任何正在进行的连接。

### getStats

getStats(selector?: MediaStreamTrack): Promise<RTCStatsReport>;

用于获取指定 MediaStreamTrack 或所有轨道的统计信息 

参数：

| 参数名   | 类型             | 必填 | 说明                                 |
| -------- | ---------------- | ---- | ------------------------------------ |
| selector | MediaStreamTrack | 否   | 指定要请求统计信息的 MediaStreamTrack |

返回值：

| 类型                    | 说明                                                         |
| ----------------------- | ------------------------------------------------------------ |
| Promise<RTCStatsReport> | 返回一个 Promise 对象，该对象在解析后会得到一个 RTCStatsReport ，其中包含了当前时间段内收集到的统计信息。 |

### generateCertificate

static generateCertificate(keygenAlgorithm: AlgorithmIdentifier): Promise\<RTCCertificate>;

生成用于 DTLS 连接的自签名证书。

#### 参数

| 参数名           | 类型                | 必填 | 说明                                                      |
| ---------------- | ------------------- | ---- | -------------------------------------------------------------- |
| keygenAlgorithm  | AlgorithmIdentifier | 是    | 用于生成密钥的算法，如 `{ name: 'ECDSA', namedCurve: 'P-256' }` |

#### 返回值

| 类型                     | 说明                          |
| ------------------------ | ------------------------------------ |
| Promise\<RTCCertificate> | 解析为生成的 RTCCertificate 证书对象 |

### setAudioRecording

setAudioRecording(recording: boolean): void

启用/禁用音频流录制，默认开启。

#### 参数

| 参数名    | 类型    | 必填 | 说明                                       |
| --------- | ------- | ---- | ------------------------------------------ |
| recording | boolean | 是   | true：启用音频录制；false：禁用音频录制。  |

#### 返回值

无

### setAudioPlayout

setAudioPlayout(playout: boolean): void

启用/禁用接收到的音频流的播放，默认启用。

#### 参数

| 参数名   | 类型             | 必填 | 说明                                 |
| -------- | ---------------- | ---- | ------------------------------------ |
| playout | boolean | 是   | 将`playout`设置为 false 将停止底层音频设备的播放，但将启动一个任务，该任务将每10ms轮询音频数据，以确保音频处理发生并更新音频统计信息。 |

#### 返回值

无

## MediaTrackCapabilities

定义媒体轨道的各种能力和属性。

### 属性

| 名称               | 类型          | 必填  | 说明                 |
|------------------|-------------|-----|--------------------|
| width            | ULongRange  | 否   | 表示媒体轨道支持的视频宽度范围    |
| height           | ULongRange  | 否   | 表示媒体轨道支持的视频高度范围    |
| aspectRatio      | DoubleRange | 否   | 表示媒体轨道支持的视频纵横比范围   |
| frameRate        | DoubleRange | 否   | 表示媒体轨道支持的帧率范围      |
| facingMode       | string[]    | 否   | 表示媒体轨道支持的摄像头朝向模式   |
| resizeMode       | string[]    | 否   | 表示媒体轨道支持的视频调整模式    |
| sampleRate       | ULongRange  | 否   | 表示媒体轨道支持的音频采样率范围   |
| sampleSize       | ULongRange  | 否   | 表示媒体轨道支持的音频样本大小范围  |
| echoCancellation | boolean[]   | 否   | 表示媒体轨道是否支持回声消除     |
| autoGainControl  | boolean[]   | 否   | 表示媒体轨道是否支持自动增益控制   |
| noiseSuppression | boolean[]   | 否   | 表示媒体轨道是否支持噪音抑制     |
| latency          | DoubleRange | 否   | 表示媒体轨道支持的音频延迟范围    |
| channelCount     | ULongRange  | 否   | 表示媒体轨道支持的音频通道数量范围  |
| deviceId         | string      | 否   | 表示媒体轨道所属的设备的唯一标识符  |
| groupId          | string      | 否   | 表示媒体轨道所属的设备组的唯一标识符 |

## MediaTrackConstraintSet

用于定义媒体轨道的约束条件。

### 属性

| 名称               | 类型               | 必填  | 说明                     |
|------------------|------------------|-----|------------------------|
| width            | ConstrainULong   | 否   | 表示媒体轨道需要满足的视频宽度约束条件，以像素为单位。用于屏幕采集时，以设备自然方向为参考（手机为竖屏） |
| height           | ConstrainULong   | 否   | 表示媒体轨道需要满足的视频高度约束条件，以像素为单位。用于屏幕采集时，以设备自然方向为参考（手机为竖屏） |
| aspectRatio      | ConstrainDouble  | 否   | 表示媒体轨道需要满足的视频纵横比约束条件   |
| frameRate        | ConstrainDouble  | 否   | 表示媒体轨道需要满足的帧率约束条件      |
| zoom             | ConstrainDouble  | 否   | 表示媒体轨道需要满足的相机变焦倍率约束条件   |
| facingMode       | ConstrainString  | 否   | 表示媒体轨道需要满足的摄像头朝向模式约束条件 |
| resizeMode       | ConstrainString  | 否   | 表示媒体轨道需要满足的视频调整模式约束条件  |
| sampleRate       | ConstrainULong   | 否   | 表示媒体轨道需要满足的音频采样率约束条件   |
| sampleSize       | ConstrainULong   | 否   | 表示媒体轨道需要满足的音频样本大小约束条件  |
| echoCancellation | ConstrainBoolean | 否   | 表示媒体轨道需要满足的回声消除约束条件    |
| autoGainControl  | ConstrainBoolean | 否   | 表示媒体轨道需要满足的自动增益控制约束条件  |
| noiseSuppression | ConstrainBoolean | 否   | 表示媒体轨道需要满足的噪音抑制约束条件    |
| latency          | ConstrainDouble  | 否   | 表示媒体轨道需要满足的音频延迟约束条件    |
| channelCount     | ConstrainULong   | 否   | 表示媒体轨道需要满足的音频通道数量约束条件  |
| deviceId         | ConstrainString  | 否   | 表示媒体轨道需要满足的设备 ID 约束条件  |
| groupId          | ConstrainString  | 否   | 表示媒体轨道需要满足的设备组 ID 约束条件 |
| ohosScreenCaptureMode             | ConstrainString | 否   | 表示媒体轨道需要满足的屏幕采集模式约束条件，可选值：'home-screen'，'specified-screen'，'specified-window'，默认为 'home-screen' |
| ohosScreenCaptureDisplayId        | ConstrainULong  | 否   | 表示媒体轨道需要满足的屏幕采集时指定屏幕 ID 约束条件，当 ohosScreenCaptureMode 为 'specified-screen' 时有效 |
| ohosScreenCaptureMissionId        | ConstrainString | 否   | 表示媒体轨道需要满足的屏幕采集时指定窗口 ID 约束条件，当 ohosScreenCaptureMode 为 'specified-window' 时有效 |
| ohosScreenCaptureWindowFilter     | ConstrainString | 否   | 表示媒体轨道需要满足的屏幕采集时指定过滤窗口 ID 约束条件 |
| ohosScreenCaptureAudioFilter      | ConstrainString | 否   | 表示媒体轨道需要满足的屏幕采集时指定过滤音频类型约束条件，可选值：'current-app'，'notification' |
| ohosScreenCaptureSkipPrivacyMode  | ConstrainString | 否   | 表示媒体轨道需要满足的屏幕采集时跳过隐私模式的窗口 ID 约束条件 |
| ohosScreenCaptureAutoRotation     | ConstrainBoolean  | 否  | 表示媒体轨道需要满足的屏幕采集时是否自动旋转约束条件，默认开启自动旋转  |

## MediaTrackConstraints

用于定义媒体轨道的约束条件，继承自 MediaTrackConstraintSet。

### 属性

| 名称       | 类型                        | 必填  | 说明               |
|----------|---------------------------|-----|------------------|
| advanced | MediaTrackConstraintSet[] | 否   | 用于指定更高级的媒体轨道约束条件 |

## MediaTrackSettings

用于描述媒体轨道的设置参数。

### 属性

| 名称               | 类型      | 必填  | 说明                 |
|------------------|---------|-----|--------------------|
| width            | number  | 否   | 表示媒体轨道的视频宽度        |
| height           | number  | 否   | 表示媒体轨道的视频高度        |
| aspectRatio      | number  | 否   | 表示媒体轨道的视频纵横比       |
| frameRate        | number  | 否   | 表示媒体轨道的帧率          |
| facingMode       | string  | 否   | 表示媒体轨道的摄像头朝向模式     |
| resizeMode       | string  | 否   | 表示媒体轨道的视频调整模式      |
| sampleRate       | number  | 否   | 表示媒体轨道的音频采样率       |
| sampleSize       | number  | 否   | 表示媒体轨道的音频样本大小      |
| echoCancellation | boolean | 否   | 表示媒体轨道是否启用回声消除     |
| autoGainControl  | boolean | 否   | 表示媒体轨道是否启用自动增益控制   |
| noiseSuppression | boolean | 否   | 表示媒体轨道是否启用噪音抑制     |
| latency          | number  | 否   | 表示媒体轨道的音频延迟        |
| channelCount     | number  | 否   | 表示媒体轨道的音频通道数量      |
| deviceId         | string  | 否   | 表示媒体轨道所属的设备的唯一标识符  |
| groupId          | string  | 否   | 表示媒体轨道所属的设备组的唯一标识符 |

## MediaTrackSupportedConstraints

用于描述媒体轨道支持的约束条件。

### 属性

| 名称               | 类型      | 必填  | 说明                    |
|------------------|---------|-----|-----------------------|
| width            | boolean | 否   | 表示媒体轨道是否支持宽度约束条件      |
| height           | boolean | 否   | 表示媒体轨道是否支持高度约束条件      |
| aspectRatio      | boolean | 否   | 表示媒体轨道是否支持纵横比约束条件     |
| frameRate        | boolean | 否   | 表示媒体轨道是否支持帧率约束条件      |
| zoom             | boolean | 否   | 表示媒体轨道是否支持变焦约束条件      |
| facingMode       | boolean | 否   | 表示媒体轨道是否支持摄像头朝向模式约束条件 |
| resizeMode       | boolean | 否   | 表示媒体轨道是否支持视频调整模式约束条件  |
| sampleRate       | boolean | 否   | 表示媒体轨道是否支持音频采样率约束条件   |
| sampleSize       | boolean | 否   | 表示媒体轨道是否支持音频样本大小约束条件  |
| echoCancellation | boolean | 否   | 表示媒体轨道是否支持回声消除约束条件    |
| autoGainControl  | boolean | 否   | 表示媒体轨道是否支持自动增益控制约束条件  |
| noiseSuppression | boolean | 否   | 表示媒体轨道是否支持噪音抑制约束条件    |
| latency          | boolean | 否   | 表示媒体轨道是否支持音频延迟约束条件    |
| channelCount     | boolean | 否   | 表示媒体轨道是否支持音频通道数量约束条件  |
| deviceId         | boolean | 否   | 表示媒体轨道是否支持设备 ID 约束条件  |
| groupId          | boolean | 否   | 表示媒体轨道是否支持设备组 ID 约束条件 |

## MediaStreamConstraints

用于指示用户在 MediaDevices.getUserMedia() 返回的 MediaStream 中包含哪种类型的 MediaStreamTracks。

### 属性

| 名称    | 类型       | 必填                                              | 说明  |
|-------|----------|-------------------------------------------------|-----|
| audio | boolean \| [MediaTrackConstraints](#mediatrackconstraints) | 否   | 如果为 true，则请求返回的 MediaStream 包含音频轨道，如果为 false, MediaStream必须不包含音频轨道。如果提供了 MediaTrackConstraints 结构体，将进一步指定视频轨道的性质和设置。 |
| video | boolean \| [MediaTrackConstraints](#mediatrackconstraints) | 否   | 如果为 true，则请求返回的 MediaStream 包含视频轨道，如果为 false, MediaStream必须不包含视频轨道。如果提供了 MediaTrackConstraints 结构体，将进一步指定视频轨道的性质和设置。 |

## DisplayMediaStreamOptions

用于指示用户在 MediaDevices.getDisplayMedia() 返回的 MediaStream 中包含哪种类型的 MediaStreamTracks。

### 属性

| 名称    | 类型       | 必填                                              | 说明  |
|-------|----------|-------------------------------------------------|-----|
| audio | boolean \| [MediaTrackConstraints](#mediatrackconstraints) | 否   | 如果为 true，则请求返回的 MediaStream 包含音频轨道，如果为 false, MediaStream必须不包含音频轨道。如果提供了 MediaTrackConstraints 结构体，将进一步指定视频轨道的性质和设置。 |
| video | boolean \| [MediaTrackConstraints](#mediatrackconstraints) | 否   | 如果为 true，则请求返回的 MediaStream 包含视频轨道，如果为 false, MediaStream必须不包含视频轨道。如果提供了 MediaTrackConstraints 结构体，将进一步指定视频轨道的性质和设置。 |


## MediaStreamTrack

用于描述媒体流轨道（如音频或视频轨道）的基本属性和方法。

### 属性

| 名称         | 类型                                              | 可读  | 可写  | 说明                             |
|------------|-------------------------------------------------|-----|-----|--------------------------------|
| kind       | string                                          | 是   | 否   | 表示媒体流轨道的类型，取值为 "audio" 或 "video" |
| id         | string                                          | 是   | 否   | 表示媒体流轨道的唯一标识符                  |
| enabled    | boolean                                         | 是   | 是   | 表示媒体流轨道是否启用                    |
| readyState | [MediaStreamTrackState](#mediastreamtrackstate) | 是   | 否   | 表示媒体流轨道的就绪状态                   |
| muted      | boolean                                         | 是   | 否   | 表示媒体源是否暂时无法提供媒体（系统中断驱动）。与 onmute/onunmute 事件共享同一真实状态源；无可靠中断信号的轨道（本地视频、远端轨道）恒为 false |
| onended    | ((this: MediaStreamTrack, ev: [Event](#event)) => void) \| null | 是   | 是   | 轨道因非主动原因结束时回调，每次结束仅触发一次；主动调用 stop() 不触发。默认 null，赋 null 解绑 |
| onmute     | ((this: MediaStreamTrack, ev: [Event](#event)) => void) \| null | 是   | 是   | 媒体源暂时无法提供媒体时回调（本地音频为系统强制中断，如来电抢占）。enabled 切换、静音帧不触发。默认 null，赋 null 解绑 |
| onunmute   | ((this: MediaStreamTrack, ev: [Event](#event)) => void) \| null | 是   | 是   | 媒体实际恢复且轨道未结束时回调一次。默认 null，赋 null 解绑 |

> **事件差异说明**：mute/unmute 需要系统中断/恢复的真实信号。当前本地音频轨道以 OHAudio 强制中断事件为状态源；本地视频轨道与远端音视频轨道暂无可靠信号，`muted` 恒为 false 且不派发 mute/unmute（列为已知差异，不以猜测逻辑冒充支持）。事件在 JS 线程派发，注册回调前发生的状态变化不补发，可通过 `readyState`/`muted` 查询当前状态。

### stop

stop(): void

移除所有通过该 MediaStreamTrack 添加到对应 VideoSource 的 VideoSink 实例（如 NativeVideoRenderer）。当对应 VideoSource 的所有 VideoSink 实例被移除后， VideoSource 会自动停止采集。

#### 参数

无

返回值

无

### getSource

getSource(): MediaSource | null

获取创建该媒体流轨道时所关联的媒体源。

#### 参数

无

返回值

| 类型                                  | 说明  |
|---------------------------------------|-------------|
| [MediaSource](#mediasource)           | 媒体源  |
| null                                  | 未找到（如远端媒体流轨道）  |

### applyConstraints

applyConstraints(constraints: MediaTrackConstraints): Promise<void>;

动态应用约束到媒体流轨道。对于视频轨道，支持通过 `width`、`height`、`frameRate` 和 `facingMode` 等约束动态调整视频采集参数；对于音频轨道，当前不支持运行时约束调整。

当传入的约束包含 `facingMode` 时，会触发摄像头切换逻辑：先验证目标摄像头是否可用，然后创建新的采集器并切换，同时保持原有视频轨道不变。切换过程中通过 `pendingSwitchCapturer_` 机制确保旧采集器在新采集器成功启动后才被释放，避免资源冲突和黑屏。

当传入的约束包含 `width`、`height` 或 `frameRate` 时，会通过 `VideoAdapter` 动态调整输出分辨率或帧率，无需重新创建采集器。

> **注意**：
> 1. 当前音频轨道不支持运行时约束调整。对音频轨道调用 `applyConstraints` 会返回 `Promise.reject`，提示不支持该操作。
> 2. 当约束包含 `facingMode` 时，Promise 的行为分两个阶段：
>    - **验证阶段**：如果目标摄像头不存在或约束无法满足（如设备无对应摄像头），`applyConstraints` 会 **reject**，返回 `"Constraints could not be satisfied'` 错误。
>    - **切换阶段**：验证通过后，Promise 在**开始切换**后即 resolve，实际摄像头切换在后台异步完成。如果切换失败（如新摄像头无法启动），Promise 不会 reject，旧采集器会继续运行以保持视频流不中断。如需感知切换结果，可通过监听 VideoSource 的 `oncapturerstarted` 事件获取状态。

#### 参数

| 参数名      | 类型                  | 必填 | 说明                                                         |
|-------------|-----------------------|------|--------------------------------------------------------------|
| constraints | MediaTrackConstraints | 是   | 要应用的约束对象，可包含 width、height、frameRate、facingMode 等 |

#### 返回值

| 类型          | 说明                                                         |
|---------------|--------------------------------------------------------------|
| Promise<void> | 返回一个 Promise 对象，约束应用成功时解析，失败时抛出错误。     |

#### 约束说明

| 约束名称   | 类型    | 说明                                                         |
|------------|---------|--------------------------------------------------------------|
| width      | number  | 视频宽度（像素）。若只提供 width 或 height 之一，则不会生效。 |
| height     | number  | 视频高度（像素）。需与 width 同时提供才生效。                  |
| frameRate  | number  | 目标帧率（fps）。通过 VideoAdapter 限制输出帧率。             |
| facingMode | string  | 摄像头朝向，取值为 `"user"`（前置）或 `"environment"`（后置）。|

#### 示例

```typescript
// 切换分辨率
try {
  await videoTrack.applyConstraints({
    width: 1920,
    height: 1080,
    frameRate: 30
  });
} catch (error) {
  console.error('Failed to apply constraints:', error);
}

// 切换摄像头（前置/后置）
try {
  await videoTrack.applyConstraints({
    facingMode: 'environment'
  });
} catch (error) {
  console.error('Failed to apply constraints:', error);
}

// 组合约束：切换摄像头并指定分辨率
try {
  await videoTrack.applyConstraints({
    width: 1280,
    height: 720,
    facingMode: 'user'
  });
} catch (error) {
  console.error('Failed to apply constraints:', error);
}
```

### getZoomRange

getZoomRange(): { min: number; max: number; supported: boolean }

查询设备当前摄像头支持的变焦范围。不同摄像头（前置/后置）支持的范围不同，切换摄像头后需重新查询。

**返回值**

| 类型                                                          | 说明                           |
|-------------------------------------------------------------|------------------------------|
| { min: number; max: number; supported: boolean }           | min 为最小变焦倍率，max 为最大变焦倍率，supported 表示设备是否支持变焦 |

## MediaStream

一个媒体流对象，它可以包含一个或多个媒体轨道（如音频轨道和视频轨道），并提供了管理这些轨道的方法和事件处理能力。

### 属性

| 名称     | 类型      | 可读  | 可写  | 说明              |
|--------|---------|-----|-----|-----------------|
| id     | string  | 是   | 否   | 表示媒体流的唯一标识符     |
| active | boolean | 是   | 否   | 表示当前媒体流是否处于活动状态 |

### addTrack

addTrack(track: MediaStreamTrack): void;

动态管理和调整音频和视频轨道

参数：

| 参数名   | 类型                                    | 必填  | 说明          |
|-------|---------------------------------------|-----|-------------|
| track | [MediaStreamTrack](#mediastreamtrack) | 是   | 要添加的音视频轨道对象 |

### removeTrack

removeTrack(track: MediaStreamTrack): void;

从媒体流中移除指定的 MediaStreamTrack

参数：

| 参数名   | 类型                                    | 必填  | 说明          |
|-------|---------------------------------------|-----|-------------|
| track | [MediaStreamTrack](#mediastreamtrack) | 是   | 要移除的音视频轨道对象 |

### getTrackById

getTrackById(trackId: string): MediaStreamTrack | null;

提供的轨道 ID 在当前 MediaStream 对象中查找并返回对应的 MediaStreamTrack 对象

参数：

| 参数名     | 类型     | 必填  | 说明         |
|---------|--------|-----|------------|
| trackId | string | 是   | 音视频轨道的唯一标识 |

返回值

| 类型                                    | 说明          |
|---------------------------------------|-------------|
| [MediaStreamTrack](#mediastreamtrack) | 找到指定 ID 的轨道 |
| null                                  | 未找到         |

### getTracks

getTracks(): MediaStreamTrack[];

获取当前 MediaStream 对象中包含的所有音频和视频轨道（MediaStreamTrack对象）的数组

返回值

| 类型                                      | 说明              |
|-----------------------------------------|-----------------|
| [MediaStreamTrack](#mediastreamtrack)[] | 包含当前媒体流中所有轨道的数组 |

### getAudioTracks

getAudioTracks(): MediaStreamTrack[];

获取当前媒体流 (MediaStream) 中所有的音频轨道 (MediaStreamTrack对象) 组成的数组

返回值

| 类型                                      | 说明                |
|-----------------------------------------|-------------------|
| [MediaStreamTrack](#mediastreamtrack)[] | 包含当前媒体流中所有音频轨道的数组 |

### getVideoTracks

getVideoTracks(): MediaStreamTrack[];

获取当前媒体流 (MediaStream) 中所有的视频轨道 (MediaStreamTrack 对象) 组成的数组

返回值

| 类型                                      | 说明                |
|-----------------------------------------|-------------------|
| [MediaStreamTrack](#mediastreamtrack)[] | 包含当前媒体流中所有视频轨道的数组 |

## RTCRtpTransceiverInit

用于在 PeerConnection 中添加 RTCRtpTransceiver ，指定传输方向、关联的媒体流以及发送编码参数。

### 属性

| 名称          | 类型                                                         | 可读        | 可写 | 说明                       |
| ------------- | ------------------------------------------------------------ |-----------| ---- | -------------------------- |
| direction     | [RTCRtpTransceiverDirection](#rtcrtptransceiverdirection) \| undefined | 是   | 是   | 传输器的初始传输方向       |
| streams       | [MediaStream](#mediastream)[] \| undefined |   是   |   是   | 与传输器关联的媒体流的数组 |
| sendEncodings | [RTCRtpEncodingParameters](#rtcrtpencodingparameters)[] \| undefined |   是   |   是   | 发送端编码参数的数组       |

## RTCRtpTransceiver

包含一个 RTCRtpSender 和一个 RTCRtpReceiver 。

### 属性

| 名称               | 类型                                 | 可读   | 可写  | 说明           |
|------------------|------------------------------------|------|-----|--------------|
| mid              | string \| null | 是   | 否  | RTP 传输器的标识符    |
| sender           | [RTCRtpSender](#rtcrtpsender)      | 是    | 否  | 用来发送媒体数据       |
| receiver         | [RTCRtpReceiver](#rtcrtpreceiver)  | 是    | 否   | 用来接收远端发送的媒体数据  |
| direction        | [RTCRtpTransceiverDirection](#rtcrtptransceiverdirection) | 是    | 是   | RTP 传输器的当前传输方向 |
| currentDirection | [RTCRtpTransceiverDirection](#rtcrtptransceiverdirection) \| null | 是   | 否  | 表示传输器当前的操作状态   |

### stop

stop(): void;

停止关联的 RTCRtpSender 和 RTCRtpReceiver 。

### setCodecPreferences

setCodecPreferences(codecs: RTCRtpCodec[]): void;

配置收发器的首选编解码器列表。

参数

| 参数名    | 类型                                                | 必填  | 说明       |
|--------|---------------------------------------------------|-----|----------|
| codecs | [RTCRtpCodec](#RTCRtpCodec)[] | 是   | 首选编解码器列表 |

## RTCSessionDescriptionInit

## RTCSessionDescription

表示 RTCPeerConnection 中的会话描述信息

### 属性

| 名称 | 类型       | 可读 | 可写 | 必填 | 说明                                     |
| ---- | ---------- | ---- | ---- | ---- | ---------------------------------------- |
| sdp  | string     | 是   | 否   | 否   | SDP 描述信息，包含会话的详细配置和参数。 |
| type | RTCSdpType | 是   | 否   | 是   | 描述会话描述的类型                       |

### toJSON

toJSON(): RTCSessionDescriptionInit;

将会话描述转换为 JSON 对象。

#### 返回值

| 类型                      | 说明                               |
| -------------------------- | ----------------------------------------- |
| RTCSessionDescriptionInit | 包含 `sdp` 和 `type` 字段的普通对象。 |

## RTCIceCandidateInit

用于初始化ICE候选者的属性集合

### 属性

| 名称             | 类型   | 必填 | 说明                  |
| ---------------- | ------ | ---- | --------------------- |
| candidate        | string | 否   | ICE 候选者的字符串表示 |
| sdpMLineIndex    | number | 否   | SDP 媒体行的索引       |
| sdpMid           | string | 否   | SDP 媒体的标识符       |
| usernameFragment | string | 否   | ICE 候选者的用户名片段 |

## RTCIceCandidate

描述 ICE 候选者的属性

### 属性

| 名称             | 类型                   | 可读 | 可写 | 必填 | 说明                   |
| ---------------- | ---------------------- | ---- | ---- | ---- | ---------------------- |
| candidate        | string                 | 是   | 否   | 是   | ICE 候选者的字符串     |
| sdpMid           | string                 | 是   | 否   | 否   | SDP 媒体的标识符       |
| sdpMLineIndex    | number                 | 是   | 否   | 否   | SDP 媒体行的索引       |
| foundation       | string                 | 是   | 否   | 否   | 候选者基础标识符       |
| component        | RTCIceComponent        | 是   | 否   | 否   | 候选者的组件           |
| priority         | number                 | 是   | 否   | 否   | 候选者的优先级         |
| address          | string                 | 是   | 否   | 否   | 候选者的 IP 地址       |
| protocol         | RTCIceProtocol         | 是   | 否   | 否   | 候选者的协议类型       |
| port             | number                 | 是   | 否   | 否   | 候选者的端口号         |
| type             | RTCIceCandidateType    | 是   | 否   | 否   | 候选者的类型           |
| tcpType          | RTCIceTcpCandidateType | 是   | 否   | 否   | TCP 候选者的类型       |
| relatedAddress   | string                 | 是   | 否   | 否   | 相关候选者的地址       |
| relatedPort      | number                 | 是   | 否   | 否   | 相关候选者的端口号     |
| usernameFragment | string                 | 是   | 否   | 否   | ICE 候选者的用户名片段 |

### toJSON

toJSON(): RTCIceCandidateInit;

通常用于将 RTCIceCandidate 对象转换为 JSON 兼容格式

返回值：

| 类型                | 说明                                                         |
| ------------------- | ------------------------------------------------------------ |
| RTCIceCandidateInit | 返回一个 RTCIceCandidateInit 对象，表示 RTCIceCandidate 在 JSON 兼容格式下的表示。 |


## RTCDataChannelInit 

可以根据需要定义数据通道的特性和行为

### 属性

| 名称              | 类型    | 必填 | 说明 |
| ----------------- | ------- | ---- | ---- |
| ordered           | boolean | 否   | 是否消息按顺序传递 |
| maxPacketLifeTime | number  | 否   | 数据包的最大生命周期 |
| maxRetransmits    | number  | 否   | 最大重传次数 |
| protocol          | string  | 否   | 数据通道使用的协议 |
| negotiated        | boolean | 否   | 数据通道是否由应用程序进行协商 |
| id                | number  | 否   | 数据通道的 ID  |


## RTCDataChannel

RTCPeerConnection 的数据通道

### 属性

| 名称                       | 类型             | 可读 | 可写 | 说明                                                     |
| -------------------------- | ---------------- | ---- | ---- | -------------------------------------------------------- |
| label                      | string           | 是   | 否   | 数据通道的标签  |
| ordered                    | boolean          | 是   | 否   | 是否消息按顺序传递  |
| maxPacketLifeTime          | number           | 是   | 否   | 数据包的最大生命周期  |
| maxRetransmits             | number           | 是   | 否   | 最大重传次数  |
| protocol                   | string           | 是   | 否   | 数据通道使用的协议  |
| negotiated                 | boolean          | 是   | 否   | 数据通道是否由应用程序进行协商  |
| id                         | number           | 是   | 否   | 数据通道的 ID |
| readyState                 | DataChannelState | 是   | 否   | 数据通道的当前状态  |
| bufferedAmount             | number           | 是   | 否   | 已发送但未确认的字节数  |
| bufferedAmountLowThreshold | number           | 是   | 是   | 当 bufferedAmount 低于该值时，将触发 bufferedamountlow 事件 |
| binaryType                 | BinaryType       | 是   | 是   | 指定二进制数据的类型（当前未使用） |
| onbufferedamountlow        | EventHandler \| null  | 是   | 是   | 该事件处理器的事件类型为 bufferedamountlow |
| onclose                    | EventHandler \| null  | 是   | 是   | 该事件处理器的事件类型为 close |
| onclosing                  | EventHandler \| null  | 是   | 是   | 该事件处理器的事件类型为 closing |
| onopen                     | EventHandler \| null  | 是   | 是   | 该事件处理器的事件类型为 open |
| onmessage                  | EventHandler \| null  | 是   | 是   | 该事件处理器的事件类型为 message |
| onerror                    | EventHandler \| null  | 是   | 是   | 该事件处理器的事件类型为 error |

### 事件

| 名称              | 接口             | 说明                                                         |
| ----------------- | --------------- | ------------------------------------------------------------ |
| bufferedamountlow | Event           | 当 RTCDataChannel 对象的 bufferedAmount 从高于其 bufferedAmountLowThreshold 降低到小于或等于其 bufferedAmountLowThreshold 时触发 |
| close             | Event           | 当 RTCDataChannel 对象的底层数据传输已经关闭时触发 |
| closing           | Event           | 当 RTCDataChannel 对象转换到 closing 状态时触发 |
| open              | Event           | 当 RTCDataChannel 对象的底层数据传输已经建立或重新建立时触发 |
| message           | MessageEvent    | 当成功接收到消息时触发（data为string或ArrayBuffer） |
| error             | RTCErrorEvent   | 当 RTCDataChannel 对象发生错误时触发 |

### close

close(): void;

关闭数据通道

### send(string)

send(data: string): void;

发送数据。

参数：

| 参数名 | 类型   | 必填 | 说明 |
| ------ | ------ | ---- | ---- |
| data   | string | 是   | 数据 |

### send(ArrayBuffer)

send(data: ArrayBuffer): void;

发送数据。

参数：

| 参数名 | 类型        | 必填 | 说明 |
| ------ | ----------- | ---- | ---- |
| data   | ArrayBuffer | 是   | 数据 |

## RTCRtpReceiver

用于表示 RTP 流的接收端点，允许应用程序通过网络接收媒体数据（音频或视频）。

### 属性

| 名称        | 类型                                     | 可读   | 可写  | 说明                        |
|-----------|----------------------------------------|------|-----|---------------------------|
| id        | string \| null                          | 是    | 否   | RTCRtpReceiver 的唯一标识符，未关联轨道时为 null |
| track     | [MediaStreamTrack](#mediastreamtrack)  | 是    | 否   | 表示与 RTCRtpReceiver 关联的媒体轨道 |
| transport | [RTCDtlsTransport](#rtcdtlstransport) \| null | 是   | 否 | 用于接收媒体数据的传输机制           |

### getParameters

getParameters(): RTCRtpReceiveParameters

用于获取当前 RTCRtpReceiver 实例的接收参数 RTCRtpReceiveParameters。

返回值：

| 类型                                                  | 说明                                                                                     |
|-----------------------------------------------------|----------------------------------------------------------------------------------------|
| [RTCRtpReceiveParameters](#rtcrtpreceiveparameters) | 返回一个 RTCRtpReceiveParameters 对象，该对象描述了当前 RTCRtpReceiver 实例的接收参数，包括编解码器、头部扩展和 RTCP 参数等。 |

### getContributingSources

getContributingSources(): RTCRtpContributingSource[];

为该接收器在过去10秒内接收到的每个唯一 CSRC 标识符返回一个 RTCRtpContributingSource ，按时间戳降序排列。

返回值

| 类型                                                    | 说明             |
|-------------------------------------------------------|----------------|
| [RTCRtpContributingSource](#rtcrtpcontributingsource) | 包含关于 CSRC 的信息。 |

### getSynchronizationSources

getSynchronizationSources(): RTCRtpSynchronizationSource[];

为该接收器在过去10秒内接收到的每个唯一 SSRC 标识符返回一个 RTCRtpSynchronizationSource ，按时间戳降序排列。

返回值

| 类型                                                          | 说明             |
|-------------------------------------------------------------|----------------|
| [RTCRtpSynchronizationSource](#rtcrtpsynchronizationsource) | 包含关于 SSRC 的信息。 |

### getStats

getStats(): Promise<RTCStatsReport>

用于获取与该接收器相关的统计信息。

返回值

| 类型                                          | 说明             |
|---------------------------------------------|----------------|
| Promise\<[RTCStatsReport](#rtcstatsreport)> | 包含与接收器相关的统计信息。 |

### getCapabilities

static getCapabilities(kind: string): RTCRtpCapabilities | null

获取特定媒体类型的 RTP 能力。

参数：

| 名称   | 类型     | 必填  | 说明                       |
|------|--------|-----|--------------------------|
| kind | string | 是   | 表示媒体类型，"audio" 或 "video" |

返回值

| 类型                                        | 说明                               |
|-------------------------------------------|----------------------------------|
| [RTCRtpCapabilities](#rtcrtpcapabilities) | 包含了对指定媒体类型的 RTP 支持的编解码器和 RTP 头扩展 |
| null                                      | 没有支持的媒体类型                        |

## RTCRtpCodingParameters

定义了用于配置 RTP 编码参数的结构。

### 属性

| 参数名 | 类型                   | 可读        | 可写  | 说明     |
|-----|----------------------|-----------|-----|--------|
| rid | string \| undefined | 是   | 是      | RTP 流的唯一标识符 |

## RTCRtpEncodingParameters

用于配置 RTP 编码参数的属性。

### 属性

| 参数名                   | 类型       | 可读  | 可写            | 说明  |
|-----------------------|----------|-----|---------------|-----|
| active                | boolean \| undefined | 是   | 是   | 表示编码是否处于活动状态  |
| maxBitrate            | number \| undefined  | 是   | 是   | 表示编码的最大比特率    |
| maxFramerate          | number \| undefined  | 是   | 是   | 表示编码的最大帧率     |
| scaleResolutionDownBy | number \| undefined  | 是   | 是   | 表示对编码分辨率的缩放比例 |

## RTCRtpCodecParameters

用于描述媒体流的编解码格式。

### 属性

| 参数名         | 类型     | 可读  | 可写  | 说明                  |
|-------------|--------|-----|-----|---------------------|
| clockRate   | number | 是   | 是   | 表示编解码器的时钟速率，以赫兹为单位  |
| channels    | number \| undefined | 是   | 是   | 表示音频编解码器的通道数        |
| mimeType    | string | 是   | 是   | 表示编解码器的 SDP  格式化参数行 |
| sdpFmtpLine | string | 是   | 是   | 表示对编码分辨率的缩放比例       |
| payloadType | number | 是   | 是   | 表示编解码器的 RTP 载荷类型    |

## RTCRtpHeaderExtensionParameters

描述 RTP（实时传输协议）头部扩展的参数。

### 属性

| 参数名    | 类型    | 可读  | 可写 | 说明                              |
| --------- | ------- | ---- | --------|---------------------------------|
| id        | number  | 是   | 是   | 表示 RTP 头部扩展的标识符（ID）          |
| uri       | string  | 是   | 是   | 表示 RTP 头部扩展的统一资源标识符（URI） |
| encrypted | boolean \| undefined | 是   | 是   | 表示 RTP 头部扩展是否加密 |

## RTCRtcpParameters

描述 RTCP（实时传输控制协议）的参数。

### 属性

| 参数名      | 类型    | 可读  | 可写 | 说明                    |
| ----------- | ------- | ---- | ------|-----------------------|
| cname       | string \| undefined  | 是   | 是   | 表示 RTCP 的 CNAME           |
| reducedSize | boolean \| undefined | 是   | 是   | 表示是否启用 RTCP 小封包模式 |

## RTCRtpParameters

描述 RTP 的参数，包括编解码器参数、头部扩展参数和 RTCP 参数。

### 属性

| 参数名              | 类型                                                                    | 可读  | 可写  | 说明                 |
|------------------|-----------------------------------------------------------------------|-----|-----|--------------------|
| codecs           | [RTCRtpCodecParameters](#rtcrtpcodecparameters)[]                     | 是   | 是   | 表示 RTP 使用的编解码器参数数组 |
| headerExtensions | [RTCRtpHeaderExtensionParameters](#rtcrtpheaderextensionparameters)[] | 是   | 是   | 表示 RTP 使用的头部扩展参数数组 |
| rtcp             | [RTCRtcpParameters](#rtcrtpparameters)                                | 是   | 是   | 表示 RTP 使用的 RTCP 参数 |

## RTCRtpSendParameters

发送 RTP 数据的特定参数。

### 属性

| 参数名           | 类型                                                      | 可读  | 可写  | 说明                   |
|---------------|---------------------------------------------------------|-----|-----|----------------------|
| encodings     | [RTCRtpEncodingParameters](#rtcrtpencodingparameters)[] | 是   | 是   | 描述了发送 RTP 数据时的编码参数数组 |
| transactionId | string                                                  | 是   | 是   | 用于唯一标识发送参数配置的事务 ID   |

## RTCRtpReceiveParameters

描述接收 RTP 数据时的参数（当前为空）。

## RTCRtpContributingSource

包含 CSRC 信息。

### 属性

| 参数名          | 类型               | 可读  | 可写  | 说明                                                  |
|--------------|------------------|-----|-------|-----------------------------------------------------|
| timestamp    | HighResTimeStamp | 是   | 是   | 表示 RTP 数据包中的帧从这个源发送到 RTCRtpReceiver 的 MediaStreamTrack 的最近时间 |
| source       | string           | 是   | 是   | CSRC 或 SSRC                                         |
| audioLevel   | string \| undefined      | 是   | 是      | 仅适用于音频接收器                       |
| rtpTimestamp | string           | 是   | 是   | RTP 时间戳                                             |


## RTCRtpSynchronizationSource

包含 SSRC 信息，继承自 RTCRtpContributingSource（当前为空）。

## RTCRtpCodec

描述了在 RTCPeerConnection 中使用的 RTP 编解码器的能力和参数

### 属性

| 名称        | 类型   | 可读  | 可写   | 说明                                                 |
| ----------- | ------ | ---- |------|----------------------------------------------------|
| mimeType    | string | 是   | 是    | 表示编解码器的媒体类型和子类型                                    |
| clockRate   | number | 是   | 是    | 表示编解码器的时钟速率，单位为 Hz                           |
| channels    | number \| undefined | 是   | 是    | 表示音频编解码器的声道数目                                  |
| sdpFmtpLine | string \| undefined | 是   | 是    | 表示编解码器的 SDP 属性行，用于指定编解码器的额外参数和配置 |

## RTCRtpHeaderExtensionCapability

描述在 RTCPeerConnection 中使用的 RTP 头扩展的能力和参数

### 属性

| 名称  | 类型     | 可读  | 可写  | 说明                 |
|-----|--------|-----|-----|--------------------|
| uri | string | 是   | 是   | 表示 RTP 头扩展的标识符 URI |

## RTCRtpCapabilities

描述在 RTCPeerConnection 中支持的 RTP 能力，包括支持的编解码器和头扩展能力。

### 属性

| 名称               | 类型                                                          | 可读  | 可写  | 说明                 |
|------------------|-------------------------------------------------------------|-----|-----|--------------------|
| codecs           | [RTCRtpCodec](#rtcrtpcodec)[]                     | 是   | 是   | 表示支持的 RTP 编解码器能力列表 |
| headerExtensions | [RTCRtpHeaderExtensionCapability](#rtcrtpheaderextensioncapability)[] | 是   | 是   | 表示支持的 RTP 头扩展能力列表  |

## RTCDTMFSender

发送 DTMF 信号

### 属性

| 名称          | 类型                  | 可读 | 可写 | 说明                                         |
| ------------- | --------------------- | ---- | ---- | -------------------------------------------- |
| canInsertDTMF | boolean               | 是   | 否   | 表示当前是否可以插入 DTMF 信号               |
| toneBuffer    | string                | 是   | 否   | 表示当前在发送队列中等待发送的 DTMF 信号音调 |
| ontonechange  | EventHandler \| null  | 是   | 是   | 该事件处理器的事件类型为tonechange |

### 事件

| 名称          | 接口                   | 说明                                                         |
| ------------- | ---------------------- | ------------------------------------------------------------ |
| ontonechange  | RTCDTMFToneChangeEvent | 当 RTCDTMFSender 对象要么刚刚开始播放音调，要么刚刚结束 toneBuffer 中音调的播放时触发 |

### insertDTMF

insertDTMF(tones: string, duration?: number, interToneGap?: number): void;

在 RTCPeerConnection 中向发送队列中插入 DTMF（双音多频）信号

参数

| 名称         | 类型   | 必填 | 说明                           |
| ------------ | ------ | ---- | ------------------------------ |
| tones        | string | 是   | 表示要发送的 DTMF 信号音调序列 |
| duration     | number | 否   | 表示每个音调的持续时间         |
| interToneGap | number | 否   | 表示音调之间的间隔时间         |

## RTCRtpSender

用于发送 RTP 数据的对象的属性和方法

### 属性

| 名称        | 类型                               | 可读   | 可写 | 说明  |
|-----------|----------------------------------|------|---|-----|
| track     | [MediaStreamTrack](#mediastreamtrack)  \| null | 是 | 否   | 表示当前与该 RTP 发送器关联的 MediaStreamTrack 对象          |
| transport | [RTCDtlsTransport](#rtcdtlstransport)  \| null | 是 | 否   | 表示可选的 RTCDtlsTransport 对象，用于指定 RTP 数据的传输    |
| dtmf      | [RTCDTMFSender](#RTCDTMFSender) \| null | 是 | 否   | 表示可选的 RTCDTMFSender 对象，用于发送 DTMF（双音多频）信号 |

### setParameters

setParameters(parameters: RTCRtpSendParameters): Promise<void>;

设置当前 RTP 发送器的参数

参数

| 名称         | 类型                                            | 必填  | 说明                             |
|------------|-----------------------------------------------|-----|--------------------------------|
| parameters | [RTCRtpSendParameters](#RTCRtpSendParameters) | 是   | 包含了发送器的编解码器、头部扩展和 RTCP 相关参数等信息 |

返回值

| 类型             | 说明           |
|----------------|--------------|
| Promise\<void> | 当设置操作成功完成时解析 |

### getParameters

getParameters(): RTCRtpSendParameters;

获取当前 RTP 发送器的参数

返回值

| 类型                                            | 说明                                           |
|-----------------------------------------------|----------------------------------------------|
| [RTCRtpSendParameters](#RTCRtpSendParameters) | 返回一个 RTCRtpSendParameters 对象，包含当前 RTP 发送器的参数 |

### replaceTrack

replaceTrack(withTrack: MediaStreamTrack | null): Promise<void>;

用于替换当前与发送器关联的媒体轨道

参数：

| 名称        | 类型                                      | 必填   | 说明              |
|-----------|-----------------------------------------|------|-----------------|
| withTrack | [MediaStreamTrack](#mediastreamtrack) \| null | 否   | 用于替换当前与发送器关联的轨道 |

返回值

| 类型             | 说明                                  |
|----------------|-------------------------------------|
| Promise\<void> | 当替换操作成功完成时解析；如果替换操作失败，则会拒绝并返回相应的错误。 |

### setStreams

setStreams(...streams: MediaStream[]): void;

用于将一个或多个 MediaStream 对象与该发送器关联

参数：

| 名称      | 类型                            | 必填 | 说明                                                       |
|---------|-------------------------------|--|----------------------------------------------------------|
| streams | [MediaStream](#mediastream)[] | 否 | 不定数量的 MediaStream 对象，用于关联到该发送器。可以传递一个或多个 MediaStream 对象。 |

### getStats

getStats(): Promise<RTCStatsReport>;

用于获取与该发送器相关的统计信息

返回值

| 类型                                          | 说明            |
|---------------------------------------------|---------------|
| Promise\<[RTCStatsReport](#rtcstatsreport)> | 包含与发送器相关的统计信息 |

### getCapabilities

static getCapabilities(kind: string): RTCRtpCapabilities | null;

获取特定媒体类型的 RTP 能力

参数：

| 名称   | 类型     | 必填  | 说明                       |
|------|--------|-----|--------------------------|
| kind | string | 是   | 表示媒体类型，"audio" 或 "video" |

返回值

| 类型                                        | 说明                               |
|-------------------------------------------|----------------------------------|
| [RTCRtpCapabilities](#rtcrtpcapabilities) | 包含了对指定媒体类型的 RTP 支持的编解码器和 RTP 头扩展 |
| null                                      | 没有支持的媒体类型                        |


## RTCDtlsTransport

用于管理 DTLS 连接。

### 属性

| 名称          | 类型                                              | 可读                             | 可写 | 说明                                |
| ------------- |-------------------------------------------------|--------------------------------| ---- | ----------------------------------- |
| iceTransport  | [RTCIceTransport](#rtcicetransport)             | 是    | 否   | 用于建立和管理底层传输的ICE传输对象 |
| state         | [RTCDtlsTransportState](#rtcdtlstransportstate) | 是    | 否   | 表示当前的 DTLS 传输状态            |
| onstatechange | EventHandler \| null                           | 是   | 是   | 该事件处理器的事件类型为 statechange |
| onerror       | EventHandler \| null                           | 是   | 是   | 该事件处理器的事件类型为 error |

### 事件

| 名称          | 接口            | 说明                           |
|-------------|---------------|------------------------------|
| statechange | Event         | 当 RTCDtlsTransport 状态改变时触发   |
| error       | RTCErrorEvent | 当在 RTCDtlsTransport 上发生错误时触发 |

### getRemoteCertificates

getRemoteCertificates(): ArrayBuffer[];

获取远程对等端提供的证书。

返回值：

| 类型            | 说明                       |
|---------------|--------------------------|
| ArrayBuffer[] | 每一个 ArrayBuffer 表示一个远程证书 |

## RTCDtlsFingerprint

用于描述 DTLS（Datagram Transport Layer Security）的指纹信息。

### 属性

| 名称      | 类型   | 必填 | 说明                                         |
| --------- | ------ | ---- | -------------------------------------------- |
| algorithm | string | 否   | 指定了用于计算指纹的算法                     |
| value     | string | 否   | 包含了实际的指纹值，是一个经过计算的唯一标识 |

## RTCCertificate

用于描述 RTCPeerConnection 中的证书信息。

### 属性

| 名称    | 类型           | 可读 | 可写 | 说明                                           |
| ------- | -------------- | ---- | ---- | ---------------------------------------------- |
| expires | EpochTimeStamp | 是   | 否   | 指定了证书的过期时间，即证书将在此时间之后失效 |

### getFingerprints

getFingerprints(): RTCDtlsFingerprint[];

用于获取与当前 `RTCCertificate` 相关联的 DTLS 指纹信息数组。

返回值

| 类型                 | 说明                                                         |
| -------------------- | ------------------------------------------------------------ |
| RTCDtlsFingerprint[] | 包含多个 RTCDtlsFingerprint 对象，每个对象描述了证书的一个 DTLS 指纹信息。 |


## RTCIceCandidatePair

表示 ICE 候选者对。

### 属性

| 参数名 | 类型            | 必填 | 说明            |
| ------ | --------------- | ---- | --------------- |
| local  | RTCIceCandidate | 否   | 本地 ICE 候选者 |
| remote | RTCIceCandidate | 否   | 远程 ICE 候选者 |

## RTCIceParameters

示 ICE 连接过程中使用的参数。

### 属性

| 参数名           | 类型   | 必填 | 说明           |
| ---------------- | ------ | ---- | -------------- |
| usernameFragment | string | 是   | ICE 用户名片段 |
| password         | string | 是   | ICE 密码       |

## RTCIceTransport

表示 ICE 传输过程中的相关属性和方法，管理和监控 ICE 连接的状态，并提供了一些方法来获取本地和远程的候选者以及其他它接参数。

### 属性

| 名称           | 类型                 | 可读 | 可写 | 说明                     |
| -------------- | -------------------- | ---- | ---- | ------------------------ |
| role           | RTCIceRole           | 是   | 否   | 表示 ICE 角色            |
| component      | RTCIceComponent      | 是   | 否   | 表示 ICE 组件类型        |
| state          | RTCIceTransportState | 是   | 否   | 表示当前的 ICE 传输状态  |
| gatheringState | RTCIceGathererState  | 是   | 否   | 表示当前的候选者收集状态 |
| onstatechange                 | EventHandler \| null  | 是   | 是   | 该事件处理器的事件类型为 statechange |
| ongatheringstatechange        | EventHandler \| null  | 是   | 是   | 该事件处理器的事件类型为 gatheringstatechange |
| onselectedcandidatepairchange | EventHandler \| null  | 是   | 是   | 该事件处理器的事件类型为 selectedcandidatepairchange |

### 事件

| 名称                        | 接口  | 说明                                                         |
| --------------------------- | ----- | ------------------------------------------------------------ |
| statechange                 | Event | 当 RTCIceTransport 状态改变时触发 |
| gatheringstatechange        | Event | 当 RTCIceTransport 的收集状态改变时触发 |
| selectedcandidatepairchange | Event | 当 RTCIceTransport 选择的候选对改变时触发 |

### getLocalCandidates *(暂未实现)*

getLocalCandidates(): RTCIceCandidate[]

> **注意**：该方法已在头文件（`ice_transport.h`）中声明，但当前版本尚未实现，调用无效。

返回值：

| 类型              | 说明                                        |
| ----------------- | ------------------------------------------- |
| RTCIceCandidate[] | 本地收集到的 ICE 候选者数组 |

### getLocalParameters *(暂未实现)*

getLocalParameters(): RTCIceParameters | null;

> **注意**：该方法已在头文件（`ice_transport.h`）中声明，但当前版本尚未实现，调用无效。

返回值：

| 类型                     | 说明                                                         |
| ------------------------ | ------------------------------------------------------------ |
| RTCIceParameters \| null | 本地的 ICE 参数（usernameFragment 和 password），无可用参数时返回 null |

### getRemoteCandidates *(暂未实现)*

getRemoteCandidates(): RTCIceCandidate[]

> **注意**：该方法已在头文件（`ice_transport.h`）中声明，但当前版本尚未实现，调用无效。

返回值：

| 类型              | 说明                                        |
| ----------------- | ------------------------------------------- |
| RTCIceCandidate[] | 远程对等方的 ICE 候选者数组 |

### getRemoteParameters *(暂未实现)*

getRemoteParameters(): RTCIceParameters | null;

> **注意**：该方法已在头文件（`ice_transport.h`）中声明，但当前版本尚未实现，调用无效。

返回值：

| 类型                     | 说明                                                         |
| ------------------------ | ------------------------------------------------------------ |
| RTCIceParameters \| null | 远程对等方的 ICE 参数（usernameFragment 和 password），无可用参数时返回 null |

### getSelectedCandidatePair

getSelectedCandidatePair(): RTCIceCandidatePair | null;

返回值：

| 类型                        | 说明                                                         |
| --------------------------- | ------------------------------------------------------------ |
| RTCIceCandidatePair \| null | 返回当前选择的 ICE 候选者对，如果没有可用的候选者对则返回 null |

## RTCSctpTransport

用于 SCTP 数据传输的属性和关联的 DTLS 传输对象。

### 属性

| 名称             | 类型                                              | 可读  | 可写  | 说明                      |
|----------------|-------------------------------------------------|-----|-----|-------------------------|
| maxChannels    | number                                          | 是   | 否   | SCTP 传输的最大通道数           |
| maxMessageSize | number                                          | 是   | 否   | SCTP 传输的最大消息大小          |
| state          | [RTCSctpTransportState](#rtcsctptransportstate) | 是   | 否   | SCTP 传输的当前状态            |
| transport      | [RTCDtlsTransport](#rtcdtlstransport)           | 是   | 否   | 用于传输 SCTP 数据的 DTLS 传输对象 |
| onstatechange  | EventHandler \| null                            | 是   | 是   | 该事件处理器的事件类型为 statechange |

## RTCStats

统计数据的基本结构，包括时间戳、统计类型和统计数据的唯一标识符。

### 属性

| 名称      | 类型             | 可读 | 可写 | 说明                                     |
| --------- | ---------------- | ---- | ---- | ---------------------------------------- |
| timestamp | HighResTimeStamp | 是   | 是   | 统计数据的时间戳                         |
| type      | RTCStatsType     | 是   | 是   | 统计数据的类型，指示这组数据属于哪个类别 |
| id        | string           | 是   | 是   | 统计数据的唯一标识符                     |

## RTCStatsReport

表示一组 RTCPeerConnection 的统计信息。

### 属性

| 名称  | 类型                  | 可读 | 可写 | 说明                                         |
| ----- | --------------------- | ---- | ---- | -------------------------------------------- |
| stats | Map<string, RTCStats> | 是   | 否   | 包含有关 RTCPeerConnection 会话或连接特定方面的详细信息 |

## RTCTransportStats

描述 RTCPeerConnection 传输统计数据的结构。

### 属性

| 名称                         | 类型                  | 可读 | 可写 | 必填 | 说明                                                  |
| ---------------------------- | --------------------- | ---- | ---- | ---- | ----------------------------------------------------- |
| packetsSent                  | number                | 是   | 是   | 否   | 发送的数据包数量                                      |
| packetsReceived              | number                | 是   | 是   | 否   | 接收的数据包数量                                      |
| bytesSent                    | number                | 是   | 是   | 否   | 发送的字节数                                          |
| bytesReceived                | number                | 是   | 是   | 否   | 接收的字节数                                          |
| iceRole                      | RTCIceRole            | 是   | 是   | 否   | ICE 连接的角色                                        |
| iceLocalUsernameFragment     | string                | 是   | 是   | 否   | ICE 本地用户名片段                                    |
| dtlsState                    | RTCDtlsTransportState | 是   | 是   | 是   | DTLS 连接的状态                                       |
| iceState                     | RTCIceTransportState  | 是   | 是   | 否   | ICE 连接的状态                                        |
| selectedCandidatePairId      | string                | 是   | 是   | 否   | 选定的 ICE 候选对的唯一标识符                         |
| localCertificateId           | string                | 是   | 是   | 否   | 本地证书的唯一标识符                                  |
| remoteCertificateId          | string                | 是   | 是   | 否   | 远端证书的唯一标识符                                  |
| tlsVersion                   | string                | 是   | 是   | 否   | TLS 使用的版本号                                      |
| dtlsCipher                   | string                | 是   | 是   | 否   | DTLS 使用的加密算法                                   |
| srtpCipher                   | string                | 是   | 是   | 否   | SRTP 使用的加密算法                                   |
| selectedCandidatePairChanges | number                | 是   | 是   | 是   | 选定的 ICE 候选对的更改次数，用于跟踪候选对的变化情况 |

## RTCRtpStreamStats

描述 RTCPeerConnection RTP 流的统计数据结构，包括流的基本信息和相关的统计指标。

### 属性

| 名称        | 类型   | 可读 | 可写 | 必填 | 说明                   |
| ----------- | ------ | ---- | ---- | ---- | ---------------------- |
| ssrc        | number | 是   | 是   | 是   | RTP 流的同步信源标识符 |
| kind        | string | 是   | 是   | 是   | 流的类型或媒体类型     |
| transportId | string | 是   | 是   | 否   | 传输的唯一标识符       |
| codecId     | string | 是   | 是   | 否   | 编解码器的唯一标识符   |

## RTCIceCandidatePairStats

描述 ICE 候选对的统计信息，包括传输的状态、数据包和字节的发送接收情况、往返时延等关键指标。

### 属性

| 名称                        | 类型                          | 可读 | 可写 | 必填 | 说明                                                         |
| --------------------------- | ----------------------------- | ---- | ---- | ---- | ------------------------------------------------------------ |
| transportId                 | string                        | 是   | 是   | 是   | ICE 候选对所使用的传输 ID，唯一标识该 ICE 候选对所属的传输通道 |
| localCandidateId            | string                        | 是   | 是   | 是   | 表示本地 ICE 候选 ID，用于标识本地候选。                     |
| remoteCandidateId           | string                        | 是   | 是   | 是   | 表示远端 ICE 候选 ID，用于标识远端候选。                     |
| state                       | RTCStatsIceCandidatePairState | 是   | 是   | 是   | 表示 ICE 候选对的状态                                        |
| nominated                   | boolean                       | 是   | 是   | 否   | 表示是否已经被选定作为当前通信的候选对。                     |
| packetsSent                 | number                        | 是   | 是   | 否   | 表示从本地候选发送的数据包数量。                             |
| packetsReceived             | number                        | 是   | 是   | 否   | 表示接收到的数据包数量。                                     |
| bytesSent                   | number                        | 是   | 是   | 否   | 表示从本地候选发送的总字节数。                               |
| bytesReceived               | number                        | 是   | 是   | 否   | 表示接收到的总字节数。                                       |
| lastPacketSentTimestamp     | HighResTimeStamp              | 是   | 是   | 否   | 表示最后一个数据包从本地候选发送的时间戳。                   |
| lastPacketReceivedTimestamp | HighResTimeStamp              | 是   | 是   | 否   | 表示最后一个数据包接收到的时间戳。                           |
| totalRoundTripTime          | number                        | 是   | 是   | 否   | 表示总的往返时延，以毫秒为单位。                             |
| currentRoundTripTime        | number                        | 是   | 是   | 否   | 表示当前的往返时延，以毫秒为单位。                           |
| availableOutgoingBitrate    | number                        | 是   | 是   | 否   | 表示可用的出口比特率，以比特每秒（bps）为单位。              |
| availableIncomingBitrate    | number                        | 是   | 是   | 否   | 表示可用的入口比特率，以比特每秒（bps）为单位。              |
| requestsReceived            | number                        | 是   | 是   | 否   | 表示接收到的请求数量。                                       |
| requestsSent                | number                        | 是   | 是   | 否   | 表示发送的请求数量。                                         |
| responsesReceived           | number                        | 是   | 是   | 否   | 表示接收到的响应数量。                                       |
| responsesSent               | number                        | 是   | 是   | 否   | 表示发送的响应数量。                                         |
| consentRequestsSent         | number                        | 是   | 是   | 否   | 表示发送的同意请求数量。                                     |
| packetsDiscardedOnSend      | number                        | 是   | 是   | 否   | 表示在发送过程中丢弃的数据包数量。                           |
| bytesDiscardedOnSend        | number                        | 是   | 是   | 否   | 表示在发送过程中丢弃的总字节数。                             |

## MediaSourceState 

描述媒体源（如音频或视频源）的状态。

### 属性

| 名称         | 类型   | 说明                                                     |
| ------------ | ------ | -------------------------------------------------------- |
| initializing | string | 表示媒体源正在初始化，即将开始获取数据但尚未开始播放。   |
| live         | string | 表示媒体源处于活动状态，正在正常播放中                   |
| ended        | string | 表示媒体源已经结束，不再提供新的数据。                   |
| muted        | string | 表示媒体源被静音，即便仍在活动状态，但无法正常播放声音。 |

## MediaSource 

表示一个媒体源（如音频或视频源）。

### 属性

| 名称    | 类型               | 可读 | 可写 | 说明            |
|-------|------------------|----|----|---------------|
| state | MediaSourceState | 是  | 否  | 表示了媒体源处于的不同状态 |

### release

release(): void

释放相关资源。MediaSource 或子类实例不再使用后必须调用该方法，否则将导致资源泄漏。

#### 参数

无

#### 返回值

无

## AudioSource

继承自MediaSource，表示一个音频源。

### setVolume

setVolume(volume: number);

设置音频源的音量。

参数：

| 参数名   | 类型     | 必填 | 说明                                                   |
| -------- | -------- | ---- | ------------------------------------------------------ |
| volume   | number   | 是   | 音量 |

## AudioTrack 

继承自 MediaStreamTrack，扩展音频轨道特性。


## VideoSource

继承自MediaSource，表示一个视频源。

### 属性

| 名称  | 类型             | 可读 | 可写 | 说明                       |
| ----- | ---------------- | ---- |----| -------------------------- |
| oncapturerstarted | EventHandler \| null | 是   | 是  | 该事件处理器的事件类型为 capturerstarted |
| oncapturerstopped | EventHandler \| null | 是   | 是  | 该事件处理器的事件类型为 capturerstopped |

### 事件

| 名称              | 接口                        | 说明                 |
|-----------------|---------------------------|--------------------|
| capturerstarted | VideoCapturerStartedEvent | 该视频源开始采集（成功或失败）时触发 |
| capturerstopped | Event                     | 视频源停止采集时触发         |

### applyDisplayRotation（OHOS 扩展）

`applyDisplayRotation(displayRotationDegree: number): void`

在开流、旋转或折叠状态稳定后，同步应用所在窗口当前显示屏的角度，单位为度，使用 0、90、180、270。
不要直接传 `Display.rotation` 的枚举数值，应转换为角度；不要提前提交尚未生效的目标角度。
方法向采集线程异步提交更新，返回时不保证新方向的帧已经到达。它不设置窗口旋转策略，也不设置照片方向。
原生采集器已有显示变化监听；此入口用于应用明确同步其窗口所对应的显示状态，当前不是每个普通接入方都必须手工调用的步骤。

## VideoTrack

继承自 MediaStreamTrack，表示一个视频轨道，配合VideoRenderController使用可实现视频的渲染。

## PeerConnectionFactory

用于创建和管理 RTCPeerConnection 相关的对象。

### 构造函数

new(options?: PeerConnectionFactoryOptions): PeerConnectionFactory

用于创建 PeerConnectionFactory 变量实例。

#### 参数

| 参数名     | 类型                           | 必填  | 说明                         |
|---------|------------------------------|-----|----------------------------|
| options | PeerConnectionFactoryOptions | 否   | 创建PeerConnectionFactory的参数 |

#### 返回值

| 类型                    | 说明                      |
|-----------------------|-------------------------|
| PeerConnectionFactory | PeerConnectionFactory实例 |

### setDefault

static setDefault(factory: PeerConnectionFactory): void

用于设置默认的 PeerConnectionFactory 实例，可以供其它模块获取使用，如 MediaDevices 的 C++ 层代码。

#### 参数

| 参数名     | 类型                    | 必填 | 说明                        |
|---------|-----------------------|----|---------------------------|
| factory | PeerConnectionFactory | 是  | 默认的 PeerConnectionFactory |

#### 返回值

无

### createPeerConnection

createPeerConnection(config: RTCConfiguration): RTCPeerConnection;

根据指定配置信息创建 RTCPeerConnection 对象。

参数：

| 参数名    | 类型               | 必填  | 说明                      |
|--------|------------------|-----|-------------------------|
| config | RTCConfiguration | 是   | 一个包含配置信息的对象，用于配置对等连接的行为 |

返回值：

| 类型                | 说明                                                     |
|-------------------|--------------------------------------------------------|
| RTCPeerConnection | 表示一个 RTCPeerConnection 对象，用于在本地和远程对等端之间传输流数据、媒体和任何其它消息 |

### createAudioSource

createAudioSource(constraints?: MediaTrackConstraints): AudioSource;

用于根据指定的媒体约束创建一个音频源对象。

| 参数名         | 类型                                              | 必填  | 说明                                                                                                                               |
|-------------|-------------------------------------------------|-----|----------------------------------------------------------------------------------------------------------------------------------|
| constraints | [MediaTrackConstraints](#mediatrackconstraints) | 否   | 一个包含媒体约束条件的对象，用于配置音频源的行为。当前仅支持 echoCancellation、autoGainControl、noiseSuppression。<br/>参阅 [AudioDeviceModule](#audiodevicemodule) |

返回值：

| 类型                          | 说明                     |
|-----------------------------|------------------------|
| [AudioSource](#audiosource) | 表示一个音频源对象，用于捕获和处理音频输入流 |

> **说明：**
> 使用麦克风需要申请 ohos.permission.MICROPHONE 权限，具体请查阅相关文档。

> **3A 约束语义说明：**
> 1. `echoCancellation`、`autoGainControl`、`noiseSuppression` 三个约束的请求值（含显式 `false`）均会传递至实际音频处理配置，不会被静默忽略；未设置时使用引擎默认值。
> 2. 当前平台未对接可独立查询/启停的系统内置 3A 开关：`AudioDeviceModule` 的 `isBuiltInAcousticEchoCancelerSupported()` / `isBuiltInNoiseSuppressorSupported()` 返回 false。音频采集与播放默认使用语音通信（VOICE_COMMUNICATION）场景，系统可能按场景提供固定的音频处理；该场景行为不受上述约束控制，应用不应将软件约束的效果等同于系统处理能力。
> 3. 上述约束仅控制 WebRTC 软件音频处理（APM）。创建后不支持通过 `applyConstraints` 动态修改音频 3A 约束。

### createAudioTrack

createAudioTrack(id: string, source: AudioSource): AudioTrack;

用于根据指定的音频源创建一个音频轨道对象。

参数：

| 参数名    | 类型                          | 必填  | 说明                  |
|--------|-----------------------------|-----|---------------------|
| id     | string                      | 是   | 表示创建的音频轨道的唯一标识符或 ID |
| source | [AudioSource](#audiosource) | 是   | 表示用于音频输入的音频源对象      |

返回值：

| 类型         | 说明                          |
|------------|-----------------------------|
| AudioTrack | 表示一个音频轨道对象，用于管理和处理音频流的发送或接收 |

### createVideoSource

createVideoSource(constraints?: MediaTrackConstraints, isScreencast?: boolean): VideoSource;

用于根据指定的媒体约束创建一个视频源对象。

#### 参数

| 参数名 | 类型 | 必填 | 说明 |
| - | - | - | - |
| constraints | [MediaTrackConstraints](#mediatrackconstraints) | 否  | 一个包含视频媒体约束条件的对象，用于指定期望的视频源，如设备ID、分辨率等，当前支持的分辨率范围是 `1280x720` 到 `3840x2160`，具体情况以设备实际支持的情况为准 |
| isScreencast | boolean | 否 | 是否是屏幕录像，true：使用屏幕录像作为视频源（当前仅支持 width、height约束条件，且不支持 advanced 属性）；false：使用相机作为视频源（当前支持 width、height、aspectRatio、framRate、facingMode、deviceId约束条件） |

#### 返回值

| 类型                          | 说明                     |
|-----------------------------|------------------------|
| [VideoSource](#videosource) | 表示一个视频源对象，用于捕获和处理视频输入流 |

#### 示例

```typescript
this.localVideoSource = this.peerConnectionFactory.createVideoSource({width: 320, height: 240, frameRate:{min:15, max:30, ideal:25}});
this.localVideoSource!.oncapturerstarted = (event: webrtc.VideoCapturerStartedEvent) => {
  console.info(TAG, 'oncapturerstarted: ' + event.success ? "success" : "failed");
};
this.localVideoSource!.oncapturerstopped = (event: webrtc.Event) => {
  console.info(TAG, 'oncapturerstopped');
};
```

> **说明：**
> 使用相机需要申请 ohos.permission.CAMERA 权限，屏幕录像需要申请 ohos.permission.CAPTURE_SCREEN 权限，具体请查阅相关文档。

### createVideoTrack

createVideoTrack(id: string, source: VideoSource): VideoTrack;

用于根据指定的视频源创建一个视频轨道对象。

参数：

| 参数名 | 类型          | 必填 | 说明                                      |
| ------ |-------------| ---- | ----------------------------------------- |
| id     | string      | 是   | 表示创建的视频轨道的唯一标识符或 ID       |
| source | [VideoSource](#videosource) | 是   | 表示用于视频输入的视频源对象 |

返回值：

| 类型       | 说明                                                   |
| ---------- | ------------------------------------------------------ |
| VideoTrack | 表示一个视频轨道对象，用于管理和处理视频流的发送或接收 |

### startAecDump

startAecDump(fd: number, max_size_bytes: number): boolean;

用于启动音频回声消除 (AEC) 的转储功能，将音频数据写入指定的文件描述符 (`fd`) 中，并限制最大文件大小 (`max_size_bytes`)。

参数：

| 参数名         | 类型   | 必填 | 说明                                                         |
| -------------- | ------ | ---- | ------------------------------------------------------------ |
| fd             | number | 是   | 表示一个文件描述符或标识符，用于指定将音频数据写入的文件     |
| max_size_bytes | number | 是   | 表示最大文件大小，以字节为单位，用于限制音频数据写入文件的大小 |

返回值：

| 类型    | 说明                                                         |
| ------- | ------------------------------------------------------------ |
| boolean | 表示启动操作的结果，返回 true 表示成功启动音频回声消除的转储功能，返回 false 表示启动失败。 |

### stopAecDump

stopAecDump(): void;

停止音频回声消除 (AEC) 的转储功能，停止向文件写入音频数据。

参数：无

返回值：无

## PeerConnectionFactoryOptions

创建PeerConnectionFactory的参数。

### 属性

| 名称                     | 类型                  | 必填  | 说明       |
|------------------------|---------------------|-----|----------|
| adm                    | AudioDeviceModule   | 否   | 音频设备管理模块 |
| videoEncoderFactory    | VideoEncoderFactory | 否   | 视频编码器工厂  |
| videoDecoderFactory    | VideoDecoderFactory | 否   | 视频解码器工厂  |
| audioProcessing        | AudioProcessing        | 否   | 音频处理实例   |

## AudioDeviceModule

音频设备模块接口。

### 属性

| 名称                     | 类型                  | 可读   | 可写  | 说明  |
|------------------------|---------------------|------|-----|-----|
| oncapturererror        | EventHandler \| null | 是   | 是   | 该事件处理器的事件类型为 capturererror |
| oncapturerstatechange  | EventHandler \| null | 是   | 是   | 该事件处理器的事件类型为 capturerstatechange |
| oncapturersamplesready | EventHandler \| null | 是   | 是   | 该事件处理器的事件类型为 capturersamplesready |
| onrenderererror        | EventHandler \| null | 是   | 是   | 该事件处理器的事件类型为 renderererror |
| onrendererstatechange  | EventHandler \| null | 是   | 是   | 该事件处理器的事件类型为 rendererstatechange |

### 事件

| 名称                   | 接口                                                                | 说明                       |
|----------------------|-------------------------------------------------------------------|--------------------------|
| capturererror        | [AudioErrorEvent](#audioerrorevent)                               | 当音频采集器发生错误时触发            |
| capturerstatechange  | [AudioStateChangeEvent](#audiostatechangeevent)                   | 当音频采集器的状态改变时触发           |
| capturersamplesready | [AudioCapturerSamplesReadyEvent](#audiocapturersamplesreadyevent) | 当新的音频样本准备好时调用,这应该只用于调试目的 |
| renderererror        | [AudioErrorEvent](#audioerrorevent)                               | 当音频渲染器发生错误时触发            |
| rendererstatechange  | [AudioStateChangeEvent](#audiostatechangeevent)                   | 当音频渲染器发生错误时触发            |

### 构造函数

new(options?: AudioDeviceModuleOptions): AudioDeviceModule

用于创建 AudioDeviceModule 变量实例。

参数：

| 参数名     | 类型                                                    | 必填  | 说明          |
|---------|-------------------------------------------------------|-----|-------------|
| options | [AudioDeviceModuleOptions](#audiodevicemoduleoptions) | 否   | 构造参数，如采样率等。 |

### setSpeakerMute

setSpeakerMute(mute: boolean): void;

开启/取消扬声器静音。

参数：

| 参数名  | 类型      | 必填  | 说明                 |
|------|---------|-----|--------------------|
| mute | boolean | 是   | true：静音，false:取消静音 |

### setMicrophoneMute

setMicrophoneMute(mute: boolean): void;

开启/取消麦克风静音。

参数：

| 参数名  | 类型      | 必填  | 说明                   |
|------|---------|-----|----------------------|
| mute | boolean | 是   | true：开启静音，false:取消静音 |

### setNoiseSuppressorEnabled

setNoiseSuppressorEnabled(enabled: boolean): boolean;

启用或禁用内置噪声抑制器。

参数：

| 参数名     | 类型      | 必填  | 说明               |
|---------|---------|-----|------------------|
| enabled | boolean | 是   | true：启用，false:禁用 |

返回值

| 类型      | 说明                 |
|---------|--------------------|
| boolean | true: 成功，false: 失败 |

### isBuiltInAcousticEchoCancelerSupported

static isBuiltInAcousticEchoCancelerSupported(): boolean

获取设备是否支持内置回声消除。

返回值

| 类型      | 说明                  |
|---------|---------------------|
| boolean | true: 支持，false: 不支持 |

### isBuiltInNoiseSuppressorSupported

static isBuiltInNoiseSuppressorSupported(): boolean

获取设备是否支持内置噪声抑制。

返回值

| 类型      | 说明                  |
|---------|---------------------|
| boolean | true: 支持，false: 不支持 |

### release

release(): void

释放相关资源。AudioDeviceModule 实例不再使用后必须调用该方法，否则将导致资源泄漏。

#### 参数

无

#### 返回值

无

## VideoEncoderFactory

视频编码器工厂接口。

### 属性

无

## HardwareVideoEncoderFactory

硬件编码器工厂，实现了 VideoEncoderFactory 接口，使用平台提供的硬件编码器。

### 属性

| 名称                    | 类型      | 可读 | 可写 | 说明                         |
|-----------------------|---------|----|----|----------------------------|
| enableH264HighProfile | boolean | 是  | 否  | 表示是否启用H264编码器的High Profile |

### 构造函数

new(enableH264HighProfile?: boolean): HardwareVideoEncoderFactory

用于创建 HardwareVideoEncoderFactory 的变量实例。

#### 参数

| 参数名                   | 类型      | 必填 | 说明                          |
|-----------------------|---------|----|-----------------------------|
| enableH264HighProfile | boolean | 否  | true：启用H264编码器的High Profile |

#### 返回值

| 类型                          | 说明          |
|-----------------------------|-------------|
| HardwareVideoEncoderFactory | 硬件视频编码器工厂实例 |

#### 示例

```typescript
let videoEncoderFactory = new webrtc.HardwareVideoEncoderFactory(false);
let peerConnectionFactory = new webrtc.PeerConnectionFactory({ videoDecoderFactory: videoEncoderFactory });
```

## SoftwareVideoEncoderFactory

软件视频编码器工厂，实现了 VideoEncoderFactory 接口。内部使用WebRTC内置的软件视频编码码器，包括 H264（可选）、VP8、VP9、AV1。

### 属性

无

### 构造函数

new(): SoftwareVideoEncoderFactory

用于创建 SoftwareVideoEncoderFactory 的变量实例。

#### 参数

无

#### 返回值

| 类型                          | 说明          |
|-----------------------------|-------------|
| SoftwareVideoEncoderFactory | 软件视频编码器工厂实例 |

## VideoDecoderFactory

视频解码器工厂接口。

### 属性

无

## HardwareVideoDecoderFactory

硬件视频解码器工厂，实现了 VideoDecoderFactory 接口，使用平台提供的硬件解码器。

### 属性

无

### 构造函数

new(): HardwareVideoDecoderFactory

用于创建 HardwareVideoDecoderFactory 的变量实例。

#### 参数

无

#### 返回值

| 类型                          | 说明          |
|-----------------------------|-------------|
| HardwareVideoDecoderFactory | 硬件视频解码器工厂实例 |

## SoftwareVideoDecoderFactory

软件视频解码器工厂，实现了 VideoDecoderFactory 接口。内部使用WebRTC内置的软件视频解码器，包括 H264（可选）、VP8、VP9、AV1。

### 属性

无

### 构造函数

new(): SoftwareVideoDecoderFactory

用于创建 SoftwareVideoDecoderFactory 的变量实例。

#### 参数

无

#### 返回值

| 类型                          | 说明          |
|-----------------------------|-------------|
| SoftwareVideoDecoderFactory | 软件视频解码器工厂实例 |

## AudioProcessingFactory

音频处理工厂接口。

### 属性

无

### 构造函数

new(): AudioProcessingFactory

用于创建 AudioProcessingFactory 的变量实例。

#### 参数

无

#### 返回值

| 类型                     | 说明       |
|------------------------|----------|
| AudioProcessingFactory | 音频处理工厂实例 |

### create

create(): AudioProcessing

创建 AudioProcessing 实例。

#### 参数

无

#### 返回值

| 类型              | 说明       |
|-----------------|----------|
| AudioProcessing | 音频处理模块实例 |

#### 示例

```typescript
let audioProcessingFactory = new webrtc.AudioProcessingFactory();
let peerConnectionFactory = new webrtc.PeerConnectionFactory({ audioProcessing: this.audioProcessingFactory.create() });
```

## AudioProcessing

音频处理模块。

## Loggable 

日志输出接口。

### logMessage

logMessage(message: string, severity: number, tag: string): void;

向日志系统中添加日志条目。

参数：

| 名称       | 类型     | 必填  | 说明                             |
|----------|--------|-----|--------------------------------|
| message  | string | 是   | 要记录的日志消息                       |
| severity | number | 是   | 日志消息的严重性或级别，参见 LoggingSeverity |
| tag      | string | 是   | 日志消息的标签或分类                     |

## AudioErrorType

用于表示音频错误类型。

| 名称                   | 类型     | 说明                |
|----------------------|--------|-------------------|
| init                 | string | 音频采集器或渲染器初始化错误    |
| start-exception      | string | 音频采集器或渲染器启动异常     |
| start-state-mismatch | string | 音频采集器或渲染器启动时状态不匹配 |
| general              | string | 音频采集器或渲染器的其它错误    |

## AudioState

用于表示音频采集器或渲染器的状态。

| 名称    | 类型      | 说明           |
|-------|---------|--------------|
| start | string  | 音频采集器或渲染器已启动 |
| stop  | string  | 音频采集器或渲染器已停止 |

## AudioError

继承自Error，用于描述音频相关的错误信息。

### 属性

| 名称   | 类型                                | 可读  | 可写  | 说明             |
|------|-----------------------------------|-----|-----|----------------|
| type | [AudioErrorType](#audioerrortype) | 是   | 否   | 提供关于错误类型的明确描述  |

## AudioErrorEvent

继承自 Event，提供了关于发生的错误的详细描述。

### 属性

| 名称    | 类型                        | 可读  | 可写  | 说明     |
|-------|---------------------------|-----|-----|--------|
| error | [AudioError](#audioerror) | 是   | 否   | 错误详细信息 |

## AudioStateChangeEvent

继承自 Event，提供了关于音频采集器或渲染器状态变化的详细描述。

### 属性

| 名称    | 类型                        | 可读  | 可写  | 说明   |
|-------|---------------------------|-----|-----|------|
| state | [AudioState](#audiostate) | 是   | 否   | 新的状态 |

## AudioCapturerSamplesReadyEvent

继承自 Event，提供了音频采集器新的样本的详细描述。

### 属性

| 名称      | 类型                            | 可读  | 可写  | 说明     |
|---------|-------------------------------|-----|-----|--------|
| samples | [AudioSamples](#audiosamples) | 是   | 否   | 音频样本描述 |

### AudioSamples

表示音频样本信息。

### 属性

| 名称           | 类型          | 可读  | 可写  | 说明                                              |
|--------------|-------------|-----|-----|-------------------------------------------------|
| audioFormat  | number      | 是   | 否   | 采样格式，参见 ohos.multimedia.audio.AudioSampleFormat |
| channelCount | number      | 是   | 否   | 通道数，参见 ohos.multimedia.audio.AudioChannel       |
| sampleRate   | number      | 是   | 否   | 采样率，参见 ohos.multimedia.audio.AudioSamplingRate  |
| data         | ArrayBuffer | 否   | 是   | 采样数据                                            |

## AudioDeviceModuleOptions

创建AudioDeviceModule的参数。

### 属性

| 名称                              | 类型      | 必填  | 说明                                                                                |
|---------------------------------|---------|-----|-----------------------------------------------------------------------------------|
| audioSource                     | number  | 否   | 音频输入源，参见 ohos.multimedia.audio.SourceType，默认为 SOURCE_TYPE_VOICE_COMMUNICATION     |
| audioFormat                     | number  | 否   | 音频输入格式，参见 ohos.multimedia.audio.AudioSampleFormat, 默认为 SAMPLE_FORMAT_S16LE        |
| inputSampleRate                 | number  | 否   | 音频输入采样率，默认为 48000HZ。                                                              |
| useStereoInput                  | boolean | 否   | 控制是否使用立体声输入，默认为 false，即单声道。                                                       |
| outputSampleRate                | number  | 否   | 音频输出采样率，默认为 48000HZ。                                                              |
| useStereoOutput                 | boolean | 否   | 控制是否使用立体声输出，默认为 false，即单声道。                                                       |
| rendererUsage                   | number  | 否   | 音频输出用途，参见 ohos.multimedia.audio.StreamUsage, 默认为 STREAM_USAGE_VOICE_COMMUNICATION |
| useLowLatency                   | boolean | 否   | 控制是否使用低延迟输入输出，默认为 false。                                                          |
| useHardwareAcousticEchoCanceler | boolean | 否   | 控制是否使用内置的HW回声消除器，默认为 false。                                                       |
| useHardwareNoiseSuppressor      | boolean | 否   | 控制是否使用内置的HW噪声抑制器，默认为 false。                                                       |

## Logging

封装了WebRTC的 logging 模块。

### injectLoggable

static injectLoggable(injectedLoggable: Loggable, severity: LoggingSeverity): void

注入一个实现了Loggable接口的自定义logger。

参数：

| 参数名              | 类型              | 必填  | 说明                  |
|------------------|-----------------|-----|---------------------|
| injectedLoggable | Loggable        | 是   | 自定义logger           |
| severity         | LoggingSeverity | 是   | 日志级别，低于此级别的日志消息将被丢弃 |

### deleteInjectedLoggable

static deleteInjectedLoggable(): void

删除注入的自定义logger。

### enableLogThreads

static enableLogThreads(): void

在日志中显示线程ID。

### enableLogTimeStamps

static enableLogTimeStamps(): void

在日志中显示程序运行的时间。

### enableLogToDebugOutput

static enableLogToDebugOutput(severity: LoggingSeverity): void

打开debug日志，并指定日志级别。

参数：

| 参数名      | 类型              | 必填  | 说明                  |
|----------|-----------------|-----|---------------------|
| severity | LoggingSeverity | 是   | 日志级别，低于此级别的日志消息将被丢弃 |

### log

static log(severity: LoggingSeverity, tag: string, message: string): void

输出日志。

参数：

| 参数名      | 类型              | 必填  | 说明       |
|----------|-----------------|-----|----------|
| severity | LoggingSeverity | 是   | 该日志消息的级别 |
| tag      | string          | 是   | 日志tag    |
| message  | string          | 是   | 日志内容     |

### d

static d(tag: string, message: string): void

输出日志，级别为 LoggingSeverity.INFO。

参数：

| 参数名     | 类型     | 必填  | 说明    |
|---------|--------|-----|-------|
| tag     | string | 是   | 日志tag |
| message | string | 是   | 日志内容  |

### e

static e(tag: string, message: string, e?: Error): void

输出日志，级别为 LoggingSeverity.ERROR。

参数：

| 参数名     | 类型     | 必填  | 说明    |
|---------|--------|-----|-------|
| tag     | string | 是   | 日志tag |
| message | string | 是   | 日志内容  |
| e       | Error  | 否   | 错误信息  |

### w

static w(tag: string, message: string, e?: Error): void

输出日志，级别为 LoggingSeverity.WARNING。

参数：

| 参数名     | 类型     | 必填  | 说明    |
|---------|--------|-----|-------|
| tag     | string | 是   | 日志tag |
| message | string | 是   | 日志内容  |
| e       | Error  | 否   | 错误信息  |

### v

static v(tag: string, message: string): void

输出日志，级别为 LoggingSeverity.VERBOSE。

参数：

| 参数名     | 类型     | 必填  | 说明    |
|---------|--------|-----|-------|
| tag     | string | 是   | 日志tag |
| message | string | 是   | 日志内容  |

## LoggingSeverity

日志级别枚举。

| 名称      | 值   | 说明                                   |
|---------|-----|--------------------------------------|
| VERBOSE | 0   | 此级别用于我们不希望出现在正常调试日志中，但应该出现在诊断日志中的数据。 |
| INFO    | 1   | 用于调试。                                |
| WARNING | 2   | 警告，需要注意的信息。                          |
| ERROR   | 3   | 错误，需要避免此类事件发生。                       |
| NONE    | 4   | 不输出。                                 |

## MediaDeviceInfo

描述媒体设备信息。

### 属性

| 名称       | 类型                                  | 可读  | 可写  | 说明                                    |
|----------|-------------------------------------|-----|-----|---------------------------------------|
| deviceId | string                              | 是   | 否   | 设备标识符，通过deviceId和kind必须能唯一标识一个设备。     |
| kind     | [MediaDeviceKind](#mediadevicekind) | 是   | 否   | 用于接收媒体数据的传输机制。                        |
| label    | string                              | 是   | 否   | 用于描述该设备（如”External USB Webcam“）。      |
| groupId  | string                              | 是   | 否   | 设备的组ID，如果两个设备属于同一个物理设备，则它们的groupId相同。 |

## MediaDevices

用于检查和访问可用的媒体设备。

### enumerateDevices

enumerateDevices(): Promise<MediaDeviceInfo[]>

收集可用的媒体输入和输出设备的信息。

返回值

| 类型                                              | 说明         |
|-------------------------------------------------|------------|
| Promise\<[MediaDeviceInfo](#mediadeviceinfo)[]> | 媒体设备信息的数组。 |

### getSupportedConstraints

getSupportedConstraints(): MediaTrackSupportedConstraints

获取媒体轨道支持的约束条件。

返回值

| 类型                                                                | 说明           |
|-------------------------------------------------------------------|--------------|
| [MediaTrackSupportedConstraints](#mediatracksupportedconstraints) | 媒体轨道支持的约束条件。 |

### getUserMedia

getUserMedia(constraints?: MediaStreamConstraints): Promise<MediaStream>

根据约束条件，获取对应的音视频源，如麦克风或相机。

**参数**

| 参数名         | 类型                                                | 必填  | 说明                                                                                                                                                                                                           |
|-------------|---------------------------------------------------|-----|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| constraints | [MediaStreamConstraints](#mediastreamconstraints) | 否   | 一个包含媒体流约束条件的对象，用于配置音视频源的性质和设置。对于音频，当前不支持 advanced 属性，且仅支持 echoCancellation、autoGainControl、noiseSuppression 约束条件（表示是否开启WebRTC内置的回声消除、自动增益控制及噪声印制算法）。对于支持的约束条件，可通过 MediaDevices.getSupportedConstraints 接口查询。 |

**返回值**

| 类型                                    | 说明                      |
|---------------------------------------|-------------------------|
| Promise\<[MediaStream](#mediastream)> | 如果满足约束条件，则返回包含对应媒体的媒体流。 |

> **说明：**
> 当前可以通过两种方式获取音视频输入媒体：
> 1、MediaDevices.getUserMedia，即本接口，使用相对简单；
> 2、PeerConnectionFactory 中的相关接口，可通过 [AudioDeviceModule](#audiodevicemodule) 等施加更多的控制；

**示例**

```typescript
let mediaDevices = new webrtc.MediaDevices();
// 获取音频流轨道，禁用回声消除；获取视频流轨道，指定理想的分辨率为640x480，并限制帧率在15到30之间
let mediaStream = await mediaDevices.getUserMedia({audio:{echoCancellation:false}, video:{width:640, height:480, framerate:{min:15, max:30}}});
```

### getDisplayMedia

getDisplayMedia(options?: DisplayMediaStreamOptions): Promise<MediaStream>

根据约束条件，获取对应的音视频源，如麦克风或桌面。

**参数**

| 参数名      | 类型                                                    | 必填 | 说明                                                         |
| ----------- | ------------------------------------------------------- | ---- | ------------------------------------------------------------ |
| options | [DisplayMediaStreamOptions](#displaymediastreamoptions) | 否   | 一个包含媒体流约束条件的对象，用于配置音视频源的性质和设置。对于视频（桌面），当前不支持 advanced 属性，且仅支持 width、height约束条件；<br/>对于音频，当前不支持 advanced 属性，且仅支持 echoCancellation、autoGainControl、noiseSuppression 约束条件（表示是否开启WebRTC内置的回声消除、自动增益控制及噪声印制算法）。<br/>可通过 MediaDevices.getSupportedConstraints 接口查询支持的约束条件。 |

**返回值**

| 类型                                    | 说明                      |
|---------------------------------------|-------------------------|
| Promise\<[MediaStream](#mediastream)> | 如果满足约束条件，则返回包含对应媒体的媒体流。 |

**示例**

```typescript
let mediaDevices = new webrtc.MediaDevices();    
let mediaStream = await mediaDevices.getDisplayMedia({video: {width: 720, height:1080}});
```

## NativeVideoRenderer

用于在对应窗口中渲染本地或远端的视频流。

### setSurfaceSize（OHOS 扩展）

`setSurfaceSize(width: number, height: number): void`

在 `init` 后传入 Surface 显示区域的实际宽高（正整数，单位 px，不是 vp，也不是相机采集分辨率）。
原生渲染器在渲染线程更新 buffer 几何和用于等比渲染的显示尺寸；它不替代 ArkUI 组件布局。
使用 `VideoRenderController` 时由控制器调用，应用无需重复调用底层方法；直接使用 `NativeVideoRenderer` 时由调用方负责同步尺寸。

### 属性

| 名称       | 类型                                              | 可读 | 可写  | 说明                                    |
|----------|-------------------------------------------------|---|-----|---------------------------------------|
| surfaceId  | string                                   | 是 | 否   | 窗口对应的 Surface Id，由 init 接口设置。       |
| videoTrack | [VideoTrack](#videotrack) \| undefined   | 是 | 否   | 关联的视频轨道，由 setVideoTrack 接口设置。     |
| onFrameResolutionChanged | EventHandler \| null                    | 是 | 是   | 当视频帧分辨率或旋转角度发生变化时触发，事件对象包含 `width`、`height`、`rotation` 字段。 |

### init

init(surfaceId: string): void

使用窗口对应的 surfaceId 初始化，创建相关的渲染环境。

#### 参数

| 参数名       | 类型     | 必填 | 说明                                                           |
|-----------|--------|----|--------------------------------------------------------------|
| surfaceId | string | 是  | 窗口的Surface Id，如使用 XComponent，可通过对应的 XComponentController 获取。 |

#### 返回值

无

#### 示例

```typescript
// 获取 surface id
let surfaceId = this.xComponentController.getXComponentSurfaceId();

let nativeVideoRenderer = new webrtc.NativeVideoRenderer();
nativeVideoRenderer.init(surfaceId);
```

### setVideoTrack

setVideoTrack(videoTrack: MediaStreamTrack | null): void

设置关联的视频轨道，以接收视频流并渲染。

#### 参数

| 参数名       | 类型     | 必填 | 说明                                                                                           |
|-----------|--------|----|----------------------------------------------------------------------------------------------|
| videoTrack | [MediaStreamTrack](#mediastreamtrack) \| null | 是  | 当 videoTrack 不为 null 且 kind 为 "video" 时，关联该视频轨道，并接收其视频流；当 videoTrack 为 null 时，解除与当前视频轨道的关联，停止接收视频流。 |

#### 返回值

无

#### 示例

```typescript
// 创建视频轨道
this.localVideoSource = this.peerConnectionFactory.createVideoSource({width: 320, height: 240});
this.localVideoTrack = this.peerConnectionFactory.createVideoTrack('video', this.localVideoSource);

let nativeVideoRenderer = new webrtc.NativeVideoRenderer();
nativeVideoRenderer.setVideoTrack(this.localVideoTrack);
```

### setMirror

setMirror(mirrorHorizontally: boolean): void

设置视频流是否水平镜像显示，默认为false。

#### 参数

| 参数名       | 类型     | 必填 | 说明                                                                                           |
|-----------|--------|----|----------------------------------------------------------------------------------------------|
| mirrorHorizontally | boolean | 是  | 如果为true，在水平方向镜像显示视频流。 |

#### 返回值

无

#### 示例

```typescript
let nativeVideoRenderer = new webrtc.NativeVideoRenderer();
nativeVideoRenderer.setMirror(true);
```

### setMirrorVertically

setMirrorVertically(mirrorVertically: boolean): void

设置视频流是否垂直镜像显示，默认为false。

#### 参数

| 参数名       | 类型     | 必填 | 说明                                                                                           |
|-----------|--------|----|----------------------------------------------------------------------------------------------|
| mirrorVertically | boolean | 是  | 如果为true，在垂直方向镜像显示视频流。 |

#### 返回值

无

#### 示例

```typescript
let nativeVideoRenderer = new webrtc.NativeVideoRenderer();
nativeVideoRenderer.setMirrorVertically(true);
```

### setScalingMode

setScalingMode(mode: number): void

设置视频流显示的缩放模式，默认为0，即缩放内容以适应窗口的大小。

#### 参数

| 参数名       | 类型     | 必填 | 说明                                                                                           |
|-----------|--------|----|----------------------------------------------------------------------------------------------|
| mode | number | 是  | 缩放模式，参见 [ScalingMode](#scalingmode) 。 |

#### 返回值

无

#### 示例

```typescript
let nativeVideoRenderer = new webrtc.NativeVideoRenderer();
nativeVideoRenderer.setScalingMode(1);
```

### release

release(): void

释放窗口对应的渲染环境，及其它资源。

#### 参数

无

#### 返回值

无

## VideoRenderController

视频渲染控制器，继承自 XComponentController，对应的 XComponent 组件创建后，使用其 surface id 初始化内部的 NativeVideoRenderer实例，XComponent组件销毁时，则释放该 NativeVideoRenderer 实例。

### setVideoTrack

setVideoTrack(track: MediaStreamTrack | null): void

通过内部的 NativeVideoRenderer实例设置或解除关联的视频轨道。

#### 参数

| 参数名       | 类型     | 必填 | 说明                                                                                           |
|-----------|--------|----|----------------------------------------------------------------------------------------------|
| track | [MediaStreamTrack](#mediastreamtrack) \| null | 是  | 当 track 不为 null 且 kind 为 "video" 时，关联该视频轨道，并接收其视频流；当 track 为 null 时，解除与当前视频轨道的关联，停止接收视频流。 |

#### 返回值

无

#### 示例

```typescript
// 初始化 XComponent 控制器
mXComponentController: VideoRenderController = new VideoRenderController();

build()
{
  // ......
  // 声明 XComponent 组件，并指定 XComponent 控制器
  XComponent({
    id: "local-video",
    type: XComponentType.SURFACE,
    controller: this.mXComponentController
  })
    .width('640px')
    .height('480px')
    .onLoad(() => {
    })
    .onDestroy(() => {
    })
  // ......
}

this.mXComponentController.setVideoTrack(this.localVideoTrack);
```

### setMirror

setMirror(mirror: boolean): void

设置视频流是否应该水平镜像显示，默认为false。

#### 参数

| 参数名       | 类型     | 必填 | 说明                                                                                           |
|-----------|--------|----|----------------------------------------------------------------------------------------------|
| mirror | boolean | 是  | 如果为true，在水平方向镜像显示视频流。 |

#### 返回值

无

#### 示例

```typescript
// 初始化 XComponent 控制器
mXComponentController: VideoRenderController = new VideoRenderController();

build()
{
  // ......
  // 声明 XComponent 组件，并指定 XComponent 控制器
  XComponent({
    id: "local-video",
    type: XComponentType.SURFACE,
    controller: this.mXComponentController
  })
  // ......
}

this.mXComponentController.setMirror(true);
```

### setMirrorVertically

setMirrorVertically(mirrorVertically: boolean): void

设置视频流是否应该垂直镜像显示，默认为false。

#### 参数

| 参数名       | 类型     | 必填 | 说明                                                                                           |
|-----------|--------|----|----------------------------------------------------------------------------------------------|
| mirrorVertically | boolean | 是  | 如果为true，在垂直方向镜像显示视频流。 |

#### 返回值

无

#### 示例

```typescript
// 初始化 XComponent 控制器
mXComponentController: VideoRenderController = new VideoRenderController();

build()
{
  // ......
  // 声明 XComponent 组件，并指定 XComponent 控制器
  XComponent({
    id: "local-video",
    type: XComponentType.SURFACE,
    controller: this.mXComponentController
  })
  // ......
}

this.mXComponentController.setMirrorVertically(true);
```

### setScalingMode

setScalingMode(mode: ScalingMode): void

设置视频流显示的缩放模式，默认为 ScalingMode.Fill，即缩放内容以适应窗口的大小。

#### 参数

| 参数名       | 类型     | 必填 | 说明                                                                                           |
|-----------|--------|----|----------------------------------------------------------------------------------------------|
| mode | [ScalingMode](#scalingmode) | 是  | 缩放模式，参见 ScalingMode 。 |

#### 返回值

无

#### 示例

```typescript
// 初始化 XComponent 控制器
mXComponentController: VideoRenderController = new VideoRenderController();

build()
{
  // ......
  // 声明 XComponent 组件，并指定 XComponent 控制器
  XComponent({
    id: "local-video",
    type: XComponentType.SURFACE,
    controller: this.mXComponentController
  })
  // ......
}

this.mXComponentController.setScalingMode(ScalingMode.Fill);
```

## ScalingMode

缩放模式枚举。

| 名称      | 值   | 说明                                   |
|---------|-----|--------------------------------------|
| Fill | 0   | 缩放内容以适应窗口的大小，如果需要可以改变内容的宽高比。 |
| AspectFill    | 1   | 缩放内容以填充窗口的大小。保持宽高比不变，部分内容可能会被裁剪。  |
| AspectFit | 2   | 缩放内容以适应窗口的大小。保持宽高比不变，窗口左右或上下可能会有黑边。  |
