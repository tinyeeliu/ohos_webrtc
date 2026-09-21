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

import webrtc from '@ohos/webrtc';

/**
 * 视频回环观测指标。数值字段为 null 表示本次采样 stats 未上报该字段,
 * 界面据此显示 '-' 而不是误导性的 0;字符串状态字段始终来自 PC 实时状态。
 */
export interface LoopbackMetrics {
  connState: string;
  sigState: string;
  gatherState: string;
  iceSender: string;
  iceReceiver: string;
  pairState: string;
  pairNominated: string;
  pairLocal: string;
  pairRemote: string;
  rttMs: number | null;
  availOutKbps: number | null;
  outFps: number | null;
  outWidth: number | null;
  outHeight: number | null;
  outBitrateKbps: number | null;
  outBytesSent: number | null;
  inFps: number | null;
  inWidth: number | null;
  inHeight: number | null;
  inPacketsLost: number | null;
  inJitterMs: number | null;
  inBitrateKbps: number | null;
  inBytesReceived: number | null;
  outboundRaw: string;
  inboundRaw: string;
  sampleCount: number;
}

export function emptyMetrics(): LoopbackMetrics {
  return {
    connState: 'new',
    sigState: 'stable',
    gatherState: 'new',
    iceSender: 'new',
    iceReceiver: 'new',
    pairState: '-',
    pairNominated: '-',
    pairLocal: '-',
    pairRemote: '-',
    rttMs: null,
    availOutKbps: null,
    outFps: null,
    outWidth: null,
    outHeight: null,
    outBitrateKbps: null,
    outBytesSent: null,
    inFps: null,
    inWidth: null,
    inHeight: null,
    inPacketsLost: null,
    inJitterMs: null,
    inBitrateKbps: null,
    inBytesReceived: null,
    outboundRaw: '',
    inboundRaw: '',
    sampleCount: 0,
  };
}

/** 字节差分码率:首次采样只建基线不产出码率,之后按 (字节差*8)/耗时 计算。 */
class RateAccumulator {
  private lastBytes: number = -1;
  private lastTs: number = 0;

  reset(): void {
    this.lastBytes = -1;
    this.lastTs = 0;
  }

  kbps(bytes: number | null, now: number): number | null {
    if (bytes === null) {
      return null;
    }
    let result: number | null = null;
    if (this.lastBytes >= 0 && now > this.lastTs && bytes >= this.lastBytes) {
      result = Math.round(((bytes - this.lastBytes) * 8) / (now - this.lastTs));
    }
    this.lastBytes = bytes;
    this.lastTs = now;
    return result;
  }
}

/** 帧计数差分帧率:stats 未上报 framesPerSecond 时用帧计数的差分兜底。 */
class FpsAccumulator {
  private lastFrames: number = -1;
  private lastTs: number = 0;

  reset(): void {
    this.lastFrames = -1;
    this.lastTs = 0;
  }

  fps(frames: number | null, now: number): number | null {
    if (frames === null) {
      return null;
    }
    let result: number | null = null;
    if (this.lastFrames >= 0 && now > this.lastTs && frames >= this.lastFrames) {
      result = Math.round(((frames - this.lastFrames) * 1000) / (now - this.lastTs));
    }
    this.lastFrames = frames;
    this.lastTs = now;
    return result;
  }
}

/**
 * 双端视频回环会话:相机采集 → pc1 发送 → pc2 接收。
 * 页面负责渲染(localTrack/remoteTrack)与指标展示(startMetrics 轮询回调)。
 */
export class LoopbackSession {
  private factory?: webrtc.PeerConnectionFactory;
  private pc1?: webrtc.RTCPeerConnection;
  private pc2?: webrtc.RTCPeerConnection;
  private videoSource?: webrtc.VideoSource;
  private pendingFrom1: webrtc.RTCIceCandidateInit[] = [];
  private pendingFrom2: webrtc.RTCIceCandidateInit[] = [];
  private iceTimer = -1;
  private metricsTimer = -1;
  private outRate = new RateAccumulator();
  private inRate = new RateAccumulator();
  private outFpsAcc = new FpsAccumulator();
  private inFpsAcc = new FpsAccumulator();
  private sampleSeq = 0;
  localTrack?: webrtc.MediaStreamTrack;
  remoteTrack?: webrtc.MediaStreamTrack;
  onRemoteTrack?: (track: webrtc.MediaStreamTrack) => void;
  onError?: (stage: string, message: string) => void;

  /** 按配置建链;encoderFactory 验证编解码配置,factoryOptions 验证工厂级选项(如掩码)。 */
  start(encoderFactory?: webrtc.VideoEncoderFactory, constraints?: webrtc.MediaTrackConstraints,
    factoryOptions?: webrtc.PeerConnectionFactoryOptions): boolean {
    const options: webrtc.PeerConnectionFactoryOptions = {};
    if (factoryOptions && factoryOptions.networkIgnoreMask !== undefined) {
      options.networkIgnoreMask = factoryOptions.networkIgnoreMask;
    }
    if (encoderFactory) {
      options.videoEncoderFactory = encoderFactory;
    }
    this.factory = new webrtc.PeerConnectionFactory(options);
    this.pc1 = this.factory.createPeerConnection({ iceServers: [] });
    this.pc2 = this.factory.createPeerConnection({ iceServers: [] });
    this.pc2.ontrack = (ev: webrtc.RTCTrackEvent) => {
      this.remoteTrack = ev.track;
      if (this.onRemoteTrack) {
        this.onRemoteTrack(ev.track);
      }
    };
    this.wireIce(this.pc1, this.pc2, true);
    this.wireIce(this.pc2, this.pc1, false);
    this.startIceFlush();
    this.resetAccumulators();

    const merged: webrtc.MediaTrackConstraints = constraints ?? { facingMode: 'environment' };
    this.videoSource = this.factory.createVideoSource(merged);
    this.localTrack = this.factory.createVideoTrack('loopback-video', this.videoSource);
    this.pc1.addTrack(this.localTrack);
    this.negotiate();
    return this.localTrack !== undefined;
  }

  /** 页面挂 PC 级事件(候选对切换/流增删等)时取连接对象。 */
  peerSender(): webrtc.RTCPeerConnection | undefined
  {
    return this.pc1;
  }

  peerReceiver(): webrtc.RTCPeerConnection | undefined
  {
    return this.pc2;
  }

  /** 周期采样 getStats 并回调指标;码率/兜底帧率按相邻两次采样差分。 */
  startMetrics(intervalMs: number, onMetrics: (m: LoopbackMetrics) => void): void
  {
    this.stopMetrics();
    this.metricsTimer = setInterval(() => {
      this.sample(onMetrics);
    }, intervalMs);
  }

  stopMetrics(): void
  {
    if (this.metricsTimer >= 0) {
      clearInterval(this.metricsTimer);
      this.metricsTimer = -1;
    }
  }

  stop(): void
  {
    this.stopMetrics();
    if (this.iceTimer >= 0) {
      clearInterval(this.iceTimer);
      this.iceTimer = -1;
    }
    this.localTrack?.stop();
    this.remoteTrack?.stop();
    this.videoSource?.release();
    this.pc1?.close();
    this.pc2?.close();
    this.factory = undefined;
  }

  private resetAccumulators(): void
  {
    this.outRate.reset();
    this.inRate.reset();
    this.outFpsAcc.reset();
    this.inFpsAcc.reset();
    this.sampleSeq = 0;
  }

  private fireError(stage: string, message: string): void
  {
    console.error(`[LoopbackSession] ${stage} failed: ${message}`);
    if (this.onError) {
      this.onError(stage, message);
    }
  }

  private wireIce(source: webrtc.RTCPeerConnection, target: webrtc.RTCPeerConnection, fromPc1: boolean): void
  {
    source.onicecandidate = (ev: webrtc.RTCPeerConnectionIceEvent) => {
      if (!ev.candidate) {
        return;
      }
      if (target.signalingState !== 'closed' && target.remoteDescription) {
        target.addIceCandidate(ev.candidate).catch(() => undefined);
      } else if (fromPc1) {
        this.pendingFrom1.push(ev.candidate);
      } else {
        this.pendingFrom2.push(ev.candidate);
      }
    };
  }

  /** remote description 就绪前到达的候选,定期补投(与测试套件同款策略)。 */
  private startIceFlush(): void
  {
    this.iceTimer = setInterval(() => {
      if (this.pc1 && this.pc1.remoteDescription) {
        const list1 = this.pendingFrom1.splice(0);
        list1.forEach((c: webrtc.RTCIceCandidateInit) => this.pc1!.addIceCandidate(c).catch(() => undefined));
      }
      if (this.pc2 && this.pc2.remoteDescription) {
        const list2 = this.pendingFrom2.splice(0);
        list2.forEach((c: webrtc.RTCIceCandidateInit) => this.pc2!.addIceCandidate(c).catch(() => undefined));
      }
    }, 50);
  }

  private async negotiate(): Promise<void>
  {
    try {
      const offer = await this.pc1!.createOffer();
      await this.pc1!.setLocalDescription(offer);
      await this.pc2!.setRemoteDescription(offer);
      const answer = await this.pc2!.createAnswer();
      await this.pc1!.setRemoteDescription(answer);
      await this.pc2!.setLocalDescription(answer);
    } catch (e) {
      this.fireError('SDP 协商', `${e}`);
    }
  }

  private async sample(onMetrics: (m: LoopbackMetrics) => void): Promise<void>
  {
    if (!this.pc1 || !this.pc2) {
      return;
    }
    const metrics = emptyMetrics();
    metrics.connState = this.pc1.connectionState;
    metrics.sigState = this.pc1.signalingState;
    metrics.gatherState = this.pc1.iceGatheringState;
    metrics.iceSender = this.pc1.iceConnectionState;
    metrics.iceReceiver = this.pc2.iceConnectionState;
    metrics.sampleCount = ++this.sampleSeq;
    try {
      const outReport = await this.pc1.getStats();
      this.parseSenderStats(outReport, metrics);
      const inReport = await this.pc2.getStats();
      this.parseReceiverStats(inReport, metrics);
    } catch (e) {
      this.fireError('getStats 采样', `${e}`);
    }
    onMetrics(metrics);
  }

  private parseSenderStats(report: webrtc.RTCStatsReport, metrics: LoopbackMetrics): void
  {
    const now = Date.now();
    let pairLocalId = '';
    let pairRemoteId = '';
    let pairBytesSent: number | null = null;
    report.stats.forEach((stat: ESObject, _key: string) => {
      const type = stat.type as string;
      if (type === 'candidate-pair' && (stat.state as string) === 'succeeded' && pairLocalId === '') {
        pairLocalId = (stat.localCandidateId as string) ?? '';
        pairRemoteId = (stat.remoteCandidateId as string) ?? '';
        metrics.pairState = (stat.state as string) ?? '-';
        metrics.pairNominated = (stat.nominated as boolean) === true ? '是' : '否';
        metrics.rttMs = this.optMs(stat.currentRoundTripTime);
        metrics.availOutKbps = this.optKbps(stat.availableOutgoingBitrate);
        pairBytesSent = this.optRound(stat.bytesSent);
      }
    });
    report.stats.forEach((stat: ESObject, _key: string) => {
      const type = stat.type as string;
      if (type === 'outbound-rtp' && (stat.kind as string) === 'video') {
        metrics.outFps = this.optRound(stat.framesPerSecond);
        if (metrics.outFps === null) {
          metrics.outFps = this.outFpsAcc.fps(this.optRound(stat.framesEncoded), now);
        }
        metrics.outWidth = this.optRound(stat.frameWidth);
        metrics.outHeight = this.optRound(stat.frameHeight);
        // outbound-rtp 缺 bytesSent 时用候选对累计值兜底(含 RTCP 开销)
        const bytes = this.optRound(stat.bytesSent) ?? pairBytesSent;
        metrics.outBytesSent = bytes;
        metrics.outBitrateKbps = this.outRate.kbps(bytes, now);
        metrics.outboundRaw = this.dumpStat(stat);
      } else if (type === 'local-candidate' && pairLocalId !== '' && (stat.id as string) === pairLocalId) {
        metrics.pairLocal = this.formatCandidate((stat.candidate as string) ?? '');
      } else if (type === 'remote-candidate' && pairRemoteId !== '' && (stat.id as string) === pairRemoteId) {
        metrics.pairRemote = this.formatCandidate((stat.candidate as string) ?? '');
      }
    });
  }

  private parseReceiverStats(report: webrtc.RTCStatsReport, metrics: LoopbackMetrics): void
  {
    const now = Date.now();
    report.stats.forEach((stat: ESObject, _key: string) => {
      const type = stat.type as string;
      if (type === 'inbound-rtp' && (stat.kind as string) === 'video') {
        metrics.inFps = this.optRound(stat.framesPerSecond);
        if (metrics.inFps === null) {
          metrics.inFps = this.inFpsAcc.fps(this.optRound(stat.framesDecoded), now);
        }
        metrics.inWidth = this.optRound(stat.frameWidth);
        metrics.inHeight = this.optRound(stat.frameHeight);
        metrics.inPacketsLost = this.optRound(stat.packetsLost);
        metrics.inJitterMs = this.optMs(stat.jitter);
        metrics.inBytesReceived = this.optRound(stat.bytesReceived);
        metrics.inBitrateKbps = this.inRate.kbps(metrics.inBytesReceived, now);
        metrics.inboundRaw = this.dumpStat(stat);
      }
    });
  }

  private optRound(field: ESObject): number | null
  {
    return field === undefined || field === null ? null : Math.round(field as number);
  }

  private optMs(field: ESObject): number | null
  {
    return field === undefined || field === null ? null : Math.round((field as number) * 1000);
  }

  private optKbps(field: ESObject): number | null
  {
    return field === undefined || field === null ? null : Math.round((field as number) / 1000);
  }

  private dumpStat(stat: ESObject): string
  {
    return JSON.stringify(stat).substring(0, 180);
  }

  /** SDP candidate 行 'candidate:基础 组件 协议 优先级 ip 端口 typ 类型' → '协议 ip:端口 类型'。 */
  private formatCandidate(raw: string): string
  {
    const parts = raw.replace('candidate:', '').trim().split(' ').filter((p: string) => p !== '');
    if (parts.length < 8) {
      return raw.substring(0, 32);
    }
    return parts[2].toUpperCase() + ' ' + parts[4] + ':' + parts[5] + ' ' + parts[7];
  }
}
