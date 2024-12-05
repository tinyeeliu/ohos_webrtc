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
 
import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium'
import webrtc from 'libohos_webrtc.so';
import { Logging, LoggingSeverity } from '../../../main/ets/log/Logging';
import { sleep } from './Helper';

const DEFAULT = 0
const TAG: string = '[RTCIceTransportTest]';

const pcf = new webrtc.PeerConnectionFactory();

let caller: webrtc.RTCPeerConnection;
let callee: webrtc.RTCPeerConnection;

const STUN_SERVER = "stun:stun.l.google.com:19302";

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

async function createIceTransport(caller: webrtc.RTCPeerConnection,
  callee: webrtc.RTCPeerConnection): Promise<webrtc.RTCIceTransport> {
  let audioSource = pcf?.createAudioSource();
  let audioTrack = pcf?.createAudioTrack("audio", audioSource);

  caller.addTrack(audioTrack);
  exchangeIceCandidates(caller, callee);
  await exchangeOfferAnswer(caller, callee);
  let transport = caller.getSenders()[0]!.transport!.iceTransport;
  return transport;
}

export default function RTCIceTransportTest() {
  describe('RTCIceTransportTest', () => {
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

    it("unconnectedIceTransportShouldHaveEmptySelectedPair", DEFAULT, async () => {
      caller.createDataChannel('');
      let offer = await caller.createOffer();
      await caller.setLocalDescription(offer);
      await callee.setRemoteDescription(offer);
      let answer = await callee.createAnswer();
      await caller.setRemoteDescription(answer);
      let iceTransport = caller.sctp?.transport.iceTransport;
      expect(iceTransport?.getSelectedCandidatePair()).assertNull();
    })

    it("iceTransportShouldBeInStateNewInitially", DEFAULT, async () => {
      const transceiver = caller.addTransceiver('audio');
      await caller.setLocalDescription();
      const iceTransport = transceiver.sender.transport?.iceTransport;
      expect(iceTransport?.state).assertEqual('new');
    })

    it("afterInitializationAllAttributesAreNotNull", DEFAULT, async () => {
      let transport = await createIceTransport(caller, callee);
      while (transport.state != 'connected') {
        await sleep(100);
      }
      console.info(TAG, 'transport.state: ', transport.state);
      expect(transport.getSelectedCandidatePair()).not().assertNull();
    })

    it("onstatechange", DEFAULT, async () => {
      let transport = await createIceTransport(caller, callee);
      let events: string[] = [];
      let promise = new Promise<void>((resolve) => {
        transport.onstatechange = () => {
          events.push(transport.state);
          console.info(TAG, 'state change: ', transport.state);
          resolve();
        };
      });
      await promise;
      expect(events.length).not().assertEqual(0);
    })

    it("onSelectedCandidatePairChange", DEFAULT, async () => {
      let transport = await createIceTransport(caller, callee);
      let events: string[] = [];
      let promise = new Promise<void>((resolve) => {
        transport.onselectedcandidatepairchange = () => {
          events.push("1");
          resolve();
        };
      });
      await promise;
      expect(events.length).not().assertEqual(0);
    })

  })
}