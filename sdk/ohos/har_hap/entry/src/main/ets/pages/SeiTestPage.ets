// Copyright (c) 2026. Licensed under the Apache License, Version 2.0.
import webrtc, { VideoRenderController, ScalingMode } from '@ohos/webrtc';
import abilityAccessCtrl from '@ohos.abilityAccessCtrl';
import display from '@ohos.display';
import util from '@ohos.util';
import window from '@ohos.window';

const SEQUENCE_BYTES: number = 4;
const BINARY_BYTES: number = 8;
const BYTE_INDICES: number[] = [0, 1, 2, 3, 4, 5, 6, 7];

@Entry
@Component
struct SeiTestPage {
  @State ready: boolean = false;
  @State running: boolean = false;
  @State text: string = 'SEI占位';
  @State binaryInputs: string[] = ['00', '00', '01', '02', '03', 'FF', '80', '7F'];
  @State continuousSending: boolean = false;
  @State seiText: string = '';
  @State seiSummary: string = '';
  @State seiBinary: string = '';
  @State videoAspectRatio: number = 4 / 3;
  private controller: VideoRenderController = new VideoRenderController();
  private sender?: webrtc.RTCPeerConnection;
  private receiver?: webrtc.RTCPeerConnection;
  private encoder?: webrtc.EncoderWrapper;
  private seiReceiver?: webrtc.RTCRtpReceiver;
  private pendingSender: webrtc.RTCIceCandidateInit[] = [];
  private pendingReceiver: webrtc.RTCIceCandidateInit[] = [];
  private senderReady: boolean = false;
  private receiverReady: boolean = false;
  private connectionTimeout: number = -1;
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
    if (this.connectionTimeout >= 0) { clearTimeout(this.connectionTimeout); this.connectionTimeout = -1; }
    this.controller.onFrameSizeChanged = undefined;
    this.controller.setVideoTrack(null);
    if (this.seiReceiver) {
      try { this.seiReceiver.onsei = null; }
      catch (error) { console.error('[SeiTestPage] Failed to remove SEI listener'); }
      this.seiReceiver = undefined;
    }
    this.encoder = undefined;
    for (const peer of [this.sender, this.receiver]) {
      if (peer) {
        peer.onicecandidate = null;
        peer.ontrack = null;
        peer.onconnectionstatechange = null;
        peer.close();
      }
    }
    this.sender = undefined;
    this.receiver = undefined;
    this.pendingSender = [];
    this.pendingReceiver = [];
    this.senderReady = false;
    this.receiverReady = false;
    this.releaseStream(this.stream);
    this.stream = undefined;
    this.ready = false;
    this.running = false;
    this.seiText = '';
    this.seiSummary = '';
    this.seiBinary = '';
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
      await this.startLoopback(factory, track, generation);
      if (generation !== this.generation) { return; }
      this.timer = setInterval(() => {
        if (generation !== this.generation) { return; }
        this.ready = this.sender?.connectionState === 'connected' &&
          this.receiver?.connectionState === 'connected' && (this.encoder?.ready ?? false);
      }, 200);
    } catch (error) {
      if (generation !== this.generation) { return; }
      console.error('[SeiTestPage] Failed to start loopback');
      this.stop();
      this.showError(`启动失败：${(error as Error).message}`);
    }
  }

  // Exchange SDP and ICE directly between two local peers; no signaling server is needed.
  private async startLoopback(factory: webrtc.PeerConnectionFactory, track: webrtc.MediaStreamTrack,
    generation: number): Promise<void> {
    const fail = (): void => {
      if (generation !== this.generation) { return; }
      console.error('[SeiTestPage] Loopback connection or decoding failed');
      this.stop();
      this.showError('编解码验证未完成，请检查连接、编码器和设备日志后重试');
    };
    const sender = factory.createPeerConnection({ iceServers: [] });
    this.sender = sender;
    const receiver = factory.createPeerConnection({ iceServers: [] });
    this.receiver = receiver;
    this.controller.onFrameSizeChanged = (width: number, height: number): void => {
      if (generation !== this.generation || width <= 0 || height <= 0) { return; }
      if (this.connectionTimeout >= 0) { clearTimeout(this.connectionTimeout); this.connectionTimeout = -1; }
      // VideoRenderController reports dimensions after applying frame rotation.
      this.videoAspectRatio = width / height;
      console.info(`[SeiVideoGeometry] received frame=${width}x${height}`);
    };
    receiver.ontrack = (event: webrtc.RTCTrackEvent): void => {
      if (generation !== this.generation || event.track.kind !== 'video') { return; }
      this.seiReceiver = event.receiver;
      event.receiver.onsei = (sei: webrtc.RTCSeiEvent): void => {
        if (generation === this.generation) { this.receive(sei); }
      };
      this.controller.setVideoTrack(event.track);
    };
    sender.onicecandidate = (event: webrtc.RTCPeerConnectionIceEvent): void => {
      if (generation !== this.generation || !event.candidate) { return; }
      const candidate = event.candidate.toJSON();
      if (this.receiverReady) {
        receiver.addIceCandidate(candidate).catch(fail);
      } else {
        this.pendingReceiver.push(candidate);
      }
    };
    receiver.onicecandidate = (event: webrtc.RTCPeerConnectionIceEvent): void => {
      if (generation !== this.generation || !event.candidate) { return; }
      const candidate = event.candidate.toJSON();
      if (this.senderReady) {
        sender.addIceCandidate(candidate).catch(fail);
      } else {
        this.pendingSender.push(candidate);
      }
    };
    receiver.onconnectionstatechange = (): void => {
      if (receiver.connectionState === 'failed') { fail(); }
    };
    this.connectionTimeout = setTimeout(fail, 15000);
    const codecs = webrtc.RTCRtpSender.getCapabilities('video')?.codecs.filter(
      (codec: webrtc.RTCRtpCodec) => codec.mimeType.toLowerCase() === 'video/h264' &&
        (codec.sdpFmtpLine ?? '').split(';').some((part: string) => part.trim() === 'packetization-mode=1')) ?? [];
    if (codecs.length === 0) { throw new Error('当前设备或构建没有 H.264 packetization-mode=1 编码能力'); }
    const transceiver = sender.addTransceiver(track, { direction: 'sendonly' });
    transceiver.setCodecPreferences(codecs);
    this.encoder = transceiver.sender.getEncoderWrapper();
    const offer = await sender.createOffer();
    if (generation !== this.generation) { return; }
    await sender.setLocalDescription(offer);
    if (generation !== this.generation) { return; }
    await receiver.setRemoteDescription(offer);
    if (generation !== this.generation) { return; }
    this.receiverReady = true;
    for (const candidate of this.pendingReceiver) {
      await receiver.addIceCandidate(candidate);
      if (generation !== this.generation) { return; }
    }
    this.pendingReceiver = [];
    const answer = await receiver.createAnswer();
    if (generation !== this.generation) { return; }
    await receiver.setLocalDescription(answer);
    if (generation !== this.generation) { return; }
    await sender.setRemoteDescription(answer);
    if (generation !== this.generation) { return; }
    this.senderReady = true;
    for (const candidate of this.pendingSender) {
      await sender.addIceCandidate(candidate);
      if (generation !== this.generation) { return; }
    }
    this.pendingSender = [];
  }

  private parseBinaryData(): Uint8Array {
    const bytes = new Uint8Array(BINARY_BYTES);
    for (let index = 0; index < BINARY_BYTES; index++) {
      const value = this.binaryInputs[index];
      if (!/^[0-9a-fA-F]{2}$/.test(value)) {
        throw new Error(`第 ${index + 1} 个字节请输入两位十六进制数（00–FF）`);
      }
      bytes[index] = parseInt(value, 16);
    }
    return bytes;
  }

  private send(): void {
    const encoder = this.encoder;
    if (!encoder?.ready) { throw new Error('H.264 编码尚未就绪'); }
    const binary = this.parseBinaryData();
    const text = new util.TextEncoder().encodeInto(this.text);
    const payload = new Uint8Array(text.length + SEQUENCE_BYTES + BINARY_BYTES);
    if (payload.byteLength > webrtc.EncoderWrapper.maxDataBytes) { throw new Error('文本过长'); }
    const sequence = ++this.sequence;
    new DataView(payload.buffer).setUint32(0, sequence);
    // Append the eight bytes entered in the UI after the UTF-8 text.
    payload.set(text, SEQUENCE_BYTES);
    payload.set(binary, SEQUENCE_BYTES + text.length);
    encoder.writeSei(payload.buffer);
    // Prove native writeSei copied the original ArkTS-owned bytes.
    payload.fill(0xff);
  }

  private receive(event: webrtc.RTCSeiEvent): void {
    if (event.uuid !== webrtc.EncoderWrapper.defaultUuid) { return; }
    const bytes = new Uint8Array(event.data);
    this.seiText = '无法解析本条 SEI';
    this.seiSummary = '格式错误';
    this.seiBinary = '';
    if (bytes.length < SEQUENCE_BYTES + BINARY_BYTES) {
      this.showError('收到的 SEI 长度不足，无法解析测试文本');
      return;
    }
    const sequence = new DataView(event.data).getUint32(0);
    this.seiSummary = `序号：${sequence}`;
    const textEnd = bytes.length - BINARY_BYTES;
    try {
      // Decode only the received UTF-8 text, excluding the sequence and binary data.
      const text = util.TextDecoder.create('utf-8', { fatal: true })
        .decodeToString(bytes.subarray(SEQUENCE_BYTES, textEnd));
      this.seiText = text.length > 0 ? text : '（空文本）';
      const binary: string[] = [];
      for (let index = textEnd; index < bytes.length; index++) {
        binary.push(bytes[index].toString(16).toUpperCase().padStart(2, '0'));
      }
      this.seiBinary = binary.join(' ');
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

  aboutToAppear(): void {
    this.controller.enableCameraPreviewAdaptation();
    this.controller.setScalingMode(ScalingMode.AspectFit);
    this.controller.setMirror(false);
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
          Text('自定义数据 = 序号（4 字节）+ 输入文本（UTF-8）+ 二进制数据（8 字节）。').fontSize(13)
          Text('序号随发送递增；文本和二进制数据由下方输入框填写，每个字节填写两位十六进制数（00–FF）。').fontSize(13)
        }.alignItems(HorizontalAlign.Start).width('100%').padding(12)
          .backgroundColor('#F5F5F5').borderRadius(8)
        Row({ space: 6 }) {
          Text('SEI文本').width(70).fontSize(14)
          TextInput({ text: this.text, placeholder: '输入 SEI 文本' }).layoutWeight(1)
            .onChange((value: string) => { this.text = value; })
        }.width('100%').alignItems(VerticalAlign.Center)
        Row({ space: 6 }) {
          Text('二进制数据').width(70).fontSize(14)
          Row({ space: 3 }) {
            ForEach(BYTE_INDICES, (index: number) => {
              TextInput({ text: this.binaryInputs[index], placeholder: '00' })
                .layoutWeight(1).height(40).fontSize(14).textAlign(TextAlign.Center)
                .padding({ left: 0, right: 0 }).borderRadius(6).maxLength(2)
                .inputFilter('[0-9a-fA-F]')
                .onChange((value: string) => { this.binaryInputs[index] = value.toUpperCase(); })
            }, (index: number) => index.toString())
          }.layoutWeight(1)
        }.width('100%').alignItems(VerticalAlign.Center)
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
            if (this.seiText.length > 0) {
              Text(`SEI文本：${this.seiText}`).fontSize(14).fontColor(Color.White)
                .maxLines(3).textOverflow({ overflow: TextOverflow.Ellipsis })
            }
            if (this.seiSummary.length > 0) {
              Text(this.seiSummary).fontSize(14).fontColor(Color.White)
            }
            if (this.seiBinary.length > 0) {
              Text(`二进制：${this.seiBinary}`).fontSize(14).fontColor(Color.White)
            }
          }.alignItems(HorizontalAlign.Start).padding(10).width('100%')
            .backgroundColor('#B3000000').hitTestBehavior(HitTestMode.None)
        }.width('100%').aspectRatio(this.videoAspectRatio).backgroundColor(Color.Black).clip(true)
      }.padding(16).width('100%')
    }.width('100%').height('100%').align(Alignment.TopStart)
  }
}
