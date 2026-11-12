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

import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium';
import webrtc, { Logging, LoggingSeverity } from '@ohos/webrtc';
import * as helper from './Helper';

const TAG: string = '[RTCRtpTransceiverTest]';

const pcf = new webrtc.PeerConnectionFactory();

let caller: webrtc.RTCPeerConnection;
let callee: webrtc.RTCPeerConnection;

async function doOfferAnswerExchange() {
  const offer = await caller.createOffer();
  await caller.setLocalDescription(offer);
  await callee.setRemoteDescription(offer);
  const answer = await callee.createAnswer();
  await callee.setLocalDescription(answer);
  await caller.setRemoteDescription(answer);
}

async function getMediaStream() {
  let audioSource = pcf.createAudioSource();
  let audioTrack = pcf.createAudioTrack("audio", audioSource);
  return new webrtc.MediaStream([audioTrack]);
}

function exchangeIceCandidates(pc1: webrtc.RTCPeerConnection, pc2: webrtc.RTCPeerConnection) {
  const doExchange = (pc1: webrtc.RTCPeerConnection, pc2: webrtc.RTCPeerConnection) => {
    pc1.onicecandidate = (event) => {
      const candidate = event.candidate;
      if (pc2.signalingState !== 'closed') {
        pc2.addIceCandidate(candidate);
      }
    };
  };

  doExchange(pc1, pc2);
  doExchange(pc2, pc1);
}

async function exchangeOffer(caller: webrtc.RTCPeerConnection, callee: webrtc.RTCPeerConnection) {
  await caller.setLocalDescription(await caller.createOffer());
  console.log(TAG, "caller.localDescription: " + caller.localDescription!.type);
  await callee.setRemoteDescription(caller.localDescription);
}

async function exchangeAnswer(caller: webrtc.RTCPeerConnection, callee: webrtc.RTCPeerConnection) {
  // 必须先执行caller.setRemoteDescription，否则callee的candidates有可能在caller的remote description设置之前到达
  const answer = await callee.createAnswer();
  await caller.setRemoteDescription(answer);
  await callee.setLocalDescription(answer);
}

async function exchangeOfferAnswer(caller: webrtc.RTCPeerConnection, callee: webrtc.RTCPeerConnection) {
  await exchangeOffer(caller, callee);
  await exchangeAnswer(caller, callee);
}

// 等待pc.connectionState变为'connected'.
async function listenToConnected(pc: webrtc.RTCPeerConnection) {
  while (pc.connectionState != 'connected') {
    await new Promise<webrtc.Event>((resolve) => {
      pc.onconnectionstatechange = resolve;
    });
    pc.onconnectionstatechange = null;
  }
}

async function waitForTrackUnmuted(_track: webrtc.MediaStreamTrack) {
  console.log(TAG, 'waitForTrackUnmuted');
  // 暂未提供muted属性和onunmute事件等
  await helper.sleep(1000);
}

// async function generateAnswer(offer) {
//   const pc = new RTCPeerConnection();
//   await pc.setRemoteDescription(offer);
//   const answer = await pc.createAnswer();
//   pc.close();
//   return answer;
// }

export default function RTCRtpTransceiverTest() {
  describe('RTCRtpTransceiverTest', () => {
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
      caller = pcf.createPeerConnection({});
      callee = pcf.createPeerConnection({});
    })
    afterEach(() => {
      // Presets a clear action, which is performed after each unit test case ends.
      // The number of execution times is the same as the number of test cases defined by **it**.
      // This API supports only one parameter: clear action function.
      caller.close();
      callee.close();
    })
    afterAll(() => {
      // Presets a clear action, which is performed after all test cases of the test suite end.
      // This API supports only one parameter: clear action function.
    })

    it('shouldAddTransceiverWithoutTrack', 0, async () => {
      expect(caller.getTransceivers().length).assertEqual(0);

      caller.addTransceiver("audio");

      const transceivers = caller.getTransceivers();
      expect(transceivers.length).assertEqual(1);

      const transceiver = transceivers[0];
      expect(transceiver.mid).assertNull();
      expect(transceiver.direction).assertEqual('sendrecv');
      expect(transceiver.currentDirection).assertNull();
      expect(transceiver.sender).not().assertNull();
      expect(transceiver.sender.track).assertNull();
      expect(transceiver.receiver).not().assertNull();
      expect(transceiver.receiver.track).not().assertNull();
      expect(transceiver.receiver.track.kind).assertEqual("audio");
      expect(transceiver.receiver.track.readyState).assertEqual("live");
    })

    it('shouldAddTransceiverWithTrack', 0, async () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack("audio", audioSource);

      caller.addTransceiver(audioTrack);

      const transceivers = caller.getTransceivers();
      expect(transceivers.length).assertEqual(1);

      const transceiver = transceivers[0];
      expect(transceiver.mid).assertNull();
      expect(transceiver.direction).assertEqual('sendrecv');
      expect(transceiver.currentDirection).assertNull();
      expect(transceiver.sender).not().assertNull();
      expect(transceiver.sender.track).not().assertNull();
      expect(transceiver.sender.track!.id).assertEqual(audioTrack.id);
      expect(transceiver.receiver).not().assertNull();
      expect(transceiver.receiver.track).not().assertNull();
      expect(transceiver.receiver.track.kind).assertEqual("audio");
    })

    it('shouldCreateTransceiverWhenAddTrack', 0, async () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack("audio", audioSource);

      caller.addTrack(audioTrack);

      const transceivers = caller.getTransceivers();
      expect(transceivers.length).assertEqual(1);

      const transceiver = transceivers[0];
      expect(transceiver.mid).assertNull();
      expect(transceiver.direction).assertEqual('sendrecv');
      expect(transceiver.currentDirection).assertNull();
      expect(transceiver.sender).not().assertNull();
      expect(transceiver.sender.track).not().assertNull();
      expect(transceiver.sender.track!.id).assertEqual(audioTrack.id);
      expect(transceiver.receiver).not().assertNull();
      expect(transceiver.receiver.track).not().assertNull();
      expect(transceiver.receiver.track.kind).assertEqual("audio");
    })

    it('shouldAddTransceiverWithDirection', 0, async () => {
      caller.addTransceiver("audio", {
        direction: "recvonly"
      });

      const transceivers = caller.getTransceivers();
      expect(transceivers.length).assertEqual(1);

      const transceiver = transceivers[0];
      expect(transceiver.mid).assertNull();
      expect(transceiver.direction).assertEqual('recvonly');
      expect(transceiver.currentDirection).assertNull();
      expect(transceiver.sender).not().assertNull();
      expect(transceiver.sender.track).assertNull();
      expect(transceiver.receiver).not().assertNull();
      expect(transceiver.receiver.track).not().assertNull();
      expect(transceiver.receiver.track.kind).assertEqual("audio");
    })

    it('shouldThrowWhenAddTransceiverWithBadKind', 0, async () => {
      try {
        caller.addTransceiver("other");
        expect().assertFail();
      } catch (e) {
        // should throw TypeError
      }
    })

    it('shouldSetAndGetDirection', 0, async () => {
      caller.addTransceiver("audio");
      expect(caller.getTransceivers().length).assertEqual(1);

      caller.getTransceivers()[0].direction = "sendonly";
      expect(caller.getTransceivers()[0].direction).assertEqual("sendonly");
      caller.getTransceivers()[0].direction = "recvonly";
      expect(caller.getTransceivers()[0].direction).assertEqual("recvonly");
      caller.getTransceivers()[0].direction = "inactive";
      expect(caller.getTransceivers()[0].direction).assertEqual("inactive");
      caller.getTransceivers()[0].direction = "sendrecv";
      expect(caller.getTransceivers()[0].direction).assertEqual("sendrecv");
    })

    it('shouldNegotiateCurrentDirectionViaOfferAnswer', 0, async () => {
      const transceiver = caller.addTransceiver('audio', {
        direction: 'recvonly'
      });
      expect(transceiver.direction).assertEqual("recvonly");
      expect(transceiver.currentDirection).assertNull();

      const offer = await caller.createOffer();
      await caller.setLocalDescription(offer);
      await callee.setRemoteDescription(offer);

      const answer = await callee.createAnswer();
      await caller.setRemoteDescription(answer);

      expect(transceiver.currentDirection).assertEqual("inactive");
      transceiver.direction = 'sendrecv';
      expect(transceiver.direction).assertEqual("sendrecv");
      expect(transceiver.currentDirection).assertEqual("inactive");
    })

    it('shouldExcludeStoppedTransceiverFromOffer', 0, async () => {
      caller.addTransceiver("audio", { direction: "sendonly" });
      caller.addTransceiver("video");
      caller.getTransceivers()[0].stop();

      const offer = await caller.createOffer();

      console.log(TAG, 'offer.sdp!.includes("m=audio"): ' + offer.sdp!.includes("m=audio"));
      expect(offer.sdp!.includes("m=audio")).assertFalse();
      console.log(TAG, 'offer.sdp!.includes("m=video"): ' + offer.sdp!.includes("m=video"));
      expect(offer.sdp!.includes("m=video")).assertTrue();
    })

    it('shouldMarkStoppedSendOnlyTransceiverInactiveInOffer', 0, async () => {
      caller.addTransceiver("audio");

      await exchangeOfferAnswer(caller, callee);

      caller.getTransceivers()[0].direction = "sendonly";
      caller.getTransceivers()[0].stop();

      const offer = await caller.createOffer();

      // assert_true(offer.sdp.includes("a=inactive"), "The audio m-section should be inactive");
      expect(offer.sdp!.includes("a=inactive")).assertTrue();
    })

    it('shouldHaveNullTransportAfterStop', 0, async () => {
      caller.addTransceiver("audio", { direction: "sendonly" });
      caller.addTransceiver("video");

      const transceiver = caller.getTransceivers()[1];
      expect(transceiver.receiver.transport).assertNull();

      transceiver.stop();
      expect(transceiver.receiver.transport).assertNull();
    })

    it('shouldSetCodecPreferences', 0, () => {
      const transceiver1 = caller.addTransceiver('audio');
      const capabilities1 = webrtc.RTCRtpReceiver.getCapabilities('audio');
      expect(capabilities1).not().assertNull();
      transceiver1.setCodecPreferences(capabilities1!.codecs);

      const transceiver2 = caller.addTransceiver('video');
      const capabilities2 = webrtc.RTCRtpReceiver.getCapabilities('video');
      expect(capabilities2).not().assertNull();
      transceiver2.setCodecPreferences(capabilities2!.codecs);
    })

    it('shouldSetCodecPreferencesWithEmptyCodecs', 0, () => {
      const transceiver = caller.addTransceiver('audio');
      transceiver.setCodecPreferences([]);
    })

    it('shouldThrowWhenSetCodecPreferencesWithMismatchedKind', 0, () => {
      const transceiver = caller.addTransceiver('audio');
      const capabilities = webrtc.RTCRtpReceiver.getCapabilities('video');
      expect(capabilities).not().assertNull();
      try {
        transceiver.setCodecPreferences(capabilities!.codecs);
        expect().assertFail();
      } catch (e) {
        // should throw InvalidModificationError
        expect(e).assertInstanceOf('Error');
      }
    })

    it('shouldThrowWhenSetCodecPreferencesWithInvalidMimeType', 0, () => {
      const transceiver = caller.addTransceiver('audio');
      const codecs: webrtc.RTCRtpCodec[] = [{
        mimeType: 'data',
        clockRate: 2000,
        channels: 2,
        sdpFmtpLine: '0-15'
      }];

      try {
        transceiver.setCodecPreferences(codecs);
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('shouldThrowWhenSetCodecPreferencesWithUserDefinedCodec', 0, () => {
      const transceiver = caller.addTransceiver('audio');
      const codecs: webrtc.RTCRtpCodec[] = [{
        mimeType: 'audio/piepiper',
        clockRate: 2000,
        channels: 2,
        sdpFmtpLine: '0-15'
      }];

      try {
        transceiver.setCodecPreferences(codecs);
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('shouldThrowWhenSetCodecPreferencesWithModifiedCodecs', 0, () => {
      const transceiver = caller.addTransceiver('audio');
      const capabilities = webrtc.RTCRtpReceiver.getCapabilities('audio');

      const codecs = [capabilities!.codecs[0]];
      codecs[0].sdpFmtpLine = "modifiedparameter=1";

      try {
        transceiver.setCodecPreferences(codecs);
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })
  })
}
