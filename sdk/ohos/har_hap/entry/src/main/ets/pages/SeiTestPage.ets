// Copyright (c) 2026. Licensed under the Apache License, Version 2.0.
import webrtc, { VideoRenderController } from '@ohos/webrtc';
import abilityAccessCtrl from '@ohos.abilityAccessCtrl';
import display from '@ohos.display';
import util from '@ohos.util';
import window from '@ohos.window';
import { PreviewLoopback } from './PreviewLoopback';

const SEQUENCE_BYTES: number = 4;
const TEST_SUFFIX: Uint8Array = new Uint8Array([0, 0, 0, 0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 255, 128, 127]);

@Entry
@Component
struct SeiTestPage {
  @State ready: boolean = false;
  @State running: boolean = false;
  @State text: string = 'SEI占位';
  @State continuousSending: boolean = false;
  @State seiText: string = '等待接收 SEI';
  @State seiSummary: string = '';
  @State videoAspectRatio: number = 4 / 3;
  private controller: VideoRenderController = new VideoRenderController();
  private loopback: PreviewLoopback = new PreviewLoopback(this.controller);
  private stream?: webrtc.MediaStream;
  private cameraSource?: webrtc.VideoSource;
  private hostWindow?: window.Window;
  private displayListening: boolean = false;
  private timer: number = -1;
  private sendTimer: number = -1;
  private generation: number = 0;
  private sequence: number = 0;

  private showError(message: string): void {
    try {
      this.getUIContext().getPromptAction().showToast({ message: message, duration: 3000 });
    } catch (error) {
      console.error('[SeiTestPage] Failed to show error message');
    }
  }

  private syncCaptureRotation(): void {
    if (!this.cameraSource || !this.hostWindow) { return; }
    const displayId = this.hostWindow.getWindowProperties().displayId;
    const currentDisplay = Number.isFinite(displayId) ? display.getDisplayByIdSync(displayId) :
      display.getDefaultDisplaySync();
    const angle = currentDisplay.rotation * 90;
    this.cameraSource.applyDisplayRotation?.(angle);
    console.info(`[SeiVideoGeometry] capture display rotation=${angle}`);
  }

  private onDisplayChange = (_displayId: number): void => {
    if (!this.running) { return; }
    try {
      this.syncCaptureRotation();
    } catch (error) {
      console.error('[SeiVideoGeometry] Failed to update capture rotation');
      this.stop();
      this.showError('相机方向更新失败，请重新开始回环');
    }
  };

  private releaseStream(stream?: webrtc.MediaStream): void {
    stream?.getTracks().forEach((track: webrtc.MediaStreamTrack) => {
      const source = track.getSource();
      track.stop();
      source?.release();
    });
  }

  private stop(): void {
    this.generation++;
    this.stopContinuousSend();
    if (this.displayListening) {
      try { display.off('change', this.onDisplayChange); }
      catch (error) { console.error('[SeiVideoGeometry] Failed to remove display listener'); }
      this.displayListening = false;
    }
    this.cameraSource = undefined;
    this.hostWindow = undefined;
    if (this.timer >= 0) { clearInterval(this.timer); this.timer = -1; }
    this.loopback.stop();
    this.releaseStream(this.stream);
    this.stream = undefined;
    this.ready = false;
    this.running = false;
    this.seiText = '等待接收 SEI';
    this.seiSummary = '';
  }

  private async start(): Promise<void> {
    this.stop();
    const generation = this.generation;
    this.running = true;
    this.videoAspectRatio = 4 / 3;
    try {
      if (typeof webrtc.EncoderWrapper === 'undefined') { throw new Error('请整库重编并替换 libohos_webrtc.so'); }
      const permissions = await abilityAccessCtrl.createAtManager()
        .requestPermissionsFromUser(getContext(), ['ohos.permission.CAMERA']);
      if (generation !== this.generation) { return; }
      if (permissions.authResults.some((value: number) => value !== 0)) { throw new Error('未获得相机权限'); }
      const hostWindow = await window.getLastWindow(getContext());
      if (generation !== this.generation) { return; }
      this.hostWindow = hostWindow;
      const factory = new webrtc.PeerConnectionFactory();
      webrtc.PeerConnectionFactory.setDefault(factory);
      const stream = await new webrtc.MediaDevices().getUserMedia({ audio: false,
        video: { width: 640, height: 480, frameRate: 15 } });
      if (generation !== this.generation) { this.releaseStream(stream); return; }
      this.stream = stream;
      const track = stream.getVideoTracks()[0];
      if (!track) { throw new Error('没有视频轨道'); }
      const source = track.getSource() as webrtc.VideoSource | null;
      if (!source || typeof source.applyDisplayRotation !== 'function') {
        throw new Error('当前 Native 库不支持相机方向适配');
      }
      this.cameraSource = source;
      // Enable the same camera rotation path as CameraPreviewSession before capture starts.
      display.on('change', this.onDisplayChange);
      this.displayListening = true;
      this.syncCaptureRotation();
      this.loopback.onError = (message: string): void => {
        if (generation !== this.generation) { return; }
        this.stop();
        this.showError(message);
      };
      this.loopback.onSei = (event: webrtc.RTCSeiEvent): void => {
        if (generation === this.generation) { this.receive(event); }
      };
      this.loopback.onFrameSizeChanged = (width: number, height: number): void => {
        if (generation !== this.generation || width <= 0 || height <= 0) { return; }
        // VideoRenderController reports dimensions after applying frame rotation.
        this.videoAspectRatio = width / height;
        console.info(`[SeiVideoGeometry] received frame=${width}x${height}`);
      };
      await this.loopback.start(factory, track, true);
      if (generation !== this.generation) { return; }
      if (!this.loopback.encoder) { this.stop(); return; }
      this.timer = setInterval(() => {
        if (!this.loopback.encoder) { this.stop(); return; }
        this.ready = this.loopback.connected && (this.loopback.encoder?.ready ?? false);
      }, 200);
    } catch (error) {
      if (generation !== this.generation) { return; }
      this.stop();
      this.showError(`启动失败：${(error as Error).message}`);
    }
  }

  private send(): void {
    const encoder = this.loopback.encoder;
    if (!encoder?.ready) { throw new Error('H.264 编码尚未就绪'); }
    const text = new util.TextEncoder().encodeInto(this.text);
    const payload = new Uint8Array(text.length + SEQUENCE_BYTES + TEST_SUFFIX.length);
    if (payload.byteLength > webrtc.EncoderWrapper.maxDataBytes) { throw new Error('文本过长'); }
    const sequence = ++this.sequence;
    new DataView(payload.buffer).setUint32(0, sequence);
    // Binary suffix exercises embedded zeros and emulation-prevention sequences.
    payload.set(text, SEQUENCE_BYTES);
    payload.set(TEST_SUFFIX, SEQUENCE_BYTES + text.length);
    encoder.writeSei(payload.buffer);
    // Prove native writeSei copied the original ArkTS-owned bytes.
    payload.fill(0xff);
  }

  private receive(event: webrtc.RTCSeiEvent): void {
    if (event.uuid !== webrtc.EncoderWrapper.defaultUuid) { return; }
    const bytes = new Uint8Array(event.data);
    this.seiText = '无法解析本条 SEI';
    this.seiSummary = `RTP 时间戳：${event.rtpTimestamp} · 格式错误`;
    if (bytes.length < SEQUENCE_BYTES + TEST_SUFFIX.length) {
      this.showError('收到的 SEI 长度不足，无法解析测试文本');
      return;
    }
    const sequence = new DataView(event.data).getUint32(0);
    this.seiSummary = `序号：${sequence}\nRTP 时间戳：${event.rtpTimestamp}`;
    const textEnd = bytes.length - TEST_SUFFIX.length;
    if (!TEST_SUFFIX.every((value: number, index: number) => bytes[textEnd + index] === value)) {
      this.showError('收到的 SEI 不符合本测试的数据格式');
      return;
    }
    try {
      // Decode only the received UTF-8 text, excluding the sequence and binary test suffix.
      const text = util.TextDecoder.create('utf-8', { fatal: true })
        .decodeToString(bytes.subarray(SEQUENCE_BYTES, textEnd));
      this.seiText = text.length > 0 ? text : '（空文本）';
    } catch (error) {
      this.showError('收到的 SEI 文本不是有效的 UTF-8');
    }
  }

  private stopContinuousSend(): void {
    if (this.sendTimer >= 0) { clearInterval(this.sendTimer); this.sendTimer = -1; }
    this.continuousSending = false;
  }

  private startContinuousSend(): void {
    if (this.continuousSending) { return; }
    try {
      this.send();
      this.continuousSending = true;
      this.sendTimer = setInterval(() => {
        try {
          this.send();
        } catch (error) {
          this.stopContinuousSend();
          this.showError((error as Error).message);
        }
      }, 200);
    } catch (error) { this.showError((error as Error).message); }
  }

  aboutToDisappear(): void { this.stop(); }
  onPageHide(): void { this.stop(); }

  build() {
    Scroll() {
      Column({ space: 12 }) {
        Row({ space: 12 }) {
          Text('SEI 本机 RTP 回环').fontSize(22).layoutWeight(1)
          Button(this.running ? '停止回环' : '开始回环').width(100).height(40).fontSize(14)
            .onClick(() => {
              if (this.running) {
                this.stop();
              } else {
                this.start();
              }
            })
        }.width('100%').alignItems(VerticalAlign.Center)
        Column({ space: 6 }) {
          Text('本端测试SEI数据').fontSize(16).fontWeight(FontWeight.Medium)
          Text('自定义数据 = 序号（4 字节）+ 输入文本（UTF-8）+ 二进制测试字节（16 字节）。').fontSize(13)
          Text('序号随发送递增；文本由下方输入框填写；二进制测试字节包含 0x00、0xFF 等内容，用于测试二进制数据传输。').fontSize(13)
          Text('另附 16 字节 UUID，标识这类自定义数据；画面上的 RTP 时间戳来自视频帧。').fontSize(13)
        }.alignItems(HorizontalAlign.Start).width('100%').padding(12)
          .backgroundColor('#F5F5F5').borderRadius(8)
        TextInput({ text: this.text, placeholder: '输入 SEI 文本' })
          .onChange((value: string) => { this.text = value; })
        Row({ space: 12 }) {
          Button('发送一条').enabled(this.ready && !this.continuousSending).onClick(() => {
            try { this.send(); } catch (error) { this.showError((error as Error).message); }
          })
          Button(this.continuousSending ? '停止发送' : '连续发送')
            .enabled(this.ready || this.continuousSending).onClick(() => {
              if (this.continuousSending) {
                this.stopContinuousSend();
              } else {
                this.startContinuousSend();
              }
            })
        }
        Stack({ alignContent: Alignment.BottomStart }) {
          XComponent({ id: 'sei-remote-video', type: XComponentType.SURFACE, controller: this.controller })
            .width('100%').height('100%')
            .onAreaChange((_oldArea: Area, newArea: Area) => {
              const width = Math.round(vp2px(Number(newArea.width)));
              const height = Math.round(vp2px(Number(newArea.height)));
              if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0) { return; }
              this.controller.setPreviewSurfaceSize(width, height);
              console.info(`[SeiVideoGeometry] render surface=${width}x${height}`);
            })
          Column({ space: 4 }) {
            Text('接收端 SEI').fontSize(12).fontColor(Color.White)
            Text(this.seiText).fontSize(18).fontWeight(FontWeight.Medium).fontColor(Color.White)
              .maxLines(3).textOverflow({ overflow: TextOverflow.Ellipsis })
            if (this.seiSummary.length > 0) {
              Text(this.seiSummary).fontSize(12).fontColor(Color.White)
            }
          }.alignItems(HorizontalAlign.Start).padding(10).width('100%')
            .backgroundColor('#B3000000').hitTestBehavior(HitTestMode.None)
        }.width('100%').aspectRatio(this.videoAspectRatio).backgroundColor(Color.Black).clip(true)
      }.padding(16).width('100%')
    }.width('100%').height('100%').align(Alignment.TopStart)
  }
}
