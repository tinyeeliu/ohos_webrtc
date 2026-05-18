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

import { describe, beforeAll, it, expect } from '@ohos/hypium'
import webrtc from 'libohos_webrtc.so';
import { Logging, LoggingSeverity } from '../../../main/ets/log/Logging';

const TAG: string = '[MediaDevicesTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function MediaDevicesTest() {
  describe('MediaDevicesTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('getSupportedConstraintsReturnsObject', 0, () => {
      let mediaDevices = new webrtc.MediaDevices();
      let constraints = mediaDevices.getSupportedConstraints();
      expect(constraints).not().assertNull();
      expect(typeof constraints).assertEqual('object');
    })

    it('getSupportedConstraintsHasWidth', 0, () => {
      let mediaDevices = new webrtc.MediaDevices();
      let constraints = mediaDevices.getSupportedConstraints();
      expect(typeof constraints.width).assertEqual('boolean');
    })

    it('getSupportedConstraintsHasHeight', 0, () => {
      let mediaDevices = new webrtc.MediaDevices();
      let constraints = mediaDevices.getSupportedConstraints();
      expect(typeof constraints.height).assertEqual('boolean');
    })

    it('getSupportedConstraintsHasFrameRate', 0, () => {
      let mediaDevices = new webrtc.MediaDevices();
      let constraints = mediaDevices.getSupportedConstraints();
      expect(typeof constraints.frameRate).assertEqual('boolean');
    })

    it('getSupportedConstraintsHasFacingMode', 0, () => {
      let mediaDevices = new webrtc.MediaDevices();
      let constraints = mediaDevices.getSupportedConstraints();
      expect(typeof constraints.facingMode).assertEqual('boolean');
    })

    it('getSupportedConstraintsHasEchoCancellation', 0, () => {
      let mediaDevices = new webrtc.MediaDevices();
      let constraints = mediaDevices.getSupportedConstraints();
      expect(typeof constraints.echoCancellation).assertEqual('boolean');
    })

    it('getSupportedConstraintsHasAutoGainControl', 0, () => {
      let mediaDevices = new webrtc.MediaDevices();
      let constraints = mediaDevices.getSupportedConstraints();
      expect(typeof constraints.autoGainControl).assertEqual('boolean');
    })

    it('getSupportedConstraintsHasNoiseSuppression', 0, () => {
      let mediaDevices = new webrtc.MediaDevices();
      let constraints = mediaDevices.getSupportedConstraints();
      expect(typeof constraints.noiseSuppression).assertEqual('boolean');
    })

    it('enumerateDevicesReturnsPromise', 0, async () => {
      let mediaDevices = new webrtc.MediaDevices();
      try {
        let devices = await mediaDevices.enumerateDevices();
        expect(Array.isArray(devices)).assertTrue();
      } catch (e) {
        console.error(TAG, 'enumerateDevices error: ', JSON.stringify(e));
      }
    })
  })
}
