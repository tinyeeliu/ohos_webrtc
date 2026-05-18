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

const TAG: string = '[PeerConnectionFactoryTest]';

export default function PeerConnectionFactoryTest() {
  describe('PeerConnectionFactoryTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('constructorWithDefaultOptions', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      expect(pcf).not().assertNull();
    })

    it('constructorWithOptions', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory({});
      expect(pcf).not().assertNull();
    })

    it('constructorWithOptionsWithAdm', 0, () => {
      let adm = new webrtc.AudioDeviceModule();
      let pcf = new webrtc.PeerConnectionFactory({
        adm: adm,
      });
      expect(pcf).not().assertNull();
    })

    it('createPeerConnection', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      let pc = pcf.createPeerConnection({
        iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
      });
      expect(pc).not().assertNull();
      expect(pc.signalingState).assertEqual('stable');
      pc.close();
    })

    it('createPeerConnectionWithEmptyConfig', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      let pc = pcf.createPeerConnection({});
      expect(pc).not().assertNull();
      pc.close();
    })

    it('createAudioSourceWithDefaultConstraints', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      let audioSource = pcf.createAudioSource();
      expect(audioSource).not().assertNull();
      expect(audioSource.state).assertEqual('live');
    })

    it('createAudioSourceWithConstraints', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      let audioSource = pcf.createAudioSource({
        echoCancellation: false,
        autoGainControl: false,
        noiseSuppression: false,
      });
      expect(audioSource).not().assertNull();
      expect(audioSource.state).assertEqual('live');
    })

    it('createAudioTrack', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack).not().assertNull();
      expect(audioTrack.kind).assertEqual('audio');
      expect(audioTrack.readyState).assertEqual('live');
      expect(audioTrack.enabled).assertTrue();
    })

    it('createAudioTrackWithDifferentIds', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      let audioSource = pcf.createAudioSource();
      let track1 = pcf.createAudioTrack('track1', audioSource);
      let track2 = pcf.createAudioTrack('track2', audioSource);
      expect(track1.id).not().assertEqual(track2.id);
    })

    it('createVideoSourceWithDefaultConstraints', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      let videoSource = pcf.createVideoSource();
      expect(videoSource).not().assertNull();
    })

    it('createVideoSourceWithConstraints', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      let videoSource = pcf.createVideoSource({
        width: 640,
        height: 480,
        frameRate: { min: 15, max: 30 },
      });
      expect(videoSource).not().assertNull();
    })

    it('createVideoSourceWithScreencast', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      let videoSource = pcf.createVideoSource(undefined, true);
      expect(videoSource).not().assertNull();
    })

    it('createVideoTrack', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();
      expect(videoTrack.kind).assertEqual('video');
      expect(videoTrack.readyState).assertEqual('live');
      expect(videoTrack.enabled).assertTrue();
    })

    it('createVideoTrackWithDifferentIds', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      let videoSource = pcf.createVideoSource();
      let track1 = pcf.createVideoTrack('track1', videoSource);
      let track2 = pcf.createVideoTrack('track2', videoSource);
      expect(track1.id).not().assertEqual(track2.id);
    })

    it('getDefaultReturnsDefaultFactory', 0, () => {
      let defaultPcf = webrtc.PeerConnectionFactory.getDefault();
      expect(defaultPcf).not().assertNull();
    })

    it('setDefaultChangesDefaultFactory', 0, () => {
      let customPcf = new webrtc.PeerConnectionFactory();
      webrtc.PeerConnectionFactory.setDefault(customPcf);
      let currentDefault = webrtc.PeerConnectionFactory.getDefault();
      expect(currentDefault).not().assertNull();

      let originalPcf = new webrtc.PeerConnectionFactory();
      webrtc.PeerConnectionFactory.setDefault(originalPcf);
    })

    it('startAecDumpWithInvalidFdReturnsFalse', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      let result = pcf.startAecDump(-1, 1024);
      expect(result).assertFalse();
    })

    it('stopAecDumpDoesNotCrash', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      pcf.stopAecDump();
    })

    it('stopAecDumpAfterStartDoesNotCrash', 0, () => {
      let pcf = new webrtc.PeerConnectionFactory();
      pcf.startAecDump(-1, 1024);
      pcf.stopAecDump();
    })
  })
}
