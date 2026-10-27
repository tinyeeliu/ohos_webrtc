// Copyright (c) 2026. Licensed under the Apache License, Version 2.0.
import webrtc, { VideoRenderController } from '@ohos/webrtc';
import abilityAccessCtrl from '@ohos.abilityAccessCtrl';
import util from '@ohos.util';
import { PreviewLoopback } from './PreviewLoopback';

@Entry
@Component
struct SeiTestPage {
  @State status: string = '请使用包含 EncoderWrapper 的新 Native 库';
  @State ready: boolean = false;
  @State text: string = 'H.264 SEI 中文测试';
  @State queued: number = 0;
  @State injected: number = 0;
  @State received: number = 0;
  @State matched: number = 0;
  @State details: string = '';
  @State burst: boolean = false;
  private controller: VideoRenderController = new VideoRenderController();
  private loopback: PreviewLoopback = new PreviewLoopback(this.controller);
  private stream?: webrtc.MediaStream;
  private timer: number = -1;
  private generation: number = 0;
  private sequence: number = 0;
  private expected: Map<number, Uint8Array> = new Map();

  private releaseStream(stream?: webrtc.MediaStream): void {
    stream?.getTracks().forEach((track: webrtc.MediaStreamTrack) => {
      const source = track.getSource();
      track.stop();
      source?.release();
    });
  }

  private stop(): void {
    this.generation++;
    if (this.timer >= 0) { clearInterval(this.timer); this.timer = -1; }
    this.loopback.stop();
    this.releaseStream(this.stream);
    this.stream = undefined;
    this.ready = false;
    this.burst = false;
    this.expected.clear();
  }

  private async start(): Promise<void> {
    this.stop();
    const generation = this.generation;
    this.queued = 0; this.injected = 0; this.received = 0; this.matched = 0;
    this.details = '';
    try {
      if (typeof webrtc.EncoderWrapper === 'undefined') { throw new Error('请整库重编并替换 libohos_webrtc.so'); }
      const permissions = await abilityAccessCtrl.createAtManager()
        .requestPermissionsFromUser(getContext(), ['ohos.permission.CAMERA']);
      if (generation !== this.generation) { return; }
      if (permissions.authResults.some((value: number) => value !== 0)) { throw new Error('未获得相机权限'); }
      const factory = new webrtc.PeerConnectionFactory();
      webrtc.PeerConnectionFactory.setDefault(factory);
      const stream = await new webrtc.MediaDevices().getUserMedia({ audio: false,
        video: { width: 640, height: 480, frameRate: 15 } });
      if (generation !== this.generation) { this.releaseStream(stream); return; }
      this.stream = stream;
      const track = stream.getVideoTracks()[0];
      if (!track) { throw new Error('没有视频轨道'); }
      this.loopback.onStatus = (message: string): void => { this.status = message; };
      this.loopback.onSei = (event: webrtc.RTCSeiEvent): void => { this.receive(event); };
      await this.loopback.start(factory, track, true);
      if (generation !== this.generation) { return; }
      this.timer = setInterval(() => {
        this.ready = this.loopback.connected && (this.loopback.encoder?.ready ?? false);
        this.injected = this.loopback.encoder?.injectedMessages ?? 0;
      }, 200);
    } catch (error) {
      this.stop();
      this.status = `启动失败：${(error as Error).message}`;
    }
  }

  private send(): void {
    const encoder = this.loopback.encoder;
    if (!encoder?.ready) { throw new Error('H.264 编码尚未就绪'); }
    if (this.expected.size >= 64) { throw new Error('已有 64 条待核对数据，请等待接收或重启'); }
    const text = new util.TextEncoder().encodeInto(this.text);
    const payload = new Uint8Array(text.length + 4 + 16);
    if (payload.byteLength > webrtc.EncoderWrapper.maxDataBytes) { throw new Error('文本过长'); }
    const sequence = ++this.sequence;
    new DataView(payload.buffer).setUint32(0, sequence);
    // Binary suffix exercises embedded zeros and emulation-prevention sequences.
    payload.set(text, 4);
    payload.set(new Uint8Array([0, 0, 0, 0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 255, 128, 127]), 4 + text.length);
    encoder.writeSei(payload.buffer);
    this.expected.set(sequence, new Uint8Array(payload));
    // Prove native writeSei copied the original ArkTS-owned bytes.
    payload.fill(0xff);
    this.queued++;
  }

  private receive(event: webrtc.RTCSeiEvent): void {
    if (event.uuid !== webrtc.EncoderWrapper.defaultUuid) { return; }
    const bytes = new Uint8Array(event.data);
    this.received++;
    if (bytes.length < 4) { this.status = '收到外部短 SEI'; return; }
    const sequence = new DataView(event.data).getUint32(0);
    const expected = this.expected.get(sequence);
    const same = event.uuid === webrtc.EncoderWrapper.defaultUuid && expected !== undefined &&
      bytes.length === expected.length && bytes.every((value: number, index: number) => value === expected[index]);
    if (same) { this.matched++; this.expected.delete(sequence); }
    this.details = `seq=${sequence}, ${same ? '逐字节一致' : '不匹配/重复'}\n` +
      `UUID=${event.uuid}\nSSRC=${event.ssrc}, RTP=${event.rtpTimestamp}\n` +
      `bytes=${bytes.length}, 回调队列丢弃=${event.droppedMessages}`;
    if (!same) { this.status = 'SEI 数据核对失败'; }
  }

  private async sendBurst(): Promise<void> {
    const generation = this.generation;
    this.burst = true;
    try {
      for (let i = 0; i < 100; i++) {
        if (generation !== this.generation) { return; }
        this.send();
        await new Promise<void>((resolve) => { setTimeout(resolve, 200); });
      }
      if (generation === this.generation) { this.status = '100 条已入队，请核对接收和一致数'; }
    } catch (error) { this.status = (error as Error).message; }
    if (generation === this.generation) { this.burst = false; }
  }

  aboutToDisappear(): void { this.stop(); }
  onPageHide(): void { this.stop(); }

  build() {
    Scroll() {
      Column({ space: 12 }) {
        Text('H.264 SEI 本机 RTP 回环').fontSize(22)
        Text(this.status)
        Row({ space: 12 }) {
          Button('启动').onClick(() => { this.start(); })
          Button('停止').onClick(() => { this.stop(); this.status = '已停止'; })
        }
        Text(`编码就绪：${this.ready}，入队 ${this.queued}，实际注入 ${this.injected}`)
        Text(`接收 ${this.received}，逐字节一致 ${this.matched}`)
        TextInput({ text: this.text, placeholder: '输入 SEI 文本' })
          .onChange((value: string) => { this.text = value; })
        Row({ space: 12 }) {
          Button('发送一条').enabled(this.ready && !this.burst).onClick(() => {
            try { this.send(); } catch (error) { this.status = (error as Error).message; }
          })
          Button('发送 100 条').enabled(this.ready && !this.burst).onClick(() => { this.sendBurst(); })
        }
        Text(this.details).fontSize(12)
        XComponent({ id: 'sei-remote-video', type: XComponentType.SURFACE, controller: this.controller })
          .width('100%').height(280)
        Text('视频与 SEI 均经过本机两个 PeerConnection 的 RTP 链路；不使用外部信令服务。').fontSize(12)
      }.padding(16).width('100%')
    }.width('100%').height('100%')
  }
}
