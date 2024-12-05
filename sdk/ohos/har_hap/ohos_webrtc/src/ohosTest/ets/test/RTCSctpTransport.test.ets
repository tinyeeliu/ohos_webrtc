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
const TAG: string = '[RTCSctpTransportTest]';

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

export default function RTCSctpTransportTest() {
  describe('RTCSctpTransportTest', () => {
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

    it("anUnconnectedPeerConnectionMustNotHaveMaxChannelsSet", DEFAULT, async () => {
      caller.createDataChannel('test');
      const offer = await caller.createOffer();
      await caller.setRemoteDescription(offer);
      const answer = await caller.createAnswer();
      await caller.setLocalDescription(answer);

      console.info(TAG, "state: ", caller.sctp?.state);
      expect(caller.sctp?.maxChannels).assertUndefined();
    })

    it("maxChannelsGetsInstantiatedAfterConnecting", DEFAULT, async () => {
      let audioSource = pcf?.createAudioSource();
      let audioTrack = pcf?.createAudioTrack("audio", audioSource);

      caller.addTrack(audioTrack);

      caller.createDataChannel("myDataChannel");
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      console.info(TAG, "maxChannelsGetsInstantiatedAfterConnecting state: ", caller.sctp?.state);
      // wait the SCTP transport goes into the "connected" state
      // await sleep(1000);
      //
      // await new Promise<void>((resolve) => {
      //   caller.sctp!.onstatechange = () => {
      //     if (caller.sctp?.state == 'connected') {
      //       resolve();
      //     }
      //   }
      // });
      while (true) {
        if (caller.sctp?.state === 'connected') {
          break;
        }
        await sleep(100);
      }
      console.info(TAG, "maxChannelsGetsInstantiatedAfterConnecting state: ", caller.sctp?.state);
      expect(caller.sctp?.maxChannels).assertEqual(callee.sctp?.maxChannels);
    })

    it("maxMessageSizeAfterInitializationNotNull", DEFAULT, async () => {
      caller.createDataChannel('');
      await exchangeOfferAnswer(caller, callee);
      console.info(TAG, "maxMessageSize: ", caller.sctp?.maxMessageSize);
      expect(caller.sctp?.maxMessageSize).not().assertUndefined();
    })

    it("RTCDtlsTransportAfterInitializationNotNull", DEFAULT, async () => {
      let audioSource = pcf?.createAudioSource();
      let audioTrack = pcf?.createAudioTrack("audio", audioSource);

      caller.addTrack(audioTrack);

      caller.createDataChannel("myDataChannel");
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      expect(caller.sctp?.transport).not().assertUndefined();
    })

    it("whenTheStateChangesTheEventWillTrigger", DEFAULT, async () => {
      let audioSource = pcf?.createAudioSource();
      let audioTrack = pcf?.createAudioTrack("audio", audioSource);

      caller.addTrack(audioTrack);

      caller.createDataChannel("myDataChannel");
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      let flag = false;
      let promise = new Promise<void>((resolve) => {
        caller.sctp!.onstatechange = () => {
          flag = true;
          resolve();
        }
      });
      await promise;
      expect(flag).assertTrue();
    })
  })
}