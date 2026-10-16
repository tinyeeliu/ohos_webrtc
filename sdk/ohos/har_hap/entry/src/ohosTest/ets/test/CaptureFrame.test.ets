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

const TAG: string = '[CaptureFrameTest]';

// I420: Y 全平面 + 两个 1/4 尺寸色度平面 = 每像素 1.5 字节
const I420_BYTES_PER_PIXEL: number = 3 / 2;

const pcf = new webrtc.PeerConnectionFactory();

export default function CaptureFrameTest() {
  describe('CaptureFrameTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('captureFrameOnVideoTrackResolvesWithI420', 0, async () => {
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('video', videoSource);

      const frame = await videoTrack.captureFrame();
      console.info(TAG, 'captureFrame: ' + JSON.stringify({
        width: frame.width,
        height: frame.height,
        rotation: frame.rotation,
        bytes: frame.data.byteLength
      }));
      expect(frame.width).assertLarger(0);
      expect(frame.height).assertLarger(0);
      // I420: Y 全平面 + 两个 1/4 尺寸色度平面
      expect(frame.data.byteLength).assertEqual(frame.width * frame.height * I420_BYTES_PER_PIXEL);

      videoTrack.stop();
      videoSource.release();
    })

    it('captureFrameRotationIsQuarterTurn', 0, async () => {
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('video', videoSource);

      const frame = await videoTrack.captureFrame();
      const legal = frame.rotation === 0 || frame.rotation === 90 || frame.rotation === 180 || frame.rotation === 270;
      expect(legal).assertTrue();

      videoTrack.stop();
      videoSource.release();
    })

    // ===== 异常参数: 非视频轨道 =====

    it('captureFrameOnAudioTrackRejects', 0, async () => {
      const audioSource = pcf.createAudioSource();
      const audioTrack = pcf.createAudioTrack('audio', audioSource);

      try {
        await audioTrack.captureFrame();
        console.error(TAG, 'captureFrame on audio track should reject but resolved');
        expect(false).assertTrue();
      } catch (e) {
        console.info(TAG, 'captureFrame on audio track rejected as expected: ' + e);
        expect(e).assertInstanceOf('Error');
      }

      audioSource.release();
    })

    it('captureFrameOnStoppedTrackRejects', 0, async () => {
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('video', videoSource);
      videoTrack.stop();

      try {
        await videoTrack.captureFrame();
        console.error(TAG, 'captureFrame on stopped track should reject but resolved');
        expect(false).assertTrue();
      } catch (e) {
        console.info(TAG, 'captureFrame on stopped track rejected as expected: ' + e);
        expect(e).assertInstanceOf('Error');
      }

      videoSource.release();
    })

    it('captureFrameOnAudioTrackDoesNotAffectVideoCapture', 0, async () => {
      const audioSource = pcf.createAudioSource();
      const audioTrack = pcf.createAudioTrack('audio', audioSource);
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('video', videoSource);

      try {
        await audioTrack.captureFrame();
      } catch (e) {
        // 预期拒绝
      }

      const frame = await videoTrack.captureFrame();
      expect(frame.width).assertLarger(0);

      videoTrack.stop();
      videoSource.release();
      audioSource.release();
    })
  })
}
