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
const TAG: string = '[PeerConnectionTest]';

const pcf = new webrtc.PeerConnectionFactory();

let caller: webrtc.RTCPeerConnection;
let callee: webrtc.RTCPeerConnection;

const STUN_SERVER = "stun:stun.l.google.com:19302";

const sdp = 'v=0\r\n' +
  'o=- 166855176514521964 2 IN IP4 127.0.0.1\r\n' +
  's=-\r\n' +
  't=0 0\r\n' +
  'a=ice-options:trickle\r\n' +
  'm=audio 9 UDP/TLS/RTP/SAVPF 111\r\n' +
  'c=IN IP4 0.0.0.0\r\n' +
  'a=rtcp:9 IN IP4 0.0.0.0\r\n' +
  'a=ice-ufrag:someufrag\r\n' +
  'a=ice-pwd:somelongpwdwithenoughrandomness\r\n' +
  'a=fingerprint:sha-256 8C:71:B3:8D:A5:38:FD:8F:A4:2E:A2:65:6C:86:52:BC:E0:6E:94:F2:9F:7C:4D:B5:DF:AF:AA:6F:44:90:8D:F4\r\n' +
  'a=setup:actpass\r\n' +
  'a=rtcp-mux\r\n' +
  'a=mid:mid1\r\n' +
  'a=sendonly\r\n' +
  'a=msid:stream1 track1\r\n' +
  'a=ssrc:1001 cname:some\r\n' +
  'a=rtpmap:111 opus/48000/2\r\n';

let offer = {
  sdp: "v=0\r\no=- 8926124112408190582 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=extmap-allow-mixed\r\na=msid-semantic: WMS\r\n",
  type: "offer"
} as webrtc.RTCSessionDescriptionInit;

let answer = {
  sdp: "v=0\r\no=- 1824345164880213868 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=extmap-allow-mixed\r\na=msid-semantic: WMS\r\n",
  type: "answer"
} as webrtc.RTCSessionDescriptionInit;

let transceiverInit = {
  direction: 'sendrecv',
  streams: [],
  sendEncodings: [{
    maxBitrate: 100000,
  }],
} as webrtc.RTCRtpTransceiverInit;

let dataChannel: webrtc.RTCDataChannelInit = {
  ordered: true,
  maxPacketLifeTime: 3000,
  protocol: "sctp-protocol",
};

async function exchangeOffer(caller: webrtc.RTCPeerConnection, callee: webrtc.RTCPeerConnection) {
  await caller.setLocalDescription(await caller.createOffer());
  console.log(TAG, "caller.localDescription: " + caller.localDescription!.type);
  await callee.setRemoteDescription(caller.localDescription);
}

async function exchangeAnswer(caller: webrtc.RTCPeerConnection, callee: webrtc.RTCPeerConnection) {
  const answer = await callee.createAnswer();
  await caller.setRemoteDescription(answer);
  await callee.setLocalDescription(answer);
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

async function generateAudioReceiveOnlyOffer(pc: webrtc.RTCPeerConnection) {
  try {
    pc.addTransceiver('audio', {
      direction: 'recvonly'
    });
    return pc.createOffer();
  } catch (e) {
    return pc.createOffer({
      iceRestart: true
    });
  }
}

async function generateAnswer(offer: webrtc.RTCSessionDescriptionInit) {
  let pc = pcf.createPeerConnection({
    iceServers: [{
      urls: STUN_SERVER
    }]
  });
  await pc.setRemoteDescription(offer);
  let answer = await pc.createAnswer();
  pc.close();
  return answer;
}

async function generateOffer() {
  let pc = pcf.createPeerConnection({
    iceServers: [{
      urls: STUN_SERVER
    }]
  });
  let offer = await pc.createOffer();
  pc.close();
  return offer;
}

async function createSetOffer(options?: webrtc.RTCOfferOptions) {
  try {
    let offer: webrtc.RTCSessionDescriptionInit;
    if (typeof options !== 'object') {
      offer = await caller?.createOffer();
    } else {
      offer = await caller?.createOffer(options);
    }
    console.info(TAG, 'createOffer: ' + JSON.stringify(offer));
    expect(offer.type).assertEqual("offer");
  } catch (e) {
    console.error(TAG, 'createOffer: ' + JSON.stringify(e));
    expect().assertFail();
  }
}

async function createSetAnswer(options?: webrtc.RTCAnswerOptions) {
  try {
    let answer = await caller?.createAnswer(options);
    console.info(TAG, 'createAnswer: ' + JSON.stringify(answer));
    expect(answer.type).assertEqual("answer");
  } catch (e) {
    console.error(TAG, 'createAnswer: ' + JSON.stringify(e));
    expect().assertFail();
  }
}

export default function PeerConnectionTest() {
  describe('PeerConnectionTest', () => {
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

    it('addAndRemoveTrack', DEFAULT, () => {
      let audioSource = pcf?.createAudioSource();
      let audioTrack = pcf?.createAudioTrack("audio", audioSource);
      expect(audioTrack.kind).assertEqual("audio");
      let rtpSender = caller.addTrack(audioTrack);

      expect(caller.getSenders().length).assertEqual(1);
      console.info(TAG, 'getSenders: ' + caller.getSenders().length);
      try {
        // removeTrack后，不能通过senders的长度来判断，因为长度不变
        caller?.removeTrack(rtpSender);
        expect(caller.getSenders().length).assertEqual(1);
        expect(caller.getSenders()[0].track).assertNull();
      } catch (e) {
        console.error(TAG, 'Error while removing track: ', JSON.stringify(e));
        expect().assertFail();
      }
    })

    it("addIceCandidate", DEFAULT, async () => {
      let candidate: webrtc.RTCIceCandidate | undefined = undefined;
      let pormise = new Promise<void>((resolve) => {
        caller.onicecandidate = (event) => {
          console.info(TAG, 'onicecandidate: ', event.candidate);
          candidate = event.candidate;
          resolve();
        };
      });

      caller.onicegatheringstatechange = (event) => {
        console.info(TAG, 'onicegatheringstatechange: ', event);
      };

      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack("id", audioSource);

      caller.addTrack(audioTrack);

      let offer = await caller?.createOffer();
      await caller?.setRemoteDescription(offer);
      console.info(TAG, 'caller.signalingState changed:', caller.signalingState);

      let answer = await caller?.createAnswer();
      await caller?.setLocalDescription(answer);
      console.info(TAG, 'caller.signalingState changed:', caller.signalingState);

      await pormise;
      expect(candidate).not().assertUndefined();

      try {
        await caller.addIceCandidate(candidate);
      } catch (e) {
        console.error(TAG, 'addIceCandidate error :', JSON.stringify(e));
        expect().assertFail();
      }
    })

    it("AddTransceiverWithStringNoInit", DEFAULT, () => {
      let tranSceiver = caller?.addTransceiver("audio");
      console.info(TAG, 'addTransceiver: ' + JSON.stringify(tranSceiver));
      expect(caller.getTransceivers().length).assertEqual(1);
    })

    it("AddTransceiverWithTrackNoInit", DEFAULT, () => {
      let audioSource = pcf?.createAudioSource({
        echoCancellation: false
      });
      let audioTrack = pcf?.createAudioTrack("audio", audioSource);
      let tranSceiver = caller?.addTransceiver(audioTrack);
      console.info(TAG, 'addTransceiver: ' + JSON.stringify(tranSceiver));
      expect(caller.getTransceivers().length).assertEqual(1);
    })

    it("AddTransceiverWithTrackAndInit", DEFAULT, () => {
      let audioSource = pcf?.createAudioSource({
        echoCancellation: false
      });
      let audioTrack = pcf?.createAudioTrack("audio", audioSource);
      let tranSceiver = caller?.addTransceiver(audioTrack, transceiverInit);
      console.info(TAG, 'addTransceiver: ' + JSON.stringify(tranSceiver));
      expect(caller.getTransceivers().length).assertEqual(1);
    })

    it("AddTransceiverWithStringAndInit", DEFAULT, () => {
      let tranSceiver = caller?.addTransceiver("audio", transceiverInit);
      console.info(TAG, 'addTransceiver: ' + JSON.stringify(tranSceiver));
      expect(caller.getTransceivers().length).assertEqual(1);
    })

    it("createOffer", DEFAULT, async () => {
      createSetOffer();
    })

    it("createOfferAddOptions", DEFAULT, async () => {
      let options: webrtc.RTCOfferOptions = {
        iceRestart: true
      }
      createSetOffer(options);
    })

    it("createAnswer", DEFAULT, async () => {
      await caller?.setRemoteDescription(offer);
      createSetAnswer();
    })

    it("createAnswerAddOptions", DEFAULT, async () => {
      await caller?.setRemoteDescription(offer);
      let options: webrtc.RTCAnswerOptions = {};
      createSetAnswer(options);
    })

    it("setLocalDescriptionWithOffer", DEFAULT, async () => {
      await caller?.setLocalDescription(offer);
      expect(caller.signalingState).assertEqual("have-local-offer");
    })

    it("setLocalDescriptionNoOffer", DEFAULT, async () => {
      await caller?.setLocalDescription();
      expect(caller.signalingState).assertEqual("have-local-offer");
      expect(caller.pendingLocalDescription).not().assertNull();
    })

    it("setLocalDescriptionWithAnswer", DEFAULT, async () => {
      await caller?.setRemoteDescription(offer);
      await caller?.setLocalDescription(answer);
      expect(caller.signalingState).assertEqual("stable");
    })

    it("setLocalDescriptionNoAnswer", DEFAULT, async () => {
      await callee.setRemoteDescription(await caller.createOffer());
      await callee.setLocalDescription();
      expect(callee.currentLocalDescription).not().assertNull();
      expect(caller.signalingState).assertEqual("stable");
    })

    it("setRemoteDescription", DEFAULT, async () => {
      expect(caller.signalingState).assertEqual("stable");
      caller.onsignalingstatechange = () => {
        console.log(TAG, 'Signaling state change: ', caller.signalingState);
      };
      await caller?.setRemoteDescription(offer);
      expect(caller.signalingState).assertEqual("have-remote-offer");
    })

    it("getReceivers", DEFAULT, () => {
      let receiver = caller.getReceivers();
      expect(receiver.length).assertEqual(0);

      let audioSource = pcf?.createAudioSource();
      let audioTrack = pcf?.createAudioTrack("audio", audioSource);

      let rtpSender = caller.addTrack(audioTrack);
      expect(caller.getSenders().length).assertEqual(1);

      let newReceiver = caller.getReceivers();
      expect(newReceiver.length).assertEqual(1);
    })

    it("createDataChannelWithInit", DEFAULT, () => {
      let channel = caller.createDataChannel("1", dataChannel);

      expect(channel.label).assertEqual("1");
      expect(channel.ordered).assertTrue();
      expect(channel.maxPacketLifeTime).assertEqual(3000);
      expect(channel.protocol).assertEqual("sctp-protocol");
    })

    it("createDataChannelNoInit", DEFAULT, () => {
      let channel = caller.createDataChannel("1");

      console.info(TAG, 'channel: ', JSON.stringify(channel));

      expect(channel.label).assertEqual("1");
      expect(channel.ordered).assertTrue();
    })

    it("setAndGetConfiguration", DEFAULT, async () => {
      expect(caller.getConfiguration().iceCandidatePoolSize).assertEqual(0)
      caller.setConfiguration();
      let config = caller.getConfiguration();

      expect(config.iceCandidatePoolSize).assertEqual(0);

      let newConfig: webrtc.RTCConfiguration = {
        iceServers: [{
          urls: "stun:stun1.l.google.com:19302"
        },],
        iceCandidatePoolSize: 10
      };
      caller.setConfiguration(newConfig);

      caller.getConfiguration();
      expect(caller.getConfiguration().iceCandidatePoolSize).assertEqual(10);

      try {
        let stat = await caller.getStats();
        console.info(TAG, 'getStats: ' + JSON.stringify(stat));
      } catch (e) {
        console.error(TAG, 'getStats error : ', JSON.stringify(e))
        expect().assertFail()
      }
    })

    it("restartIce", DEFAULT, async () => {
      let flag = false;
      let promise = new Promise<void>((resolve) => {
        caller.onnegotiationneeded = async () => {
          flag = true;
          resolve();
        }
      });
      caller.restartIce();
      await promise;
      expect(flag).assertTrue();
    })

    it("afterCloseCallerSetConfigurationWillFail", DEFAULT, () => {
      caller.close();
      try {
        caller.setConfiguration();
        expect().assertFail();
      } catch (e) {
        console.error(TAG, "close:", e);
      }
    })

    it("directionWillChanged", DEFAULT, async () => {
      let calleePromise = callee.createOffer().then((offer) => {
        return caller.setRemoteDescription(offer);
      }).then(() => {
        return caller.createAnswer();
      });
      caller.addTransceiver('audio', {
        direction: 'recvonly'
      });

      let callerPromise = caller.createOffer().then(() => {
        caller.getTransceivers()[0].direction = 'inactive';
      });
      await Promise.all([callerPromise, calleePromise]);
      expect(caller.getTransceivers()[0].direction).assertEqual('inactive');
    })

    it("transceiverSenderTrackDoesNotRevertToAnOldState", DEFAULT, async () => {
      let offer = await callee.createOffer();
      await caller.setRemoteDescription(offer);
      let answer = await caller.createAnswer();
      await caller.setLocalDescription(answer);
      caller.addTransceiver('audio', {
        direction: 'recvonly'
      });
      await caller.createOffer();
      caller.addTrack(caller.getReceivers()[0].track);
      expect(caller.getSenders()[0].track!.id).assertEqual(caller.getReceivers()[0].track.id);
    })

    it("inValidSdp", DEFAULT, async () => {
      try {
        await caller.setRemoteDescription({
          sdp: 'bogus', type: 'offer'
        });
      } catch (e) {
        console.error(TAG, "e: ", e);
        expect(e).assertInstanceOf("Error");
      }
    })

    it("rollbackInvalidSdp", DEFAULT, async () => {
      try {
        const offer = await generateAudioReceiveOnlyOffer(caller);
        await caller.setRemoteDescription(offer);
        await caller.setRemoteDescription({
          type: 'rollback',
          sdp: '!<Invalid SDP Content>;'
        });
      } catch (e) {
        console.error(TAG, "e: ", e);
        expect(e).assertInstanceOf("Error");
      }
    });

    it("localOfferCreatedBeforeSetRemoteDescriptionThenRollbackShouldStillBeUsable", DEFAULT, async () => {
      caller.addTransceiver('audio', {
        direction: 'recvonly'
      });
      let offer1 = await caller.createOffer();
      callee.addTransceiver('audio', {
        direction: 'recvonly'
      });
      let offer2 = await callee.createOffer();
      try {
        await caller.setRemoteDescription(offer2);
        await caller.setRemoteDescription({
          type: "rollback"
        });
        await caller.setLocalDescription(offer1);
      } catch (e) {
        console.error(TAG, "e: ", e);
        expect().assertFail();
      }
    })

    it("removeTrackWithASenderBeingRolledBackDoesNotCrashOrThrow", DEFAULT, async () => {
      callee.addTransceiver('audio');
      let offer = await callee.createOffer();
      await caller.setRemoteDescription(offer);
      let transceiver: webrtc.RTCRtpTransceiver[] = caller.getTransceivers();
      caller.setRemoteDescription({
        type: 'rollback'
      });
      caller.removeTrack(transceiver[0].sender);
    })

    it("setRemoteDescriptionPranswerWhenStable", DEFAULT, async () => {
      let offer = await caller.createOffer();
      console.info(TAG, "signalingState: ", caller.signalingState);
      try {
        caller.setRemoteDescription({
          type: 'pranswer', sdp: offer.sdp
        });
      } catch (e) {
        console.error(TAG, "e: ", e);
        expect(e).assertEqual("InvalidStateError");
      }
    })

    it("setRemoteDescriptionWithValidOfferShouldSucceed", DEFAULT, async () => {
      caller.createDataChannel('datachannel');
      let states: webrtc.RTCSignalingState[] = [];
      let promise = new Promise<void>((resolve) => {
        callee.onsignalingstatechange = () => {
          console.info(TAG, "callee.signalingState: ", callee.signalingState);
          states.push(callee.signalingState);
          resolve();
        };
      });
      let offer = await caller.createOffer();
      await callee.setRemoteDescription(offer);
      await promise;
      expect(callee.signalingState).assertEqual('have-remote-offer');
      expect(callee.currentRemoteDescription).assertUndefined();
      expect(states).assertDeepEquals(['have-remote-offer']);
    })

    it("setRemoteDescriptionWithInvalidSdp", DEFAULT, async () => {
      try {
        await caller.setRemoteDescription({
          type: 'offer',
          sdp: 'Invalid SDP'
        });
        console.error(TAG, 'Expect promise to be rejected');
        expect().assertFail();
      } catch (e) {
        console.error(TAG, "errorDetail: ", e);
      }
    });

    it("onsignalingstatechangeBeforeSetLocalDescription", DEFAULT, async () => {
      const offer = await caller.createOffer();
      let eventSequence = '';

      let promise = new Promise<void>((resolve) => {
        caller.onsignalingstatechange = () => {
          console.info(TAG, "signalingState: ", caller.signalingState);
          eventSequence += 'onsignalingstatechange;';
          resolve();
        };
      })

      await caller.setLocalDescription(offer);
      await promise;
      eventSequence += 'setLocalDescription;';
      expect(eventSequence).assertEqual('onsignalingstatechange;setLocalDescription;');
    });

    it("setLocalDescriptionRollbackFromLocalOffer", DEFAULT, async () => {
      let states: webrtc.RTCSignalingState [] = [];
      let promise1 = new Promise<void>((resolve) => {
        caller.onsignalingstatechange = () => {
          states.push(caller.signalingState);
          resolve();
        }
      })
      let offer = await caller.createOffer();
      await caller.setLocalDescription(offer);
      expect(caller.signalingState).assertEqual('have-local-offer');
      expect(caller.localDescription).not().assertNull();
      expect(caller.pendingLocalDescription).assertDeepEquals(caller.localDescription);
      expect(caller.currentLocalDescription).assertUndefined();
      await promise1;
      let promise2 = new Promise<void>((resolve) => {
        caller.onsignalingstatechange = () => {
          states.push(caller.signalingState);
          resolve();
        }
      })
      await caller.setLocalDescription({
        type: 'rollback'
      });
      expect(caller.signalingState).assertEqual('stable');
      expect(caller.localDescription).assertUndefined();
      expect(caller.pendingLocalDescription).assertUndefined();
      expect(caller.currentLocalDescription).assertUndefined();
      await promise2;
      expect(states).assertDeepEquals(['have-local-offer', 'stable']);
    })


    it("setLocalDescriptionRollbackFromState", DEFAULT, async () => {
      try {
        await caller.setLocalDescription({
          type: 'rollback'
        })
        expect().assertFail();
      } catch (e) {
        console.error(TAG, "e: ", e);
      }
    })

    it("setLocalDescriptionRollbackAfterSettingAnswerDescriptionShouldRejectWithInvalidStateError", DEFAULT,
      async () => {
        let offer = await generateAudioReceiveOnlyOffer(caller);
        await caller.setRemoteDescription(offer);
        let answer = await caller.createAnswer();
        await caller.setLocalDescription(answer);
        try {
          await caller.setLocalDescription({
            type: 'rollback'
          });
          expect().assertFail();
        } catch (e) {
          console.error(TAG, "e: ", e);
        }
      })

    it("setLocalDescriptionRollbackIgnoreInvalidSdp", DEFAULT, async () => {
      let offer = await caller.createOffer()
      caller.setLocalDescription(offer)
      try {
        caller.setLocalDescription({
          type: 'rollback',
          sdp: '!<Invalid SDP Content>;'
        });
      } catch (e) {
        expect().assertFail();
      }
    })

    it("setLocalDescriptionRollbackShouldUpdateInternalStateWithAQueuedTaskInTheRightOrder", DEFAULT, async () => {
      caller.addTransceiver('audio', {
        direction: 'recvonly'
      });
      await caller.setLocalDescription(await caller.createOffer());
      expect(caller.signalingState).assertEqual("have-local-offer");
      expect(caller.pendingLocalDescription).not().assertNull();
      expect(caller.pendingLocalDescription).assertDeepEquals(caller.localDescription);
      expect(caller.pendingRemoteDescription).assertUndefined()
      const sldPromise = caller.setLocalDescription({
        type: "rollback"
      });
      const stablePromise = new Promise<string>((resolve, reject) => {
        caller.onsignalingstatechange = () => {
          resolve(caller.signalingState);
        }
      });
      let raceValue = await Promise.race([stablePromise, sldPromise]);
      expect(caller.signalingState).assertEqual("stable");
      expect(caller.pendingLocalDescription).assertUndefined();
      expect(caller.pendingRemoteDescription).assertUndefined();

      await sldPromise;
    })

    it("setLocalDescriptionPranswerFromStable", DEFAULT, async () => {
      let offer = await caller.createOffer();
      try {
        await caller.setLocalDescription({
          type: 'pranswer', sdp: offer.sdp
        });
      } catch (e) {
        console.error(TAG, "e: ", e);
        expect(e).assertInstanceOf("Error");
      }
    })

    it("ParameterlessSldStable", DEFAULT, async () => {
      let transceiver = caller.addTransceiver('audio');
      expect(transceiver.mid).assertNull();
      await caller.setLocalDescription();
      expect(transceiver.mid).not().assertNull();
    })

    it("ParameterlessRemoteoffer", DEFAULT, async () => {
      caller.addTransceiver('audio');

      let promise = new Promise<void>((resolve) => {
        callee.ontrack = async () => {
          console.info(TAG, "currentDirection: ", callee.getTransceivers()[0].currentDirection);
          resolve();
        }
      })
      await callee.setRemoteDescription(await caller.createOffer());
      await callee.setLocalDescription();
      await promise;
      expect(callee.getTransceivers()[0].currentDirection).assertEqual('recvonly');
    })

    it("exchangeOA", DEFAULT, async () => {
      try {
        await caller.setLocalDescription();
        await callee.setRemoteDescription(caller.pendingLocalDescription);

        await callee.setLocalDescription();
        await caller.setRemoteDescription(callee.currentLocalDescription);
      } catch (e) {
        console.error(TAG, "e: ", e);
        expect().assertFail();
      }
    })

    it("setLocalDescriptionWithValidOfferShouldSucceed", DEFAULT, async () => {
      let states: String = "";
      let promise = new Promise<void>((resolve) => {
        caller.onsignalingstatechange = () => {
          states = caller.signalingState;
          resolve();
        };
      })

      let offer = await generateAudioReceiveOnlyOffer(caller);
      await caller.setLocalDescription(offer);
      expect(caller.signalingState).assertEqual('have-local-offer');
      expect(caller.currentLocalDescription).assertUndefined();
      await promise;
      expect(states).assertEqual('have-local-offer');
    })

    it("localOfferCreatedBeforeSetRemoteDescriptionRemoteOfferThenRollbackShouldStillBeUsable", DEFAULT, async () => {
      try {
        caller.addTransceiver('audio', {
          direction: 'recvonly'
        });
        let offer = await caller.createOffer();
        callee.addTransceiver('audio', {
          direction: 'recvonly'
        });
        let offer2 = await callee.createOffer();

        await caller.setRemoteDescription(offer2);
        await caller.setRemoteDescription({
          type: "rollback"
        });
        await caller.setLocalDescription(offer);
      } catch (e) {
        console.error(TAG, "e: ", e);
        expect().assertFail();
      }
    })

    it("offerSetRepeatedly", DEFAULT, async () => {
      caller.addTransceiver('audio', {
        direction: 'recvonly'
      });
      await caller.setLocalDescription(await caller.createOffer());

      const offer = await caller.createOffer();
      await caller.setLocalDescription(offer);
      await callee.setRemoteDescription(offer);
      const answer = await callee.createAnswer();
      await callee.setLocalDescription(answer);
      await caller.setRemoteDescription(answer);

      expect(caller.getTransceivers().length).assertEqual(1);
      expect(caller.getTransceivers()[0].receiver.track.kind).assertEqual("audio");
      expect(callee.getTransceivers().length).assertEqual(1);
      expect(callee.getTransceivers()[0].receiver.track.kind).assertEqual("audio");
    })

    it("setLocalDescriptionAnswerStateInvalidStateError", DEFAULT, async () => {
      let offer = await caller.createOffer()
      console.info(TAG, "signalingState: ", caller.signalingState);
      try {
        await caller.setLocalDescription({
          type: 'answer', sdp: offer.sdp
        });
        expect().assertFail()
      } catch (e) {
        console.error(TAG, "error: ", e);
      }
    })

    it("setLocalDescriptionAnswerWhenLocalOffer", DEFAULT, async () => {
      let offer = await caller.createOffer();
      await caller.setLocalDescription(offer);
      let answer = await generateAnswer(offer);
      console.info(TAG, "signalingState: ", caller.signalingState);
      try {
        await caller.setLocalDescription(answer);
        expect().assertFail()
      } catch (e) {
        console.error(TAG, "error: ", e);
      }
    })

    it("setPreviouslyAnswerCreateOfferShouldWork", DEFAULT, async () => {
      try {
        caller.addTransceiver('audio', {
          direction: 'recvonly'
        });
        let offer = await caller.createOffer();
        await callee.setRemoteDescription(offer);
        let answer = await callee.createAnswer();
        await callee.setRemoteDescription({
          type: "rollback"
        });
        callee.addTransceiver('video', {
          direction: 'recvonly'
        });
        await callee.createOffer();
        await callee.setRemoteDescription(offer);
        await callee.setLocalDescription(answer);
      } catch (e) {
        console.error(TAG, "e: ", e);
        expect().assertFail();
      }
    })

    it("setTransceiverInactive", DEFAULT, async () => {
      caller.addTransceiver('audio');
      const offer = await caller.createOffer();
      await caller.setLocalDescription(offer);

      await callee.setRemoteDescription(offer);
      callee.getTransceivers()[0].stop();
      const answer = await callee.createAnswer();

      await caller.setRemoteDescription(answer);

      expect(caller.getTransceivers()[0].currentDirection).assertEqual('inactive');
    })

    it("negotiationNeededWhenReturningToStableDoesNotFireTooEarly", DEFAULT, async () => {
      caller.addTransceiver("audio");
      await caller.setLocalDescription(await caller.createOffer());
      caller.restartIce();
      let promise = new Promise<void>((resolve) => {
        caller.onnegotiationneeded = () => {
          resolve();
        }
      })
      await callee.setRemoteDescription(caller.localDescription);
      await callee.setLocalDescription(await callee.createAnswer());
      await caller.setRemoteDescription(callee.localDescription);
      await promise;
    })

    it("addTransceiverWithInactiveDirectionNotCauseOntrack", DEFAULT, async () => {
      caller.addTransceiver('audio', {
        direction: 'inactive'
      });
      let flag: Boolean = false;
      callee.ontrack = () => {
        flag = true;
      }
      await callee.setRemoteDescription(await caller.createOffer());
      await sleep(1000);
      expect(flag).assertEqual(false);
    })

    it("negotiationneededFireWhenSignalingBackStateAfterSetRemoteDescription", DEFAULT, async () => {
      caller.addTransceiver('audio');
      const offer = await caller.createOffer();
      await caller.setLocalDescription(offer);
      let flag = false;
      let promise = new Promise<void>((resolve) => {
        caller.onnegotiationneeded = () => {
          flag = true;
          resolve();
        }
      })
      caller.createDataChannel('test');
      await caller.setRemoteDescription(await generateAnswer(offer));
      await promise;
      expect(flag).assertTrue();
    })

    it("negotiationneededFireWhenSignalingBackStateAfterSetLocalDescription", DEFAULT, async () => {
      caller.addTransceiver('audio');
      let flag = false;
      let promise = new Promise<void>((resolve) => {
        caller.onnegotiationneeded = () => {
          flag = true;
          resolve();
        }
      })
      await caller.setRemoteDescription(await generateOffer());
      caller.createDataChannel('test');
      await caller.setLocalDescription(await caller.createAnswer());
      console.info(TAG, "signalingState: ", caller.signalingState);
      await promise;
      expect(flag).assertTrue();
    })

    it("UpdatingDirectionTransceiverShouldCauseNegotiationneeded", DEFAULT, async () => {
      let transceiver = caller.addTransceiver('audio', {
        direction: 'sendrecv'
      });
      let event = "";
      let promise = new Promise<void>((resolve) => {
        caller.onnegotiationneeded = () => {
          event = 'recvonly';
          resolve();
        }
      });
      let offer = await caller.createOffer();
      await caller.setLocalDescription(offer);
      let answer = await generateAnswer(offer);
      await caller.setRemoteDescription(answer);
      transceiver.direction = 'recvonly';
      await promise;
      expect(event).assertEqual('recvonly');
    })

    it("setStreamsShouldCauseNegotiationneeded", DEFAULT, async () => {
      let transceiver = caller.addTransceiver('audio', {
        direction: 'sendrecv'
      });
      let offer = await caller.createOffer();
      await caller.setLocalDescription(offer);
      let answer = await generateAnswer(offer);
      await caller.setRemoteDescription(answer);

      let stream = new webrtc.MediaStream();
      let flag = false;
      let promise = new Promise<void>((resolve) => {
        caller.onnegotiationneeded = () => {
          flag = true;
          resolve();
        }
      });
      transceiver.sender.setStreams(stream);
      await promise;
      expect(flag).assertTrue();
    })

    it("setStreamsDifferentStreamShouldCauseNegotiationneeded", DEFAULT, async () => {
      let transceiver = caller.addTransceiver('audio', {
        direction: 'sendrecv'
      });
      let stream = new webrtc.MediaStream();
      transceiver.sender.setStreams(stream);

      let offer = await caller.createOffer();
      await caller.setLocalDescription(offer);
      let answer = await generateAnswer(offer);
      await caller.setRemoteDescription(answer);

      let stream2 = new webrtc.MediaStream();
      let flag = false;
      let promise = new Promise<void>((resolve) => {
        caller.onnegotiationneeded = () => {
          flag = true;
          resolve();
        }
      });
      transceiver.sender.setStreams(stream2);
      await promise;
      expect(flag).assertTrue();
    })

    it("setStreamsWithAdditionalStreamShouldCauseNegotiationneeded", DEFAULT, async () => {
      let transceiver = caller.addTransceiver('audio', {
        direction: 'sendrecv'
      });
      let stream = new webrtc.MediaStream();
      transceiver.sender.setStreams(stream);

      let offer = await caller.createOffer();
      await caller.setLocalDescription(offer);
      let answer = await generateAnswer(offer);
      await caller.setRemoteDescription(answer);

      let stream2 = new webrtc.MediaStream();
      let flag = false;
      let promise = new Promise<void>((resolve) => {
        caller.onnegotiationneeded = () => {
          flag = true;
          resolve();
        }
      })
      transceiver.sender.setStreams(stream, stream2);
      await promise;
      expect(flag).assertTrue();
    })

    it("sendDataInDatachannelEvent", DEFAULT, async () => {
      let message = 'meow meow!';

      callee.ondatachannel = (event) => {
        console.info(TAG, "send data");
        let dc2 = event.channel;
        dc2.send(message);
      };

      const dc1 = caller.createDataChannel('fire-me!');
      let data: string = "";
      let promise = new Promise<void>((resolve) => {
        dc1.onmessage = (event: webrtc.MessageEvent<string>) => {
          data = event.data;
          resolve();
        };
      });
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await promise;
      expect(data).assertEqual(message);
    })

    it("closeChannelNotCauseOnopenInDatachannelEvent", DEFAULT, async () => {
      let flag = false;
      let promise = new Promise<void>((resolve) => {
        callee.ondatachannel = async (event) => {
          const dc = event.channel;
          dc.onopen = () => {
            flag = true;
            expect().assertFail();
          };
          dc.close();
          resolve();
        };
      })
      caller.createDataChannel('fire-me!');

      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await promise;
      expect(flag).assertEqual(false);
    })

    it("closeChannelWhenSendNotCauseOnopenInDataChannelEvent", DEFAULT, async () => {
      let message = 'meow meow!';
      callee.ondatachannel = async (event) => {
        let dc2 = event.channel;
        dc2.onopen = () => {
          expect().assertFail();
        };
        dc2.send(message);
        dc2.close();
      };
      let dc1 = caller.createDataChannel('fire-me!');
      let res = "";
      let promise = new Promise<void>((resolve) => {
        dc1.onmessage = (event: webrtc.MessageEvent<string>) => {
          res = event.data;
          resolve();
        };
      })

      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await promise;
      expect(res).assertEqual(message);
    })

    it("inBandNegotiatedChannelMatchSameConfigurationAsLocalPeer", DEFAULT, async () => {
      let dc1 = caller.createDataChannel('test', {
        ordered: false,
        maxRetransmits: 1,
        protocol: 'custom'
      });
      expect(dc1.label).assertEqual('test');
      expect(dc1.ordered).assertEqual(false);
      expect(dc1.maxPacketLifeTime).assertUndefined();
      expect(dc1.maxRetransmits).assertEqual(1);
      expect(dc1.protocol).assertEqual('custom');
      expect(dc1.negotiated).assertEqual(false);

      let promise = new Promise<void>((resolve) => {
        callee.ondatachannel = async (event) => {
          const dc2 = event.channel;
          expect(dc2.label).assertEqual('test');
          expect(dc2.ordered).assertEqual(false);
          expect(dc2.maxPacketLifeTime).assertUndefined();
          expect(dc2.maxRetransmits).assertEqual(1);
          expect(dc2.protocol).assertEqual('custom');
          expect(dc2.negotiated).assertEqual(false);
          expect(dc2.id).assertEqual(dc1.id);
          resolve();
        };
      });

      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await promise;
    })

    it("inBandNegotiatedChannelMatchSameDefaultConfigurationAsLocalPeer", DEFAULT, async () => {
      let dc1 = caller.createDataChannel('');

      expect(dc1.label).assertEqual('');
      expect(dc1.ordered).assertEqual(true);
      expect(dc1.maxPacketLifeTime).assertUndefined();
      expect(dc1.maxRetransmits).assertUndefined();
      expect(dc1.protocol).assertEqual('');
      expect(dc1.negotiated).assertEqual(false);

      let promise = new Promise<void>((resolve) => {
        callee.ondatachannel = async (event) => {
          let dc2 = event.channel;
          expect(dc2.label).assertEqual('');
          expect(dc2.ordered).assertEqual(true);
          expect(dc2.maxPacketLifeTime).assertUndefined();
          expect(dc2.maxRetransmits).assertUndefined();
          expect(dc2.protocol).assertEqual('');
          expect(dc2.negotiated).assertEqual(false);
          expect(dc2.id).assertEqual(dc1.id);
          resolve();
        }
      });

      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await promise;
    })

    it("negotiatedChannelShouldNotFireDatachannelEventOnRemotePeer", DEFAULT, async () => {
      callee.ondatachannel = () => {
        console.error(TAG, "datachannel event should not be fired");
        expect().assertFail();
      }

      caller.createDataChannel('test', {
        negotiated: true,
        id: 42
      });

      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await sleep(1000);
    })

    it("initialIceGatheringStateShouldBeNew", DEFAULT, () => {
      expect(caller.iceGatheringState).assertEqual('new');
    })

    it("stateInIceConnectionState", DEFAULT, () => {
      expect(caller.iceConnectionState).assertEqual('new');
      caller.close();
      expect(caller.iceConnectionState).assertEqual('closed');
    })

    it("initialPeerConnectionShouldHaveListOfZeroSendersReceiversTransceivers", DEFAULT, async () => {
      let senders = caller.getSenders();
      expect(senders).assertDeepEquals([]);
      let receivers = caller.getReceivers();
      expect(receivers).assertDeepEquals([]);
      let transceivers = caller.getTransceivers();
      expect(transceivers).assertDeepEquals([]);
    })

    it("pendingLocalDescriptionIsSurfacedAtTheRightTime", DEFAULT, async () => {
      let offer = await caller.createOffer();
      expect(caller.pendingLocalDescription).assertUndefined();
      await caller.setLocalDescription(offer);
      expect(caller.pendingLocalDescription).not().assertUndefined();
      expect(caller.pendingLocalDescription).assertDeepEquals(caller.localDescription);
    })

    it("pendingRemoteDescriptionIsSurfacedAtTheRightTime", DEFAULT, async () => {
      let offer = await caller.createOffer();
      expect(caller.pendingRemoteDescription).assertUndefined();
      await caller.setRemoteDescription(offer);
      expect(caller.pendingRemoteDescription).not().assertUndefined();
      expect(caller.pendingRemoteDescription).assertDeepEquals(caller.remoteDescription);
    })

    it("currentLocalDescriptionIsSurfacedAtTheRightTime", DEFAULT, async () => {
      let offer = await caller.createOffer();
      await caller.setLocalDescription(offer);
      await callee.setRemoteDescription(offer);
      let answer = await callee.createAnswer();

      expect(callee.currentLocalDescription).assertUndefined();
      await callee.setLocalDescription(answer);
      expect(callee.currentLocalDescription).not().assertNull();
      expect(callee.currentLocalDescription).assertDeepEquals(callee.localDescription);
    })

    it("currentRemoteDescriptionIsSurfacedAtTheRightTime", DEFAULT, async () => {
      let offer = await caller.createOffer();
      await caller.setLocalDescription(offer);
      await callee.setRemoteDescription(offer);
      let answer = await callee.createAnswer();

      expect(caller.currentRemoteDescription).assertUndefined();
      await caller.setRemoteDescription(answer);
      expect(caller.currentRemoteDescription).not().assertNull();
      expect(caller.currentRemoteDescription).assertDeepEquals(caller.remoteDescription);
    })

    it("createDataChannelAttributeDefaultValues", DEFAULT, () => {
      let dc = caller.createDataChannel('');

      expect(dc instanceof webrtc.RTCDataChannel).assertTrue();
      expect(dc.label).assertEqual('');
      expect(dc.ordered).assertEqual(true);
      expect(dc.maxPacketLifeTime).assertUndefined();
      expect(dc.maxRetransmits).assertUndefined();
      expect(dc.protocol).assertEqual('');
      expect(dc.negotiated).assertFalse()
      expect(dc.id).assertUndefined()
      expect(dc.readyState).assertEqual('connecting');
      expect(dc.bufferedAmount).assertEqual(0);
      expect(dc.bufferedAmountLowThreshold).assertEqual(0);
      expect(dc.binaryType).assertEqual('blob');
    })

    it("createDataChannelWithProvidedParametersShouldInitializeAttributesToProvidedValues", DEFAULT, async () => {
      let dc = caller.createDataChannel('test', {
        ordered: false,
        maxRetransmits: 1,
        // Note: maxPacketLifeTime is not set in this test.
        protocol: 'custom',
        negotiated: true,
        id: 3
      });
      expect(dc instanceof webrtc.RTCDataChannel).assertTrue();
      expect(dc.label).assertEqual('test');
      expect(dc.ordered).assertEqual(false);
      expect(dc.maxPacketLifeTime).assertUndefined();
      expect(dc.maxRetransmits).assertEqual(1);
      expect(dc.protocol).assertEqual('custom');
      expect(dc.negotiated).assertEqual(true);
      expect(dc.id).assertEqual(3);
      expect(dc.readyState).assertEqual('connecting');
      expect(dc.bufferedAmount).assertEqual(0);
      expect(dc.bufferedAmountLowThreshold).assertEqual(0);
      // 创建RTCDataChannel对象时，binaryType属性必须初始化为字符串"blob"
      expect(dc.binaryType).assertEqual('blob');

      let dc2 = caller.createDataChannel('test2', {
        ordered: false,
        maxPacketLifeTime: 42
      });
      expect(dc2.label).assertEqual('test2');
      expect(dc2.maxPacketLifeTime).assertEqual(42);
      expect(dc2.maxRetransmits).assertUndefined();
    })

    it("createDataChannelWithOrderedUndefinedShouldSucceed", DEFAULT, () => {
      let dc2 = caller.createDataChannel('', {
        ordered: undefined
      });
      expect(dc2.ordered).assertTrue();
    })

    it("createDataChannelWithMaxPacketLifeTime0ShouldSucceed", DEFAULT, () => {
      let dc = caller.createDataChannel('', {
        maxPacketLifeTime: 0
      });
      expect(dc.maxPacketLifeTime).assertEqual(0);
    })

    it("createDataChannelWithMaxRetransmits0ShouldSucceed", DEFAULT, () => {
      const dc = caller.createDataChannel('', {
        maxRetransmits: 0
      });
      expect(dc.maxRetransmits).assertEqual(0);
    })

    it("createDataChannelWithBothMaxPacketLifeTimeAndMaxRetransmitsUndefinedShouldSucceed", DEFAULT, () => {
      try {
        caller.createDataChannel('', {
          maxPacketLifeTime: undefined,
          maxRetransmits: undefined
        });
      } catch (e) {
        expect().assertFail()
      }
    })

    it("channelsCreatedAfterSetRemoteDescriptionShouldHaveIdAssigned", DEFAULT, async () => {
      let negotiatedDc = caller.createDataChannel('negotiated-channel', {
        negotiated: true,
        id: 42,
      });
      expect(negotiatedDc.id).assertEqual(42);

      let dc1 = caller.createDataChannel('channel');
      expect(dc1.id).assertUndefined();

      let offer = await caller.createOffer();
      await Promise.all([caller.setLocalDescription(offer), callee.setRemoteDescription(offer)]);
      let answer = await callee.createAnswer();
      await caller.setRemoteDescription(answer);

      expect(dc1.id).not().assertUndefined();
      expect(dc1.id).assertLargerOrEqual(0);
      expect(dc1.id).assertLess(65535);

      let dc2 = caller.createDataChannel('channel');

      expect(dc2.id).not().assertUndefined();
      expect(dc2.id).assertLarger(0);
      expect(dc2.id).assertLess(65535);
      expect(dc2).not().assertEqual(dc1);
      expect(dc2.label).assertEqual(dc1.label);
      expect(dc2.id).not().assertEqual(dc1.id);
      expect(negotiatedDc.id).assertEqual(42);
    })

    it("createAnswerReturnsRTCSessionDescriptionInit", DEFAULT, async () => {
      let offer = await caller.createOffer();
      await caller.setRemoteDescription(offer);
      let answer = await caller.createAnswer();
      expect(typeof answer).assertEqual('object');
      expect(answer instanceof webrtc.RTCSessionDescription).assertFalse();
    })

    it("canTrickleIceCandidatesPropertyIsTrueAfterSetRemoteDescriptionWithA", DEFAULT, () => {
      caller.setRemoteDescription(new webrtc.RTCSessionDescription({
        type: 'offer', sdp: sdp
      }));
      expect(caller.canTrickleIceCandidates).assertTrue();
    })

    it("addTransceiverAudioShouldReturnAnAudioTransceiver", DEFAULT, () => {
      let transceiver = caller.addTransceiver('audio');
      expect(transceiver instanceof webrtc.RTCRtpTransceiver).assertTrue();

      expect(transceiver.mid).assertNull();

      expect(transceiver.direction).assertEqual('sendrecv');
      expect(transceiver.currentDirection).assertNull();
      expect([transceiver]).assertDeepEquals(caller.getTransceivers());

      let sender = transceiver.sender;

      expect(sender instanceof webrtc.RTCRtpSender).assertTrue();
      expect(sender.track).assertNull();
      expect([sender]).assertDeepEquals(caller.getSenders());

      let receiver = transceiver.receiver;
      expect(receiver instanceof webrtc.RTCRtpReceiver);

      let track = receiver.track;
      expect(track instanceof webrtc.MediaStreamTrack).assertTrue();
      expect(track.kind).assertEqual('audio');
      expect(track.readyState).assertEqual('live');
      expect([receiver]).assertDeepEquals(caller.getReceivers());
    })

  })
}