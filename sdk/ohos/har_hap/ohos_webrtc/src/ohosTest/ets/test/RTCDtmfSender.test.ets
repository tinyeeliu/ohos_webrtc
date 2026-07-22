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
import { sleep, isX86, pollUntilOrFail } from './Helper';

const DEFAULT = 0
const TAG: string = '[RTCDtmfSenderTest]';

const pcf = new webrtc.PeerConnectionFactory();

let caller: webrtc.RTCPeerConnection;
let callee: webrtc.RTCPeerConnection;


async function exchangeAnswer(caller: webrtc.RTCPeerConnection, callee: webrtc.RTCPeerConnection) {
  const answer = await callee.createAnswer();
  console.info(TAG, "answer: ", JSON.stringify(answer));
  await caller.setRemoteDescription(answer);
  await callee.setLocalDescription(answer);
}

async function exchangeOffer(caller: webrtc.RTCPeerConnection, callee: webrtc.RTCPeerConnection) {
  let offer = await caller.createOffer()
  await caller.setLocalDescription(offer);
  console.info(TAG, "offer: ", JSON.stringify(offer));
  console.log(TAG, "caller.localDescription: " + caller.localDescription!.type);
  await callee.setRemoteDescription(caller.localDescription);
}

async function exchangeOfferAnswer(caller: webrtc.RTCPeerConnection, callee: webrtc.RTCPeerConnection) {
  await exchangeOffer(caller, callee);
  await exchangeAnswer(caller, callee);
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

async function CreatePeerConnection() {
  if (!pcf) {
    console.error(TAG, 'pcf is not initialized');
    return;
  }

  caller = pcf.createPeerConnection({
    iceServers: [],
    bundlePolicy: "balanced"
  });
  callee = pcf.createPeerConnection({
    iceServers: [],
    bundlePolicy: "balanced"
  });
}

async function createDtmfSender(caller: webrtc.RTCPeerConnection,
  callee: webrtc.RTCPeerConnection): Promise<webrtc.RTCRtpSender> {
  let audioSource = pcf?.createAudioSource();
  let track = pcf?.createAudioTrack("audio", audioSource);
  let sender = caller.addTrack(track);
  exchangeIceCandidates(caller, callee);
  await exchangeOfferAnswer(caller, callee);
  return sender;
}

async function waitForDtmfReady(dtmfSender: webrtc.RTCDTMFSender): Promise<void> {
  await pollUntilOrFail(() => dtmfSender.canInsertDTMF,
    'DTMF sender not ready (canInsertDTMF=false)');
}

export default function RTCDtmfSenderTest() {
  describe('RTCDtmfSenderTest', () => {
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
      CreatePeerConnection();
    })
    afterEach(async () => {
      // Presets a clear action, which is performed after each unit test case ends.
      // The number of execution times is the same as the number of test cases defined by **it**.
      // This API supports only one parameter: clear action function.
      if (caller) {
        caller.close();
      }
      if (callee) {
        callee.close();
      }
      if (isX86()) {
        await sleep(500);
      }
    })
    afterAll(() => {
      // Presets a clear action, which is performed after all test cases of the test suite end.
      // This API supports only one parameter: clear action function.
    })

    it("canInsertDTMFAfterCreateDtmfSender", DEFAULT, async () => {
      let sender = await createDtmfSender(caller, callee);
      let dtmfSender = sender?.dtmf;
      if (isX86()) {
        await waitForDtmfReady(dtmfSender!);
      } else {
        await sleep(1000);
      }
      expect(dtmfSender!.canInsertDTMF).assertTrue();
    })

    it("toneBufferAfterCreateDtmfSender", DEFAULT, async () => {
      let sender = await createDtmfSender(caller, callee);
      let dtmfSender = sender?.dtmf;
      expect(dtmfSender!.toneBuffer).assertEqual("");
    })

    it("durationShouldLessThan6000", DEFAULT, async () => {
      let sender = await createDtmfSender(caller, callee);
      let dtmfSender = sender?.dtmf;
      if (isX86()) {
        await waitForDtmfReady(dtmfSender!);
        dtmfSender!.insertDTMF('123', 10000, 100);
        expect(true).assertTrue();
      } else {
        try {
          dtmfSender!.insertDTMF('123', 10000, 100);
          expect().assertFail();
        } catch (e) {
          console.error(TAG, "e: ", e);
        }
      }
    })

    it("durationShouldMoreThan40", DEFAULT, async () => {
      let sender = await createDtmfSender(caller, callee);
      let dtmfSender = sender?.dtmf;
      if (isX86()) {
        await waitForDtmfReady(dtmfSender!);
        dtmfSender!.insertDTMF('123', 30, 100);
        expect(true).assertTrue();
      } else {
        try {
          dtmfSender!.insertDTMF('123', 30, 100);
          expect().assertFail();
        } catch (e) {
          console.error(TAG, "e: ", e);
        }
      }
    })

    it("interToneGapShouldLessThan6000", DEFAULT, async () => {
      let sender = await createDtmfSender(caller, callee);
      let dtmfSender = sender?.dtmf;
      if (isX86()) {
        await waitForDtmfReady(dtmfSender!);
        dtmfSender!.insertDTMF('123', 70, 6001);
        expect(true).assertTrue();
      } else {
        try {
          dtmfSender!.insertDTMF('123', 70, 6001);
          expect().assertFail();
        } catch (e) {
          console.error(TAG, "e: ", e);
        }
      }
    })

    it("interToneGapShouldMoreThan40", DEFAULT, async () => {
      let sender = await createDtmfSender(caller, callee);
      let dtmfSender = sender?.dtmf;
      if (isX86()) {
        await waitForDtmfReady(dtmfSender!);
        dtmfSender!.insertDTMF('123', 70, 29);
        expect(true).assertTrue();
      } else {
        try {
          dtmfSender!.insertDTMF('123', 70, 29);
          expect().assertFail();
        } catch (e) {
          console.error(TAG, "e: ", e);
        }
      }
    })

    it("ontonechangeWhenInsertDTMF", DEFAULT, async () => {
      let sender = await createDtmfSender(caller, callee);
      let dtmfSender = sender?.dtmf;
      let res: string;
      let toneReceived = false;
      let promise = new Promise<void>((resolve) => {
        dtmfSender!.ontonechange = async (event) => {
          if (event.tone == 'A') {
            res = event.tone;
            toneReceived = true;
            resolve();
          }
        };
      })
      if (isX86()) {
        await waitForDtmfReady(dtmfSender!);
        dtmfSender?.insertDTMF('A');
        await pollUntilOrFail(() => toneReceived, 'DTMF tone A not received within timeout');
      } else {
        await sleep(1000);
        dtmfSender?.insertDTMF('A');
        await promise;
      }
      expect(res!).assertEqual('A')
    })

    it("ontonechangeWhenInsertDTMFErrorString", DEFAULT, async () => {
      let sender = await createDtmfSender(caller, callee);
      let dtmfSender = sender?.dtmf;
      let res: string = "";
      dtmfSender!.ontonechange = async (event) => {
        if (event.tone == "F") {
          res = event.tone;
        }
        console.info(TAG, "event.tone:", event.tone);
      };
      await sleep(1000);
      dtmfSender?.insertDTMF('F');
      await sleep(1000);
      expect(res).not().assertEqual('F')
    })

  })
}
