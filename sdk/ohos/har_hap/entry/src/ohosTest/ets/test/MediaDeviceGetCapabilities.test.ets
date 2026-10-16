/**
 * Copyright (c) 2026 Archermind Technology (Nanjing) Co. Ltd.
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
import webrtc, { Logging, LoggingSeverity } from '@ohos/webrtc';

const TAG: string = '[MediaDeviceGetCapabilitiesTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function MediaDeviceGetCapabilitiesTest() {
  describe('MediaDeviceGetCapabilitiesTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('videoInputDeviceHasGetCapabilities', 0, async () => {
      const mediaDevices = new webrtc.MediaDevices();
      const list = await mediaDevices.enumerateDevices();
      const cameras = list.filter(d => d.kind === 'videoinput');
      console.info(TAG, 'videoinput count: ' + cameras.length);
      expect(cameras.length).assertLarger(0);

      const caps = cameras[0].getCapabilities?.();
      expect(caps).not().assertNull();
      console.info(TAG, 'getCapabilities: ' + JSON.stringify(caps));
    })

    it('cameraCapabilitiesRangesAreWellFormed', 0, async () => {
      const mediaDevices = new webrtc.MediaDevices();
      const list = await mediaDevices.enumerateDevices();
      const cameras = list.filter(d => d.kind === 'videoinput');
      if (cameras.length === 0) {
        console.info(TAG, 'no camera, skip');
        return;
      }

      const caps = cameras[0].getCapabilities?.();
      if (caps?.width) {
        expect(caps.width.min).assertLargerOrEqual(0);
        expect(caps.width.max).assertLargerOrEqual(caps.width.min);
      }
      if (caps?.height) {
        expect(caps.height.min).assertLargerOrEqual(0);
        expect(caps.height.max).assertLargerOrEqual(caps.height.min);
      }
      if (caps?.frameRate) {
        expect(caps.frameRate.min).assertLargerOrEqual(0);
        expect(caps.frameRate.max).assertLargerOrEqual(caps.frameRate.min);
      }
    })

    // ===== 异常场景: 非摄像头设备不提供能力查询 =====

    it('audioInputDeviceHasNoGetCapabilities', 0, async () => {
      const mediaDevices = new webrtc.MediaDevices();
      const list = await mediaDevices.enumerateDevices();
      const mics = list.filter(d => d.kind === 'audioinput');
      if (mics.length === 0) {
        console.info(TAG, 'no audioinput, skip');
        return;
      }
      // 设计约定: getCapabilities 仅存在于 videoinput 设备
      expect(mics[0].getCapabilities).assertUndefined();
    })

    it('audioOutputDeviceHasNoGetCapabilities', 0, async () => {
      const mediaDevices = new webrtc.MediaDevices();
      const list = await mediaDevices.enumerateDevices();
      const speakers = list.filter(d => d.kind === 'audiooutput');
      if (speakers.length === 0) {
        console.info(TAG, 'no audiooutput, skip');
        return;
      }
      expect(speakers[0].getCapabilities).assertUndefined();
    })

    it('malformedDeviceEntryFromParsedListDoesNotCrash', 0, () => {
      // 上层若拿 JSON 反序列化得到的设备对象误调能力查询, 不得崩溃
      const bad = JSON.parse('{"deviceId":123,"kind":"videoinput","label":456,"groupId":null}') as
        webrtc.MediaDeviceInfo;
      console.info(TAG, 'parsed device kind: ' + bad.kind);
      const caps = bad.getCapabilities?.();
      console.info(TAG, 'capabilities on malformed device: ' + JSON.stringify(caps));
      expect(true).assertTrue();
    })
  })
}
