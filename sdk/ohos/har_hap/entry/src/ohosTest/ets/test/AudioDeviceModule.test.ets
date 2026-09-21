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

const TAG: string = '[AudioDeviceModuleTest]';

export default function AudioDeviceModuleTest() {
  describe('AudioDeviceModuleTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('setNoiseSuppressorEnabledReturnsFalseWithoutDevice', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      let result = adm.setNoiseSuppressorEnabled(true);
      expect(result).assertFalse();
    })

    it('setSpeakerMuteThenCreatePcAndSignaling', 0, async () => {
      let adm = new webrtc.AudioDeviceModule();
      adm.setSpeakerMute(true);
      let factory = new webrtc.PeerConnectionFactory({ adm: adm });
      let caller = factory.createPeerConnection({});
      let callee = factory.createPeerConnection({});
      let audioSource = factory.createAudioSource();
      let audioTrack = factory.createAudioTrack('audio', audioSource);
      caller.addTrack(audioTrack);
      await caller.setLocalDescription(await caller.createOffer());
      await callee.setRemoteDescription(caller.localDescription);
      await callee.setLocalDescription(await callee.createAnswer());
      await caller.setRemoteDescription(callee.localDescription);
      expect(caller.signalingState).assertEqual('stable');
      expect(callee.signalingState).assertEqual('stable');
      caller.close();
      callee.close();
      audioSource.release();
    })

    it('setMicrophoneMuteThenCreatePcAndSignaling', 0, async () => {
      let adm = new webrtc.AudioDeviceModule();
      adm.setMicrophoneMute(true);
      let factory = new webrtc.PeerConnectionFactory({ adm: adm });
      let caller = factory.createPeerConnection({});
      let callee = factory.createPeerConnection({});
      let audioSource = factory.createAudioSource();
      let audioTrack = factory.createAudioTrack('audio', audioSource);
      caller.addTrack(audioTrack);
      await caller.setLocalDescription(await caller.createOffer());
      await callee.setRemoteDescription(caller.localDescription);
      await callee.setLocalDescription(await callee.createAnswer());
      await caller.setRemoteDescription(callee.localDescription);
      expect(caller.signalingState).assertEqual('stable');
      expect(callee.signalingState).assertEqual('stable');
      caller.close();
      callee.close();
      audioSource.release();
    })

    it('releaseThenCreateNewAdmDoesNotCrash', 0, () => {
      let adm1 = new webrtc.AudioDeviceModule();
      adm1.release();
      let adm2 = new webrtc.AudioDeviceModule();
      let factory = new webrtc.PeerConnectionFactory({ adm: adm2 });
      let pc = factory.createPeerConnection({});
      expect(pc.signalingState).assertEqual('stable');
      pc.close();
    })

    it('isBuiltInAcousticEchoCancelerSupportedReturnsBoolean', 0, () => {
      let result = webrtc.AudioDeviceModule.isBuiltInAcousticEchoCancelerSupported();
      expect(typeof result).assertEqual('boolean');
    })

    it('isBuiltInNoiseSuppressorSupportedReturnsBoolean', 0, () => {
      let result = webrtc.AudioDeviceModule.isBuiltInNoiseSuppressorSupported();
      expect(typeof result).assertEqual('boolean');
    })

    it('callbackPropertiesExist', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      expect(adm.oncapturererror).assertNull();
      expect(adm.oncapturerstatechange).assertNull();
      expect(adm.oncapturersamplesready).assertNull();
      expect(adm.onrenderererror).assertNull();
      expect(adm.onrendererstatechange).assertNull();
    })

    // ---- #11: 硬件 AEC/NS options 诚实化 ----

    it('constructAdmWithHardwareAecOptions', 0, () => {
      const admOn = new webrtc.AudioDeviceModule({ useHardwareAcousticEchoCanceler: true });
      expect(admOn).not().assertNull();
      const admOff = new webrtc.AudioDeviceModule({ useHardwareAcousticEchoCanceler: false });
      expect(admOff).not().assertNull();
    })

    it('constructAdmWithStringHardwareAecThrows', 0, () => {
      let thrown = false;
      try {
        const options: ESObject = {};
        options.useHardwareAcousticEchoCanceler = 'yes';
        new webrtc.AudioDeviceModule(options);
      } catch (e) {
        thrown = true;
      }
      expect(thrown).assertTrue();
    })

    it('constructAdmWithNumberNoiseSuppressorThrows', 0, () => {
      let thrown = false;
      try {
        const options: ESObject = {};
        options.useHardwareNoiseSuppressor = 1;
        new webrtc.AudioDeviceModule(options);
      } catch (e) {
        thrown = true;
      }
      expect(thrown).assertTrue();
    })

    it('builtInEffectSupportQueriesReturnFalseOnApi12Devices', 0, () => {
      // API12 无法按设备查询系统 AEC、且无系统 NS 接口,如实返回 false
      expect(webrtc.AudioDeviceModule.isBuiltInAcousticEchoCancelerSupported()).assertFalse();
      expect(webrtc.AudioDeviceModule.isBuiltInNoiseSuppressorSupported()).assertFalse();
    })

    it('setNoiseSuppressorEnabledReturnsFalseHonestly', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      expect(adm.setNoiseSuppressorEnabled(true)).assertFalse();
      expect(adm.setNoiseSuppressorEnabled(false)).assertFalse();
    })
  })
}
