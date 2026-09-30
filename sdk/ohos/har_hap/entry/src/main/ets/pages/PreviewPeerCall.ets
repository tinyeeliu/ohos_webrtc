import webrtc, { VideoRenderController, ScalingMode } from '@ohos/webrtc';
import webSocket from '@ohos.net.webSocket';
import { tryPreviewPlatformAction } from './PreviewPlatformCompat';

interface PeerMessage {
  sdp?: webrtc.RTCSessionDescriptionInit;
  ice?: webrtc.RTCIceCandidateInit;
}

/** Page-owned signaling; camera selection and recovery remain in CameraPreviewSession. */
export class PreviewPeerCall {
  public onStatus?: (status: string, registered: boolean, active: boolean) => void;
  private socket?: webSocket.WebSocket;
  private peer?: webrtc.RTCPeerConnection;
  private sender?: webrtc.RTCRtpSender;
  private generation: number = 0;
  private registered: boolean = false;
  private calling: boolean = false;
  private remoteReady: boolean = false;
  private pendingIce: webrtc.RTCIceCandidateInit[] = [];
  private queue: Promise<void> = Promise.resolve();
  private controller: VideoRenderController;

  constructor(controller: VideoRenderController) {
    this.controller = controller;
    controller.enableCameraPreviewAdaptation();
    controller.setMirror(false);
    controller.setScalingMode(ScalingMode.AspectFit);
  }

  private status(message: string): void {
    this.onStatus?.(message, this.registered && !this.calling, this.socket !== undefined);
  }

  stop(message: string = '已断开'): void {
    this.generation++;
    const generation = this.generation;
    let closeFailed = false;
    const socket = this.socket;
    this.socket = undefined;
    if (socket) {
      tryPreviewPlatformAction(() => { socket.off('open'); }, 'remove socket open');
      tryPreviewPlatformAction(() => { socket.off('message'); }, 'remove socket message');
      tryPreviewPlatformAction(() => { socket.off('error'); }, 'remove socket error');
      tryPreviewPlatformAction(() => { socket.off('close'); }, 'remove socket close');
      const reportCloseFailure = (): void => {
        console.warn('[PreviewPeerCall] Signaling socket close failed');
        if (generation === this.generation) { this.status('Local session stopped; signaling close failed'); }
      };
      closeFailed = !tryPreviewPlatformAction(() => {
        socket.close().then((closed: boolean) => {
          if (!closed) { reportCloseFailure(); }
        }).catch(reportCloseFailure);
      }, 'close signaling socket');
    }
    const peer = this.peer;
    if (peer) {
      tryPreviewPlatformAction(() => { peer.ontrack = null; }, 'remove peer track listener');
      tryPreviewPlatformAction(() => { peer.onicecandidate = null; }, 'remove peer ICE listener');
      tryPreviewPlatformAction(() => { peer.onconnectionstatechange = null; }, 'remove peer state listener');
      if (!tryPreviewPlatformAction(() => { peer.close(); }, 'close peer')) { closeFailed = true; }
    }
    this.peer = undefined;
    this.sender = undefined;
    this.registered = false;
    this.calling = false;
    this.remoteReady = false;
    this.pendingIce = [];
    this.queue = Promise.resolve();
    if (!tryPreviewPlatformAction(() => { this.controller.setVideoTrack(null); }, 'detach remote video')) {
      closeFailed = true;
    }
    this.status(closeFailed ? 'Local session stopped; resource cleanup failed' : message);
  }

  private enqueue(action: () => Promise<void>): void {
    const generation = this.generation;
    this.queue = this.queue.then(async () => {
      if (generation === this.generation) { await action(); }
    }).catch((error: Error) => {
      if (generation === this.generation) { this.stop(`Failed: ${error.message}`); }
    });
  }

  private async send(message: string): Promise<void> {
    if (!this.socket) { throw new Error('Signaling disconnected'); }
    const sent = await this.socket.send(message);
    if (!sent) { throw new Error('Signaling send failed'); }
  }

  start(factory: webrtc.PeerConnectionFactory, track: webrtc.MediaStreamTrack,
    server: string, localId: string): void {
    this.stop();
    const generation = this.generation;
    try {
      const peer = factory.createPeerConnection({ iceServers: [{ urls: 'stun:stun.l.google.com:19302' }] });
      this.peer = peer;
      this.sender = peer.addTrack(track);
      peer.ontrack = (event: webrtc.RTCTrackEvent): void => {
        if (generation === this.generation && event.track.kind === 'video') {
          this.controller.setVideoTrack(event.track);
          this.status('已接收对端视频轨道，请核对两端画面');
        }
      };
      peer.onicecandidate = (event: webrtc.RTCPeerConnectionIceEvent): void => {
        if (generation !== this.generation || !event.candidate) { return; }
        const message: PeerMessage = { ice: event.candidate.toJSON() };
        this.enqueue(async () => { await this.send(JSON.stringify(message)); });
      };
      peer.onconnectionstatechange = (): void => {
        if (generation !== this.generation) { return; }
        if (peer.connectionState === 'failed') {
          this.stop('Peer connection failed; reconnect');
        } else {
          this.status(`Peer: ${peer.connectionState}`);
        }
      };
      const socket = webSocket.createWebSocket();
      this.socket = socket;
      socket.on('open', () => {
        if (generation === this.generation) {
          this.enqueue(async () => { await this.send(`HELLO ${localId}`); });
        }
      });
      socket.on('message', (error, data: string | ArrayBuffer) => {
        if (generation !== this.generation) { return; }
        if (error && error.code !== 0) { this.stop('Signaling receive failed'); return; }
        if (typeof data !== 'string') { return; }
        this.enqueue(async () => { await this.receive(data, peer, generation); });
      });
      socket.on('error', () => {
        if (generation === this.generation) { this.stop('Signaling error; check server and network'); }
      });
      socket.on('close', () => {
        if (generation === this.generation) { this.stop('Signaling closed; reconnect'); }
      });
      this.status('正在连接信令服务器');
      socket.connect(server).then((success: boolean) => {
        if (!success && generation === this.generation) { this.stop('Signaling connection failed'); }
      }).catch(() => {
        if (generation === this.generation) { this.stop('Signaling connection failed'); }
      });
    } catch (error) { this.stop(`Failed: ${(error as Error).message}`); }
  }

  call(peerId: string): void {
    if (!this.registered || this.calling) { return; }
    this.calling = true;
    this.status('正在呼叫对端');
    this.enqueue(async () => { await this.send(`SESSION ${peerId}`); });
  }

  replaceTrack(track: webrtc.MediaStreamTrack | undefined): void {
    const sender = this.sender;
    if (sender) {
      this.enqueue(async () => { await sender.replaceTrack(track ?? null); });
    }
  }

  private async receive(data: string, peer: webrtc.RTCPeerConnection, generation: number): Promise<void> {
    if (data === 'HELLO') { this.registered = true; this.status('已注册，可呼叫或等待对端呼叫'); return; }
    if (data.startsWith('ERROR')) { throw new Error(data); }
    if (data === 'SESSION_OK' || data === 'OFFER_REQUEST') {
      this.calling = true;
      this.status('Negotiating video connection');
      const offer = await peer.createOffer();
      if (generation !== this.generation) { return; }
      await peer.setLocalDescription(offer);
      if (generation !== this.generation) { return; }
      const message: PeerMessage = { sdp: offer };
      await this.send(JSON.stringify(message));
      return;
    }
    const message = JSON.parse(data) as PeerMessage;
    if (message.sdp) {
      this.calling = true;
      this.status('Negotiating video connection');
      await peer.setRemoteDescription(message.sdp);
      if (generation !== this.generation) { return; }
      this.remoteReady = true;
      for (const ice of this.pendingIce) {
        await peer.addIceCandidate(ice);
        if (generation !== this.generation) { return; }
      }
      this.pendingIce = [];
      if (message.sdp.type === 'offer') {
        const answer = await peer.createAnswer();
        if (generation !== this.generation) { return; }
        await peer.setLocalDescription(answer);
        if (generation !== this.generation) { return; }
        const response: PeerMessage = { sdp: answer };
        await this.send(JSON.stringify(response));
      }
    } else if (message.ice) {
      if (this.remoteReady) { await peer.addIceCandidate(message.ice); } else { this.pendingIce.push(message.ice); }
    }
  }
}
