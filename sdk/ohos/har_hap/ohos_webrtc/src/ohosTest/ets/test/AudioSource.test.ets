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

const TAG: string = '[AudioSourceTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function AudioSourceTest() {
  describe('AudioSourceTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('createAudioSourceWithDefaultConstraints', 0, () => {
      let audioSource = pcf.createAudioSource();
      expect(audioSource).not().assertNull();
    })

    it('createAudioSourceWithConstraints', 0, () => {
      let audioSource = pcf.createAudioSource({
        echoCancellation: false,
        autoGainControl: false,
        noiseSuppression: false,
      });
      expect(audioSource).not().assertNull();
    })

    it('stateIsLive', 0, () => {
      let audioSource = pcf.createAudioSource();
      expect(audioSource.state).assertEqual('live');
    })

    it('setVolume', 0, () => {
      let audioSource = pcf.createAudioSource();
      audioSource.setVolume(0.5);
    })

    it('setVolumeToMax', 0, () => {
      let audioSource = pcf.createAudioSource();
      audioSource.setVolume(1.0);
    })

    it('setVolumeToMin', 0, () => {
      let audioSource = pcf.createAudioSource();
      audioSource.setVolume(0.0);
    })

    it('release', 0, () => {
      let audioSource = pcf.createAudioSource();
      audioSource.release();
    })

    it('stateAfterRelease', 0, () => {
      let audioSource = pcf.createAudioSource();
      audioSource.release();
      try {
        let state = audioSource.state;
        console.info(TAG, 'state after release: ' + state);
      } catch (e) {
        console.info(TAG, 'expected error after release: ' + JSON.stringify(e));
        expect(e).assertInstanceOf('Error');
      }
    })

    it('setVolumeAfterReleaseThrows', 0, () => {
      let audioSource = pcf.createAudioSource();
      audioSource.release();
      try {
        audioSource.setVolume(0.5);
        console.error(TAG, 'should have thrown');
        expect().assertFail();
      } catch (e) {
        console.info(TAG, 'expected error: ' + JSON.stringify(e));
        expect(e).assertInstanceOf('Error');
      }
    })

    it('doubleReleaseDoesNotCrash', 0, () => {
      let audioSource = pcf.createAudioSource();
      audioSource.release();
      try {
        audioSource.release();
      } catch (e) {
        console.info(TAG, 'second release threw: ' + JSON.stringify(e));
      }
    })
  })
}
