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

const TAG: string = '[CameraTorchFocusTest]';

const pcf = new webrtc.PeerConnectionFactory();

// ohos.multimedia.camera 枚举: FocusMode MANUAL=0/CONTINUOUS_AUTO=1/AUTO=2/LOCKED=3;
// ExposureMode LOCKED=0/AUTO=1/CONTINUOUS_AUTO=2
const FOCUS_MODE_AUTO: number = 2;
const EXPOSURE_MODE_AUTO: number = 2;
const FOCUS_POINT_CENTER: number = 0.5;
const INVALID_MODE_VALUES: number[] = [-1, 999, NaN];
const INVALID_FOCUS_POINTS: number[][] = [[-0.5, 0.5], [0.5, 1.5], [NaN, 0.5], [0.5, NaN]];

export default function CameraTorchFocusTest() {
  describe('CameraTorchFocusTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    // ========== torch ==========

    it('hasTorchReturnsBoolean', 0, () => {
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('video', videoSource);

      const supported = videoSource.hasTorch();
      console.info(TAG, 'hasTorch: ' + supported);
      expect(typeof supported).assertEqual('boolean');

      videoTrack.stop();
      videoSource.release();
    })

    it('setTorchRoundTripDoesNotCrash', 0, () => {
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('video', videoSource);

      if (!videoSource.hasTorch()) {
        console.info(TAG, 'torch not supported, skip round trip');
      } else {
        videoSource.setTorch(true);
        videoSource.setTorch(false);
      }
      expect(true).assertTrue();

      videoTrack.stop();
      videoSource.release();
    })

    it('setTorchWithoutTorchThrows', 0, () => {
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('video', videoSource);

      if (videoSource.hasTorch()) {
        console.info(TAG, 'torch supported, skip');
        videoTrack.stop();
        videoSource.release();
        return;
      }
      try {
        videoSource.setTorch(true);
        console.error(TAG, 'setTorch without torch should throw');
        expect(false).assertTrue();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('torchNotAvailableOnScreencastSource', 0, () => {
      const screencastSource = pcf.createVideoSource({}, true);
      expect(screencastSource.hasTorch()).assertFalse();
      try {
        screencastSource.release();
      } catch (e) {
      }
    })

    // ========== 对焦/测光 ==========

    it('setFocusModeAutoDoesNotCrash', 0, () => {
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('video', videoSource);

      videoSource.setFocusMode(FOCUS_MODE_AUTO);
      expect(true).assertTrue();

      videoTrack.stop();
      videoSource.release();
    })

    it('setFocusPointCenterDoesNotCrash', 0, () => {
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('video', videoSource);

      videoSource.setFocusPoint(FOCUS_POINT_CENTER, FOCUS_POINT_CENTER);
      expect(true).assertTrue();

      videoTrack.stop();
      videoSource.release();
    })

    it('setExposureModeAutoDoesNotCrash', 0, () => {
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('video', videoSource);

      videoSource.setExposureMode(EXPOSURE_MODE_AUTO);
      expect(true).assertTrue();

      videoTrack.stop();
      videoSource.release();
    })

    // ===== 异常参数: 非法枚举值/越界坐标应抛错, 且不破坏后续有效调用 =====

    it('setFocusModeInvalidValueThrows', 0, () => {
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('video', videoSource);

      const invalidValues: number[] = INVALID_MODE_VALUES;
      for (const mode of invalidValues) {
        try {
          videoSource.setFocusMode(mode);
          console.error(TAG, 'setFocusMode(' + mode + ') should throw');
          expect(false).assertTrue();
        } catch (e) {
          expect(e).assertInstanceOf('Error');
        }
      }

      videoSource.setFocusMode(FOCUS_MODE_AUTO);
      videoTrack.stop();
      videoSource.release();
    })

    it('setFocusPointOutOfRangeThrows', 0, () => {
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('video', videoSource);

      const invalidPoints: number[][] = INVALID_FOCUS_POINTS;
      for (const point of invalidPoints) {
        try {
          videoSource.setFocusPoint(point[0], point[1]);
          console.error(TAG, 'setFocusPoint(' + point + ') should throw');
          expect(false).assertTrue();
        } catch (e) {
          expect(e).assertInstanceOf('Error');
        }
      }

      videoSource.setFocusPoint(FOCUS_POINT_CENTER, FOCUS_POINT_CENTER);
      videoTrack.stop();
      videoSource.release();
    })

    it('setExposureModeInvalidValueThrows', 0, () => {
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('video', videoSource);

      const invalidValues: number[] = INVALID_MODE_VALUES;
      for (const mode of invalidValues) {
        try {
          videoSource.setExposureMode(mode);
          console.error(TAG, 'setExposureMode(' + mode + ') should throw');
          expect(false).assertTrue();
        } catch (e) {
          expect(e).assertInstanceOf('Error');
        }
      }

      videoSource.setExposureMode(EXPOSURE_MODE_AUTO);
      videoTrack.stop();
      videoSource.release();
    })

    it('focusControlsOnScreencastSourceThrow', 0, () => {
      const screencastSource = pcf.createVideoSource({}, true);
      try {
        screencastSource.setFocusMode(FOCUS_MODE_AUTO);
        expect(false).assertTrue();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
      try {
        screencastSource.setFocusPoint(FOCUS_POINT_CENTER, FOCUS_POINT_CENTER);
        expect(false).assertTrue();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
      try {
        screencastSource.setExposureMode(EXPOSURE_MODE_AUTO);
        expect(false).assertTrue();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
      try {
        screencastSource.release();
      } catch (e) {
      }
    })
  })
}
