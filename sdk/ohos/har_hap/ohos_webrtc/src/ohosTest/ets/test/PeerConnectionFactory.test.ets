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

const TAG: string = '[PeerConnectionFactoryTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function PeerConnectionFactoryTest() {
  describe('PeerConnectionFactoryTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('createPeerConnectionReturnsValidPc', 0, () => {
      let pc = pcf.createPeerConnection({
        iceServers: []
      });
      expect(pc.signalingState).assertEqual('stable');
      expect(pc.iceConnectionState).assertEqual('new');
      pc.close();
    })

    it('createAudioTrackProperties', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack.kind).assertEqual('audio');
      expect(audioTrack.readyState).assertEqual('live');
      expect(audioTrack.enabled).assertTrue();
      audioSource.release();
    })

    it('createVideoSourceWithConstraintsStateIsLive', 0, async () => {
      let constraints: webrtc.MediaTrackConstraints = {
        width: 640,
        height: 480,
        frameRate: 30,
      };
      let videoSource = pcf.createVideoSource(constraints);
      expect(videoSource).not().assertNull();

      let videoTrack = pcf.createVideoTrack('video', videoSource);
      let pc = pcf.createPeerConnection({});
      pc.addTrack(videoTrack);
      let offer = await pc.createOffer();
      expect(offer.type).assertEqual('offer');
      expect(offer.sdp).assertContain('m=video');
      pc.close();
      try {
        videoSource.release();
      } catch (e) {
      }
    })

    it('startAecDumpWithInvalidFdReturnsFalse', 0, () => {
      let result = pcf.startAecDump(-1, 1024);
      expect(result).assertFalse();
    })

    it('stopAecDumpDoesNotCrash', 0, () => {
      pcf.stopAecDump();
    })

    it('stopAecDumpAfterStartDoesNotCrash', 0, () => {
      pcf.startAecDump(0, 1024);
      pcf.stopAecDump();
    })

    it('setDefaultDoesNotCrash', 0, () => {
      let factory = new webrtc.PeerConnectionFactory();
      webrtc.PeerConnectionFactory.setDefault(factory);
    })
  })
}
