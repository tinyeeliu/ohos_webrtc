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

import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium'
import webrtc, { Logging, LoggingSeverity } from '@ohos/webrtc';
import {
  UDP_HOST_CANDIDATE_STRING,
  TCP_SRFLX_CANDIDATE_STRING,
  ARBITRARY_CANDIDATE_STRING
} from './TestFixtures';

const DEFAULT = 0

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

    it('shouldCreateRTCIceCandidateWithSdpMidAudio', DEFAULT, () => {
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

    it('shouldCreateRTCIceCandidateWithSdpMLineIndexZero', DEFAULT, () => {
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

    it('shouldCreateRTCIceCandidateWithSdpMidAndSdpMLineIndexZero', DEFAULT, () => {
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

    it('shouldCreateRTCIceCandidateWithValidCandidateStringAndSdpMid', DEFAULT, () => {
      let iceCandidateInit: webrtc.RTCIceCandidateInit = {
        candidate: UDP_HOST_CANDIDATE_STRING,
        sdpMid: "audio",
      };
      let res = new webrtc.RTCIceCandidate(iceCandidateInit);
      expect(res.candidate).assertEqual(UDP_HOST_CANDIDATE_STRING);
      expect(res.sdpMid).assertEqual("audio");
      expect(res.sdpMLineIndex).assertUndefined();
      expect(res.usernameFragment).assertEqual("thC8");
    })

    it('shouldCreateRTCIceCandidateWithNonDefaultValuesForAllFields', DEFAULT, () => {
      let iceCandidateInit: webrtc.RTCIceCandidateInit = {
        candidate: UDP_HOST_CANDIDATE_STRING,
        sdpMid: 'video',
        sdpMLineIndex: 1,
        usernameFragment: 'test'
      };
      let res = new webrtc.RTCIceCandidate(iceCandidateInit);
      expect(res.candidate).assertEqual(UDP_HOST_CANDIDATE_STRING);
      expect(res.sdpMid).assertEqual('video');
      expect(res.sdpMLineIndex).assertEqual(1);
      expect(res.usernameFragment).assertEqual('test');
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

    it('shouldCreateRTCIceCandidateWithNonDefaultTcpCandidateFields', DEFAULT, () => {
      let iceCandidateInit: webrtc.RTCIceCandidateInit = {
        candidate: TCP_SRFLX_CANDIDATE_STRING,
        sdpMid: 'video',
        sdpMLineIndex: 1,
        usernameFragment: 'user1'
      };
      let res = new webrtc.RTCIceCandidate(iceCandidateInit);

      expect(res.candidate).assertEqual(TCP_SRFLX_CANDIDATE_STRING);
      expect(res.sdpMid).assertEqual('video');
      expect(res.sdpMLineIndex).assertEqual(1);
      expect(res.usernameFragment).assertEqual('user1');
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

    it('shouldCreateRTCIceCandidateWithInvalidSdpMid', DEFAULT, () => {
      let iceCandidateInit: webrtc.RTCIceCandidateInit = {
        candidate: "",
        sdpMid: ARBITRARY_CANDIDATE_STRING
      };
      let res = new webrtc.RTCIceCandidate(iceCandidateInit);

      expect(res.candidate).assertEqual("");
      expect(res.sdpMid).assertEqual(ARBITRARY_CANDIDATE_STRING);
      expect(res.sdpMLineIndex).assertUndefined();
      expect(res.usernameFragment).assertUndefined();
    })

    it('shouldCreateRTCIceCandidateWithInvalidSdpMLineIndex', DEFAULT, () => {
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
