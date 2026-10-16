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
import { pollUntilOrFail } from './Helper';

const TAG: string = '[AudioProcessingFactoryTest]';

let captureGotFrame = false;

export default function AudioProcessingFactoryTest() {
  describe('AudioProcessingFactoryTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('createWithNoOptionsReturnsAudioProcessing', 0, () => {
      const factory = new webrtc.AudioProcessingFactory();
      const ap = factory.create();
      expect(ap).not().assertNull();
    })

    it('createWithEmptyOptionsReturnsAudioProcessing', 0, () => {
      const factory = new webrtc.AudioProcessingFactory();
      const ap = factory.create({});
      expect(ap).not().assertNull();
    })

    it('createWithCallbacksReturnsAudioProcessing', 0, () => {
      const factory = new webrtc.AudioProcessingFactory();
      const ap = factory.create({
        capturePostProcessing: (frame: webrtc.AudioProcessingFrame) => {
          // 就地改写 frame.data 即变声/降噪
        },
        renderPreProcessing: (frame: webrtc.AudioProcessingFrame) => {
        }
      });
      expect(ap).not().assertNull();
    })

    it('delegatedApInjectableIntoPeerConnectionFactory', 0, () => {
      const factory = new webrtc.AudioProcessingFactory();
      const ap = factory.create({
        capturePostProcessing: (frame: webrtc.AudioProcessingFrame) => {
        }
      });
      const pcf = new webrtc.PeerConnectionFactory({ audioProcessing: ap });
      const pc = pcf.createPeerConnection({ iceServers: [] });
      expect(pc.signalingState).assertEqual('stable');
      pc.close();
    })

    it('captureCallbackReceivesValidFrames', 0, async () => {
      // 需要真实麦克风采集才有回调
      captureGotFrame = false;
      const apFactory = new webrtc.AudioProcessingFactory();
      const ap = apFactory.create({
        capturePostProcessing: (frame: webrtc.AudioProcessingFrame) => {
          captureGotFrame = true;
          expect(frame.sampleRate).assertLarger(0);
          expect(frame.channels).assertLarger(0);
          expect(frame.data.length).assertLarger(0);
        }
      });
      const pcf = new webrtc.PeerConnectionFactory({ audioProcessing: ap });
      const audioSource = pcf.createAudioSource();
      const audioTrack = pcf.createAudioTrack('audio', audioSource);
      const pc = pcf.createPeerConnection({ iceServers: [] });
      pc.addTrack(audioTrack);
      const offer = await pc.createOffer();
      await pc.setLocalDescription(offer);

      await pollUntilOrFail(() => captureGotFrame, 'capture post processing callback not fired');
      pc.close();
      audioSource.release();
    })

    // ===== 异常参数: 非法 options 类型与非法插槽类型应抛错(fail-fast) =====

    it('createWithStringOptionsThrows', 0, () => {
      const factory = new webrtc.AudioProcessingFactory();
      const bad = JSON.parse('"oops"') as webrtc.AudioProcessingOptions;
      try {
        factory.create(bad);
        console.error(TAG, 'create("oops") should throw');
        expect(false).assertTrue();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('createWithNonFunctionSlotsThrows', 0, () => {
      const factory = new webrtc.AudioProcessingFactory();
      const bad = JSON.parse('{"capturePostProcessing":"x"}') as webrtc.AudioProcessingOptions;
      try {
        factory.create(bad);
        console.error(TAG, 'create with string slot should throw');
        expect(false).assertTrue();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('createWithNonFunctionRenderSlotThrows', 0, () => {
      const factory = new webrtc.AudioProcessingFactory();
      const bad = JSON.parse('{"renderPreProcessing":123}') as webrtc.AudioProcessingOptions;
      try {
        factory.create(bad);
        console.error(TAG, 'create with number slot should throw');
        expect(false).assertTrue();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('createWithNullOptionsReturnsBuiltin', 0, () => {
      const factory = new webrtc.AudioProcessingFactory();
      const bad = JSON.parse('null') as webrtc.AudioProcessingOptions;
      const ap = factory.create(bad);
      expect(ap).not().assertNull();
    })

    it('throwingCallbackDoesNotBreakFactoryReuse', 0, () => {
      const factory = new webrtc.AudioProcessingFactory();
      const ap = factory.create({
        capturePostProcessing: (frame: webrtc.AudioProcessingFrame) => {
          throw new Error('callback error on purpose');
        }
      });
      expect(ap).not().assertNull();
      const again = factory.create();
      expect(again).not().assertNull();
    })
  })
}
