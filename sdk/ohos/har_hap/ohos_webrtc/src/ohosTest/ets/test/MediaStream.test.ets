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

const TAG: string = '[MediaStreamTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function MediaStreamTest() {
  describe('MediaStreamTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('addTrackAndActive', 0, () => {
      let stream = new webrtc.MediaStream();
      expect(stream.active).assertFalse();
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      stream.addTrack(audioTrack);
      expect(stream.active).assertTrue();
      audioSource.release();
    })

    it('removeTrackAndActive', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let stream = new webrtc.MediaStream([audioTrack]);
      expect(stream.active).assertTrue();
      stream.removeTrack(audioTrack);
      expect(stream.active).assertFalse();
      audioSource.release();
    })

    it('getTrackByIdFound', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let stream = new webrtc.MediaStream([audioTrack]);
      let found = stream.getTrackById(audioTrack.id);
      expect(found).not().assertNull();
      expect(found!.id).assertEqual(audioTrack.id);
      audioSource.release();
    })

    it('getTrackByIdNotFound', 0, () => {
      let stream = new webrtc.MediaStream();
      let found = stream.getTrackById('nonexistent');
      expect(found).assertNull();
    })

    it('mixedAudioAndVideoTracks', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      let stream = new webrtc.MediaStream([audioTrack, videoTrack]);
      expect(stream.getTracks().length).assertEqual(2);
      expect(stream.getAudioTracks().length).assertEqual(1);
      expect(stream.getVideoTracks().length).assertEqual(1);
      audioSource.release();
      videoSource.release();
    })

    it('idPropertyIsString', 0, () => {
      let stream = new webrtc.MediaStream();
      expect(typeof stream.id).assertEqual('string');
      expect(stream.id.length).assertLarger(0);
    })

    it('copyConstructorPreservesTracks', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let original = new webrtc.MediaStream([audioTrack]);
      let copy = new webrtc.MediaStream(original);
      expect(copy.getTracks().length).assertEqual(1);
      expect(copy.getAudioTracks()[0].id).assertEqual(audioTrack.id);
      audioSource.release();
    })
  })
}
