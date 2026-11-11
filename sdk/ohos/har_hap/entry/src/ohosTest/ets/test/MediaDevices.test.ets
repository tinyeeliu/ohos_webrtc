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
import webrtc, { Logging, LoggingSeverity } from '@ohos/webrtc';

const TAG: string = '[MediaDevicesTest]';

export default function MediaDevicesTest() {
  describe('MediaDevicesTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('shouldReturnArrayOfMediaDeviceInfoWhenEnumerateDevices', 0, async () => {
      let mediaDevices = new webrtc.MediaDevices();
      let devices = await mediaDevices.enumerateDevices();
      expect(Array.isArray(devices)).assertTrue();
      if (devices.length > 0) {
        let device = devices[0];
        expect(typeof device.deviceId).assertEqual('string');
        expect(device.kind === 'audioinput' || device.kind === 'audiooutput' || device.kind === 'videoinput').assertTrue();
        expect(typeof device.groupId).assertEqual('string');
      }
    })

    it('shouldRejectGetUserMediaWithNoAudioAndVideo', 0, async () => {
      let mediaDevices = new webrtc.MediaDevices();
      try {
        await mediaDevices.getUserMedia({ audio: false, video: false });
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('shouldReturnMediaStreamWhenGetUserMediaWithAudio', 0, async () => {
      let mediaDevices = new webrtc.MediaDevices();
      let stream = await mediaDevices.getUserMedia({ audio: true });
      let track: webrtc.MediaStreamTrack | null = null;
      try {
        expect(stream instanceof webrtc.MediaStream).assertTrue();
        let audioTracks = stream.getAudioTracks();
        expect(audioTracks.length).assertEqual(1);
        track = audioTracks[0];
        expect(track.kind).assertEqual('audio');
        expect(typeof track.id).assertEqual('string');
        expect(track.id.length).assertLarger(0);
        expect(track.readyState).assertEqual('live');
        expect(track.enabled).assertTrue();
        expect(stream.getVideoTracks().length).assertEqual(0);
        expect(stream.active).assertTrue();
      } finally {
        if (track) {
          track.stop();
        }
      }
    })

    it('shouldReturnValidObjectWhenGetSupportedConstraints', 0, () => {
      let mediaDevices = new webrtc.MediaDevices();
      let constraints = mediaDevices.getSupportedConstraints();
      expect(constraints).not().assertNull();
      expect(typeof constraints.width).assertEqual('boolean');
      expect(typeof constraints.height).assertEqual('boolean');
      expect(typeof constraints.frameRate).assertEqual('boolean');
      expect(typeof constraints.echoCancellation).assertEqual('boolean');
      expect(typeof constraints.autoGainControl).assertEqual('boolean');
      expect(typeof constraints.noiseSuppression).assertEqual('boolean');
      expect(typeof constraints.deviceId).assertEqual('boolean');
      expect(typeof constraints.groupId).assertEqual('boolean');
    })
  })
}
