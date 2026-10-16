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

import { describe, beforeAll, it, expect } from '@ohos/hypium'
import webrtc, { Logging, LoggingSeverity } from '@ohos/webrtc';
import { pollUntilOrFail } from './Helper';

const TAG: string = '[PeerConnectionRemoveTrackTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function PeerConnectionRemoveTrackTest() {
  describe('PeerConnectionRemoveTrackTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('onRemoveTrackPropertyInitiallyNull', 0, () => {
      const pc = pcf.createPeerConnection({ iceServers: [] });
      expect(pc.onremovetrack).assertNull();
      pc.close();
    })

    it('onRemoveTrackFiresWithReceiverAfterRemoveTrack', 0, async () => {
      const caller = pcf.createPeerConnection({ iceServers: [] });
      const callee = pcf.createPeerConnection({ iceServers: [] });
      const audioSource = pcf.createAudioSource();
      const audioTrack = pcf.createAudioTrack('audio', audioSource);
      const stream = new webrtc.MediaStream([audioTrack]);

      let trackReceiverId: string | null = null;
      callee.ontrack = (ev: webrtc.RTCTrackEvent) => {
        trackReceiverId = ev.receiver.id;
      };
      let removeFired = false;
      let removedReceiverId: string | null = null;
      callee.onremovetrack = (ev: webrtc.RTCRemoveTrackEvent) => {
        removeFired = true;
        removedReceiverId = ev.receiver.id;
        console.info(TAG, 'onremovetrack receiver: ' + ev.receiver.id);
        expect(ev.receiver).not().assertNull();
      };

      const sender = caller.addTrack(audioTrack, stream);
      caller.onicecandidate = (event) => {
        if (callee.signalingState !== 'closed') {
          callee.addIceCandidate(event.candidate);
        }
      };
      callee.onicecandidate = (event) => {
        if (caller.signalingState !== 'closed') {
          caller.addIceCandidate(event.candidate);
        }
      };

      await caller.setLocalDescription(await caller.createOffer());
      await callee.setRemoteDescription(caller.localDescription);
      const answer = await callee.createAnswer();
      await caller.setRemoteDescription(answer);
      await callee.setLocalDescription(answer);
      await pollUntilOrFail(() => trackReceiverId !== null, 'ontrack not fired');

      caller.removeTrack(sender);
      await caller.setLocalDescription(await caller.createOffer());
      await callee.setRemoteDescription(caller.localDescription);
      const reanswer = await callee.createAnswer();
      await caller.setRemoteDescription(reanswer);
      await callee.setLocalDescription(reanswer);
      await pollUntilOrFail(() => removeFired, 'onremovetrack not fired');

      expect(removedReceiverId).assertEqual(trackReceiverId);

      caller.close();
      callee.close();
      audioSource.release();
    })

    // ===== 异常参数: null 与抛异常处理器不得中断撤轨流程 =====

    it('nullHandlerDoesNotBreakRemoveTrackFlow', 0, async () => {
      const caller = pcf.createPeerConnection({ iceServers: [] });
      const callee = pcf.createPeerConnection({ iceServers: [] });
      const audioSource = pcf.createAudioSource();
      const audioTrack = pcf.createAudioTrack('audio', audioSource);

      callee.onremovetrack = null;
      const sender = caller.addTrack(audioTrack);
      await caller.setLocalDescription(await caller.createOffer());
      await callee.setRemoteDescription(caller.localDescription);
      const answer = await callee.createAnswer();
      await caller.setRemoteDescription(answer);
      await callee.setLocalDescription(answer);

      caller.removeTrack(sender);
      await caller.setLocalDescription(await caller.createOffer());
      await callee.setRemoteDescription(caller.localDescription);
      const reanswer = await callee.createAnswer();
      await caller.setRemoteDescription(reanswer);
      await callee.setLocalDescription(reanswer);
      expect(callee.signalingState).assertEqual('stable');

      caller.close();
      callee.close();
      audioSource.release();
    })

    it('throwingHandlerDoesNotBreakRemoveTrackFlow', 0, async () => {
      const caller = pcf.createPeerConnection({ iceServers: [] });
      const callee = pcf.createPeerConnection({ iceServers: [] });
      const audioSource = pcf.createAudioSource();
      const audioTrack = pcf.createAudioTrack('audio', audioSource);

      callee.onremovetrack = (ev: webrtc.RTCRemoveTrackEvent) => {
        throw new Error('handler error on purpose');
      };
      const sender = caller.addTrack(audioTrack);
      await caller.setLocalDescription(await caller.createOffer());
      await callee.setRemoteDescription(caller.localDescription);
      const answer = await callee.createAnswer();
      await caller.setRemoteDescription(answer);
      await callee.setLocalDescription(answer);

      caller.removeTrack(sender);
      await caller.setLocalDescription(await caller.createOffer());
      await callee.setRemoteDescription(caller.localDescription);
      const reanswer = await callee.createAnswer();
      await caller.setRemoteDescription(reanswer);
      await callee.setLocalDescription(reanswer);
      expect(callee.signalingState).assertEqual('stable');

      caller.close();
      callee.close();
      audioSource.release();
    })
  })
}
