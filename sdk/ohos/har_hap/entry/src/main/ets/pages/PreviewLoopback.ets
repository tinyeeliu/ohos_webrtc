/**
 * Copyright (c) 2026 Archermind Technology (Nanjing) Co. Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import webrtc, { VideoRenderController, ScalingMode } from '@ohos/webrtc';

/** Demo-only encode/decode verification. No signaling server or STUN/TURN service. */
export class PreviewLoopback {
  public onError?: (message: string) => void;
  public onSei?: (event: webrtc.RTCSeiEvent) => void;
  public onFrameSizeChanged?: (width: number, height: number) => void;
  public encoder?: webrtc.EncoderWrapper;
  get connected(): boolean {
    return this.sender?.connectionState === 'connected' && this.receiver?.connectionState === 'connected';
  }
  private sender?: webrtc.RTCPeerConnection;
  private receiver?: webrtc.RTCPeerConnection;
  private generation: number = 0;
  private timeout: number = -1;
  private pendingSender: webrtc.RTCIceCandidateInit[] = [];
  private pendingReceiver: webrtc.RTCIceCandidateInit[] = [];
  private senderReady: boolean = false;
  private receiverReady: boolean = false;
  private controller: VideoRenderController;

  constructor(controller: VideoRenderController) {
    this.controller = controller;
    controller.enableCameraPreviewAdaptation();
    controller.setScalingMode(ScalingMode.AspectFit);
    controller.setMirror(false);
  }

  stop(): void {
    this.generation++;
    this.encoder = undefined;
    if (this.timeout >= 0) { clearTimeout(this.timeout); this.timeout = -1; }
    this.controller.onFrameSizeChanged = undefined;
    this.controller.setVideoTrack(null);
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
  }

  async start(factory: webrtc.PeerConnectionFactory, track: webrtc.MediaStreamTrack,
    enableSei: boolean = false): Promise<void> {
    this.stop();
    const generation = this.generation;
    const fail = (): void => {
      if (generation !== this.generation) { return; }
      this.stop();
      this.onError?.('编解码验证未完成，请检查连接、编码器和设备日志后重试');
    };
    try {
      const sender = factory.createPeerConnection({ iceServers: [] });
      this.sender = sender;
      const receiver = factory.createPeerConnection({ iceServers: [] });
      this.receiver = receiver;
      this.controller.onFrameSizeChanged = (width: number, height: number): void => {
        if (generation !== this.generation) { return; }
        if (this.timeout >= 0) { clearTimeout(this.timeout); this.timeout = -1; }
        this.onFrameSizeChanged?.(width, height);
      };
      receiver.ontrack = (event: webrtc.RTCTrackEvent): void => {
        if (generation === this.generation && event.track.kind === 'video') {
          if (enableSei) {
            event.receiver.onsei = (sei: webrtc.RTCSeiEvent): void => {
              if (generation === this.generation) { this.onSei?.(sei); }
            };
          }
          this.controller.setVideoTrack(event.track);
        }
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
      this.timeout = setTimeout(fail, 15000);
      if (enableSei) {
        const codecs = webrtc.RTCRtpSender.getCapabilities('video')?.codecs.filter(
          (codec: webrtc.RTCRtpCodec) => codec.mimeType.toLowerCase() === 'video/h264' &&
            (codec.sdpFmtpLine ?? '').split(';').some((part: string) => part.trim() === 'packetization-mode=1')) ?? [];
        if (codecs.length === 0) { throw new Error('当前设备或构建没有 H.264 packetization-mode=1 编码能力'); }
        const transceiver = sender.addTransceiver(track, { direction: 'sendonly' });
        transceiver.setCodecPreferences(codecs);
        this.encoder = transceiver.sender.getEncoderWrapper();
      } else {
        sender.addTrack(track);
      }
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
    } catch (e) {
      if (generation !== this.generation) { return; }
      this.stop();
      this.onError?.(`回环失败：${(e as Error).message}`);
    }
  }
}
