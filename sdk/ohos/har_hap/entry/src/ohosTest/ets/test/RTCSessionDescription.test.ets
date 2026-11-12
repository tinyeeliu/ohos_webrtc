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

/** 测试类型： unit（单元测试） */

import { describe, beforeAll, it, expect } from '@ohos/hypium'
import webrtc, { Logging, LoggingSeverity } from '@ohos/webrtc';

const TAG: string = '[RTCSessionDescriptionTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function RTCSessionDescriptionTest() {
  describe('RTCSessionDescriptionTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('shouldMatchSdpFromRealOfferToLocalDescription', 0, async () => {
      let pc = pcf.createPeerConnection({});
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      pc.addTrack(audioTrack);
      let offerInit = await pc.createOffer();
      await pc.setLocalDescription(offerInit);
      let localDesc = pc.localDescription;
      expect(localDesc).not().assertNull();
      expect(localDesc!.type).assertEqual('offer');
      expect(localDesc!.sdp.length > 0).assertTrue();
      expect(localDesc!.sdp).assertContain('m=audio');
      pc.close();
      audioSource.release();
    })

    it('shouldSetRemoteDescriptionWithRTCSessionDescriptionObject', 0, async () => {
      let caller = pcf.createPeerConnection({});
      let callee = pcf.createPeerConnection({});

      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      caller.addTrack(audioTrack);

      let offerInit = await caller.createOffer();
      await caller.setLocalDescription(offerInit);

      let offerDesc = new webrtc.RTCSessionDescription({
        type: offerInit.type,
        sdp: offerInit.sdp
      });
      expect(offerDesc.type).assertEqual('offer');
      expect(offerDesc.sdp).assertEqual(offerInit.sdp);

      await callee.setRemoteDescription(offerDesc);
      expect(callee.signalingState).assertEqual('have-remote-offer');

      let answerInit = await callee.createAnswer();
      await callee.setLocalDescription(answerInit);

      let answerDesc = new webrtc.RTCSessionDescription({
        type: answerInit.type,
        sdp: answerInit.sdp
      });
      expect(answerDesc.type).assertEqual('answer');

      await caller.setRemoteDescription(answerDesc);
      expect(caller.signalingState).assertEqual('stable');

      caller.close();
      callee.close();
      audioSource.release();
    })

    it('shouldReturnValidRTCSessionDescriptionInitFromToJSON', 0, async () => {
      let pc = pcf.createPeerConnection({});
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      pc.addTrack(audioTrack);
      let offerInit = await pc.createOffer();
      await pc.setLocalDescription(offerInit);
      let localDesc = pc.localDescription;
      expect(localDesc).not().assertNull();
      let json = localDesc!.toJSON();
      expect(json.type).assertEqual('offer');
      expect(json.sdp).assertEqual(localDesc!.sdp);

      let reconstructedDesc = new webrtc.RTCSessionDescription({
        type: json.type,
        sdp: json.sdp
      });
      expect(reconstructedDesc.type).assertEqual(json.type);
      expect(reconstructedDesc.sdp).assertEqual(json.sdp);

      pc.close();
      audioSource.release();
    })
  })
}
