import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium'
import webrtc from 'libohos_webrtc.so';
import { Logging, LoggingSeverity } from '../../../main/ets/log/Logging';

const DEFAULT = 0
const TAG: string = '[RTCDtlsTransportTest]';

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

function sleep(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function createDtlsTransport(caller: webrtc.RTCPeerConnection, callee: webrtc.RTCPeerConnection) {
  let audioSource = pcf?.createAudioSource();
  let audioTrack = pcf?.createAudioTrack("audio", audioSource);
  caller.addTrack(audioTrack);
  exchangeIceCandidates(caller, callee);
  await exchangeOfferAnswer(caller, callee);
}

export default function RTCDtlsTransportTest() {
  describe('RTCDtlsTransportTest', () => {
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

    it("transportGoesToConnectedState", DEFAULT, async () => {
      createDtlsTransport(caller, callee);
      await sleep(300);
      // ToDo 有时transport还没初始化好
      let transport1 = caller.getTransceivers()[0].sender.transport;
      let transport2 = callee.getTransceivers()[0].sender.transport;
      let flag1: string | undefined = "";
      let flag2: string | undefined = "";
      transport1!.onstatechange = () => {
        console.info(TAG, "state: ", transport1?.state);
        if (transport1?.state == "connected") {
          flag1 = transport1?.state;
        }
      }
      transport2!.onstatechange = () => {
        console.info(TAG, "state: ", transport2?.state);
        if (transport2?.state == "connected") {
          flag2 = transport2?.state;
        }
      }
      await sleep(5000);
      expect(flag1).assertEqual("connected");
      expect(flag2).assertEqual("connected");
    })

    it("closeCausesTheLocalTransportToCloseImmediately", DEFAULT, async () => {
      createDtlsTransport(caller, callee);
      await sleep(300);
      let transport1 = caller.getTransceivers()[0].sender.transport;
      let transport2 = callee.getTransceivers()[0].sender.transport;
      let flag: string | undefined = "";
      transport1!.onstatechange = () => {
        console.info(TAG, "state: ", transport1?.state);
        if (transport1?.state == "closed") {
          flag = transport1?.state;
        }
      }
      caller.close();
      await sleep(1000);
      expect(flag).assertEqual("closed");
    })

    it("closeCausesTheOtherEndTransportToClose", DEFAULT, async () => {
      // ToDo state改变的时间不太好把握
      createDtlsTransport(caller, callee);
      await sleep(300);
      let transport1 = caller.getTransceivers()[0].sender.transport;
      let transport2 = callee.getTransceivers()[0].sender.transport;
      let flag: string | undefined = "";
      transport1!.onstatechange = () => {
        console.info(TAG, "state1: ", transport1?.state);
      }
      transport2!.onstatechange = () => {
        console.info(TAG, "state2: ", transport2?.state);
        if (transport2?.state == "closed") {
          flag = transport2?.state;
        }
      }
      await sleep(1000);
      caller.close();
      await sleep(1000);
      expect(flag).assertEqual("closed");
    })

    it("iceTransportIsNotNullAfterCreateDtlsTransport", DEFAULT, async () => {
      createDtlsTransport(caller, callee);
      await sleep(1000);
      expect(caller.getSenders()[0]!.transport).not().assertNull();
    })

    it("stateIsNotNullAfterCreateDtlsTransport", DEFAULT, async () => {
      createDtlsTransport(caller, callee);
      await sleep(1000);
      console.info(TAG, "state: ", caller.getSenders()[0]!.transport!.state);
      expect(caller.getSenders()[0]!.transport!.state).not().assertNull();
    })

    it("getRemoteCertificatesIsNotNullAfterCreateDtlsTransport", DEFAULT, async () => {
      createDtlsTransport(caller, callee);
      await sleep(1000);
      console.info(TAG, "getRemoteCertificates: ",
        JSON.stringify(caller.getSenders()[0]?.transport?.getRemoteCertificates()));
      expect(caller.getSenders()[0]?.transport?.getRemoteCertificates()).not().assertNull();
    })

  })
}