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

const TAG: string = '[PeerConnectionFactoryTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function PeerConnectionFactoryTest() {
  describe('PeerConnectionFactoryTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('createPeerConnectionReturnsValidPc', 0, () => {
      let pc = pcf.createPeerConnection({
        iceServers: []
      });
      expect(pc.signalingState).assertEqual('stable');
      expect(pc.iceConnectionState).assertEqual('new');
      pc.close();
    })

    it('createAudioTrackProperties', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack.kind).assertEqual('audio');
      expect(audioTrack.readyState).assertEqual('live');
      expect(audioTrack.enabled).assertTrue();
      audioSource.release();
    })

    it('createVideoSourceWithConstraintsStateIsLive', 0, async () => {
      let constraints: webrtc.MediaTrackConstraints = {
        width: 640,
        height: 480,
        frameRate: 30,
      };
      let videoSource = pcf.createVideoSource(constraints);
      expect(videoSource).not().assertNull();

      let videoTrack = pcf.createVideoTrack('video', videoSource);
      let pc = pcf.createPeerConnection({});
      pc.addTrack(videoTrack);
      let offer = await pc.createOffer();
      expect(offer.type).assertEqual('offer');
      expect(offer.sdp).assertContain('m=video');
      pc.close();
      try {
        videoSource.release();
      } catch (e) {
      }
    })

    it('startAecDumpWithInvalidFdReturnsFalse', 0, () => {
      let result = pcf.startAecDump(-1, 1024);
      expect(result).assertFalse();
    })

    it('stopAecDumpDoesNotCrash', 0, () => {
      pcf.stopAecDump();
    })

    it('stopAecDumpAfterStartDoesNotCrash', 0, () => {
      pcf.startAecDump(0, 1024);
      pcf.stopAecDump();
    })

    it('setDefaultDoesNotCrash', 0, () => {
      let factory = new webrtc.PeerConnectionFactory();
      webrtc.PeerConnectionFactory.setDefault(factory);
    })

    it('createFactoryWithVpnNetworkIgnoreMaskCreatesUsablePc', 0, () => {
      // ADAPTER_TYPE_VPN = 1 << 3
      const factory = new webrtc.PeerConnectionFactory({ networkIgnoreMask: 1 << 3 });
      const pc = factory.createPeerConnection({ iceServers: [] });
      expect(pc.signalingState).assertEqual('stable');
      pc.close();
    })

    it('createFactoryWithCombinedNetworkIgnoreMaskCreatesUsablePc', 0, () => {
      // WIFI | CELLULAR | VPN = 0x0E
      const factory = new webrtc.PeerConnectionFactory({ networkIgnoreMask: 0x0E });
      const pc = factory.createPeerConnection({ iceServers: [] });
      expect(pc.iceConnectionState).assertEqual('new');
      pc.close();
    })

    it('createFactoryWithZeroNetworkIgnoreMaskKeepsDefaultBehavior', 0, () => {
      const factory = new webrtc.PeerConnectionFactory({ networkIgnoreMask: 0 });
      const pc = factory.createPeerConnection({ iceServers: [] });
      expect(pc.signalingState).assertEqual('stable');
      pc.close();
    })

    it('createFactoryWithNegativeNetworkIgnoreMaskDoesNotThrow', 0, () => {
      // -1 sets all adapter bits; construction itself must stay valid (cross-platform).
      const factory = new webrtc.PeerConnectionFactory({ networkIgnoreMask: -1 });
      expect(factory).not().assertNull();
    })

    it('createFactoryWithStringNetworkIgnoreMaskThrows', 0, () => {
      let thrown = false;
      try {
        const options: ESObject = {};
        options.networkIgnoreMask = 'vpn';
        new webrtc.PeerConnectionFactory(options);
      } catch (e) {
        thrown = true;
      }
      expect(thrown).assertTrue();
    })

    it('createFactoryWithBooleanNetworkIgnoreMaskThrows', 0, () => {
      let thrown = false;
      try {
        const options: ESObject = {};
        options.networkIgnoreMask = true;
        new webrtc.PeerConnectionFactory(options);
      } catch (e) {
        thrown = true;
      }
      expect(thrown).assertTrue();
    })

    it('createFactoryWithObjectNetworkIgnoreMaskThrows', 0, () => {
      let thrown = false;
      try {
        const options: ESObject = {};
        options.networkIgnoreMask = {};
        new webrtc.PeerConnectionFactory(options);
      } catch (e) {
        thrown = true;
      }
      expect(thrown).assertTrue();
    })

    it('createFactoryWithNullNetworkIgnoreMaskThrows', 0, () => {
      let thrown = false;
      try {
        const options: ESObject = {};
        options.networkIgnoreMask = null;
        new webrtc.PeerConnectionFactory(options);
      } catch (e) {
        thrown = true;
      }
      expect(thrown).assertTrue();
    })

    // audio-only 双端完整 SDP 协商（x86 无摄像头降级场景核心路径）
    it('audioOnly_SDPNegotiation_shouldSucceed', 0, async () => {
      const factory = new webrtc.PeerConnectionFactory();
      const caller = factory.createPeerConnection({ iceServers: [] });
      const callee = factory.createPeerConnection({ iceServers: [] });
      const audioSource = factory.createAudioSource();
      const audioTrack = factory.createAudioTrack('audio', audioSource);
      caller.addTrack(audioTrack);

      const offer = await caller.createOffer();
      await caller.setLocalDescription(offer);
      expect(caller.signalingState).assertEqual('have-local-offer');

      await callee.setRemoteDescription(caller.localDescription!);
      expect(callee.signalingState).assertEqual('have-remote-offer');

      const answer = await callee.createAnswer();
      await callee.setLocalDescription(answer);
      expect(callee.signalingState).assertEqual('stable');

      await caller.setRemoteDescription(callee.localDescription!);
      expect(caller.signalingState).assertEqual('stable');
      expect(caller.getSenders().length).assertLarger(0);
      expect(caller.getSenders()[0].track).not().assertNull();

      audioSource.release();
      caller.close();
      callee.close();
    })

    // VideoSource 创建/释放后工厂可正常回收，验证无泄漏崩溃
    it('dispose_afterCreateVideoSource_shouldNotCrash', 0, () => {
      const factory = new webrtc.PeerConnectionFactory();
      let videoSource: webrtc.VideoSource | undefined;
      try {
        videoSource = factory.createVideoSource({ width: 320, height: 240 }, false);
        if (videoSource) {
          videoSource.release();
        }
        expect(true).assertTrue();
      } catch (e) {
        console.error(TAG, 'dispose after createVideoSource error: ' + JSON.stringify(e));
        expect().assertFail();
      }
    })
  })
}
