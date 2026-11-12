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

/** 测试类型： e2e（端到端测试） */

import { describe, beforeAll, it, expect } from '@ohos/hypium'
import webrtc, { Logging, LoggingSeverity } from '@ohos/webrtc';
import { sleep, isX86 } from './Helper';
import { createStunServerConfiguration } from './TestFixtures';

const TAG: string = '[PeerConnectionLoopbackE2ETest]';

const pcf = new webrtc.PeerConnectionFactory();

function exchangeIceCandidates(pc1: webrtc.RTCPeerConnection, pc2: webrtc.RTCPeerConnection): void {
  const doExchange = (from: webrtc.RTCPeerConnection, to: webrtc.RTCPeerConnection) => {
    from.onicecandidate = (event) => {
      const candidate = event.candidate;
      if (candidate && to.signalingState !== 'closed') {
        to.addIceCandidate(candidate);
      }
    };
  };
  doExchange(pc1, pc2);
  doExchange(pc2, pc1);
}

async function exchangeOfferAnswer(caller: webrtc.RTCPeerConnection,
  callee: webrtc.RTCPeerConnection): Promise<void> {
  await caller.setLocalDescription(await caller.createOffer());
  await callee.setRemoteDescription(caller.localDescription);
  const answer = await callee.createAnswer();
  await caller.setRemoteDescription(answer);
  await callee.setLocalDescription(answer);
}

export default function PeerConnectionLoopbackE2ETest() {
  describe('PeerConnectionLoopbackE2ETest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    // 完整媒体链路 E2E：采集（getUserMedia）→ 编码 → 传输（ICE/DTLS）→ 解码 → 对端 ontrack。
    it('shouldEstablishAudioLoopbackCallEndToEnd', 0, async () => {
      const mediaDevices = new webrtc.MediaDevices();
      let stream: webrtc.MediaStream;
      try {
        stream = await mediaDevices.getUserMedia({ audio: true });
      } catch (e) {
        console.warn(TAG, 'getUserMedia audio unavailable, skip: ' + JSON.stringify(e));
        return;
      }
      const caller = pcf.createPeerConnection(createStunServerConfiguration());
      const callee = pcf.createPeerConnection(createStunServerConfiguration());
      const remoteTracks: webrtc.MediaStreamTrack[] = [];
      callee.ontrack = (event) => {
        if (event.track) {
          remoteTracks.push(event.track);
        }
      };
      const audioTracks = stream.getAudioTracks();
      expect(audioTracks.length).assertEqual(1);
      caller.addTrack(audioTracks[0]);

      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);

      let connected = false;
      const maxWaitMs = 15000;
      const pollIntervalMs = 300;
      let elapsed = 0;
      while (elapsed < maxWaitMs) {
        const callerState = caller.iceConnectionState;
        const calleeState = callee.iceConnectionState;
        if ((callerState === 'connected' || callerState === 'completed') &&
            (calleeState === 'connected' || calleeState === 'completed')) {
          connected = true;
          break;
        }
        if (callerState === 'failed' || calleeState === 'failed') {
          break;
        }
        await sleep(pollIntervalMs);
        elapsed += pollIntervalMs;
      }

      if (!isX86()) {
        // 真机：完整 E2E 断言——ICE 连通且对端收到媒体轨。
        expect(connected).assertTrue();
        expect(remoteTracks.length).assertLarger(0);
        expect(remoteTracks[0].kind).assertEqual('audio');
        const stats = await caller.getStats();
        expect(stats).assertNotNull();
      } else {
        // x86 模拟器无完整网络/编解码链路：验证信令协商到达 stable 且媒体轨已声明。
        expect(caller.signalingState).assertEqual('stable');
        expect(callee.signalingState).assertEqual('stable');
        expect(callee.getReceivers().length).assertLarger(0);
      }

      caller.close();
      callee.close();
      for (const track of audioTracks) {
        track.stop();
      }
      for (const track of remoteTracks) {
        track.stop();
      }
    })

    it('shouldNegotiateVideoTrackWhenCameraAvailableEndToEnd', 0, async () => {
      const mediaDevices = new webrtc.MediaDevices();
      let stream: webrtc.MediaStream;
      try {
        stream = await mediaDevices.getUserMedia({ audio: false, video: true });
      } catch (e) {
        console.warn(TAG, 'getUserMedia video unavailable, skip: ' + JSON.stringify(e));
        return;
      }
      const caller = pcf.createPeerConnection(createStunServerConfiguration());
      const callee = pcf.createPeerConnection(createStunServerConfiguration());
      const videoTracks = stream.getVideoTracks();
      expect(videoTracks.length).assertEqual(1);
      caller.addTrack(videoTracks[0]);

      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await sleep(2000);

      expect(caller.signalingState).assertEqual('stable');
      expect(callee.signalingState).assertEqual('stable');
      expect(callee.getReceivers().length).assertLarger(0);

      caller.close();
      callee.close();
      for (const track of videoTracks) {
        track.stop();
      }
    })
  })
}
