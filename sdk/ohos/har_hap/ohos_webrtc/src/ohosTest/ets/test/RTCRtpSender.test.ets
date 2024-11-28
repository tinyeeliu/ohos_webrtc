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
 
import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium';
import webrtc from 'libohos_webrtc.so';
import { Logging, LoggingSeverity } from '../../../main/ets/log/Logging';
import { sleep } from './Helper';
import { validateRtpCapabilities } from './RTCRtpCapabilitiesHelper';

const TAG: string = '[RTCRtpSenderTest]';
const STUN_SERVER = "stun:stun.l.google.com:19302";

const pcf = new webrtc.PeerConnectionFactory();

let caller: webrtc.RTCPeerConnection;

let audioRtpSender: webrtc.RTCRtpSender;

async function createAudioRtpSender() {
  let audioSource = pcf.createAudioSource();
  let audioTrack = pcf.createAudioTrack("audio", audioSource);
  audioRtpSender = caller.addTrack(audioTrack);
}

// async function destroyAudioRtpSender() {
//   pc.removeTrack(audioRtpSender);
//   audioRtpSender = null;
// }

export default function RTCRtpSenderTest() {
  describe('RTCRtpSenderTest', () => {
    // Defines a test suite. Two parameters are supported: test suite name and test suite function.
    beforeAll(() => {
      // Presets an action, which is performed only once before all test cases of the test suite start.
      // This API supports only one parameter: preset action function.
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })
    beforeEach(() => {
      // Presets an action, which is performed before each unit test case starts.
      // The number of execution times is the same as the number of test cases defined by **it**.
      // This API supports only one parameter: preset action function.
      caller = pcf.createPeerConnection({
        iceServers: [{
          urls: STUN_SERVER
        }]
      })
    })
    afterEach(() => {
      // Presets a clear action, which is performed after each unit test case ends.
      // The number of execution times is the same as the number of test cases defined by **it**.
      // This API supports only one parameter: clear action function.
      if (caller) {
        caller.close();
      }
    })
    afterAll(() => {
      // Presets a clear action, which is performed after all test cases of the test suite end.
      // This API supports only one parameter: clear action function.
    })

    it('getCapabilitiesWithAudioKind', 0, () => {
      let capabilities = webrtc.RTCRtpSender.getCapabilities('audio');
      console.debug(TAG, "getCapabilities: " + JSON.stringify(capabilities));
      validateRtpCapabilities(capabilities!);
    })

    it('getCapabilitiesWithVideoKind', 0, () => {
      let capabilities = webrtc.RTCRtpSender.getCapabilities('video');
      console.debug(TAG, "getCapabilities: " + JSON.stringify(capabilities));
      validateRtpCapabilities(capabilities!);
    })

    it('getCapabilitiesWithInvalidKind', 0, () => {
      let capabilities = webrtc.RTCRtpSender.getCapabilities('whatever');
      console.debug(TAG, "getCapabilities: " + JSON.stringify(capabilities));
      expect(capabilities).assertNull();
    })

    it('dtmfOfVideoSenderIsNull', 0, async () => {
      const t1 = caller.addTransceiver("audio");
      const t2 = caller.addTransceiver("video");

      expect(t1.sender!.dtmf).not().assertNull();
      expect(t2.sender!.dtmf).assertNull();
    })

    it('getTransportInitially', 0, async () => {
      const transceiver = caller.addTransceiver('audio', {
        direction: 'recvonly'
      });

      expect(transceiver.sender!.transport).assertNull();
    })

    it('getTransportAfterSetLocalDescription', 0, async () => {
      const transceiver = caller.addTransceiver('audio', {
        direction: 'recvonly'
      });
      await caller.setLocalDescription();
      expect(transceiver.sender!.transport instanceof webrtc.RTCDtlsTransport).assertTrue();
    })

    it('canGetTrack', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack("audio", audioSource);
      audioRtpSender = caller.addTrack(audioTrack);

      expect(audioRtpSender).not().assertUndefined();
      expect(audioRtpSender.track).not().assertUndefined();
      expect(audioTrack.id).assertEqual(audioRtpSender.track!.id);
    })

    it('canSetParameters', 0, async () => {
      createAudioRtpSender();

      let parameters = audioRtpSender.getParameters();
      expect(parameters.encodings.length).assertEqual(1);

      try {
        await audioRtpSender.setParameters(parameters);
      } catch (e) {
        console.error(TAG, "canSetParameters: " + JSON.stringify(e));
        expect().assertFail();
      }
    })

    it('setParametersWhenTransceiverIsStopped', 0, async () => {
      const transceiver = caller.addTransceiver('audio');
      const sender = transceiver.sender;

      const param = sender!.getParameters();
      transceiver.stop();

      try {
        await sender!.setParameters(param);
        expect().assertFail();
      } catch (e) {
        console.error(TAG, "setParameters rejected: " + JSON.stringify(e));
      }
    })

    it('setParametersWithSame', 0, async () => {
      const transceiver = caller.addTransceiver('audio');
      const sender = transceiver.sender;

      const param = sender!.getParameters();

      await sender!.setParameters(param);
    })

    it('replaceTrackWithClosedConnection', 0, async () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack("audio", audioSource);

      const transceiver = caller.addTransceiver('audio');
      const sender = transceiver.sender;
      caller.close();

      try {
        await sender!.replaceTrack(audioTrack);
        expect().assertFail();
      } catch (e) {
        console.error(TAG, 'failed to replace track - ' + e);
      }
    })

    it('replaceTrackWithStoppedTransceiver', 0, async () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack("audio", audioSource);

      const transceiver = caller.addTransceiver('audio');
      const sender = transceiver.sender;

      transceiver.stop();

      try {
        await sender!.replaceTrack(audioTrack);
        expect().assertFail();
      } catch (e) {
        console.error(TAG, 'failed to replace track - ' + e);
      }
    })

    it('replaceTrackWithDifferentKind', 0, async () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack("audio", audioSource);

      const transceiver = caller.addTransceiver('video');
      const sender = transceiver.sender;
      caller.close();

      try {
        await sender!.replaceTrack(audioTrack);
        expect().assertFail();
      } catch (e) {
        console.error(TAG, 'failed to replace track - ' + e);
      }
    })

    it('replaceTrackWithNull', 0, async () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack("audio", audioSource);

      const transceiver = caller.addTransceiver(audioTrack);
      const sender = transceiver.sender;

      expect(sender!.track!.id).assertEqual(audioTrack.id);

      try {
        await sender!.replaceTrack(null);
        expect(sender!.track).assertNull();
      } catch (e) {
        expect().assertFail();
      }
    })
  })
}
