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

import webrtc, { Logging } from '@ohos/webrtc';
import abilityAccessCtrl, { PermissionRequestResult } from '@ohos.abilityAccessCtrl';
import webSocket from '@ohos.net.webSocket';
import { BusinessError } from '@kit.BasicServicesKit';
import { CollapsibleResults } from './common/CollapsibleResults';
import router from '@ohos.router';

const TAG: string = '[ConnectivityTest]';

// ADAPTER_TYPE: ETHERNET=1<<0, WIFI=1<<1, CELLULAR=1<<2, VPN=1<<3, LOOPBACK=1<<4
const MASK_NONE: number = 0;
const MASK_CELLULAR: number = 1 << 2;
const MASK_VPN: number = 1 << 3;
const MASK_CELLULAR_AND_VPN: number = MASK_CELLULAR | MASK_VPN;
const MAX_CANDIDATE_LINES: number = 16;
const GAIN_MUTE: number = 0;
const GAIN_BYPASS: number = 1;
const GAIN_BOOST: number = 2;
const STUN_URL: string = 'stun:stun.l.google.com:19302';
// H264 profile-level-id 前两字节(profile_idc) -> 人可读档位名
const H264_PROFILE_NAMES: Map<string, string> = new Map<string, string>([
  ['42', 'Baseline 基础档'], ['4d', 'Main 主档'], ['58', 'Extended 扩展档'],
  ['64', 'High 高档'], ['6e', 'High10 高比特档'], ['f4', 'High422 高档']
]);

interface CallSignal {
  type: string;
  role?: string;
  sdp?: webrtc.RTCSessionDescriptionInit;
  candidate?: webrtc.RTCIceCandidateInit;
}

interface CallInboundStats extends webrtc.RTCStats {
  kind?: string;
  packetsReceived?: number;
  bytesReceived?: number;
}

@Entry
@Component
struct ConnectivityTestPage {
  @State statusText: string = '等待权限...';
  @State h264Results: string[] = [];
  @State loopResults: string[] = [];
  @State apResults: string[] = [];
  @State codecLines: string[] = [];
  @State events: string[] = [];
  @State candidateLines: string[] = [];
  @State selectedPairText: string = '-';
  @State selectedMask: number = MASK_NONE;
  @State customMaskInput: string = '4';
  @State connected: boolean = false;
  @State trackAdded: boolean = false;
  @State frameCount: number = 0;
  @State lastRms: string = '-';
  @State lastFrameInfo: string = '-';
  @State gain: number = GAIN_BYPASS;
  @State apRunning: boolean = false;
  @State recDotOn: boolean = false;

  private micGranted: boolean = false;
  private loopFactory?: webrtc.PeerConnectionFactory;
  private caller?: webrtc.RTCPeerConnection;
  private callee?: webrtc.RTCPeerConnection;
  private audioSource?: webrtc.AudioSource;
  private audioTrack?: webrtc.AudioTrack;
  private sender?: webrtc.RTCRtpSender;
  private eventSeq: number = 0;
  @State candidateCount: number = 0;
  private apFactory?: webrtc.PeerConnectionFactory;
  private apAudioSource?: webrtc.AudioSource;
  private apCaller?: webrtc.RTCPeerConnection;
  private apCallee?: webrtc.RTCPeerConnection;
  private apStatsTimer?: number;
  private apFrameCount: number = 0;
  private apRmsText: string = '-';
  private apFrameInfoText: string = '-';
  @State callServerInput: string = '';
  @State callSignalState: string = '未连接';
  @State callPcState: string = '-';
  @State callRecvLevel: string = '-';
  @State callRecvRate: string = '-';
  @State callInCall: boolean = false;
  @State callPlayoutOn: boolean = true;
  @State callResults: string[] = [];
  private callWs?: webSocket.WebSocket;
  private callFactory?: webrtc.PeerConnectionFactory;
  private callPc?: webrtc.RTCPeerConnection;
  private callAudioSource?: webrtc.AudioSource;
  private callAudioTrack?: webrtc.AudioTrack;
  private callSender?: webrtc.RTCRtpSender;
  private callReceiver?: webrtc.RTCRtpReceiver;
  private callStatsTimer?: number;
  private callLastBytes: number = 0;

  aboutToAppear(): void {
    Logging.enableLogThreads();
    Logging.enableLogTimeStamps();
    this.showH264Level();

    abilityAccessCtrl.createAtManager()
      .requestPermissionsFromUser(getContext(), ['ohos.permission.MICROPHONE'])
      .then(async (result: PermissionRequestResult) => {
        let granted = true;
        for (let index = 0; index < result.permissions.length; index++) {
          if (result.authResults[index] !== abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
            granted = false;
          }
        }
        if (!granted) {
          this.setStatus('麦克风权限未授予');
          return;
        }
        this.micGranted = true;
        this.setStatus('就绪');
      });
  }

  aboutToDisappear(): void {
    this.teardown();
    this.stopApCapture();
    this.cleanupRealCall();
  }

  setStatus(text: string) {
    console.info(TAG, text);
    this.statusText = text;
  }

  pushResult(list: string[], result: string) {
    console.info(TAG, result);
    list.push(result);
  }

  logEvent(event: string) {
    this.eventSeq++;
    const line = `[${this.eventSeq}] ${event}`;
    console.info(TAG, line);
    this.events.push(line);
  }

  sleep(ms: number): Promise<void> {
    return new Promise<void>((resolve: () => void) => {
      setTimeout(resolve, ms);
    });
  }

  // ===== H264 声明档位: 进页自动查询并转为人可读描述 =====
  describeH264Codec(fmtp: string): string {
    const match = fmtp.match(/profile-level-id=([0-9a-fA-F]{6})/);
    const hex = match === null || match[1] === undefined ? '000000' : match[1];
    const profileKey = hex.substring(0, 2).toLowerCase();
    const profile = H264_PROFILE_NAMES.get(profileKey) ?? `未知档位(${profileKey})`;
    const levelIdc = parseInt(hex.substring(4, 6), 16);
    const note = levelIdc >= 51 ? '约 1080p60 上限' :
      levelIdc >= 41 ? '约 1080p30 上限' :
      levelIdc >= 31 ? '约 720p30 上限' : '低于 720p 上限';
    return `${profile} Level ${(levelIdc / 10).toFixed(1)} (${note})`;
  }

  showH264Level() {
    this.codecLines = [];
    try {
      const send = webrtc.RTCRtpSender.getCapabilities('video');
      const recv = webrtc.RTCRtpReceiver.getCapabilities('video');
      const h264Send = send ? send.codecs.filter(c => c.mimeType.indexOf('H264') >= 0) : [];
      const h264Recv = recv ? recv.codecs.filter(c => c.mimeType.indexOf('H264') >= 0) : [];
      this.pushResult(this.h264Results, `✓ 已自动查询: H264 编码声明 ${h264Send.length} 条 / 解码声明 ${h264Recv.length} 条`);
      for (const codec of h264Send) {
        this.codecLines.push(`[编码] ${this.describeH264Codec(codec.sdpFmtpLine ?? '')}`);
      }
      for (const codec of h264Recv) {
        this.codecLines.push(`[解码] ${this.describeH264Codec(codec.sdpFmtpLine ?? '')}`);
      }
      if (this.codecLines.length === 0) {
        this.codecLines.push('未查询到 H264 声明');
      }
    } catch (e) {
      this.pushResult(this.h264Results, `✗ 查询声明失败 - ${e}`);
    }
  }

  // ===== 回环连接: 事件面挂 callee(接收侧), 候选收集在 caller(发送侧) =====
  attachCalleeEvents() {
    const callee = this.callee!;

    callee.ontrack = (ev: webrtc.RTCTrackEvent) => {
      this.logEvent(`ontrack: track=${ev.track.id} receiver=${ev.receiver.id}`);
    };
    callee.onaddstream = (ev: webrtc.MediaStreamEvent) => {
      this.logEvent(`onaddstream: streamId=${ev.streamId}`);
    };
    callee.onremovestream = (ev: webrtc.MediaStreamEvent) => {
      this.logEvent(`onremovestream: streamId=${ev.streamId}`);
    };
    callee.onremovetrack = (ev: webrtc.RTCRemoveTrackEvent) => {
      this.logEvent(`onremovetrack: receiver=${ev.receiver.id}`);
    };
    callee.oniceconnectionstatechange = () => {
      this.logEvent(`oniceconnectionstatechange: ${callee.iceConnectionState}`);
    };
    callee.onconnectionstatechange = () => {
      this.logEvent(`onconnectionstatechange: ${callee.connectionState}`);
    };
  }

  // 候选交换同时记录候选地址: 屏蔽某网卡后, 该网卡地址不应出现
  wireIceExchange() {
    const caller = this.caller!;
    const callee = this.callee!;
    caller.onicecandidate = (event) => {
      const cand = event.candidate;
      if (cand) {
        this.candidateCount++;
        if (this.candidateLines.length < MAX_CANDIDATE_LINES) {
          this.candidateLines.push(`${this.candidateCount}. ${cand.type} ${cand.protocol} ${cand.address}:${cand.port}`);
        }
        if (callee.signalingState !== 'closed') {
          callee.addIceCandidate(cand);
        }
      }
    };
    callee.onicecandidate = (event) => {
      if (event.candidate && caller.signalingState !== 'closed') {
        caller.addIceCandidate(event.candidate);
      }
    };
  }

  async exchangeOfferAnswer(): Promise<void> {
    await this.exchangeDescriptions(this.caller!, this.callee!);
  }

  // 完成 offer/answer 协商才会启动音频发送流(仅本地 offer 不启动), 采集随之跑起来
  async exchangeDescriptions(caller: webrtc.RTCPeerConnection, callee: webrtc.RTCPeerConnection): Promise<void> {
    await caller.setLocalDescription(await caller.createOffer());
    await callee.setRemoteDescription(caller.localDescription);
    const answer = await callee.createAnswer();
    await caller.setRemoteDescription(answer);
    await callee.setLocalDescription(answer);
  }

  // ===== 建连后查询选中候选对: 掩码生效的实证 =====
  async querySelectedPair() {
    this.selectedPairText = '查询中...';
    for (let attempt = 0; attempt < 10; attempt++) {
      const transceivers = this.caller!.getTransceivers();
      for (const transceiver of transceivers) {
        const transport = transceiver.receiver.transport;
        if (!transport) {
          continue;
        }
        const pair = transport.iceTransport.getSelectedCandidatePair();
        if (pair && pair.local && pair.remote) {
          const local = pair.local;
          const remote = pair.remote;
          this.selectedPairText =
            `本地 ${local.type}/${local.protocol} ${local.address}:${local.port} ↔ ` +
            `远端 ${remote.type}/${remote.protocol} ${remote.address}`;
          this.pushResult(this.loopResults, `✓ 选中候选对: ${this.selectedPairText}`);
          return;
        }
      }
      await this.sleep(500);
    }
    this.selectedPairText = '5s 内未查询到选中候选对';
    this.pushResult(this.loopResults, '✗ 未取到选中候选对');
  }

  async setupLoopback() {
    if (this.connected) {
      this.setStatus('回环已存在, 请先清理');
      return;
    }
    if (!this.micGranted) {
      this.setStatus('麦克风权限未授予');
      return;
    }
    const maskLabel = this.selectedMask === MASK_NONE ? '无' : String(this.selectedMask);
    this.setStatus(`以掩码 ${maskLabel} 建立回环`);
    this.candidateLines = [];
    this.candidateCount = 0;
    try {
      const options: webrtc.PeerConnectionFactoryOptions = {};
      if (this.selectedMask > 0) {
        options.networkIgnoreMask = this.selectedMask;
      }
      this.loopFactory = new webrtc.PeerConnectionFactory(options);
      this.caller = this.loopFactory.createPeerConnection({ iceServers: [] });
      this.callee = this.loopFactory.createPeerConnection({ iceServers: [] });
      this.attachCalleeEvents();
      this.wireIceExchange();

      this.audioSource = this.loopFactory.createAudioSource();
      this.audioTrack = this.loopFactory.createAudioTrack('audio', this.audioSource);
      const stream = new webrtc.MediaStream([this.audioTrack]);
      this.sender = this.caller.addTrack(this.audioTrack, stream);

      await this.exchangeOfferAnswer();
      this.connected = true;
      this.trackAdded = true;
      this.setStatus('回环已建立, 观察候选列表/选中对与事件时间线');
      this.pushResult(this.loopResults, `✓ 掩码=${maskLabel}: 协商成功, 候选 ${this.candidateCount} 条(见列表)`);
      this.querySelectedPair();
    } catch (e) {
      this.logEvent(`✗ 建立失败 - ${e}`);
    }
  }

  // ===== 撤轨并重协商: 应看到 onremovetrack -> onremovestream =====
  async removeTrackAndRenegotiate() {
    if (!this.connected || !this.trackAdded) {
      this.setStatus('请先建立连接并加轨');
      return;
    }
    this.setStatus('移除轨道并重协商');
    try {
      this.caller!.removeTrack(this.sender!);
      await this.exchangeOfferAnswer();
      this.trackAdded = false;
    } catch (e) {
      this.logEvent(`✗ 移除轨道失败 - ${e}`);
    }
  }

  // ===== 重新加轨: onaddstream 应再次出现 =====
  async addTrackBack() {
    if (!this.connected) {
      this.setStatus('请先建立连接');
      return;
    }
    if (this.trackAdded) {
      this.setStatus('轨道已在发送中');
      return;
    }
    this.setStatus('重新添加轨道');
    try {
      const stream = new webrtc.MediaStream([this.audioTrack!]);
      this.sender = this.caller!.addTrack(this.audioTrack!, stream);
      await this.exchangeOfferAnswer();
      this.trackAdded = true;
    } catch (e) {
      this.logEvent(`✗ 重新添加轨道失败 - ${e}`);
    }
  }

  teardown() {
    this.caller?.close();
    this.callee?.close();
    this.audioSource?.release();
    this.caller = undefined;
    this.callee = undefined;
    this.audioSource = undefined;
    this.loopFactory = undefined;
    this.connected = false;
    this.trackAdded = false;
    this.candidateLines = [];
    this.candidateCount = 0;
    this.selectedPairText = '-';
    this.loopResults = [];
  }

  clearTimeline() {
    this.events = [];
    this.eventSeq = 0;
  }

  // ===== 采集侧处理插槽: 增益演示(就地改写帧数据) =====
  async startApCapture() {
    if (this.apRunning) {
      return;
    }
    if (!this.micGranted) {
      this.setStatus('麦克风权限未授予');
      return;
    }
    this.setStatus('启动带处理插槽的采集');
    this.apFrameCount = 0;
    this.apRmsText = '-';
    this.apFrameInfoText = '-';
    this.frameCount = 0;
    this.lastRms = '-';
    this.lastFrameInfo = '-';
    try {
      const apFactory = new webrtc.AudioProcessingFactory();
      const ap = apFactory.create({
        capturePostProcessing: (frame: webrtc.AudioProcessingFrame) => {
          this.apFrameCount++;
          const samples = frame.data;
          let sum = 0;
          for (let i = 0; i < samples.length; i++) {
            if (this.gain !== GAIN_BYPASS) {
              samples[i] = samples[i] * this.gain;
            }
            sum += samples[i] * samples[i];
          }
          this.apRmsText = Math.sqrt(sum / samples.length).toFixed(4);
          this.apFrameInfoText = `${frame.sampleRate}Hz x${frame.channels}ch ${samples.length}样本`;
        }
      });

      this.apFactory = new webrtc.PeerConnectionFactory({ audioProcessing: ap });
      this.apAudioSource = this.apFactory.createAudioSource();
      const audioTrack = this.apFactory.createAudioTrack('audio', this.apAudioSource);
      const stream = new webrtc.MediaStream([audioTrack]);
      this.apCaller = this.apFactory.createPeerConnection({ iceServers: [] });
      this.apCallee = this.apFactory.createPeerConnection({ iceServers: [] });
      this.apCaller.onicecandidate = (event: webrtc.RTCPeerConnectionIceEvent) => {
        if (event.candidate && this.apCallee && this.apCallee.signalingState !== 'closed') {
          this.apCallee.addIceCandidate(event.candidate);
        }
      };
      this.apCallee.onicecandidate = (event: webrtc.RTCPeerConnectionIceEvent) => {
        if (event.candidate && this.apCaller && this.apCaller.signalingState !== 'closed') {
          this.apCaller.addIceCandidate(event.candidate);
        }
      };
      this.apCaller.addTrack(audioTrack, stream);
      await this.exchangeDescriptions(this.apCaller, this.apCallee);

      this.apRunning = true;
      this.startApStats();
      this.setStatus('采集中, 请说话...');
      this.pushResult(this.apResults, '✓ 插槽已注入, 回环协商完成, 采集运行中');
    } catch (e) {
      this.pushResult(this.apResults, `✗ 启动失败 - ${e}`);
    }
  }

  // 插槽回调在采集线程高频触发(约百次/秒), REC 闪烁与统计统一降频刷新到界面
  startApStats() {
    if (this.apStatsTimer) {
      return;
    }
    this.apStatsTimer = setInterval(() => {
      this.recDotOn = !this.recDotOn;
      this.frameCount = this.apFrameCount;
      this.lastRms = this.apRmsText;
      this.lastFrameInfo = this.apFrameInfoText;
    }, 500);
  }

  stopApCapture() {
    if (this.apStatsTimer) {
      clearInterval(this.apStatsTimer);
      this.apStatsTimer = undefined;
    }
    this.recDotOn = false;
    this.apCaller?.close();
    this.apCallee?.close();
    this.apCaller = undefined;
    this.apCallee = undefined;
    this.apAudioSource?.release();
    this.apAudioSource = undefined;
    this.apFactory = undefined;
    this.apRunning = false;
  }

  setGain(value: number) {
    this.gain = value;
    this.setStatus(`增益已设为 ${value}x ${value === GAIN_MUTE ? '(静音)' : ''}`);
  }

  async useCustomMask() {
    const mask = parseInt(this.customMaskInput, 10);
    if (isNaN(mask) || mask < 0) {
      this.pushResult(this.loopResults, `已拒绝无效掩码输入 (${this.customMaskInput})`);
      return;
    }
    this.selectedMask = mask;
    await this.setupLoopback();
  }

  // ===== 真实通话: 信令服务器配对 PC 网页, 麦克风音频真实双向收发 =====
  sendCallSignal(message: CallSignal): void {
    this.callWs?.send(JSON.stringify(message), (err: BusinessError, value: boolean) => {});
  }

  startRealCall(): void {
    if (this.callInCall) {
      return;
    }
    if (!this.micGranted) {
      this.setStatus('麦克风权限未授予');
      return;
    }
    const server = this.callServerInput.trim();
    if (!server.startsWith('ws://') && !server.startsWith('wss://')) {
      this.pushResult(this.callResults, '✗ 服务器地址无效, 应形如 ws://192.168.1.10:3000');
      return;
    }
    this.callResults = [];
    this.callLastBytes = 0;
    this.callRecvLevel = '-';
    this.callRecvRate = '-';
    this.callSignalState = '连接服务器中';
    this.callWs = webSocket.createWebSocket();
    this.callWs.on('open', (err: BusinessError, value: Object) => {
      this.callInCall = true;
      this.callSignalState = '已连服务器, 加入房间';
      this.sendCallSignal({ type: 'join', role: 'device' });
    });
    this.callWs.on('message', (err: BusinessError, value: string | ArrayBuffer | boolean) => {
      if (typeof value === 'string') {
        this.handleCallSignal(value);
      }
    });
    this.callWs.on('close', (err: BusinessError, value: webSocket.CloseResult) => {
      if (this.callInCall) {
        this.pushResult(this.callResults, '✗ 服务器连接断开, 通话中止');
      }
      this.cleanupRealCall();
    });
    this.callWs.on('error', (err: BusinessError) => {
      this.callSignalState = `服务器连接失败 (${err.code})`;
      this.pushResult(this.callResults, `✗ 服务器连接失败 - ${err.message}`);
    });
    this.callWs.connect(server, (err: BusinessError, value: boolean) => {});
  }

  handleCallSignal(text: string): void {
    let message: CallSignal;
    try {
      message = JSON.parse(text) as CallSignal;
    } catch (e) {
      return;
    }
    if (message.type === 'waiting') {
      this.callSignalState = '已加入房间, 请在 PC 打开网页';
    } else if (message.type === 'peer-joined') {
      this.callSignalState = '对端已加入, 发起通话';
      this.negotiateCall();
    } else if (message.type === 'answer' && message.sdp) {
      this.acceptCallAnswer(message.sdp);
    } else if (message.type === 'ice' && message.candidate) {
      this.callPc?.addIceCandidate(message.candidate);
    } else if (message.type === 'peer-left') {
      this.pushResult(this.callResults, '✗ 对端已离开, 通话结束');
      this.cleanupRealCall();
    } else if (message.type === 'full') {
      this.pushResult(this.callResults, '✗ 房间已满, 请先在 PC 刷新网页');
      this.cleanupRealCall();
    }
  }

  async negotiateCall(): Promise<void> {
    try {
      this.callFactory = new webrtc.PeerConnectionFactory();
      this.callPc = this.callFactory.createPeerConnection({ iceServers: [{ urls: STUN_URL }] });
      const pc = this.callPc;
      pc.onicecandidate = (event: webrtc.RTCPeerConnectionIceEvent) => {
        const candidate = event.candidate;
        if (candidate) {
          this.sendCallSignal({
            type: 'ice',
            candidate: {
              candidate: candidate.candidate,
              sdpMid: candidate.sdpMid ?? undefined,
              sdpMLineIndex: candidate.sdpMLineIndex ?? undefined
            }
          });
        }
      };
      pc.ontrack = (event: webrtc.RTCTrackEvent) => {
        this.callReceiver = event.receiver;
        this.pushResult(this.callResults, `✓ 收到 PC 音频轨: ${event.track.id}`);
      };
      pc.onaddstream = (event: webrtc.MediaStreamEvent) => {
        this.pushResult(this.callResults, `✓ 收到 PC 音频流: ${event.streamId}`);
      };
      pc.onconnectionstatechange = () => {
        this.callPcState = pc.connectionState;
        if (pc.connectionState === 'connected') {
          this.pushResult(this.callResults, '✓ 与 PC 网页连接成功, 双向音频传输中');
          this.startCallStats();
        } else if (pc.connectionState === 'failed') {
          this.pushResult(this.callResults, '✗ 连接失败, 检查两端是否同一局域网且地址正确');
        }
      };

      this.callAudioSource = this.callFactory.createAudioSource();
      this.callAudioTrack = this.callFactory.createAudioTrack('callAudio', this.callAudioSource);
      const stream = new webrtc.MediaStream([this.callAudioTrack]);
      this.callSender = pc.addTrack(this.callAudioTrack, stream);

      const offer = await pc.createOffer();
      await pc.setLocalDescription(offer);
      this.sendCallSignal({ type: 'offer', sdp: offer });
      this.pushResult(this.callResults, '✓ 本机麦克风已发声, offer 已发送');
    } catch (e) {
      this.pushResult(this.callResults, `✗ 发起通话失败 - ${e}`);
    }
  }

  async acceptCallAnswer(sdp: webrtc.RTCSessionDescriptionInit): Promise<void> {
    try {
      await this.callPc?.setRemoteDescription(sdp);
      this.pushResult(this.callResults, '✓ 收到 PC 应答, 协商完成');
    } catch (e) {
      this.pushResult(this.callResults, `✗ 处理应答失败 - ${e}`);
    }
  }

  startCallStats(): void {
    if (this.callStatsTimer) {
      return;
    }
    this.callStatsTimer = setInterval(() => {
      this.pollCallStats();
    }, 1000);
  }

  // PC 音频真实性判定: 电平来自同步源, 收包字节来自 inbound-rtp 统计
  async pollCallStats(): Promise<void> {
    const receiver = this.callReceiver;
    if (!receiver) {
      return;
    }
    const sources = receiver.getSynchronizationSources();
    if (sources.length > 0) {
      const level = sources[0].audioLevel;
      this.callRecvLevel = (level === undefined || level === null) ? '无音频到达' : level.toFixed(2);
    } else {
      this.callRecvLevel = '无音频到达';
    }
    try {
      const report = await receiver.getStats();
      report.stats.forEach((stat: webrtc.RTCStats) => {
        const inbound = stat as CallInboundStats;
        if (inbound.kind === 'audio' && inbound.bytesReceived !== undefined) {
          const bytes = inbound.bytesReceived;
          const delta = bytes - this.callLastBytes;
          this.callRecvRate = delta > 0 ? `${delta} B/s (累计 ${bytes} B)` : '未增长';
          this.callLastBytes = bytes;
        }
      });
    } catch (e) {
      this.pushResult(this.callResults, `✗ 统计查询失败 - ${e}`);
    }
  }

  toggleCallPlayout(): void {
    this.callPlayoutOn = !this.callPlayoutOn;
    this.callPc?.setAudioPlayout(this.callPlayoutOn);
    this.setStatus(this.callPlayoutOn ? '对端声音已开启' : '对端声音已关闭');
  }

  cleanupRealCall(): void {
    if (this.callStatsTimer) {
      clearInterval(this.callStatsTimer);
      this.callStatsTimer = undefined;
    }
    this.callPc?.close();
    this.callPc = undefined;
    this.callSender = undefined;
    this.callReceiver = undefined;
    this.callAudioTrack = undefined;
    this.callAudioSource?.release();
    this.callAudioSource = undefined;
    this.callFactory = undefined;
    const ws = this.callWs;
    if (ws) {
      this.callWs = undefined;
      ws.off('open');
      ws.off('message');
      ws.off('close');
      ws.off('error');
      ws.close();
    }
    this.callInCall = false;
    this.callSignalState = '未连接';
    this.callPcState = '-';
    this.callRecvLevel = '-';
    this.callRecvRate = '-';
    this.callLastBytes = 0;
  }

  build() {
    Scroll() {
      Column() {
        Row() {
          Button('返回')
            .width(72)
            .height(36)
            .fontSize(14)
            .onClick(() => { router.back(); })
          Text('连接网络音频测试')
            .fontSize(22)
            .fontWeight(FontWeight.Bold)
            .margin({ left: 12 })
        }
        .width('100%')
        .margin({ bottom: 12 })

        Text(`状态: ${this.statusText}`)
          .fontSize(15)
          .fontColor(0x007DFF)
          .margin({ bottom: 12 })

        Text('H264 声明档位 (进入页面自动查询)')
          .fontSize(16)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 6 })

        Text('声明档位=设备声称的最高支持能力, 用于协商, 不代表实测画质。profile(档位名)决定编码工具集, Level(数字)决定分辨率/帧率上限: 3.1≈720p30, 4.1≈1080p30, 5.1≈1080p60。实际通话取双方交集, 档位越高画质潜力越大。')
          .fontSize(12)
          .fontColor(0x999999)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 8 })

        CollapsibleResults({ items: this.h264Results })

        CollapsibleResults({ items: this.codecLines, maxVisible: 5 })

        Text('预期: 声明值随设备真实能力提升(3.1 -> 4.1 -> 5.1, High 高档解锁更多编码工具), 1080p 协商不再被压到 720p。')
          .fontSize(12)
          .fontColor(0x999999)
          .margin({ bottom: 16 })

        Text('回环连接 (networkIgnoreMask + 流/轨事件)')
          .fontSize(16)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 6 })

        Row() {
          Radio({ value: 'none', group: 'mask' })
            .checked(this.selectedMask === MASK_NONE)
            .onChange((checked: boolean) => { if (checked) { this.selectedMask = MASK_NONE; } })
          Text('不屏蔽任何网卡').fontSize(13).margin({ right: 20 })
          Radio({ value: 'cellular', group: 'mask' })
            .checked(this.selectedMask === MASK_CELLULAR)
            .onChange((checked: boolean) => { if (checked) { this.selectedMask = MASK_CELLULAR; } })
          Text('禁用蜂窝网络(4)').fontSize(13)
        }
        .width('100%')
        .margin({ bottom: 6 })

        Row() {
          Radio({ value: 'vpn', group: 'mask' })
            .checked(this.selectedMask === MASK_VPN)
            .onChange((checked: boolean) => { if (checked) { this.selectedMask = MASK_VPN; } })
          Text('禁用VPN(8)').fontSize(13).margin({ right: 20 })
          Radio({ value: 'both', group: 'mask' })
            .checked(this.selectedMask === MASK_CELLULAR_AND_VPN)
            .onChange((checked: boolean) => { if (checked) { this.selectedMask = MASK_CELLULAR_AND_VPN; } })
          Text('蜂窝+VPN都禁用(12)').fontSize(13)
        }
        .width('100%')
        .margin({ bottom: 8 })

        Row() {
          Text('自定义掩码:')
            .fontSize(14)
            .width(90)
          TextInput({ text: this.customMaskInput, placeholder: '如 12' })
            .width(110)
            .height(40)
            .fontSize(14)
            .type(InputType.Number)
            .onChange((value: string) => { this.customMaskInput = value; })
          Button('按自定义掩码连接')
            .height(40)
            .fontSize(14)
            .margin({ left: 8 })
            .layoutWeight(1)
            .onClick(() => { this.useCustomMask(); })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Button(this.connected ? '回环已建立' : '1. 按所选掩码建立回环连接')
          .width('100%')
          .height(40)
          .fontSize(14)
          .enabled(!this.connected)
          .onClick(() => { this.setupLoopback(); })
          .margin({ bottom: 8 })

        Text('验证口径: 真机开 WiFi+蜂窝后选"禁蜂窝"建连, 下方候选列表应无蜂窝(rmnet)地址, 选中对走 WiFi。')
          .fontSize(12)
          .fontColor(0x999999)
          .margin({ bottom: 8 })

        Text(`调用方候选 (${this.candidateCount} 条):`)
          .fontSize(14)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 4 })

        CollapsibleResults({ items: this.candidateLines, maxVisible: 5 })

        Text(`选中候选对: ${this.selectedPairText}`)
          .fontSize(13)
          .fontColor(0x007DFF)
          .margin({ top: 4, bottom: 4 })
          .alignSelf(ItemAlign.Start)

        CollapsibleResults({ items: this.loopResults })

        Row() {
          Button('2. 移除轨道并重新协商')
            .height(40)
            .fontSize(13)
            .layoutWeight(1)
            .enabled(this.connected && this.trackAdded)
            .onClick(() => { this.removeTrackAndRenegotiate(); })
          Button('3. 重新添加轨道')
            .height(40)
            .fontSize(13)
            .layoutWeight(1)
            .margin({ left: 8 })
            .enabled(this.connected && !this.trackAdded)
            .onClick(() => { this.addTrackBack(); })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Row() {
          Button('清理连接')
            .width('100%')
            .height(40)
            .fontSize(13)
            .onClick(() => {
              this.teardown();
              this.setStatus('已清理');
            })
        }
        .margin({ bottom: 8 })

        Text('事件时间线 (被调用方):')
          .fontSize(14)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 4 })

        Text('预期顺序: 加轨后 ontrack -> onaddstream; 移除轨道后 onremovetrack -> onremovestream')
          .fontSize(12)
          .fontColor(0x999999)
          .margin({ bottom: 4 })

        CollapsibleResults({ items: this.events, maxVisible: 5 })

        Row() {
          Button('清空时间线')
            .height(36)
            .fontSize(13)
            .layoutWeight(1)
            .onClick(() => { this.clearTimeline(); })
        }
        .width('100%')
        .margin({ bottom: 16 })

        Text('音频处理注入 (capturePostProcessing 插槽)')
          .fontSize(16)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 6 })

        if (this.apRunning) {
          Row() {
            Circle({ width: 10, height: 10 })
              .fill('#FF3B30')
              .opacity(this.recDotOn ? 1 : 0.2)
            Text('REC')
              .fontSize(12)
              .fontColor('#FF3B30')
              .margin({ left: 6 })
          }
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 6 })
        }

        Text(`帧计数: ${this.frameCount}    最近电平(RMS): ${this.lastRms}`)
          .fontSize(15)
          .margin({ bottom: 4 })
        Text(`最近帧: ${this.lastFrameInfo}`)
          .fontSize(13)
          .fontColor(0x666666)
          .margin({ bottom: 8 })

        Button(this.apRunning ? '采集中, 请说话...' : '启动采集 (带处理插槽)')
          .width('100%')
          .height(44)
          .fontSize(15)
          .enabled(!this.apRunning)
          .onClick(() => { this.startApCapture(); })
          .margin({ bottom: 8 })

        Row() {
          Button('停止采集')
            .height(40)
            .fontSize(14)
            .layoutWeight(1)
            .enabled(this.apRunning)
            .onClick(() => {
              this.stopApCapture();
              this.setStatus('已停止');
            })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Row() {
          Radio({ value: 'mute', group: 'gain' })
            .checked(this.gain === GAIN_MUTE)
            .onChange((checked: boolean) => { if (checked) { this.setGain(GAIN_MUTE); } })
          Text('0x 静音').fontSize(13).margin({ right: 14 })
          Radio({ value: 'bypass', group: 'gain' })
            .checked(this.gain === GAIN_BYPASS)
            .onChange((checked: boolean) => { if (checked) { this.setGain(GAIN_BYPASS); } })
          Text('1x 直通').fontSize(13).margin({ right: 14 })
          Radio({ value: 'boost', group: 'gain' })
            .checked(this.gain === GAIN_BOOST)
            .onChange((checked: boolean) => { if (checked) { this.setGain(GAIN_BOOST); } })
          Text('2x 增强').fontSize(13)
        }
        .width('100%')
        .margin({ bottom: 8 })

        Text('验证口径: 切换增益后对麦克风说话, RMS 应明显变化(0x 时接近 0, 2x 时约为 1x 的两倍)。变声/AI 降噪等复杂处理同理挂在该插槽。')
          .fontSize(12)
          .fontColor(0x999999)
          .margin({ bottom: 4 })

        CollapsibleResults({ items: this.apResults })

        Text('真实通话 (连接 PC 网页)')
          .fontSize(16)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 6 })

        Row() {
          Text('服务器:')
            .fontSize(14)
            .width(64)
          TextInput({ text: this.callServerInput, placeholder: 'ws://PC的IP:3000' })
            .height(40)
            .fontSize(13)
            .layoutWeight(1)
            .onChange((value: string) => { this.callServerInput = value; })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Text(`信令: ${this.callSignalState}    连接: ${this.callPcState}`)
          .fontSize(14)
          .margin({ bottom: 4 })
        Text(`PC 到本机音频: 电平 ${this.callRecvLevel}    收包 ${this.callRecvRate}`)
          .fontSize(14)
          .margin({ bottom: 8 })

        Row() {
          Button(this.callInCall ? '通话进行中' : '1. 连接服务器开始通话')
            .height(40)
            .fontSize(14)
            .layoutWeight(1)
            .enabled(!this.callInCall)
            .onClick(() => { this.startRealCall(); })
          Button('2. 挂断并清理')
            .height(40)
            .fontSize(14)
            .layoutWeight(1)
            .margin({ left: 8 })
            .enabled(this.callInCall)
            .onClick(() => {
              this.cleanupRealCall();
              this.setStatus('通话已结束');
            })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Row() {
          Button(this.callPlayoutOn ? '3. 关闭对端声音' : '3. 开启对端声音')
            .height(40)
            .fontSize(14)
            .layoutWeight(1)
            .enabled(this.callInCall)
            .onClick(() => { this.toggleCallPlayout(); })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Text('验证口径: PC 执行 npm start 后浏览器打开 http://localhost:3000, 真机填入 ws://PC的IP:3000 开始通话。对真机麦克风说话, PC 页面应听到声音且电平跳动; 对 PC 麦克风说话, 本机应出声且"电平/收包"持续增长, 即双向真实音频流。')
          .fontSize(12)
          .fontColor(0x999999)
          .margin({ bottom: 4 })

        CollapsibleResults({ items: this.callResults })
      }
      .width('100%')
      .padding(16)
      .alignItems(HorizontalAlign.Center)
    }
    .width('100%')
    .height('100%')
    .scrollBar(BarState.Auto)
    .scrollable(ScrollDirection.Vertical)
  }
}
