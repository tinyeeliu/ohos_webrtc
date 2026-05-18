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

const TAG: string = '[AudioDeviceModuleTest]';

export default function AudioDeviceModuleTest() {
  describe('AudioDeviceModuleTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('constructorWithDefaultOptions', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      expect(adm).not().assertNull();
    })

    it('constructorWithOptions', 0, () => {
      let adm = new webrtc.AudioDeviceModule({
        audioSource: 0,
        audioFormat: 0,
        inputSampleRate: 48000,
        outputSampleRate: 48000,
        useStereoInput: false,
        useStereoOutput: false,
        useLowLatency: false,
        useHardwareAcousticEchoCanceler: false,
        useHardwareNoiseSuppressor: false,
      });
      expect(adm).not().assertNull();
    })

    it('setSpeakerMute', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      adm.setSpeakerMute(true);
      adm.setSpeakerMute(false);
    })

    it('setMicrophoneMute', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      adm.setMicrophoneMute(true);
      adm.setMicrophoneMute(false);
    })

    it('setNoiseSuppressorEnabledReturnsFalse', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      let result = adm.setNoiseSuppressorEnabled(true);
      expect(result).assertFalse();
    })

    it('isBuiltInAcousticEchoCancelerSupported', 0, () => {
      let result = webrtc.AudioDeviceModule.isBuiltInAcousticEchoCancelerSupported();
      expect(typeof result).assertEqual('boolean');
    })

    it('isBuiltInNoiseSuppressorSupported', 0, () => {
      let result = webrtc.AudioDeviceModule.isBuiltInNoiseSuppressorSupported();
      expect(typeof result).assertEqual('boolean');
    })

    it('release', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      adm.release();
    })

    it('oncapturererrorCanBeSet', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      adm.oncapturererror = (event) => {
        console.info(TAG, 'oncapturererror: ' + JSON.stringify(event));
      };
      expect(adm.oncapturererror).not().assertNull();
    })

    it('oncapturerstatechangeCanBeSet', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      adm.oncapturerstatechange = (event) => {
        console.info(TAG, 'oncapturerstatechange: ' + JSON.stringify(event));
      };
      expect(adm.oncapturerstatechange).not().assertNull();
    })

    it('onrenderererrorCanBeSet', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      adm.onrenderererror = (event) => {
        console.info(TAG, 'onrenderererror: ' + JSON.stringify(event));
      };
      expect(adm.onrenderererror).not().assertNull();
    })

    it('onrendererstatechangeCanBeSet', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      adm.onrendererstatechange = (event) => {
        console.info(TAG, 'onrendererstatechange: ' + JSON.stringify(event));
      };
      expect(adm.onrendererstatechange).not().assertNull();
    })

    it('oncapturersamplesreadyCanBeSet', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      adm.oncapturersamplesready = (event) => {
        console.info(TAG, 'oncapturersamplesready: ' + JSON.stringify(event));
      };
      expect(adm.oncapturersamplesready).not().assertNull();
    })

    it('eventCallbacksCanBeSetToNull', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      adm.oncapturererror = (event) => {};
      adm.oncapturererror = null;
      expect(adm.oncapturererror).assertNull();
    })
  })
}
