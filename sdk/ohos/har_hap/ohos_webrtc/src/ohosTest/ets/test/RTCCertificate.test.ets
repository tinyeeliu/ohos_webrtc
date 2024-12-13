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

const DEFAULT = 0
const TAG: string = '[RTCCertificateTest]';

export default function RTCCertificateTest() {
  describe('RTCCertificateTest', () => {
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

    it("generateCertificateWithString", DEFAULT, async () => {
      let Certificate = await webrtc.RTCPeerConnection.generateCertificate("RSA");
      expect(Certificate.expires).not().assertUndefined();
      expect(Certificate.getFingerprints()[0].algorithm).not().assertUndefined();
      expect(Certificate.getFingerprints()[0].value).not().assertUndefined();
      expect(Certificate.getFingerprints().length).assertEqual(1);
    })

    it("generateCertificateWithObject", DEFAULT, async () => {
      const keygenAlgorithm: webrtc.AlgorithmIdentifier = {
        name: "RSA",
      };
      try {
        await webrtc.RTCPeerConnection.generateCertificate(keygenAlgorithm);
        expect().assertFail();
      } catch (e) {
        console.error(TAG, "e: ", e);
      }
    })

    it("generateCertificateWithErrorString", DEFAULT, async () => {
      let Certificate = await webrtc.RTCPeerConnection.generateCertificate("1");
      expect(Certificate.expires).not().assertUndefined();
      expect(Certificate.getFingerprints()[0].algorithm).not().assertUndefined();
      expect(Certificate.getFingerprints()[0].value).not().assertUndefined();
    })

  })
}