import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium'
import webrtc from 'libohos_webrtc.so';
import { Logging, LoggingSeverity } from '../../../main/ets/log/Logging';

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

function sleep(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
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
      await exchangeOfferAnswer(caller, callee);
      let iceTransport = caller.sctp?.transport.iceTransport;
      expect(iceTransport?.getSelectedCandidatePair()?.local?.usernameFragment).assertEqual("");
      expect(iceTransport?.getSelectedCandidatePair()?.remote?.usernameFragment).assertEqual("");
    })

    it("iceTransportShouldBeInStateNewInitially", DEFAULT, async () => {
      const transceiver = caller.addTransceiver('audio');
      await caller.setLocalDescription();
      const iceTransport = transceiver.sender.transport?.iceTransport;
      console.info(TAG, "gatheringState: ", iceTransport!.gatheringState);

      iceTransport!.ongatheringstatechange = () => {
        console.info(TAG, "gatheringState: ", iceTransport!.gatheringState);
      }
      expect(iceTransport?.state).assertEqual('new');
      // TODO gatheringState 可能是因为速度太快了 直接就是gathering 跳过了new
      // expect(iceTransport?.gatheringState).assertEqual('new');
    })

    it("iceTransportShouldTransitionToGatheringThenCompleteAfterSLD", DEFAULT, async () => {
      const transceiver = caller.addTransceiver('audio');
      await caller.setLocalDescription();
      const iceTransport = transceiver.sender.transport?.iceTransport;

      iceTransport!.ongatheringstatechange = () => {
        console.info(TAG, 'gatheringState: ', iceTransport?.gatheringState);
      };
      await sleep(1000);
      expect(iceTransport?.gatheringState).not().assertEqual("new");
    })

    it("afterInitializationAllAttributesAreNotNull", DEFAULT, async () => {
      let transport = await createIceTransport(caller, callee);
      await sleep(1000);
      expect(transport.role).not().assertUndefined();
      expect(transport.component).not().assertUndefined();
      expect(transport.state).not().assertUndefined();
      expect(transport.gatheringState).not().assertUndefined();
      expect(transport.getSelectedCandidatePair()).not().assertUndefined();
    })

    it("onstatechange", DEFAULT, async () => {
      let transport = await createIceTransport(caller, callee);
      let events: string[] = [];
      transport.onstatechange = () => {
        events.push(transport.state);
        console.info(TAG, 'state change: ', transport.state);
      };
      await sleep(5000);
      expect(events.length).not().assertEqual(0);
    })

    it("onSelectedCandidatePairChange", DEFAULT, async () => {
      let transport = await createIceTransport(caller, callee);
      let events: string[] = [];
      transport.onselectedcandidatepairchange = () => {
        events.push("1");
      };
      await sleep(1000);
      expect(events.length).not().assertEqual(0);
    })

    it("onGatheringStateChange", DEFAULT, async () => {
      let transport = await createIceTransport(caller, callee);
      let events: string[] = [];
      transport.ongatheringstatechange = () => {
        events.push(transport.gatheringState);
        console.info(TAG, 'gatheringState change: ', transport.gatheringState);
      };
      await sleep(1000);
      // ToDo 有时候状态变的太快，在我监听前就跑结束了
      expect(events.length).not().assertEqual(0);
    })

  })
}