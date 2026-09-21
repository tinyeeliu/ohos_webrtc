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
import { BackTitleBar, VideoMetricsPanel } from './common/UiPanels';
import { LoopbackSession, LoopbackMetrics, emptyMetrics } from './common/LoopbackSession';

const TAG: string = '[MediaChecksTest]';
const MAX_WAIT_MS: number = 10000;
const SAMPLES_POLL_MS: number = 200;
const CHUNK_SIZE: number = 16 * 1024;
const TOTAL_CHUNKS: number = 128; // 2MB
const PAUSE_THRESHOLD: number = 512 * 1024; // 高水位:暂停发送
const RESUME_THRESHOLD: number = 64 * 1024; // 低水位:恢复发送
const BP_POLL_MS: number = 50;

/**
 * 媒体能力检验页:四个检验区块共用一页——
 * - 硬件 AEC/NS options 检验:采集源类型选择与如实降级;
 * - 播放侧逐样本回调检验:远端音频 onaudiosamplesready 驱动音量条;
 * - DataChannel 背压检验:bufferedamountchange 高/低水位流控;
 * - 视频能力检验:相机采集→回环→双画面实时渲染 + 抓帧快照,用画面效果验证视频链路。
 * applyConstraints 参数配置测试见 ApplyConstraintsTestPage。
 */
@Entry
@Component
struct MediaChecksPage {
  // 硬件 AEC/NS 检验区
  @State optionsLogs: string[] = [];

  // 播放侧逐样本回调检验区
  @State samplesLogs: string[] = [];
  @State samplesRunning: boolean = false;
  @State level: number = 0; // 0~1 音量
  @State sampleCount: number = 0;

  // DataChannel 背压检验区
  @State bpLogs: string[] = [];
  @State bpRunning: boolean = false;
  @State bufferedAmount: number = 0;
  @State sentBytes: number = 0;
  @State bpPaused: boolean = false;

  // 视频能力检验区
  @State isVideoRunning: boolean = false;
  @State videoStatus: string = '未启动';
  @State videoMetrics: LoopbackMetrics = emptyMetrics();
  @State videoFrameInfo: string = '尚未抓帧';
  @State videoPixelMap: image.PixelMap | undefined = undefined;
  private videoSession = new LoopbackSession();
  private videoLocalRenderer: VideoRenderController = new VideoRenderController();
  private videoRemoteRenderer: VideoRenderController = new VideoRenderController();

  aboutToAppear(): void {
    abilityAccessCtrl.createAtManager()
      .requestPermissionsFromUser(getContext(), ['ohos.permission.CAMERA', 'ohos.permission.MICROPHONE'])
      .then((result: PermissionRequestResult) => {
        for (let index = 0; index < result.permissions.length; index++) {
          if (result.authResults[index] !== abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
            console.error(TAG, 'camera/microphone permission not granted');
            return;
          }
        }
      });
  }

  aboutToDisappear(): void {
    this.stopVideoLoopback();
  }

  private appendOptionsLog(message: string): void {
    console.info(`${TAG} ${message}`);
    this.optionsLogs.push(message);
    if (this.optionsLogs.length > 100) {
      this.optionsLogs.splice(0, this.optionsLogs.length - 100);
    }
  }

  private appendSamplesLog(message: string): void {
    console.info(`${TAG} ${message}`);
    this.samplesLogs.push(message);
    if (this.samplesLogs.length > 100) {
      this.samplesLogs.splice(0, this.samplesLogs.length - 100);
    }
  }

  private appendBpLog(message: string): void {
    console.info(`${TAG} ${message}`);
    this.bpLogs.push(message);
    if (this.bpLogs.length > 150) {
      this.bpLogs.splice(0, this.bpLogs.length - 150);
    }
  }

  // ===== 硬件 AEC/NS options 检验区 =====

  private runAecCases(): void {
    try {
      new webrtc.AudioDeviceModule({ useHardwareAcousticEchoCanceler: true });
      this.appendOptionsLog('场景1 AEC=true: ADM 创建成功(采集源 VOICE_COMMUNICATION,系统 AEC 路径开启)');
    } catch (e) {
      this.appendOptionsLog(`场景1 AEC=true: FAIL (${e})`);
    }
    try {
      new webrtc.AudioDeviceModule({ useHardwareAcousticEchoCanceler: false });
      this.appendOptionsLog('场景2 AEC=false: ADM 创建成功(采集源普通 MIC,关闭系统 AEC 路径)');
    } catch (e) {
      this.appendOptionsLog(`场景2 AEC=false: FAIL (${e})`);
    }
    try {
      // 字段类型非法须绕过静态检查构造,运行期由原生侧抛 TypeError
      const options = JSON.parse('{"useHardwareAcousticEchoCanceler":"yes"}') as webrtc.AudioDeviceModuleOptions;
      new webrtc.AudioDeviceModule(options);
      this.appendOptionsLog('场景3 非法参数: FAIL (未抛错)');
    } catch (e) {
      this.appendOptionsLog('场景3 非法参数(字符串): PASS(已抛 TypeError)');
    }
  }

  private runNsCases(): void {
    const aecSupported = webrtc.AudioDeviceModule.isBuiltInAcousticEchoCancelerSupported();
    const nsSupported = webrtc.AudioDeviceModule.isBuiltInNoiseSuppressorSupported();
    this.appendOptionsLog(`isBuiltInAcousticEchoCancelerSupported: ${aecSupported}(API12 无法按设备查询)`);
    this.appendOptionsLog(`isBuiltInNoiseSuppressorSupported: ${nsSupported}(无系统 NS 接口)`);
    const adm = new webrtc.AudioDeviceModule();
    const nsEnabled = adm.setNoiseSuppressorEnabled(true);
    this.appendOptionsLog(`setNoiseSuppressorEnabled(true): ${nsEnabled}(降噪走软件 APM,如实返回 false)`);
    try {
      const options = JSON.parse('{"useHardwareNoiseSuppressor":1}') as webrtc.AudioDeviceModuleOptions;
      new webrtc.AudioDeviceModule(options);
      this.appendOptionsLog('场景5 非法参数: FAIL (未抛错)');
    } catch (e) {
      this.appendOptionsLog('场景5 非法参数(数字): PASS(已抛 TypeError)');
    }
  }

  // ===== 播放侧逐样本回调检验区 =====

  // 交错 S16 的 RMS 音量(0~1)
  private computeLevel(samples: webrtc.AudioSamples): number {
    const view = new Int16Array(samples.data);
    if (view.length === 0) {
      return 0;
    }
    let sum = 0;
    const step = Math.max(1, Math.floor(view.length / 512)); // 抽样防止长帧卡 UI
    for (let i = 0; i < view.length; i += step) {
      const v = view[i] / 32768.0;
      sum += v * v;
    }
    return Math.min(1, Math.sqrt(sum / Math.ceil(view.length / step)) * 4);
  }

  private setupReceiver(pc2: webrtc.RTCPeerConnection): void {
    pc2.ontrack = (ev: webrtc.RTCTrackEvent) => {
      const track = ev.track;
      this.appendSamplesLog(`收到远端轨道: kind=${track.kind}`);
      track.onaudiosamplesready = (sampleEv: webrtc.AudioTrackSamplesEvent) => {
        this.sampleCount++;
        this.level = this.computeLevel(sampleEv.samples);
      };
    };
  }

  private async runSamples(): Promise<void> {
    this.samplesRunning = true;
    this.sampleCount = 0;
    const factory = new webrtc.PeerConnectionFactory();
    const pc1 = factory.createPeerConnection({ iceServers: [] });
    const pc2 = factory.createPeerConnection({ iceServers: [] });

    pc1.onicecandidate = (ev: webrtc.RTCPeerConnectionIceEvent) => {
      if (ev.candidate) {
        pc2.addIceCandidate(ev.candidate);
      }
    };
    pc2.onicecandidate = (ev: webrtc.RTCPeerConnectionIceEvent) => {
      if (ev.candidate) {
        pc1.addIceCandidate(ev.candidate);
      }
    };
    this.setupReceiver(pc2);

    const audioSource = factory.createAudioSource();
    const audioTrack = factory.createAudioTrack('samples-demo-audio', audioSource);
    pc1.addTrack(audioTrack);

    await pc1.setLocalDescription(await pc1.createOffer());
    await pc2.setRemoteDescription(pc1.localDescription!);
    const answer = await pc2.createAnswer();
    await pc1.setRemoteDescription(answer);
    await pc2.setLocalDescription(answer);

    let elapsed = 0;
    while (elapsed < MAX_WAIT_MS && this.sampleCount === 0) {
      await sleep(SAMPLES_POLL_MS);
      elapsed += SAMPLES_POLL_MS;
    }
    this.appendSamplesLog(`样本事件: ${this.sampleCount > 0 ? '已到达' : '超时未到达'}`);
    await sleep(3000); // 观察音量条 3 秒

    pc2.getReceivers().forEach((receiver: webrtc.RTCRtpReceiver) => {
      if (receiver.track) {
        receiver.track!.onaudiosamplesready = null;
      }
    });
    audioSource.release();
    pc1.close();
    pc2.close();
    this.appendSamplesLog('已停止订阅并关闭');
    this.samplesRunning = false;
  }

  // ===== DataChannel 背压检验区 =====

  private setupChannel(channel: webrtc.RTCDataChannel): void {
    channel.bufferedAmountLowThreshold = RESUME_THRESHOLD;
    channel.onbufferedamountchange = (ev: webrtc.RTCBufferedAmountChangeEvent) => {
      this.bufferedAmount = ev.bufferedAmount;
    };
    channel.onbufferedamountlow = () => {
      this.appendBpLog(`[低水位] bufferedAmount=${this.bufferedAmount},恢复发送`);
    };
  }

  private async sendWithBackpressure(channel: webrtc.RTCDataChannel): Promise<void> {
    const chunk = 'x'.repeat(CHUNK_SIZE);
    while (this.sentBytes < TOTAL_CHUNKS * CHUNK_SIZE) {
      if (this.bufferedAmount > PAUSE_THRESHOLD) {
        if (!this.bpPaused) {
          this.bpPaused = true;
          this.appendBpLog(`[高水位] bufferedAmount=${this.bufferedAmount},暂停发送`);
        }
        await sleep(BP_POLL_MS);
        continue;
      }
      this.bpPaused = false;
      channel.send(chunk);
      this.sentBytes += CHUNK_SIZE;
    }
    this.appendBpLog(`发送完成: ${this.sentBytes} 字节`);
  }

  private async runBackpressure(): Promise<void> {
    this.bpRunning = true;
    this.sentBytes = 0;
    this.bufferedAmount = 0;
    const factory = new webrtc.PeerConnectionFactory();
    const pc1 = factory.createPeerConnection({ iceServers: [] });
    const pc2 = factory.createPeerConnection({ iceServers: [] });
    const channel = pc1.createDataChannel('backpressure-demo');
    this.setupChannel(channel);

    let received = 0;
    pc2.ondatachannel = (ev: webrtc.RTCDataChannelEvent) => {
      ev.channel.onmessage = () => {
        received += CHUNK_SIZE;
      };
    };

    await pc1.setLocalDescription(await pc1.createOffer());
    await pc2.setRemoteDescription(pc1.localDescription!);
    const answer = await pc2.createAnswer();
    await pc1.setRemoteDescription(answer);
    await pc2.setLocalDescription(answer);
    await sleep(500);

    await this.sendWithBackpressure(channel);
    let elapsed = 0;
    while (elapsed < 5000 && received < this.sentBytes) {
      await sleep(200);
      elapsed += 200;
    }
    this.appendBpLog(`接收完成: ${received} 字节`);

    channel.close();
    pc1.close();
    pc2.close();
    this.bpRunning = false;
  }

  // ===== 视频能力检验区 =====

  private startVideoLoopback(): void {
    this.videoStatus = '正在建立回环...';
    this.videoSession = new LoopbackSession();
    this.videoSession.onError = (stage: string, message: string) => {
      this.videoStatus = `${stage}失败: ${message}`;
    };
    this.videoSession.onRemoteTrack = (track: webrtc.MediaStreamTrack) => {
      this.videoRemoteRenderer.setVideoTrack(track);
      this.videoRemoteRenderer.setScalingMode(ScalingMode.AspectFit);
    };
    const ok = this.videoSession.start(undefined, { facingMode: 'environment', width: 640, height: 480 }, undefined);
    const localTrack = this.videoSession.localTrack;
    if (!ok || !localTrack) {
      this.videoStatus = '启动失败:视频源创建失败(检查相机权限或相机是否被占用)';
      return;
    }
    this.videoLocalRenderer.setVideoTrack(localTrack);
    this.videoLocalRenderer.setScalingMode(ScalingMode.AspectFit);
    this.videoSession.startMetrics(1000, (m: LoopbackMetrics) => {
      this.videoMetrics = m;
    });
    this.videoStatus = '回环运行中:双画面有图像即采集→编码→传输→解码→渲染全链路打通';
    this.isVideoRunning = true;
  }

  private stopVideoLoopback(): void {
    if (!this.isVideoRunning && this.videoMetrics.sampleCount === 0) {
      return;
    }
    this.videoSession.stop();
    this.isVideoRunning = false;
    this.videoMetrics = emptyMetrics();
    this.videoStatus = '已停止';
  }

  private async captureVideoFrame(): Promise<void> {
    const track = this.videoSession.localTrack;
    if (!track) {
      this.videoFrameInfo = '抓帧失败:本地轨道不存在';
      return;
    }
    try {
      const frame = await track.captureFrame();
      this.videoFrameInfo = `抓帧成功 ${frame.width}x${frame.height} ${frame.data.byteLength} 字节(RGBA_8888)`;
      const opts: image.InitializationOptions = {
        size: { width: frame.width, height: frame.height },
        pixelFormat: image.PixelMapFormat.RGBA_8888,
      };
      this.videoPixelMap = await image.createPixelMap(frame.data, opts);
    } catch (e) {
      this.videoFrameInfo = `抓帧失败: ${e}`;
    }
    console.info(`${TAG} ${this.videoFrameInfo}`);
  }

  private videoResText(w: number | null, h: number | null): string {
    return w !== null && h !== null ? `${w}x${h}` : '-';
  }

  private videoFpsText(v: number | null): string {
    return v === null ? '-' : `${v}`;
  }

  build() {
    Scroll() {
      Column({ space: 10 }) {
        BackTitleBar({ title: '媒体能力检验' })

        this.SectionTitle('一、硬件 AEC / NS 检验')
        this.OptionsPanel()

        this.SectionTitle('二、播放侧逐样本回调检验')
        this.SamplesPanel()

        this.SectionTitle('三、DataChannel 背压检验')
        this.BackpressurePanel()

        this.SectionTitle('四、视频能力检验(画面效果验证)')
        this.VideoCheckPanel()
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
  SectionTitle(title: string) {
    Text(title)
      .fontSize(16)
      .fontWeight(FontWeight.Bold)
      .width('100%')
      .padding({ top: 8, bottom: 4 })
      .backgroundColor(0xF1F3F5)
  }

  @Builder
  OptionsPanel() {
    Column({ space: 8 }) {
      Button('场景1-3: AEC 源类型 + 异常参数')
        .fontSize(12)
        .width('100%')
        .onClick(() => {
          this.runAecCases();
        })
      Button('场景4-5: NS 降级 + 查询')
        .fontSize(12)
        .width('100%')
        .onClick(() => {
          this.runNsCases();
        })

      Button('清空日志')
        .fontSize(12)
        .width('100%')
        .onClick(() => {
          this.optionsLogs = [];
        })

      ForEach(this.optionsLogs, (log: string) => {
        Text(log).fontSize(12).fontColor(Color.Gray).width('100%')
      }, (log: string, index: number) => index.toString())
    }
    .width('100%')
  }

  @Builder
  SamplesPanel() {
    Column({ space: 8 }) {
      Progress({ value: this.level * 100, total: 100 })
        .width('100%')
        .color(this.level > 0.7 ? Color.Red : Color.Blue)

      Text(`已收 ${this.sampleCount} 帧样本`).fontSize(13).fontColor(Color.Gray)

      Button(this.samplesRunning ? '运行中...' : '启动 loopback 演示')
        .fontSize(12)
        .width('100%')
        .enabled(!this.samplesRunning)
        .onClick(() => {
          this.runSamples();
        })

      ForEach(this.samplesLogs, (log: string) => {
        Text(log).fontSize(12).fontColor(Color.Gray).width('100%')
      }, (log: string, index: number) => index.toString())
    }
    .width('100%')
  }

  @Builder
  VideoCheckPanel() {
    Column({ space: 8 }) {
      Row({ space: 8 }) {
        Column({ space: 4 }) {
          Text('本地采集(pc1 发送)').fontSize(12)
          XComponent({ id: 'mc-video-local', type: XComponentType.SURFACE, controller: this.videoLocalRenderer })
            .width('100%')
            .height(140)
          Text(`出站 ${this.videoResText(this.videoMetrics.outWidth, this.videoMetrics.outHeight)} ` +
            `@ ${this.videoFpsText(this.videoMetrics.outFps)}fps`)
            .fontSize(10)
            .fontColor(Color.Gray)
        }
        .layoutWeight(1)

        Column({ space: 4 }) {
          Text('远端接收(pc2 解码渲染)').fontSize(12)
          XComponent({ id: 'mc-video-remote', type: XComponentType.SURFACE, controller: this.videoRemoteRenderer })
            .width('100%')
            .height(140)
          Text(`入站 ${this.videoResText(this.videoMetrics.inWidth, this.videoMetrics.inHeight)} ` +
            `@ ${this.videoFpsText(this.videoMetrics.inFps)}fps`)
            .fontSize(10)
            .fontColor(Color.Gray)
        }
        .layoutWeight(1)
      }
      .width('100%')

      Text(`状态: ${this.videoStatus}`).fontSize(12).fontColor(0x007DFF).width('100%')

      Row({ space: 8 }) {
        Button(this.isVideoRunning ? '停止' : '启动视频回环')
          .fontSize(12)
          .layoutWeight(1)
          .onClick(() => {
            if (this.isVideoRunning) {
              this.stopVideoLoopback();
            } else {
              this.startVideoLoopback();
            }
          })
        Button('抓帧快照')
          .fontSize(12)
          .layoutWeight(1)
          .enabled(this.isVideoRunning)
          .onClick(() => {
            this.captureVideoFrame();
          })
      }
      .width('100%')

      Text(this.videoFrameInfo).fontSize(12).fontColor(Color.Gray).width('100%')

      if (this.videoPixelMap) {
        Image(this.videoPixelMap!)
          .width('100%')
          .height(150)
          .objectFit(ImageFit.Contain)
          .borderRadius(8)
      }

      VideoMetricsPanel({ panelTitle: '视频回环实测指标', m: this.videoMetrics })
    }
    .width('100%')
  }

  @Builder
  BackpressurePanel() {
    Column({ space: 8 }) {
      Row({ space: 16 }) {
        Text(`积压: ${this.bufferedAmount} B`).fontSize(13)
        Text(`已发: ${this.sentBytes} B`).fontSize(13)
        Text(this.bpPaused ? '已暂停' : '发送中').fontSize(13).fontColor(this.bpPaused ? Color.Red : Color.Green)
      }
      .width('100%')

      Progress({ value: this.sentBytes, total: TOTAL_CHUNKS * CHUNK_SIZE }).width('100%')

      Button(this.bpRunning ? '运行中...' : `发送 2MB(高水位 ${PAUSE_THRESHOLD / 1024}KB)`)
        .fontSize(12)
        .width('100%')
        .enabled(!this.bpRunning)
        .onClick(() => {
          this.runBackpressure();
        })
      Button('清空日志')
        .fontSize(12)
        .width('100%')
        .onClick(() => {
          this.bpLogs = [];
        })

      ForEach(this.bpLogs, (log: string) => {
        Text(log).fontSize(12).fontColor(Color.Gray).width('100%')
      }, (log: string, index: number) => index.toString())
    }
    .width('100%')
  }
}

function sleep(ms: number): Promise<void> {
  return new Promise<void>((resolve) => {
    setTimeout(resolve, ms);
  });
}
