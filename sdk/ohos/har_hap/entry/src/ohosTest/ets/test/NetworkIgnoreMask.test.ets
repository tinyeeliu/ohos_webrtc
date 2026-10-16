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

const TAG: string = '[NetworkIgnoreMaskTest]';

// ADAPTER_TYPE: ETHERNET=1<<0, WIFI=1<<1, CELLULAR=1<<2, VPN=1<<3, LOOPBACK=1<<4
const MASK_CELLULAR: number = 1 << 2;
const MASK_VPN_OR_LOOPBACK: number = (1 << 3) | (1 << 4);

export default function NetworkIgnoreMaskTest() {
  describe('NetworkIgnoreMaskTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('factoryCreatedWithCellularMask', 0, () => {
      const factory = new webrtc.PeerConnectionFactory({ networkIgnoreMask: MASK_CELLULAR });
      expect(factory).not().assertNull();
      const pc = factory.createPeerConnection({ iceServers: [] });
      expect(pc.signalingState).assertEqual('stable');
      pc.close();
    })

    it('factoryCreatedWithZeroMask', 0, () => {
      const factory = new webrtc.PeerConnectionFactory({ networkIgnoreMask: 0 });
      expect(factory).not().assertNull();
    })

    it('factoryCreatedWithCombinedMask', 0, () => {
      const factory = new webrtc.PeerConnectionFactory({ networkIgnoreMask: MASK_VPN_OR_LOOPBACK });
      expect(factory).not().assertNull();
    })

    // ===== 异常参数: 非法类型/非法值均不得导致工厂创建失败 =====

    it('factoryWithStringMaskDoesNotCrash', 0, () => {
      const bad = JSON.parse('{"networkIgnoreMask":"4"}') as webrtc.PeerConnectionFactoryOptions;
      const factory = new webrtc.PeerConnectionFactory(bad);
      expect(factory).not().assertNull();
    })

    it('factoryWithNullMaskDoesNotCrash', 0, () => {
      const bad = JSON.parse('{"networkIgnoreMask":null}') as webrtc.PeerConnectionFactoryOptions;
      const factory = new webrtc.PeerConnectionFactory(bad);
      expect(factory).not().assertNull();
    })

    it('factoryWithObjectMaskDoesNotCrash', 0, () => {
      const bad = JSON.parse('{"networkIgnoreMask":{"min":0,"max":4}}') as webrtc.PeerConnectionFactoryOptions;
      const factory = new webrtc.PeerConnectionFactory(bad);
      expect(factory).not().assertNull();
    })

    it('factoryWithNaNMaskDoesNotCrash', 0, () => {
      const factory = new webrtc.PeerConnectionFactory({ networkIgnoreMask: NaN });
      expect(factory).not().assertNull();
    })

    it('factoryWithFloatMaskDoesNotCrash', 0, () => {
      const factory = new webrtc.PeerConnectionFactory({ networkIgnoreMask: 4.5 });
      expect(factory).not().assertNull();
    })

    it('factoryWithNegativeMaskDoesNotCrash', 0, () => {
      const factory = new webrtc.PeerConnectionFactory({ networkIgnoreMask: -1 });
      expect(factory).not().assertNull();
    })

    // 合法掩码不影响本机 SDP 协商(loopback 候选仍可用)
    it('maskDoesNotAffectSignalingExchange', 0, async () => {
      const factory = new webrtc.PeerConnectionFactory({ networkIgnoreMask: MASK_CELLULAR });
      const caller = factory.createPeerConnection({ iceServers: [] });
      const callee = factory.createPeerConnection({ iceServers: [] });
      const audioSource = factory.createAudioSource();
      const audioTrack = factory.createAudioTrack('audio', audioSource);
      caller.addTrack(audioTrack);

      await caller.setLocalDescription(await caller.createOffer());
      await callee.setRemoteDescription(caller.localDescription);
      const answer = await callee.createAnswer();
      await caller.setRemoteDescription(answer);
      await callee.setLocalDescription(answer);
      expect(caller.signalingState).assertEqual('stable');

      caller.close();
      callee.close();
      audioSource.release();
    })
  })
}
