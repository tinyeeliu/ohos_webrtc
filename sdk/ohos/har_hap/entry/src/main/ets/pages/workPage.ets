/**
 * Copyright (c) 2024 Archermind Technology (Nanjing) Co. Ltd.
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

import webrtc, { Logging, LoggingSeverity, VideoRenderController, ScalingMode } from '@ohos/webrtc';
import abilityAccessCtrl, { PermissionRequestResult } from '@ohos.abilityAccessCtrl';
import webSocket from '@ohos.net.webSocket';
import promptAction from '@ohos.promptAction';
import buffer from '@ohos.buffer';
import fs from '@ohos.file.fs';
import audio from '@ohos.multimedia.audio';
import util from '@ohos.util';
import { BusinessError } from '@kit.BasicServicesKit';

const TAG: string = '[Index]';

// 用于测试的信令服务器
const SIGNAL_SERVER = 'wss://youzhi.life:8443';
// 用于测试的Web客户端地址
// https://webrtc.youzhi.life/
// 用于测试的STUN服务器地址
const STUN_SERVER = "stun:stun.l.google.com:19302"
const SEI_SEQUENCE_BYTES: number = 4;
const SEI_BINARY_BYTES: number = 8;
const SEI_BYTE_INDICES: number[] = [0, 1, 2, 3, 4, 5, 6, 7];

interface MessageData {
  sdp?: webrtc.RTCSessionDescriptionInit;
  ice?: webrtc.RTCIceCandidateInit;
}

interface RtpStats extends webrtc.RTCStats {
  kind?: string;
  frameWidth?: number;
  frameHeight?: number;
}

@Entry
@Component
struct Index {
  @State buttonTitle: string = 'Connect';
  @State statusText: string = '';
  @State private myId: string = '';
  @State private peerId: string = '';
  @State muteBtnTitle: string = 'Mute';
  private muted: boolean = false;
  @State micBtnTitle: string = 'MIC ON';
  private micOFF: boolean = false;
  @State localResolution: string = '未获取';
  @State remoteResolution: string = '未获取';
  @State canChangeResolution: boolean = false;
  @State private customResWidth: string = '1280';
  @State private customResHeight: string = '720';
  @State private seiText: string = 'SEI占位';
  @State private seiBinaryInputs: string[] = ['00', '00', '01', '02', '03', 'FF', '80', '7F'];
  @State private seiReady: boolean = false;
  @State private seiContinuous: boolean = false;
  @State private receivedSeiText: string = '';
  @State private receivedSeiSequence: string = '';
  @State private receivedSeiBinary: string = '';
  private seiEncoder?: webrtc.EncoderWrapper;
  private seiReceiver?: webrtc.RTCRtpReceiver;
  private seiSequence: number = 0;
  private seiReadyTimer: number = -1;
  private seiSendTimer: number = -1;
  private pageActive: boolean = false;
  private pageVisible: boolean = false;
  private peerGeneration: number = 0;
  private reconnectTimer: number = -1;
  private statsTimer?: number = undefined;
  private mXComponentController1: VideoRenderController = new VideoRenderController();
  private mXComponentController2: VideoRenderController = new VideoRenderController();
  private ws: webSocket.WebSocket = webSocket.createWebSocket();
  private localAudioSource?: webrtc.AudioSource;
  private localAudioSource2?: webrtc.AudioSource;
  private localVideoSource?: webrtc.VideoSource;
  private localAudioTrack?: webrtc.MediaStreamTrack;
  private localAudioTrack2?: webrtc.MediaStreamTrack;
  private localVideoTrack?: webrtc.MediaStreamTrack;
  private remoteAudioTrack?: webrtc.MediaStreamTrack;
  private remoteVideoTrack?: webrtc.MediaStreamTrack;
  private adm?: webrtc.AudioDeviceModule;
  private pcf?: webrtc.PeerConnectionFactory;
  private pc?: webrtc.RTCPeerConnection;
  private recvChannel?: webrtc.RTCDataChannel;
  private sendChannel?: webrtc.RTCDataChannel;
  private displayMediaStream?: webrtc.MediaStream;
  private mediaDevices = new webrtc.MediaDevices();
  private onDataChannelMessageReceived = (event: webrtc.MessageEvent<string | ArrayBuffer>) => {
    // this is Index
    console.info(TAG, "dataChannel.OnMessage: ", event);
    console.info(TAG, "dataChannel.OnMessage: (typeof event.data) = ", typeof event.data);
    console.info(TAG, "dataChannel.OnMessage: (event.data instanceof ArrayBuffer) = ",
      event.data instanceof ArrayBuffer);

    this.setStatus("Received data channel message");

    if (typeof event.data === 'string' || event.data instanceof String) {
      console.info(TAG, 'Incoming string message: ' + event.data);
    } else {
      console.info(TAG, 'Incoming data message');
    }
  }

  //
  build() {
    Scroll() {
      Column() {
        Row() {
          Text("状态")
            .fontSize(20)
            .fontWeight(FontWeight.Normal)
            .margin({ right: 24 })
            .flexShrink(0)
          Text(this.statusText)
            .fontSize(20)
            .fontWeight(FontWeight.Bold)
            .layoutWeight(1)
            .maxLines(20)
            .textOverflow({ overflow: TextOverflow.Clip })
            .textAlign(TextAlign.Start)
        }
        .width('100%')
        .alignSelf(ItemAlign.Start)

        Row() {
          Text("本机ID")
            .fontSize(20)
            .fontWeight(FontWeight.Normal)
            .margin({ right: 24 })
            .align(Alignment.Start)
          Text(this.myId)
            .fontSize(20)
            .fontWeight(FontWeight.Bold)
        }
        .alignSelf(ItemAlign.Start)
        .padding({ top: 10 })

        Row() {
          TextInput({ placeholder: '请输入对端ID' })
            .width(200)
            .height(54)
            .fontSize(28)
            .fontWeight(FontWeight.Normal)
            .type(InputType.Number)
            .layoutWeight('20%')
            .onChange((value: string) => {
              this.peerId = value;
            })

          Button(this.buttonTitle)
            .width(150)
            .height(54)
            .fontSize(20)
            .fontWeight(FontWeight.Bold)
            .margin({ top: 20 })
            .onClick(() => {
              if (this.buttonTitle == 'Disconnect') {
                this.pauseSei();
                this.ws.close();
                this.buttonTitle = 'Connect';
                this.canChangeResolution = false;
                return;
              }

              if (this.peerId.length < 2 || this.peerId.length > 4) {
                promptAction.showToast({ message: "请输入2到4位数ID" });
                return;
              }

              this.sendMessage('SESSION ' + this.peerId);
              this.buttonTitle = 'Disconnect';
            })
            .margin({ left: 10, right: 20 })

          Row() {
            Checkbox({ name: 'remote_offerer', group: 'checkboxGroup' })
              .select(false)
              .selectedColor(0x39a2db)
              .onChange((value: boolean) => {
                console.info('remote_offerer change is' + value)

              }).margin({ right: 8 })

            Text("Remote offerer")
              .fontSize(20)
              .fontWeight(FontWeight.Normal)
          }.layoutWeight('20%').visibility(Visibility.Hidden)

        }
        .alignSelf(ItemAlign.Start)
        .padding({ top: 24 })

        Column() {
          Row() {
            Button(this.muteBtnTitle)
              .layoutWeight(1)
              .height(54)
              .fontSize(15)
              .fontWeight(FontWeight.Bold)
              .onClick(() => {
                console.log(TAG, "audio muted: " + this.muted);
                this.muted = !this.muted;
                this.muteBtnTitle = this.muted ? 'Unmute' : 'Mute';
                this.pc?.setAudioPlayout(!this.muted);
              })
              .margin({ right: 6 })

            Button(this.micBtnTitle)
              .layoutWeight(1)
              .height(54)
              .fontSize(15)
              .fontWeight(FontWeight.Bold)
              .onClick(() => {
                console.log(TAG, "Microphone : " + this.micOFF);
                this.micOFF = !this.micOFF;
                this.micBtnTitle = this.micOFF ? 'MIC OFF' : 'MIC ON';
                this.pc?.setAudioRecording(!this.micOFF);
              })
              .margin({ left: 6, right: 6 })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .alignSelf(ItemAlign.Start)
          .padding({ top: 24 })

          Column() {
            Text('自定义本地分辨率')
              .fontSize(18)
              .fontWeight(FontWeight.Normal)
              .alignSelf(ItemAlign.Start)

            Row() {
              Text('宽')
                .fontSize(16)
                .margin({ right: 6 })
                .flexShrink(0)
              TextInput({ text: this.customResWidth, placeholder: '宽' })
                .layoutWeight(1)
                .height(44)
                .fontSize(16)
                .type(InputType.Number)
                .onChange((value: string) => {
                  this.customResWidth = value.replace(/[^0-9]/g, '');
                })

              Text('高')
                .fontSize(16)
                .margin({ left: 10, right: 6 })
                .flexShrink(0)
              TextInput({ text: this.customResHeight, placeholder: '高' })
                .layoutWeight(1)
                .height(44)
                .fontSize(16)
                .type(InputType.Number)
                .onChange((value: string) => {
                  this.customResHeight = value.replace(/[^0-9]/g, '');
                })

              Button('设置')
                .height(44)
                .fontSize(15)
                .fontWeight(FontWeight.Medium)
                .margin({ left: 8 })
                .flexShrink(0)
                .onClick(() => {
                  const w = parseInt(this.customResWidth, 10);
                  const h = parseInt(this.customResHeight, 10);
                  if (isNaN(w) || isNaN(h) || w < 1 || h < 1) {
                    promptAction.showToast({ message: '请输入有效的正整数宽高（≥1）' });
                    return;
                  }
                  this.changeResolution(w, h);
                })
            }
            .width('100%')
            .margin({ top: 8 })
          }
          .width('100%')
          .alignSelf(ItemAlign.Start)
          .padding({ top: 24 })
        }


        Row() {
          Text("本地分辨率: ")
            .fontSize(20)
            .fontWeight(FontWeight.Normal)
            .margin({ right: 12 })
          Text(this.localResolution)
            .fontSize(20)
            .fontWeight(FontWeight.Bold)
            .fontColor(0x007DFF)
        }
        .alignSelf(ItemAlign.Start)
        .padding({ top: 12 })

        Row() {
          Text("远端分辨率: ")
            .fontSize(20)
            .fontWeight(FontWeight.Normal)
            .margin({ right: 12 })
          Text(this.remoteResolution)
            .fontSize(20)
            .fontWeight(FontWeight.Bold)
            .fontColor(0x007DFF)
        }
        .alignSelf(ItemAlign.Start)
        .padding({ top: 12 })

        Row() {
          XComponent({
            id: "local-video",
            type: XComponentType.SURFACE,
            controller: this.mXComponentController1
          })
            .layoutWeight(1)
            .height(220)
            .onLoad((context?) => {
              console.log(TAG, "context: " + context);
              console.log(TAG, "surfaceId: " + this.mXComponentController1.getXComponentSurfaceId());
            })
            .onDestroy(() => {
              console.log(TAG, "onDestroy");
            })

          XComponent({
            id: "remote-video",
            type: XComponentType.SURFACE,
            controller: this.mXComponentController2
          })
            .layoutWeight(1)
            .height(220)
            .margin({ left: 8 })
            .onLoad((context?) => {
              console.log(TAG, "context: " + JSON.stringify(context));
              console.log(TAG, "surfaceId: " + this.mXComponentController2.getXComponentSurfaceId());
            })
            .onDestroy(() => {
              console.log(TAG, "onDestroy");
            })
        }
        .width('100%')
        .alignSelf(ItemAlign.Start)
        .padding({ top: 10 })

        Column({ space: 10 }) {
          Text('本端发送 SEI').fontSize(16).fontWeight(FontWeight.Medium)
          Row({ space: 6 }) {
            Text('SEI文本').width(70).fontSize(14)
            TextInput({ text: this.seiText, placeholder: '输入 SEI 文本' }).layoutWeight(1)
              .onChange((value: string) => { this.seiText = value; })
          }.width('100%').alignItems(VerticalAlign.Center)
          Row({ space: 6 }) {
            Text('二进制数据').width(70).fontSize(14)
            Row({ space: 3 }) {
              ForEach(SEI_BYTE_INDICES, (index: number) => {
                TextInput({ text: this.seiBinaryInputs[index], placeholder: '00' })
                  .layoutWeight(1).height(40).fontSize(14).textAlign(TextAlign.Center)
                  .padding({ left: 0, right: 0 }).borderRadius(6).maxLength(2)
                  .inputFilter('[0-9a-fA-F]')
                  .onChange((value: string) => { this.seiBinaryInputs[index] = value.toUpperCase(); })
              }, (index: number) => index.toString())
            }.layoutWeight(1)
          }.width('100%').alignItems(VerticalAlign.Center)
          Text('每格填写两位十六进制数（00–FF）').fontSize(12).fontColor('#666666')
          Row({ space: 12 }) {
            Button('发送一条').fontSize(14).enabled(this.seiReady && !this.seiContinuous)
              .onClick(() => {
                try { this.sendSei(); }
                catch (error) { this.showSeiError((error as Error).message); }
              })
            Button(this.seiContinuous ? '停止发送' : '连续发送').fontSize(14)
              .enabled(this.seiReady || this.seiContinuous)
              .onClick(() => {
                if (this.seiContinuous) { this.stopContinuousSei(); }
                else { this.startContinuousSei(); }
              })
          }
        }.alignItems(HorizontalAlign.Start).width('100%').padding(12)
          .margin({ top: 16 }).backgroundColor('#F5F5F5').borderRadius(8)

        Column({ space: 6 }) {
          Text('本端接收 SEI').fontSize(16).fontWeight(FontWeight.Medium)
          if (this.receivedSeiText.length > 0) {
            Text(`SEI文本：${this.receivedSeiText}`).fontSize(14)
              .maxLines(3).textOverflow({ overflow: TextOverflow.Ellipsis })
          }
          if (this.receivedSeiSequence.length > 0) {
            Text(`序号：${this.receivedSeiSequence}`).fontSize(14)
          }
          if (this.receivedSeiBinary.length > 0) {
            Text(`二进制：${this.receivedSeiBinary}`).fontSize(14)
          }
        }.alignItems(HorizontalAlign.Start).width('100%').padding(12)
          .margin({ top: 12 }).backgroundColor('#F5F5F5').borderRadius(8)

      }
      .width('100%')
      .padding({
        left: 16,
        right: 16,
        top: 16,
        bottom: 16
      })
      .justifyContent(FlexAlign.Start)
    }
    .width('100%')
    .height('100%')
    .scrollBar(BarState.Auto)
    .scrollable(ScrollDirection.Vertical)
    .edgeEffect(EdgeEffect.Spring)
  }

  aboutToAppear(): void {
    this.pageActive = true;
    this.pageVisible = true;
    // 设置音频策略，允许和其它音频流并发
    let strategy: audio.AudioSessionStrategy = {
      concurrencyMode: audio.AudioConcurrencyMode.CONCURRENCY_MIX_WITH_OTHERS
    };

    let audioManager = audio.getAudioManager();
    let audioSessionManager = audioManager.getSessionManager();
    audioSessionManager.activateAudioSession(strategy).then(() => {
      console.info(TAG, 'activateAudioSession SUCCESS');
    }).catch((err: BusinessError) => {
      console.error(`ERROR: ${err}`);
    });

    Logging.enableLogThreads();
    Logging.enableLogTimeStamps();
    Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);

    abilityAccessCtrl.createAtManager()
      .requestPermissionsFromUser(getContext(), ['ohos.permission.CAMERA', 'ohos.permission.MICROPHONE'])
      .then(async (result: PermissionRequestResult) => {
        if (!this.pageActive) { return; }
        let granted = true;
        for (let index = 0; index < result.permissions.length; index++) {
          console.info(TAG,
            "request permission [" + result.permissions[index] + "] with result: " + result.authResults[index]);
          if (result.authResults[index] != abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
            granted = false;
          }
        }

        if (!granted) {
          this.setError("Permission Not Granted");
          return;
        }

        console.info(TAG, "Connecting to server");
        this.setStatus("Connecting to server");
        this.ws.connect(SIGNAL_SERVER, (err, success) => {
        })
      });

    this.ws.on('open', (err, data) => {
      if (!this.pageActive) { return; }
      if (this.reconnectTimer >= 0) { clearTimeout(this.reconnectTimer); this.reconnectTimer = -1; }
      console.info(TAG, 'websocket open');
      this.myId = this.getRandomId();
      this.sendMessage("HELLO " + this.myId);
      this.setStatus("Registering with server");
    });

    this.ws.on('close', (err, data: webSocket.CloseResult) => {
      if (!this.pageActive) { return; }
      console.info(TAG, 'websocket closed');
      this.setStatus('Disconnected from server');

      this.peerGeneration++;
      this.resetSei();
      this.stopStatsPolling();
      this.buttonTitle = 'Connect';
      this.canChangeResolution = false;
      this.pc?.close();
      this.pc = undefined;

      this.scheduleReconnect();
    });

    this.ws.on('error', (err) => {
      if (!this.pageActive) { return; }
      console.info(TAG, 'websocket err: ' + err.code);
      this.setError("Unable to connect to server: " + err.message);

      this.scheduleReconnect();
    });

    this.ws.on('message', (err, data: string | ArrayBuffer) => {
      if (!this.pageActive) { return; }
      console.info(TAG, 'websocket receive message: ' + data);
      if (data === 'HELLO') {
        this.setStatus("Registered with server, waiting for call");
        this.createPeerConnection();
      } else if (data === 'SESSION_OK') {
        this.setStatus("Starting negotiation");
        this.sendOffer();
      } else if (data === 'OFFER_REQUEST') {
        this.sendOffer();
      } else {
        if (data.toString().startsWith('ERROR')) {
          this.handleIncomingError(data.toString());
        } else {
          try {
            let msg = JSON.parse(data.toString()) as MessageData;
            if (msg && msg.sdp) {
              this.receiveSdp(msg.sdp);
            } else if (msg && msg.ice) {
              this.receiveIce(msg.ice);
            } else {
              console.error(TAG, 'receive unknown message: ' + data);
              this.handleIncomingError("Unknown incoming JSON");
            }
          } catch (error) {
            console.error(TAG, 'receive invalid message: ' + data);
          }
        }
      }
    });

    this.createAudioDeviceModule();
    this.createPeerConnectionFactory();
  }

  aboutToDisappear(): void {
    this.pageActive = false;
    this.pageVisible = false;
    this.peerGeneration++;
    if (this.reconnectTimer >= 0) { clearTimeout(this.reconnectTimer); this.reconnectTimer = -1; }
    this.resetSei();
    this.stopStatsPolling();
    this.pcf?.stopAecDump();
    Logging.deleteInjectedLoggable();

    this.pcf = undefined;

    this.adm?.release();
    this.adm = undefined;

    this.pc?.close();
    this.pc = undefined;

    this.localAudioSource?.release();
    this.localAudioSource = undefined;
    this.localVideoSource?.release();
    this.localVideoSource = undefined;

    this.localAudioTrack?.stop();
    this.localAudioTrack = undefined;
    this.localAudioTrack2?.stop();
    this.localAudioTrack2 = undefined;
    this.localVideoTrack?.stop();
    this.localVideoTrack = undefined;

    this.ws.close();

    this.ws.off('open');
    this.ws.off('close');
    this.ws.off('error');
    this.ws.off('message');
  }

  onPageShow(): void {
    this.pageVisible = true;
    if (this.pc?.connectionState === 'connected') { this.startSeiPolling(); }
    console.info(TAG, 'onPageShow');
  }

  onPageHide(): void {
    this.pageVisible = false;
    this.pauseSei();
    console.info(TAG, 'onPageHide');
  }

  private scheduleReconnect(): void {
    if (!this.pageActive || this.reconnectTimer >= 0) { return; }
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = -1;
      if (this.pageActive) { this.ws.connect(SIGNAL_SERVER); }
    }, 1000);
  }

  private showSeiError(message: string): void {
    // Do not put application-supplied SEI text or bytes into logs.
    console.error(TAG, 'SEI operation failed');
    try { promptAction.showToast({ message: message, duration: 3000 }); }
    catch (error) { console.error(TAG, 'Failed to show SEI error'); }
  }

  private stopContinuousSei(): void {
    if (this.seiSendTimer >= 0) { clearInterval(this.seiSendTimer); this.seiSendTimer = -1; }
    this.seiContinuous = false;
  }

  private pauseSei(): void {
    this.stopContinuousSei();
    if (this.seiReadyTimer >= 0) { clearInterval(this.seiReadyTimer); this.seiReadyTimer = -1; }
    this.seiReady = false;
    try { this.seiEncoder?.clearSei(); }
    catch (error) { console.error(TAG, 'Failed to clear pending SEI'); }
  }

  private resetSei(): void {
    this.pauseSei();
    if (this.seiReceiver) {
      try { this.seiReceiver.onsei = null; }
      catch (error) { console.error(TAG, 'Failed to detach SEI receiver'); }
    }
    this.seiReceiver = undefined;
    this.seiEncoder = undefined;
    this.receivedSeiText = '';
    this.receivedSeiSequence = '';
    this.receivedSeiBinary = '';
  }

  private startSeiPolling(): void {
    if (this.seiReadyTimer >= 0) { clearInterval(this.seiReadyTimer); this.seiReadyTimer = -1; }
    const peer = this.pc;
    if (!this.pageVisible || !peer || !this.seiEncoder) { return; }
    this.seiReadyTimer = setInterval(() => {
      if (this.pc !== peer) { return; }
      if (!this.pageVisible) { this.pauseSei(); return; }
      this.seiReady = peer.connectionState === 'connected' && (this.seiEncoder?.ready ?? false);
      if (!this.seiReady && this.seiContinuous) { this.stopContinuousSei(); }
    }, 200);
  }

  private configureSeiSender(transceiver: webrtc.RTCRtpTransceiver): void {
    if (typeof webrtc.EncoderWrapper === 'undefined') { return; }
    try {
      const codecs = webrtc.RTCRtpSender.getCapabilities('video')?.codecs ?? [];
      const preferred = codecs.filter((codec: webrtc.RTCRtpCodec) =>
        codec.mimeType.toLowerCase() === 'video/h264' &&
        (codec.sdpFmtpLine ?? '').split(';').some((part: string) => part.trim() === 'packetization-mode=1'));
      if (preferred.length > 0) {
        // Prefer SEI-capable H.264 while retaining other codecs for ordinary calls.
        transceiver.setCodecPreferences(preferred.concat(codecs.filter(
          (codec: webrtc.RTCRtpCodec) => !preferred.includes(codec))));
      }
      this.seiEncoder = transceiver.sender.getEncoderWrapper();
    } catch (error) {
      this.seiEncoder = undefined;
      this.showSeiError('SEI 发送初始化失败');
    }
  }

  private bindSeiReceiver(peer: webrtc.RTCPeerConnection, receiver: webrtc.RTCRtpReceiver): void {
    if (typeof webrtc.EncoderWrapper === 'undefined') { return; }
    try {
      if (this.seiReceiver) { this.seiReceiver.onsei = null; }
      this.seiReceiver = receiver;
      receiver.onsei = (event: webrtc.RTCSeiEvent): void => {
        if (this.pageVisible && this.pc === peer && this.seiReceiver === receiver) { this.receiveSei(event); }
      };
    } catch (error) { this.showSeiError('SEI 接收初始化失败'); }
  }

  private sendSei(): void {
    if (!this.pageVisible || this.pc?.connectionState !== 'connected' || !this.seiEncoder?.ready) {
      throw new Error('请先建立 H.264 视频通话');
    }
    const binary = new Uint8Array(SEI_BINARY_BYTES);
    for (let index = 0; index < SEI_BINARY_BYTES; index++) {
      const value = this.seiBinaryInputs[index];
      if (!/^[0-9a-fA-F]{2}$/.test(value)) {
        throw new Error(`第 ${index + 1} 个字节请输入两位十六进制数（00–FF）`);
      }
      binary[index] = parseInt(value, 16);
    }
    const text = new util.TextEncoder().encodeInto(this.seiText);
    const length = SEI_SEQUENCE_BYTES + text.length + SEI_BINARY_BYTES;
    if (length > webrtc.EncoderWrapper.maxDataBytes) { throw new Error('文本过长'); }
    const payload = new Uint8Array(length);
    const sequence = (this.seiSequence + 1) >>> 0;
    new DataView(payload.buffer).setUint32(0, sequence);
    payload.set(text, SEI_SEQUENCE_BYTES);
    payload.set(binary, SEI_SEQUENCE_BYTES + text.length);
    this.seiEncoder.writeSei(payload.buffer);
    this.seiSequence = sequence;
  }

  private startContinuousSei(): void {
    if (this.seiContinuous) { return; }
    try {
      this.sendSei();
      this.seiContinuous = true;
      this.seiSendTimer = setInterval(() => {
        try { this.sendSei(); }
        catch (error) {
          this.stopContinuousSei();
          this.showSeiError((error as Error).message);
        }
      }, 200);
    } catch (error) { this.showSeiError((error as Error).message); }
  }

  private receiveSei(event: webrtc.RTCSeiEvent): void {
    if (event.uuid !== webrtc.EncoderWrapper.defaultUuid) { return; }
    this.receivedSeiText = '';
    this.receivedSeiSequence = '';
    this.receivedSeiBinary = '';
    try {
      const bytes = new Uint8Array(event.data);
      if (bytes.length < SEI_SEQUENCE_BYTES + SEI_BINARY_BYTES) {
        throw new Error('收到的 SEI 长度不足');
      }
      const textEnd = bytes.length - SEI_BINARY_BYTES;
      const text = util.TextDecoder.create('utf-8', { fatal: true })
        .decodeToString(bytes.subarray(SEI_SEQUENCE_BYTES, textEnd));
      const binary: string[] = [];
      for (let index = textEnd; index < bytes.length; index++) {
        binary.push(bytes[index].toString(16).toUpperCase().padStart(2, '0'));
      }
      this.receivedSeiText = text.length > 0 ? text : '（空文本）';
      this.receivedSeiSequence = new DataView(event.data).getUint32(0).toString();
      this.receivedSeiBinary = binary.join(' ');
    } catch (error) { this.showSeiError('收到的 SEI 不符合本测试的数据格式'); }
  }

  getRandomId() {
    return Math.floor(Math.random() * (9000 - 10) + 10).toString();
  }

  setStatus(text: string) {
    console.info(TAG, text);
    this.statusText = text;
  }

  setError(error: string) {
    console.error(TAG, error);
    this.statusText = error;
  }

  handleIncomingError(error: string) {
    this.setError('ERROR: ' + error);
    this.ws.close();
  }

  async createAudioDeviceModule() {
    this.adm = new webrtc.AudioDeviceModule({ useStereoInput: true, useStereoOutput: true });
    this.adm.oncapturererror = (event) => {
      console.error(TAG, 'oncapturererror: ' + JSON.stringify(event));
    };
    this.adm.oncapturerstatechange = (event) => {
      console.info(TAG, 'oncapturerstatechange: ' + JSON.stringify(event));
    };
    this.adm.oncapturersamplesready = (event) => {
      console.debug(TAG, 'oncapturersamplesready: ' + JSON.stringify(event));
    };
    this.adm.onrenderererror = (event) => {
      console.error(TAG, 'onrenderererror: ' + JSON.stringify(event));
    };
    this.adm.onrendererstatechange = (event) => {
      console.info(TAG, 'onrendererstatechange: ' + JSON.stringify(event));
    };
    Logging.d(TAG, 'this.adm: ' + JSON.stringify(this.adm));
  }

  async createPeerConnectionFactory() {
    // 勿仅指定 HardwareVideoEncoder/DecoderFactory：模拟器无 video/avc 能力时
    // 接收方向 codec 列表为空，会导致 setLocalDescription 失败。
    this.pcf = new webrtc.PeerConnectionFactory({
      adm: this.adm
    });
    Logging.d(TAG, 'this.pcf: ' + JSON.stringify(this.pcf));

    // MediaDevices 依赖默认的 PeerConnnectionFactory，且需要与创建 PeerConnection 的是同一个。
    webrtc.PeerConnectionFactory.setDefault(this.pcf);

    let filesDir = getContext().filesDir;
    let file = fs.openSync(filesDir + '/test.txt', fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
    this.pcf.startAecDump(file.fd, -1);
  }

  // 手机首选后置摄像头（environment）；PC/模拟器无后置时回退为不限定 facingMode。
  createVideoSourceWithFallback(width: number, height: number): webrtc.VideoSource | undefined {
    let src: webrtc.VideoSource | undefined;
    try {
      src = this.pcf?.createVideoSource({ width, height, facingMode: 'environment' }, false);
    } catch (e) {
      console.warn(TAG, `createVideoSource(environment) threw: ${JSON.stringify(e)}`);
    }
    if (!src) {
      console.warn(TAG, 'environment facingMode unsatisfied, retry without facingMode');
      try {
        src = this.pcf?.createVideoSource({ width, height }, false);
      } catch (e) {
        console.error(TAG, `createVideoSource(fallback) threw: ${JSON.stringify(e)}`);
      }
    }
    return src;
  }

  async prepareLocalMedia() {
    if (!this.localAudioTrack) {
      this.localAudioSource = this.pcf?.createAudioSource({ echoCancellation: true, noiseSuppression: true });
      console.info(TAG, 'audioSource: ' + JSON.stringify(this.localAudioSource));

      this.localAudioTrack = this.pcf?.createAudioTrack("audio", this.localAudioSource);
      console.info(TAG, 'audioTrack: ' + JSON.stringify(this.localAudioTrack));
    }

    if (!this.localVideoTrack) {
      this.localVideoSource = this.createVideoSourceWithFallback(1280, 720);
      console.info(TAG, 'videoSource: ' + JSON.stringify(this.localVideoSource));
      if (!this.localVideoSource) {
        // 摄像头不可用（API18 以下模拟器等），降级为 audio-only 模式。
        this.setStatus('No camera available, audio-only mode');
        this.mXComponentController1.setVideoTrack(null);
        return;
      }
      this.localVideoSource.oncapturerstarted = (event: webrtc.VideoCapturerStartedEvent) => {
        console.info(TAG, 'oncapturerstarted: event=' + JSON.stringify(event));
      };
      this.localVideoSource.oncapturerstopped = (event: webrtc.Event) => {
        console.info(TAG, 'oncapturerstopped: event=' + JSON.stringify(event));
      };

      this.localVideoTrack = this.pcf?.createVideoTrack("video", this.localVideoSource);
      console.info(TAG, 'videoTrack: ' + JSON.stringify(this.localVideoTrack));

      if (this.localVideoTrack) {
        this.mXComponentController1.setVideoTrack(this.localVideoTrack);
        this.canChangeResolution = true;
      }

      this.mXComponentController1.setMirror(false);
      this.mXComponentController1.setScalingMode(ScalingMode.AspectFit);
    }
  }

  async prepareDisplayMedia() {
    if (!this.displayMediaStream) {
      this.displayMediaStream = await this.mediaDevices.getDisplayMedia({
        video: {
          width: 720,
          height: 1280,
          ohosScreenCaptureAudioFilter: 'current-app', // 过滤当前应用播放的音频
        },
        audio: false, // 系统声音不捕获
        ohosSystemAudio: true
      });

      this.displayMediaStream.getTracks()
        .forEach((track) => console.info(TAG, 'display media Stream track: ' + JSON.stringify(track)));

      this.localVideoTrack = this.displayMediaStream.getVideoTracks()[0];
      console.info(TAG, 'videoTrack: ' + JSON.stringify(this.localVideoTrack));

      if (this.localVideoTrack) {
        let videoSource = this.localVideoTrack.getSource();
        if (videoSource) {
          this.localVideoSource = videoSource as webrtc.VideoSource;
          console.info(TAG, 'videoSource: ' + JSON.stringify(this.localVideoSource));
          this.localVideoSource!.oncapturerstarted = (event: webrtc.VideoCapturerStartedEvent) => {
            console.info(TAG, 'oncapturerstarted: event=' + JSON.stringify(event));
          };
          this.localVideoSource!.oncapturerstopped = (event: webrtc.Event) => {
            console.info(TAG, 'oncapturerstopped: event=' + JSON.stringify(event));
          };
        }

        this.mXComponentController1.setVideoTrack(this.localVideoTrack);
        this.mXComponentController1.setScalingMode(ScalingMode.AspectFit);
      }

      let audioTracks = this.displayMediaStream.getAudioTracks();
      if (audioTracks.length > 0) {
        this.localAudioTrack = audioTracks[0];
        console.info(TAG, 'audioTrack: ' + JSON.stringify(this.localAudioTrack));
      }
      if (audioTracks.length > 1) {
        this.localAudioTrack2 = audioTracks[1];
        console.info(TAG, 'audioTrack2: ' + JSON.stringify(this.localAudioTrack2));
      }
    }
  }

  async createPeerConnection() {
    if (!this.pageActive) { return; }
    const generation = ++this.peerGeneration;
    this.resetSei();
    this.stopStatsPolling();
    this.pc?.close();
    this.pc = undefined;
    // 设置捕获相机画面
    await this.prepareLocalMedia();
    if (!this.pageActive || generation !== this.peerGeneration) { return; }
    // 设置捕获设备桌面
    // await this.prepareDisplayMedia();

    this.pc = this.pcf?.createPeerConnection({
      iceServers: [{ urls: STUN_SERVER }]
    });

    if (!this.pc) {
      console.error(TAG, 'failed to create RTCPeerConnection');
      return;
    }

    const peer = this.pc;
    this.pc.ontrack = (event: webrtc.RTCTrackEvent) => {
      if (this.pc !== peer) { return; }
      console.info(TAG, 'ontrack: ' + JSON.stringify(event));
      if (event.track.kind == 'video') {
        this.bindSeiReceiver(peer, event.receiver);
        console.info(TAG, 'Received remote video track, setting to renderer');
        this.remoteVideoTrack = event.track;
        // 确保每次收到远端视频轨道时都设置到渲染器
        this.mXComponentController2.setVideoTrack(event.track);
        this.mXComponentController2.setScalingMode(ScalingMode.AspectFit);
      } else if (event.track.kind == 'audio') {
        this.remoteAudioTrack = event.track;
      }
    };
    this.pc.onsignalingstatechange = (event) => {
      if (this.pc !== peer) { return; }
      console.info(TAG, 'onsignalingstatechange: ' + this.pc?.signalingState);
    }
    this.pc.onconnectionstatechange = (event) => {
      if (this.pc !== peer) { return; }
      console.info(TAG, 'onconnectionstatechange: ' + this.pc?.connectionState);
      if (this.pc?.connectionState == 'connected') {
        this.startSeiPolling();
        // 连接建立后开始获取统计信息
        this.startStatsPolling();
        // 确保按钮可用（如果视频轨道已创建）
        if (this.localVideoTrack) {
          this.canChangeResolution = true;
        }
      } else if (this.pc?.connectionState == 'failed') {
        this.pauseSei();
        this.setError("Peer connection failed");
        this.ws.close();
        this.stopStatsPolling();
        this.canChangeResolution = false;
      } else if (this.pc?.connectionState == 'closed' || this.pc?.connectionState == 'disconnected') {
        this.pauseSei();
        this.stopStatsPolling();
        this.canChangeResolution = false;
      }
    }
    this.pc.onnegotiationneeded = (event) => {
      if (this.pc !== peer) { return; }
      // this is Index
      console.info(TAG, 'onnegotiationneeded: this=' + JSON.stringify(this.pc));
      console.info(TAG, 'onnegotiationneeded: event=' + JSON.stringify(event));
      // 当需要重新协商时（如切换分辨率），自动发送 offer
      // 但只在连接已建立且处于稳定状态时发送
      if (this.pc && this.pc.signalingState === 'stable' && this.pc.connectionState === 'connected') {
        console.info(TAG, 'Auto sending offer due to negotiation needed');
        this.sendOffer();
      }
    }

    this.pc.onicecandidate = (event) => {
      if (this.pc !== peer) { return; }
      console.info(TAG, 'onicecandidate: event=' + JSON.stringify(event));
      if (event.candidate) {
        this.sendMessage(JSON.stringify({ ice: event.candidate }));
      }
    };
    this.pc.onicecandidateerror = (event) => {
      if (this.pc !== peer) { return; }
      console.error(TAG, 'onicecandidateerror: event=' + JSON.stringify(event));
    };
    this.pc.oniceconnectionstatechange = (event) => {
      if (this.pc !== peer) { return; }
      console.info(TAG, 'oniceconnectionstatechange: ' + this.pc?.iceConnectionState);
      if (this.pc?.iceConnectionState === 'failed') {
        this.setError("Ice connection failed");
        this.ws.close();
      }
    };
    this.pc.onicegatheringstatechange = (event) => {
      if (this.pc !== peer) { return; }
      console.info(TAG, 'onicegatheringstatechange: ' + this.pc?.iceGatheringState);
      switch (this.pc?.iceGatheringState) {
        case "new":
          console.info(TAG, 'onicegatheringstatechange: gathering is either just starting or has been reset');
          break;
        case "gathering":
          console.info(TAG, 'onicegatheringstatechange: gathering has begun or is ongoing');
          break;
        case "complete":
          console.info(TAG, 'onicegatheringstatechange: gathering has ended');
          break;
      }
    };
    this.pc.ondatachannel = (event) => {
      if (this.pc !== peer) { return; }
      console.info(TAG, 'ondatachannel: event=' + JSON.stringify(event));
      this.setStatus("Data channel created");
      this.recvChannel = event.channel;
      this.recvChannel.onopen = this.onDataChannelOpen;
      this.recvChannel.onmessage = this.onDataChannelMessageReceived;
      this.recvChannel.onerror = this.onDataChannelError;
      this.recvChannel.onclose = this.onDataChannelClose;
    }

    if (this.localAudioTrack) {
      let rtpSender1 = this.pc.addTrack(this.localAudioTrack);
      console.info(TAG, 'rtpSender1: ' + JSON.stringify(rtpSender1));
    }
    if (this.localAudioTrack2) {
      let rtpSender2 = this.pc.addTrack(this.localAudioTrack2);
      console.info(TAG, 'rtpSender2: ' + JSON.stringify(rtpSender2));
    }
    if (this.localVideoTrack) {
      const transceiver = this.pc.addTransceiver(this.localVideoTrack, { direction: 'sendrecv' });
      this.configureSeiSender(transceiver);
      let rtpSender3 = transceiver.sender;
      console.info(TAG, 'rtpSender3: ' + JSON.stringify(rtpSender3));
    }

    console.info(TAG, 'transceivers: ' + JSON.stringify(this.pc.getTransceivers()));

    this.sendChannel = this.pc.createDataChannel("send");
    console.info(TAG, 'sendChannel: ' + JSON.stringify(this.sendChannel));
    this.sendChannel.onopen = (event) => {
      // this is Index
      console.info(TAG, "=dataChannel.OnOpen: ", event);

      this.sendChannel?.send("Hi! (from ArkTS)");
      let data1 = buffer.alloc(20, 0);
      this.sendChannel?.send(data1.buffer);

      let data2 = new Int32Array(new ArrayBuffer(20));
      data2.fill(1);
      this.sendChannel?.send(data2.buffer);

      let data3 = new Uint8Array(20);
      data3.fill(2);
      this.sendChannel?.send(data3.buffer);
    };
    this.sendChannel.onmessage = this.onDataChannelMessageReceived;
    this.sendChannel.onerror = this.onDataChannelError;
    this.sendChannel.onclose = this.onDataChannelClose;
  }

  onDataChannelOpen(event: webrtc.Event) {
    // this is webrtc.RTCDataChannel
    console.info(TAG, "dataChannel.OnOpen: ", event);
    if (this instanceof webrtc.RTCDataChannel) {
      this.send("Hi! (from ArkTS)");
    }
  }

  async onDataChannelError(event: webrtc.Event) {
    // this is webrtc.RTCDataChannel
    console.info(TAG, "dataChannel.OnError: ", event);
  }

  async onDataChannelClose(event: webrtc.Event) {
    // this is webrtc.RTCDataChannel
    console.info(TAG, "dataChannel.OnClose", event);
  }

  async sendOffer() {
    try {
      let offer = await this.pc?.createOffer();
      console.info(TAG, 'createOffer: ' + JSON.stringify(offer));

      this.pc?.setLocalDescription(offer);

      this.sendMessage(JSON.stringify({ sdp: offer }));
    } catch (e) {
      console.error(TAG, 'sendOffer: ' + JSON.stringify(e));
    }
  }

  async sendAnswer() {
    this.setStatus("Got SDP offer, creating answer");

    this.pc?.createAnswer().then((desc) => {
      console.info(TAG, 'Success to create answer: ' + JSON.stringify(desc));
      this.pc?.setLocalDescription(desc).then(() => {
        this.setStatus("Sending SDP answer");
        this.sendMessage(JSON.stringify({ sdp: this.pc?.localDescription }));
      });
    }).catch((e: Error) => {
      console.error(TAG, 'Failed to create answer: ' + e.message);
      this.setError(e.message);
    });
  }

  async sendMessage(message: string) {
    console.info(TAG, 'websocket send: ' + message);
    this.ws.send(message, (error, success) => {
      if (success) {
        console.info(TAG, 'websocket send success: ' + message);
      } else {
        console.error(TAG,
          'websocket send error: ' + error);
      }
    });
  }

  async receiveSdp(sdp: webrtc.RTCSessionDescriptionInit) {
    if (sdp.type === 'offer') {
      console.info(TAG, 'receive offer');
      this.pc?.setRemoteDescription(sdp).then(() => {
        this.setStatus("Remote SDP set");
        this.sendAnswer();
      }).catch((e: Error) => {
        console.error(TAG, "Set remote description error: " + e);
        this.setError(e.message);
      });
    } else if (sdp.type === 'answer') {
      console.info(TAG, 'receive answer');
      this.pc?.setRemoteDescription(sdp).then(() => {
        this.setStatus("Remote SDP set");
        // 确保远端视频轨道在重新协商后仍然显示
        if (this.remoteVideoTrack) {
          console.info(TAG, 'Re-setting remote video track after answer');
          this.mXComponentController2.setVideoTrack(this.remoteVideoTrack);
          this.mXComponentController2.setScalingMode(ScalingMode.AspectFit);
        }
      }).catch((e: Error) => {
        console.error(TAG, "Set remote description error: " + e);
        this.setError(e.message);
      });
    }
  }

  async receiveIce(ice: webrtc.RTCIceCandidateInit) {
    console.info(TAG, 'receive icecandidate');
    this.pc?.addIceCandidate(ice).catch((e: Error) => {
      console.error(TAG, "add ice candidate error: " + e);
      this.setError(e.message);
    });
  }

  async changeResolution(width: number, height: number): Promise<void> {
    if (!this.pc || !this.localVideoTrack) {
      console.error(TAG, "Cannot change resolution: peer connection or video track not available");
      return;
    }

    const label = `${width} x ${height}`;
    console.info(TAG, `Changing resolution to ${label}`);

    const prevVideoTrack = this.localVideoTrack;
    const prevVideoSource = this.localVideoSource;
    let newVideoSource: webrtc.VideoSource | undefined = undefined;
    let newVideoTrack: webrtc.VideoTrack | undefined = undefined;
    let senderSwappedToNew = false;

    try {
      const senders = this.pc.getSenders();
      const videoSender = senders.find(sender => sender.track && sender.track.kind === 'video');

      if (!videoSender) {
        console.error(TAG, "Video sender not found");
        return;
      }

      newVideoSource = this.createVideoSourceWithFallback(width, height);

      if (!newVideoSource) {
        console.error(TAG, "Failed to create new video source");
        return;
      }

      newVideoSource.oncapturerstarted = (event: webrtc.VideoCapturerStartedEvent) => {
        console.info(TAG, 'oncapturerstarted: event=' + JSON.stringify(event));
      };
      newVideoSource.oncapturerstopped = (event: webrtc.Event) => {
        console.info(TAG, 'oncapturerstopped: event=' + JSON.stringify(event));
      };

      newVideoTrack = this.pcf?.createVideoTrack("video", newVideoSource);
      if (!newVideoTrack) {
        console.error(TAG, "Failed to create new video track");
        newVideoSource.release();
        return;
      }

      await videoSender.replaceTrack(newVideoTrack);
      senderSwappedToNew = true;

      this.mXComponentController1.setVideoTrack(newVideoTrack);
      this.mXComponentController1.setMirror(false);
      this.mXComponentController1.setScalingMode(ScalingMode.AspectFit);

      this.localVideoSource = newVideoSource;
      this.localVideoTrack = newVideoTrack;
    } catch (error) {
      newVideoTrack?.stop();
      newVideoSource?.release();
      if (senderSwappedToNew && this.pc) {
        try {
          const senders = this.pc.getSenders();
          const rollbackSender = senders.find(sender => sender.track && sender.track.kind === 'video');
          if (rollbackSender) {
            await rollbackSender.replaceTrack(prevVideoTrack);
          }
        } catch (rollbackErr) {
          console.error(TAG, "replaceTrack rollback failed: " + rollbackErr);
        }
      }
      console.error(TAG, "Error changing resolution: " + error);
      this.setError("Failed to change resolution: " + error);
      return;
    }

    try {
      prevVideoTrack.stop();
    } catch (stopErr) {
      console.error(TAG, "stop previous video track failed: " + stopErr);
    }
    if (prevVideoSource !== undefined) {
      try {
        prevVideoSource.release();
      } catch (relErr) {
        console.error(TAG, "release previous video source failed: " + relErr);
      }
    }

    console.info(TAG, `Resolution changed to ${label}`);
  }

  startStatsPolling() {
    this.stopStatsPolling();

    this.updateStats();

    this.statsTimer = setInterval(() => {
      this.updateStats();
    }, 2000) as number;
  }

  stopStatsPolling() {
    if (this.statsTimer !== undefined) {
      clearInterval(this.statsTimer);
      this.statsTimer = undefined;
    }
  }

  async updateStats() {
    if (!this.pc) {
      return;
    }

    try {
      const statsReport = await this.pc.getStats();

      let localWidth = 0;
      let localHeight = 0;

      let remoteWidth = 0;
      let remoteHeight = 0;

      statsReport.stats.forEach((stats, id) => {
        if (stats.type === 'outbound-rtp') {
          const outboundStats = stats as RtpStats;
          if (outboundStats.kind === 'video') {
            const width = outboundStats.frameWidth;
            const height = outboundStats.frameHeight;
            if (width && height && width > 0 && height > 0) {
              localWidth = width;
              localHeight = height;
            }
          }
        }
        if (stats.type === 'inbound-rtp') {
          const inboundStats = stats as RtpStats;
          if (inboundStats.kind === 'video') {
            const width = inboundStats.frameWidth;
            const height = inboundStats.frameHeight;
            if (width && height && width > 0 && height > 0) {
              remoteWidth = width;
              remoteHeight = height;
            }
          }
        }
      });

      if (localWidth > 0 && localHeight > 0) {
        this.localResolution = `${localWidth}x${localHeight}`;
      } else {
        this.localResolution = '未获取';
      }

      if (remoteWidth > 0 && remoteHeight > 0) {
        this.remoteResolution = `${remoteWidth}x${remoteHeight}`;
      } else {
        this.remoteResolution = '未获取';
      }
    } catch (error) {
      console.error(TAG, "Error getting stats: " + error);
    }
  }
}
