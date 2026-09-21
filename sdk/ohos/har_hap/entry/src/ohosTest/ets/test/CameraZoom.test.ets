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

const TAG: string = '[CameraZoomTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function CameraZoomTest() {
  describe('CameraZoomTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    // ========== getZoomRange ==========

    it('getZoomRangeOnVideoTrackReturnsValidStructure', 0, () => {
      let videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();

      let range = videoTrack!.getZoomRange();
      console.info(TAG, 'getZoomRange: ' + JSON.stringify(range));
      expect(range).not().assertNull();
      expect(typeof range.supported).assertEqual('boolean');
      expect(typeof range.min).assertEqual('number');
      expect(typeof range.max).assertEqual('number');
      if (range.supported) {
        expect(range.min > 0).assertTrue();
        expect(range.max >= range.min).assertTrue();
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('getZoomRangeOnAudioTrackReturnsNotSupported', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);

      let range = audioTrack.getZoomRange();
      console.info(TAG, 'getZoomRange on audio: ' + JSON.stringify(range));
      expect(range.supported).assertFalse();

      audioSource.release();
    })

    // ========== applyConstraints with zoom ==========

    it('applyConstraintsZoomResolvesOnVideoTrack', 0, async () => {
      let videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      let videoTrack = pcf.createVideoTrack('video', videoSource);

      try {
        await videoTrack.applyConstraints({ zoom: 2.0 });
        console.info(TAG, 'applyConstraints({ zoom: 2.0 }) resolved');
        expect(true).assertTrue();
      } catch (e) {
        console.error(TAG, 'applyConstraints({ zoom: 2.0 }) rejected: ' + e);
        expect(false).assertTrue();
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsZoomOneResolvesOnVideoTrack', 0, async () => {
      let videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      let videoTrack = pcf.createVideoTrack('video', videoSource);

      try {
        await videoTrack.applyConstraints({ zoom: 1.0 });
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsZoomOutOfRangeResolvesWithClamp', 0, async () => {
      let videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      let videoTrack = pcf.createVideoTrack('video', videoSource);

      let range = videoTrack.getZoomRange();
      if (!range.supported) {
        console.info(TAG, 'zoom not supported, skip out-of-range test');
        videoTrack.stop();
        videoSource.release();
        return;
      }

      try {
        await videoTrack.applyConstraints({ zoom: 9999.0 });
        console.info(TAG, 'applyConstraints({ zoom: 9999 }) resolved (clamped to ' + range.max + ')');
        expect(true).assertTrue();
      } catch (e) {
        console.error(TAG, 'applyConstraints({ zoom: 9999 }) rejected: ' + e);
        expect(false).assertTrue();
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsZoomZeroRejectsOnVideoTrack', 0, async () => {
      let videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      let videoTrack = pcf.createVideoTrack('video', videoSource);

      try {
        await videoTrack.applyConstraints({ zoom: 0 });
        console.error(TAG, 'applyConstraints({ zoom: 0 }) should reject but resolved');
        expect(false).assertTrue();
      } catch (e) {
        console.info(TAG, 'applyConstraints({ zoom: 0 }) rejected as expected: ' + e);
        expect(true).assertTrue();
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsZoomNegativeRejectsOnVideoTrack', 0, async () => {
      let videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      let videoTrack = pcf.createVideoTrack('video', videoSource);

      try {
        await videoTrack.applyConstraints({ zoom: -1.0 });
        console.error(TAG, 'applyConstraints({ zoom: -1.0 }) should reject but resolved');
        expect(false).assertTrue();
      } catch (e) {
        console.info(TAG, 'applyConstraints({ zoom: -1.0 }) rejected as expected: ' + e);
        expect(true).assertTrue();
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsWithZoomAndFrameRateResolves', 0, async () => {
      let videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      let videoTrack = pcf.createVideoTrack('video', videoSource);

      try {
        await videoTrack.applyConstraints({ zoom: 1.5, frameRate: 30 });
        console.info(TAG, 'applyConstraints({ zoom: 1.5, frameRate: 30 }) resolved');
        expect(true).assertTrue();
      } catch (e) {
        console.error(TAG, 'applyConstraints({ zoom: 1.5, frameRate: 30 }) rejected: ' + e);
        expect(false).assertTrue();
      }

      videoTrack.stop();
      videoSource.release();
    })

    // ========== #4: VideoSource.setZoom ==========

    it('setZoomWithValidRatioOnVideoSource', 0, () => {
      let videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      try {
        videoSource.setZoom(2.0);
        expect(true).assertTrue();
      } catch (e) {
        console.error(TAG, 'setZoom(2.0) threw: ' + e);
        expect(false).assertTrue();
      }
      videoSource.release();
    })

    it('setZoomWithStringRatioThrowsTypeError', 0, () => {
      let videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      let thrown = false;
      try {
        const ratio: ESObject = '2.0';
        videoSource.setZoom(ratio);
      } catch (e) {
        thrown = true;
      }
      expect(thrown).assertTrue();
      videoSource.release();
    })

    it('setZoomWithZeroRatioThrowsRangeError', 0, () => {
      let videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      let thrown = false;
      try {
        videoSource.setZoom(0);
      } catch (e) {
        thrown = true;
      }
      expect(thrown).assertTrue();
      videoSource.release();
    })

    it('setZoomWithNegativeRatioThrowsRangeError', 0, () => {
      let videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      let thrown = false;
      try {
        videoSource.setZoom(-1.5);
      } catch (e) {
        thrown = true;
      }
      expect(thrown).assertTrue();
      videoSource.release();
    })
  })
}
