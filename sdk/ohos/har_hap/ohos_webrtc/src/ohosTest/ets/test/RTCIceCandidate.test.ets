import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium'
import webrtc from 'libohos_webrtc.so';
import { Logging, LoggingSeverity } from '../../../main/ets/log/Logging';

const DEFAULT = 0

const candidateString = 'candidate:1905690388 1 udp 2113937151 192.168.0.1 58041 typ host generation 0 ufrag thC8 network-cost 50';
const candidateString2 = 'candidate:435653019 2 tcp 1845501695 192.168.0.196 4444 typ srflx raddr www.example.com rport 22222 tcptype active';
const arbitraryString = '<arbitrary string[0] content>;';

export default function RTCIceCandidateTest() {
  describe('RTCIceCandidateTest', () => {
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
    })
    afterEach(() => {
      // Presets a clear action, which is performed after each unit test case ends.
      // The number of execution times is the same as the number of test cases defined by **it**.
      // This API supports only one parameter: clear action function.
    })
    afterAll(() => {
      // Presets a clear action, which is performed after all test cases of the test suite end.
      // This API supports only one parameter: clear action function.
    })

    it("newRTCIceCandidateSetSdpMidAudio", DEFAULT, () => {
      let iceCandidateInit: webrtc.RTCIceCandidateInit = {
        candidate: "",
        sdpMid: "audio",
      };
      let res = new webrtc.RTCIceCandidate(iceCandidateInit);
      expect(res.candidate).assertEqual("");
      expect(res.sdpMid).assertEqual("audio");
      expect(res.sdpMLineIndex).assertUndefined();
      expect(res.usernameFragment).assertUndefined();
    })

    it("newRTCIceCandidateSdpMLineIndexZero", DEFAULT, () => {
      let iceCandidateInit: webrtc.RTCIceCandidateInit = {
        candidate: "",
        sdpMLineIndex: 0,
      };
      let res = new webrtc.RTCIceCandidate(iceCandidateInit);
      expect(res.candidate).assertEqual("");
      expect(res.sdpMid).assertUndefined();
      expect(res.sdpMLineIndex).assertEqual(0);
      expect(res.usernameFragment).assertUndefined();
    })

    it("newRTCIceCandidateSetSdpMidAudioSdpMLineIndexZero", DEFAULT, () => {
      let iceCandidateInit: webrtc.RTCIceCandidateInit = {
        candidate: "",
        sdpMLineIndex: 0,
        sdpMid: "audio",
      };
      let res = new webrtc.RTCIceCandidate(iceCandidateInit);
      expect(res.candidate).assertEqual("");
      expect(res.sdpMid).assertEqual("audio");
      expect(res.sdpMLineIndex).assertEqual(0);
      expect(res.usernameFragment).assertUndefined();
    })

    it("newRTCIceCandidateWithValidCandidateStringAndSdpMid", DEFAULT, () => {
      let iceCandidateInit: webrtc.RTCIceCandidateInit = {
        candidate: candidateString,
        sdpMid: "audio",
      };
      let res = new webrtc.RTCIceCandidate(iceCandidateInit);
      expect(res.candidate).assertEqual(candidateString);
      expect(res.sdpMid).assertEqual("audio");
      expect(res.sdpMLineIndex).assertUndefined();
      expect(res.usernameFragment).assertEqual("thC8");
    })

    it("newRTCIceCandidateWithNonDefaultValuesForAllFields", DEFAULT, () => {
      let iceCandidateInit: webrtc.RTCIceCandidateInit = {
        candidate: candidateString,
        sdpMid: 'video',
        sdpMLineIndex: 1,
        usernameFragment: 'test'
      };
      let res = new webrtc.RTCIceCandidate(iceCandidateInit);
      expect(res.candidate).assertEqual(candidateString);
      expect(res.sdpMid).assertEqual('video');
      expect(res.sdpMLineIndex).assertEqual(1);
      // TODO usernameFragment 是获取的 candidate 的 username
      // expect(res.usernameFragment).assertEqual('test');
      expect(res.foundation).assertEqual('1905690388');
      expect(res.component).assertEqual('rtp');
      expect(res.priority).assertEqual(2113937151);
      expect(res.address).assertEqual('192.168.0.1');
      expect(res.protocol).assertEqual('udp');
      expect(res.port).assertEqual(58041);
      expect(res.type).assertEqual("host");
      expect(res.tcpType).assertUndefined();
      expect(res.relatedAddress).assertUndefined();
      expect(res.relatedPort).assertEqual(0);
    })

    it("newRTCIceCandidateWithNonDefaultValuesForAllFieldsTcpCandidate", DEFAULT, () => {
      let iceCandidateInit: webrtc.RTCIceCandidateInit = {
        candidate: candidateString2,
        sdpMid: 'video',
        sdpMLineIndex: 1,
        usernameFragment: 'user1'
      };
      let res = new webrtc.RTCIceCandidate(iceCandidateInit);

      expect(res.candidate).assertEqual(candidateString2);
      expect(res.sdpMid).assertEqual('video');
      expect(res.sdpMLineIndex).assertEqual(1);
      // TODO usernameFragment 是获取的 candidate 的 username
      // expect(res.usernameFragment).assertEqual('user1');
      expect(res.foundation).assertEqual('435653019');
      expect(res.component).assertEqual('rtcp');
      expect(res.priority).assertEqual(1845501695);
      expect(res.address).assertEqual('192.168.0.196');
      expect(res.protocol).assertEqual('tcp');
      expect(res.port).assertEqual(4444);
      expect(res.type).assertEqual("srflx");
      expect(res.tcpType).assertEqual("active");
      expect(res.relatedAddress).assertEqual('www.example.com');
      expect(res.relatedPort).assertEqual(22222);
    })

    it("newRTCIceCandidateWithInvalidSdpMid", DEFAULT, () => {
      let iceCandidateInit: webrtc.RTCIceCandidateInit = {
        candidate: "",
        sdpMid: arbitraryString
      };
      let res = new webrtc.RTCIceCandidate(iceCandidateInit);

      expect(res.candidate).assertEqual("");
      expect(res.sdpMid).assertEqual(arbitraryString);
      expect(res.sdpMLineIndex).assertUndefined();
      expect(res.usernameFragment).assertUndefined();
    })

    it("newRTCIceCandidateWithInvalidSdpMLineIndex", DEFAULT, () => {
      let iceCandidateInit: webrtc.RTCIceCandidateInit = {
        candidate: "",
        sdpMLineIndex: 65535
      };
      let res = new webrtc.RTCIceCandidate(iceCandidateInit);

      expect(res.candidate).assertEqual("");
      expect(res.sdpMid).assertUndefined();
      expect(res.sdpMLineIndex).assertEqual(65535);
      expect(res.usernameFragment).assertUndefined();
    })
  })
}