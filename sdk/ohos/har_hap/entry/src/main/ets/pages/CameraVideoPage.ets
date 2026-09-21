/**
 * Copyright (c) 2026 Archermind Technology (Nanjing) Co. Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import webrtc, { VideoRenderController, ScalingMode } from '@ohos/webrtc';
import image from '@ohos.multimedia.image';
import abilityAccessCtrl, { PermissionRequestResult } from '@ohos.abilityAccessCtrl';
import { LoopbackSession, LoopbackMetrics, emptyMetrics } from './common/LoopbackSession';
import { BackTitleBar, VideoMetricsPanel } from './common/UiPanels';

const TAG: string = '[CameraVideoTest]';
const ADAPTER_TYPE_WIFI: number = 1 << 1;
const ADAPTER_TYPE_CELLULAR: number = 1 << 2;
const ADAPTER_TYPE_VPN: number = 1 << 3;
const COLLECTION_FIELDS_LIMIT: number = 6;

/** 枚举选项的展示模型:label 进下拉框,detail 常驻展示当前选中项的完整含义。 */
interface EnumOption {
  label: string;
  detail: string;
}

const MODE_OPTIONS: EnumOption[] = [
  {
    label: '相机单端验证',
    detail: '只使用摄像头、不建立任何网络连接:实时预览、滑杆变焦(applyConstraints)、' +
      'setZoom 接口变焦、切换前后摄像头、查询采集能力范围、单帧抓取并展示快照。',
  },
  {
    label: '视频回环验证',
    detail: '在同一设备内建立发送端(pc1)与接收端(pc2):相机采集→编码→RTP 传输→解码→渲染,' +
      '本地/远端双画面对比,配合下方实测指标验证完整视频链路。',
  },
];

const CODEC_OPTIONS: EnumOption[] = [
  {
    label: '默认策略 · 硬件编码优先',
    detail: '不注入编码器工厂,SDK 默认行为:优先使用硬件 H264/H265 编码,硬编不可用时自动回落软件编码。',
  },
  {
    label: 'H264 强制软编',
    detail: '注入 HardwareVideoEncoderFactory 并设置 forceSWCodecList=[\'H264\']:H264 硬件编码器拒建、' +
      '回落软件编码,可与默认策略对比帧率/码率/功耗差异。',
  },
  {
    label: 'H265 强制软编',
    detail: '注入 HardwareVideoEncoderFactory 并设置 forceSWCodecList=[\'H265\']:H265 硬件编码器拒建、' +
      '回落软件编码。',
  },
];

const MASK_OPTIONS: EnumOption[] = [
  {
    label: '不屏蔽任何网卡(mask=0)',
    detail: 'networkIgnoreMask=0(默认):WiFi/蜂窝/VPN 等所有网卡都参与 ICE 候选收集,媒体可走任意网卡。',
  },
  {
    label: '仅屏蔽 VPN(mask=8)',
    detail: 'networkIgnoreMask=1<<3:收集 ICE 候选时忽略 VPN 网卡,媒体数据不走 VPN,用于验证掩码位生效。',
  },
  {
    label: '屏蔽 WiFi+蜂窝+VPN(mask=14)',
    detail: 'networkIgnoreMask=WiFi(2)|蜂窝(4)|VPN(8):三类网卡全部忽略;若设备没有其他网卡,' +
      '链路将无法建立(ICE 停在 checking/failed),可反向验证掩码确实生效。',
  },
];

const MODE_SELECTS: SelectOption[] = MODE_OPTIONS.map((o: EnumOption): SelectOption => ({ value: o.label }));
const CODEC_SELECTS: SelectOption[] = CODEC_OPTIONS.map((o: EnumOption): SelectOption => ({ value: o.label }));
const MASK_SELECTS: SelectOption[] = MASK_OPTIONS.map((o: EnumOption): SelectOption => ({ value: o.label }));

/**
 * 摄像头/视频合并验证页:相机单端与视频回环共用后置摄像头,模式互斥——
 * 相机模式:预览、滑杆变焦(applyConstraints)、VideoSource.setZoom/switchCamera、
 * getCapabilities 采集能力、captureFrame 抓帧;
 * 回环模式:可切换编解码与网卡掩码的双端回环,统一观测出站/入站指标、
 * PC 事件计数(候选对切换、流增删)与 getStats 数组/对象字段。
 */
@Entry
@Component
struct CameraVideoPage {
  @State mode: number = 0; // 0 相机单端 1 视频回环;互斥,避免两个相机源抢占同一摄像头

  @State statusText: string = '申请权限...';
  @State capsText: string = '能力未读取';
  @State zoomRatio: number = 1.0;
  @State zoomMin: number = 1.0;
  @State zoomMax: number = 6.0;
  @State frameInfo: string = '尚未抓帧';
  @State pixelMap: image.PixelMap | undefined = undefined;
  @State captureCount: number = 0;
  @State lastCostMs: number = 0;
  private permissionGranted: boolean = false;
  private factory?: webrtc.PeerConnectionFactory;
  private videoSource?: webrtc.VideoSource;
  private videoTrack?: webrtc.MediaStreamTrack;
  private renderer: VideoRenderController = new VideoRenderController();
  private lastZoomSendTime = 0;

  @State isRunning: boolean = false;
  @State forceMode: number = 0; // 0 默认硬编 1 H264强软 2 H265强软
  @State maskMode: number = 0; // 0 不屏蔽 1 屏蔽VPN 2 屏蔽WiFi+蜂窝+VPN
  @State metrics: LoopbackMetrics = emptyMetrics();
  @State sessionErrorText: string = '';
  @State pairChangeCount: number = 0;
  @State lastPairReason: string = '-';
  @State addStreamCount: number = 0;
  @State removeStreamCount: number = 0;
  @State collectionFields: string[] = [];
  private session = new LoopbackSession();
  private collectionTimer = -1;
  private localRenderer: VideoRenderController = new VideoRenderController();
  private remoteRenderer: VideoRenderController = new VideoRenderController();

  aboutToAppear(): void {
    abilityAccessCtrl.createAtManager()
      .requestPermissionsFromUser(getContext(), ['ohos.permission.CAMERA', 'ohos.permission.MICROPHONE'])
      .then((result: PermissionRequestResult) => {
        this.permissionGranted = this.hasAllPermissions(result);
        if (this.permissionGranted) {
          if (this.mode === 0) {
            this.setupCamera();
          }
          console.info(`${TAG} 权限就绪`);
        } else {
          this.statusText = '相机/麦克风权限未授予';
        }
      });
  }

  aboutToDisappear(): void {
    this.stopCamera();
    this.stopLoopback();
  }

  private hasAllPermissions(result: PermissionRequestResult): boolean {
    let granted = true;
    for (let index = 0; index < result.permissions.length; index++) {
      if (result.authResults[index] !== abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
        granted = false;
      }
    }
    return granted;
  }

  private switchMode(target: number): void {
    if (this.mode === target) {
      return;
    }
    if (target === 1) {
      this.stopCamera();
    } else {
      this.stopLoopback();
      if (this.permissionGranted) {
        this.setupCamera();
      } else {
        this.statusText = '相机/麦克风权限未授予';
      }
    }
    this.mode = target;
  }

  private setupCamera(): void {
    if (!this.factory) {
      this.factory = new webrtc.PeerConnectionFactory();
    }
    this.videoSource = this.factory.createVideoSource({ facingMode: 'environment', width: 640, height: 480 });
    this.videoSource.oncameraswitchdone = (ev: webrtc.CameraSwitchDoneEvent) => {
      this.statusText = 'switchCamera 成功';
    };
    this.videoSource.oncameraswitcherror = (ev: webrtc.CameraSwitchErrorEvent) => {
      this.statusText = 'switchCamera 失败(设备可能仅一个摄像头)';
    };
    this.videoTrack = this.factory.createVideoTrack('camera-test-video', this.videoSource);
    this.renderer.setVideoTrack(this.videoTrack);
    this.renderer.setScalingMode(ScalingMode.AspectFit);
    this.statusText = '预览中(拖动滑杆实时变焦,或抓取当前帧)';
    setTimeout(() => {
      this.queryZoomRange();
      this.queryCapabilities();
    }, 1500);
  }

  private stopCamera(): void {
    this.videoTrack?.stop();
    this.videoTrack = undefined;
    this.videoSource?.release();
    this.videoSource = undefined;
    this.pixelMap = undefined;
  }

  private queryZoomRange(): void {
    try {
      const range: webrtc.ZoomRange | undefined = this.videoTrack?.getZoomRange();
      if (range) {
        this.zoomMin = range.min;
        this.zoomMax = range.max;
        this.zoomRatio = Math.max(this.zoomMin, Math.min(this.zoomRatio, this.zoomMax));
      }
    } catch (e) {
      console.error(`${TAG} queryZoomRange: ${e}`);
    }
  }

  private queryCapabilities(): void {
    try {
      const mediaDevices: webrtc.MediaDevices = new webrtc.MediaDevices();
      mediaDevices.enumerateDevices().then((devices: webrtc.MediaDeviceInfo[]) => {
        const cameras: webrtc.MediaDeviceInfo[] =
          devices.filter((d: webrtc.MediaDeviceInfo) => d.kind === 'videoinput');
        if (cameras.length === 0) {
          this.capsText = '未枚举到相机';
          return;
        }
        const caps: webrtc.MediaTrackCapabilities = cameras[0].getCapabilities();
        this.capsText = `width ${caps.width!.min}~${caps.width!.max}`;
        this.capsText += ` height ${caps.height!.min}~${caps.height!.max}`;
        this.capsText += ` fps ${caps.frameRate!.min}~${caps.frameRate!.max}`;
      });
    } catch (e) {
      this.capsText = `能力读取失败: ${e}`;
    }
  }

  private applyZoomThrottled(ratio: number): void {
    const now = Date.now();
    if (now - this.lastZoomSendTime < 32) {
      return;
    }
    this.lastZoomSendTime = now;
    try {
      this.videoTrack?.applyConstraints({ zoom: ratio });
    } catch (e) {
      console.error(`${TAG} applyZoom: ${e}`);
    }
  }

  // VideoSource 直达硬件变焦入口,与滑杆的 applyConstraints({zoom}) 路径互补
  private applySourceZoom(): void {
    try {
      this.videoSource?.setZoom(this.zoomRatio);
      this.statusText = `setZoom(API): ${this.zoomRatio.toFixed(1)}x 已下发`;
    } catch (e) {
      this.statusText = `setZoom(API) 失败: ${e}`;
    }
  }

  private switchCameraNext(): void {
    try {
      this.videoSource?.switchCamera();
      this.statusText = 'switchCamera 已下发,等待回调...';
    } catch (e) {
      this.statusText = `switchCamera 异常: ${e}`;
    }
  }

  private async toPixelMap(frame: webrtc.TrackFrame): Promise<image.PixelMap> {
    const opts: image.InitializationOptions = {
      size: { width: frame.width, height: frame.height },
      pixelFormat: image.PixelMapFormat.RGBA_8888,
    };
    return await image.createPixelMap(frame.data, opts);
  }

  private async captureOnce(): Promise<void> {
    const track: webrtc.MediaStreamTrack | undefined = this.videoTrack;
    if (!track) {
      return;
    }
    const start = Date.now();
    try {
      const frame: webrtc.TrackFrame = await track.captureFrame();
      this.lastCostMs = Date.now() - start;
      this.captureCount++;
      this.frameInfo = `第${this.captureCount}帧 ${frame.width}x${frame.height} ` +
        `${frame.data.byteLength} 字节 耗时${this.lastCostMs}ms`;
      this.pixelMap = await this.toPixelMap(frame);
    } catch (e) {
      this.frameInfo = `抓帧失败: ${e}`;
    }
    console.info(`${TAG} ${this.frameInfo}`);
  }

  private buildEncoderFactory(): webrtc.VideoEncoderFactory | undefined {
    if (this.forceMode === 1) {
      return new webrtc.HardwareVideoEncoderFactory({ forceSWCodecList: ['H264'] });
    }
    if (this.forceMode === 2) {
      return new webrtc.HardwareVideoEncoderFactory({ forceSWCodecList: ['H265'] });
    }
    return undefined;
  }

  private maskValue(): number {
    if (this.maskMode === 1) {
      return ADAPTER_TYPE_VPN;
    }
    if (this.maskMode === 2) {
      return ADAPTER_TYPE_WIFI | ADAPTER_TYPE_CELLULAR | ADAPTER_TYPE_VPN;
    }
    return 0;
  }

  private isMasked(bit: number): boolean {
    return (this.maskValue() & bit) !== 0;
  }

  private buildFactoryOptions(): webrtc.PeerConnectionFactoryOptions {
    return { networkIgnoreMask: this.maskValue() };
  }

  private wirePeerEvents(): void {
    const receiver: webrtc.RTCPeerConnection | undefined = this.session.peerReceiver();
    if (!receiver) {
      return;
    }
    receiver.onselectedcandidatepairchange = (ev: webrtc.SelectedCandidatePairChangeEvent) => {
      this.pairChangeCount++;
      this.lastPairReason = ev.reason === '' ? 'initial' : ev.reason;
      console.info(`${TAG} pair change #${this.pairChangeCount}: ${this.lastPairReason}`);
    };
    receiver.onaddstream = (ev: webrtc.MediaStreamEvent) => {
      this.addStreamCount++;
      console.info(`${TAG} onaddstream #${this.addStreamCount}: ${ev.stream!.id}`);
    };
    receiver.onremovestream = (ev: webrtc.MediaStreamEvent) => {
      this.removeStreamCount++;
      console.info(`${TAG} onremovestream #${this.removeStreamCount}: ${ev.stream!.id}`);
    };
  }

  private startLoopback(): void {
    this.sessionErrorText = '';
    this.session = new LoopbackSession();
    this.session.onError = (stage: string, message: string) => {
      this.sessionErrorText = `${stage}失败: ${message}`;
    };
    this.session.onRemoteTrack = (track: webrtc.MediaStreamTrack) => {
      this.remoteRenderer.setVideoTrack(track);
      this.remoteRenderer.setScalingMode(ScalingMode.AspectFit);
    };
    const ok = this.session.start(
      this.buildEncoderFactory(), { facingMode: 'environment', width: 640, height: 480 }, this.buildFactoryOptions());
    const localTrack: webrtc.MediaStreamTrack | undefined = this.session.localTrack;
    if (!ok || !localTrack) {
      this.sessionErrorText = '回环启动失败:视频源创建失败(检查相机权限或相机是否被占用)';
      return;
    }
    this.wirePeerEvents();
    this.localRenderer.setVideoTrack(localTrack);
    this.localRenderer.setScalingMode(ScalingMode.AspectFit);
    this.session.startMetrics(2000, (m: LoopbackMetrics) => {
      this.metrics = m;
    });
    this.collectionTimer = setInterval(() => {
      this.refreshCollectionFields();
    }, 3000);
    this.isRunning = true;
  }

  private stopLoopback(): void {
    if (this.collectionTimer >= 0) {
      clearInterval(this.collectionTimer);
      this.collectionTimer = -1;
    }
    this.session.stop();
    this.isRunning = false;
    this.metrics = emptyMetrics();
    this.collectionFields = [];
  }

  /** 收集 stats 里以数组/对象返回的成员(#8 的验证点)。 */
  private async refreshCollectionFields(): Promise<void> {
    const pc: webrtc.RTCPeerConnection | undefined = this.session.peerSender();
    if (!pc) {
      return;
    }
    try {
      const report: webrtc.RTCStatsReport = await pc.getStats();
      const found: string[] = [];
      report.stats.forEach((stat: ESObject, _key: string) => {
        this.collectFromStat(stat, found);
      });
      this.collectionFields = found.slice(0, COLLECTION_FIELDS_LIMIT);
    } catch (e) {
      console.error(`${TAG} refresh failed: ${e}`);
    }
  }

  private collectFromStat(stat: ESObject, out: string[]): void {
    Object.keys(stat).forEach((key: string) => {
      const value: Object = stat[key] as Object;
      if (out.length >= COLLECTION_FIELDS_LIMIT) {
        return;
      }
      if (Array.isArray(value)) {
        const list = value as Object[];
        out.push(`${key}: [${list.length}] ${JSON.stringify(value).substring(0, 50)}`);
      } else if (typeof value === 'object' && value !== null) {
        out.push(`${key}: {obj} ${JSON.stringify(value).substring(0, 50)}`);
      }
    });
  }

  private resLabel(w: number | null, h: number | null): string {
    return w !== null && h !== null ? `${w}x${h}` : '-';
  }

  private fpsLabel(v: number | null): string {
    return v === null ? '-' : `${v}`;
  }

  build() {
    Scroll() {
      Column({ space: 10 }) {
        BackTitleBar({ title: '摄像头/视频验证' })

        this.ModeSelectBlock()

        if (this.mode === 0) {
          this.CameraPanel()
        } else {
          this.LoopbackPanel()
        }
      }
      .width('100%')
      .padding(12)
    }
    .width('100%')
    .height('100%')
    .scrollBar(BarState.Auto)
    .scrollable(ScrollDirection.Vertical)
    .edgeEffect(EdgeEffect.Spring)
  }

  @Builder
  DetailBox(detail: string) {
    Text(detail)
      .fontSize(11)
      .fontColor(0x666666)
      .width('100%')
      .padding(8)
      .borderRadius(6)
      .backgroundColor(0xF7F8FA)
  }

  @Builder
  ModeSelectBlock() {
    Column({ space: 6 }) {
      Text('模式选择(枚举,下拉切换)').fontSize(13).fontColor(Color.Gray).width('100%')
      Select(MODE_SELECTS)
        .selected(this.mode)
        .value(MODE_OPTIONS[this.mode].label)
        .fontColor(0x333333)
        .width('100%')
        .onSelect((index: number) => {
          this.switchMode(index);
        })
      this.DetailBox(MODE_OPTIONS[this.mode].detail)
    }
    .width('100%')
  }

  @Builder
  CameraPanel() {
    Column({ space: 10 }) {
      XComponent({ id: 'cam-video', type: XComponentType.SURFACE, controller: this.renderer })
        .width('100%')
        .height(220)

      Text(`状态: ${this.statusText}`).fontSize(13).fontColor(0x007DFF).width('100%')

      Row({ space: 8 }) {
        Text(`${this.zoomRatio.toFixed(1)}x`).fontSize(14).width(48)
        Slider({ value: this.zoomRatio, min: this.zoomMin, max: this.zoomMax, step: 0.1 })
          .layoutWeight(1)
          .onChange((value: number) => {
            this.zoomRatio = value;
            this.applyZoomThrottled(value);
          })
        Button('抓帧')
          .fontSize(14)
          .onClick(() => {
            this.captureOnce();
          })
      }
      .width('100%')

      Row({ space: 8 }) {
        Button('setZoom(API)')
          .layoutWeight(1)
          .onClick(() => {
            this.applySourceZoom();
          })
        Button('切换摄像头')
          .layoutWeight(1)
          .onClick(() => {
            this.switchCameraNext();
          })
      }
      .width('100%')

      Text(`采集能力: ${this.capsText}`).fontSize(12).fontColor(Color.Gray).width('100%')
      Text(this.frameInfo).fontSize(12).fontColor(Color.Gray).width('100%')

      if (this.pixelMap) {
        Image(this.pixelMap!)
          .width('100%')
          .height(160)
          .objectFit(ImageFit.Contain)
          .borderRadius(8)
      }
    }
    .width('100%')
  }

  @Builder
  MaskChip(name: string, masked: boolean) {
    Text(masked ? `${name}·已屏蔽` : `${name}·可用`)
      .fontSize(10)
      .fontColor(masked ? Color.Red : Color.Green)
      .padding({ left: 8, right: 8, top: 3, bottom: 3 })
      .borderRadius(10)
      .backgroundColor(masked ? 0xFDECEC : 0xEAF7EA)
  }

  @Builder
  LoopbackPanel() {
    Column({ space: 10 }) {
      Column({ space: 6 }) {
        Text('编解码策略(枚举,下拉选择)').fontSize(13).fontColor(Color.Gray).width('100%')
        Select(CODEC_SELECTS)
          .selected(this.forceMode)
          .value(CODEC_OPTIONS[this.forceMode].label)
          .fontColor(0x333333)
          .width('100%')
          .onSelect((index: number) => {
            this.forceMode = index;
          })
        this.DetailBox(CODEC_OPTIONS[this.forceMode].detail)
      }
      .width('100%')

      Column({ space: 6 }) {
        Text('网卡掩码(枚举,下拉选择)').fontSize(13).fontColor(Color.Gray).width('100%')
        Select(MASK_SELECTS)
          .selected(this.maskMode)
          .value(MASK_OPTIONS[this.maskMode].label)
          .fontColor(0x333333)
          .width('100%')
          .onSelect((index: number) => {
            this.maskMode = index;
          })
        this.DetailBox(MASK_OPTIONS[this.maskMode].detail)
        Text(`当前生效 networkIgnoreMask=${this.maskValue()},候选收集按下表忽略对应网卡:`)
          .fontSize(11)
          .fontColor(0x555555)
          .width('100%')
        Row({ space: 6 }) {
          this.MaskChip('WiFi', this.isMasked(ADAPTER_TYPE_WIFI))
          this.MaskChip('蜂窝', this.isMasked(ADAPTER_TYPE_CELLULAR))
          this.MaskChip('VPN', this.isMasked(ADAPTER_TYPE_VPN))
        }
        .width('100%')
      }
      .width('100%')

      Button(this.isRunning ? '停止回环(可换配置后重启)' : '启动回环')
        .width('100%')
        .onClick(() => {
          if (this.isRunning) {
            this.stopLoopback();
          } else {
            this.startLoopback();
          }
        })

      if (this.isRunning) {
        Text(`运行中配置:${CODEC_OPTIONS[this.forceMode].label} | ${MASK_OPTIONS[this.maskMode].label}`)
          .fontSize(11)
          .fontColor(Color.Green)
          .width('100%')
      } else {
        Text('未运行:指标面板显示等待采样;修改上方下拉配置后需点击启动才生效')
          .fontSize(11)
          .fontColor(0x999999)
          .width('100%')
      }

      if (this.sessionErrorText !== '') {
        Text(this.sessionErrorText).fontSize(12).fontColor(Color.Red).width('100%')
      }

      Row({ space: 8 }) {
        Column({ space: 4 }) {
          Text('本地采集(pc1 发送)').fontSize(12)
          XComponent({ id: 'vl-local', type: XComponentType.SURFACE, controller: this.localRenderer })
            .width('100%')
            .height(130)
          Text(`出站 ${this.resLabel(this.metrics.outWidth, this.metrics.outHeight)} ` +
            `@ ${this.fpsLabel(this.metrics.outFps)}fps`)
            .fontSize(10)
            .fontColor(Color.Gray)
        }
        .layoutWeight(1)

        Column({ space: 4 }) {
          Text('远端接收(pc2 解码渲染)').fontSize(12)
          XComponent({ id: 'vl-remote', type: XComponentType.SURFACE, controller: this.remoteRenderer })
            .width('100%')
            .height(130)
          Text(`入站 ${this.resLabel(this.metrics.inWidth, this.metrics.inHeight)} ` +
            `@ ${this.fpsLabel(this.metrics.inFps)}fps`)
            .fontSize(10)
            .fontColor(Color.Gray)
        }
        .layoutWeight(1)
      }
      .width('100%')

      VideoMetricsPanel({ m: this.metrics })

      Row({ space: 8 }) {
        Text(`候选对切换 ${this.pairChangeCount}(${this.lastPairReason})`)
          .fontSize(12)
          .layoutWeight(1)
        Text(`流事件 add:${this.addStreamCount} remove:${this.removeStreamCount}`)
          .fontSize(12)
          .layoutWeight(1)
      }
      .width('100%')

      Text('getStats 数组/对象字段(每3s)')
        .fontSize(12)
        .fontWeight(FontWeight.Bold)
        .width('100%')

      ForEach(this.collectionFields, (field: string) => {
        Text(field).fontSize(10).fontColor(0x007DFF).width('100%')
      }, (field: string) => field)
    }
    .width('100%')
  }
}
