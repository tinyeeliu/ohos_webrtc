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
import { validateRtpCapabilities } from './RTCRtpCapabilitiesHelper';
import { validateReceiverRtpParameters } from './RTCRtpParametersHelper';
import * as helper from './Helper';

const TAG: string = '[RTCRtpReceiverTest]';
const STUN_SERVER = "stun:stun.l.google.com:19302";

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

export default function RTCRtpReceiverTest() {
  describe('RTCRtpReceiverTest', () => {
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
      });
      callee = pcf.createPeerConnection({
        iceServers: [{
          urls: STUN_SERVER
        }]
      });
    })
    afterEach(() => {
      // Presets a clear action, which is performed after each unit test case ends.
      // The number of execution times is the same as the number of test cases defined by **it**.
      // This API supports only one parameter: clear action function.
      if (caller) {
        caller.close();
      }
      if (callee) {
        callee.close();
      }
    })
    afterAll(() => {
      // Presets a clear action, which is performed after all test cases of the test suite end.
      // This API supports only one parameter: clear action function.
    })

    it('getCapabilitiesWithAudioKind', 0, () => {
      let capabilities = webrtc.RTCRtpReceiver.getCapabilities('audio');
      console.debug(TAG, "getCapabilities: " + JSON.stringify(capabilities));
      // expect(capabilities).not().assertNull();
      validateRtpCapabilities(capabilities!);
    })

    it('getCapabilitiesWithVideoKind', 0, () => {
      let capabilities = webrtc.RTCRtpReceiver.getCapabilities('video');
      console.debug(TAG, "getCapabilities: " + JSON.stringify(capabilities));
      // expect(capabilities).assertNull();
      validateRtpCapabilities(capabilities!);
    })

    it('getCapabilitiesWithInvalidKind', 0, () => {
      let capabilities = webrtc.RTCRtpReceiver.getCapabilities('whatever');
      console.debug(TAG, "getCapabilities: " + JSON.stringify(capabilities));
      expect(capabilities).assertNull();
    })

    it('getTransportInitially', 0, () => {
      const transceiver = caller.addTransceiver('audio', { direction: 'recvonly' });
      expect(transceiver.receiver!.transport).assertNull();
    })

    it('getTransportAfterSetLocalDescription', 0, async () => {
      const transceiver = caller.addTransceiver('audio', { direction: 'recvonly' });
      await caller.setLocalDescription();
      expect(transceiver.receiver!.transport instanceof webrtc.RTCDtlsTransport).assertTrue();
    })

    it('getParametersWithAudioReceiver', 0, async () => {
      caller.addTransceiver('audio');

      await doOfferAnswerExchange();

      const transceivers = callee.getTransceivers();
      expect(transceivers.length).assertLarger(0);
      const transceiver = transceivers[0];
      expect(transceiver.receiver).not().assertUndefined();

      const param = transceiver.receiver!.getParameters();
      validateReceiverRtpParameters(param);
      expect(param.headerExtensions.length).assertLarger(0);
      expect(param.codecs.length).assertLarger(0);
    })

    it('getParametersWithVideoReceiver', 0, async () => {
      caller.addTransceiver('video');

      await doOfferAnswerExchange();

      const transceivers = callee.getTransceivers();
      expect(transceivers.length).assertLarger(0);
      const transceiver = transceivers[0];
      expect(transceiver.receiver).not().assertUndefined();

      const param = transceiver!.receiver!.getParameters();
      validateReceiverRtpParameters(param);
      expect(param.headerExtensions.length).assertLarger(0);
      expect(param.codecs.length).assertLarger(0);
    })

    it('getParametersWithSimulcastVideoReceiver', 0, async () => {
      caller.addTransceiver('audio', {
        sendEncodings: [
          {
            rid: "rid1"
          },
          {
            rid: "rid2"
          }
        ]
      });

      await doOfferAnswerExchange();

      const transceivers = callee.getTransceivers();
      expect(transceivers.length).assertLarger(0);
      const transceiver = transceivers[0];
      expect(transceiver.receiver).not().assertUndefined();

      const param = transceiver!.receiver!.getParameters();
      validateReceiverRtpParameters(param);
      expect(param.headerExtensions.length).assertLarger(0);
      expect(param.codecs.length).assertLarger(0);
    })

    it('getStats', 0, async () => {
      const stream = await getMediaStream();
      const tracks = stream.getTracks();
      callee.addTrack(tracks[0], stream);

      const receiver = caller.addTransceiver('audio').receiver;

      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await waitForTrackUnmuted(receiver!.track);
      const statsReport = await receiver!.getStats();

      let found = false;
      statsReport.stats.forEach((value: webrtc.RTCStats, _key: string) => {
        if (value.type === 'inbound-rtp') {
          found = true;
        }
      });
      expect(found).assertTrue();
    })
  })
}
