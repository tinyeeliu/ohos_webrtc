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

import { describe, beforeAll, it, expect } from '@ohos/hypium'
import webrtc from 'libohos_webrtc.so';
import { Logging, LoggingSeverity } from '../../../main/ets/log/Logging';

const TAG: string = '[MediaStreamTrackTest]';


const pcf = new webrtc.PeerConnectionFactory();

export default function MediaStreamTrackTest() {
  describe('MediaStreamTrackTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('enabledFalseDoesNotChangeReadyState', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack.readyState).assertEqual('live');
      audioTrack.enabled = false;
      expect(audioTrack.readyState).assertEqual('live');
      audioSource.release();
    })

    it('stopChangesReadyStateToEnded', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack.readyState).assertEqual('live');
      audioTrack.stop();
      expect(audioTrack.readyState).assertEqual('ended');
      audioSource.release();
    })

    it('getSourceAfterStopStillReturnsSource', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      audioTrack.stop();
      let source = audioTrack.getSource();
      expect(source).not().assertNull();
      audioSource.release();
    })

    it('addTrackToPeerConnectionCreatesSenderWithTrack', 0, () => {
      let pc = pcf.createPeerConnection({});
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let sender = pc.addTrack(audioTrack);
      expect(sender.track).not().assertNull();
      expect(sender.track!.id).assertEqual(audioTrack.id);
      expect(sender.track!.kind).assertEqual('audio');
      pc.close();
      audioSource.release();
    })

    it('stopTrackRemovesItFromSender', 0, () => {
      let pc = pcf.createPeerConnection({});
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let sender = pc.addTrack(audioTrack);
      expect(sender.track).not().assertNull();
      audioTrack.stop();
      expect(sender.track).assertNull();
      pc.close();
      audioSource.release();
    })

    it('replaceTrackUpdatesSenderTrack', 0, async () => {
      let pc = pcf.createPeerConnection({});
      let audioSource1 = pcf.createAudioSource();
      let audioTrack1 = pcf.createAudioTrack('audio1', audioSource1);
      let audioSource2 = pcf.createAudioSource();
      let audioTrack2 = pcf.createAudioTrack('audio2', audioSource2);
      let sender = pc.addTrack(audioTrack1);
      expect(sender.track!.id).assertEqual(audioTrack1.id);
      await sender.replaceTrack(audioTrack2);
      expect(sender.track!.id).assertEqual(audioTrack2.id);
      pc.close();
      audioSource1.release();
      audioSource2.release();
    })

    it('calleeReceivesTrackViaOntrack', 0, async () => {
      let caller = pcf.createPeerConnection({
        iceServers: []
      });
      let callee = pcf.createPeerConnection({
        iceServers: []
      });

      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      caller.addTrack(audioTrack);

      caller.onicecandidate = (event) => {
        if (callee.signalingState !== 'closed' && callee.remoteDescription) {
          callee.addIceCandidate(event.candidate).catch(() => {});
        }
      };
      callee.onicecandidate = (event) => {
        if (caller.signalingState !== 'closed' && caller.remoteDescription) {
          caller.addIceCandidate(event.candidate).catch(() => {});
        }
      };

      let ontrackResolve: ((track: webrtc.MediaStreamTrack) => void) | null = null;
      let ontrackPromise = new Promise<webrtc.MediaStreamTrack>((resolve) => {
        ontrackResolve = resolve;
      });
      callee.ontrack = (event) => {
        if (ontrackResolve) {
          ontrackResolve(event.track);
          ontrackResolve = null;
        }
      };

      await caller.setLocalDescription(await caller.createOffer());
      await callee.setRemoteDescription(caller.localDescription);
      await callee.setLocalDescription(await callee.createAnswer());
      await caller.setRemoteDescription(callee.localDescription);

      let timeoutPromise = new Promise<null>((resolve) => {
        setTimeout(() => resolve(null), 10000);
      });
      let track = await Promise.race([ontrackPromise, timeoutPromise]);
      expect(track).not().assertNull();
      expect(track!.kind).assertEqual('audio');
      expect(track!.readyState).assertEqual('live');

      caller.close();
      callee.close();
      audioSource.release();
    })
  })
}
