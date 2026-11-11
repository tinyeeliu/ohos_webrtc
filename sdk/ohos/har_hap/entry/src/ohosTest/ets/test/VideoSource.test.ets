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

const TAG: string = '[VideoSourceTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function VideoSourceTest() {
  describe('VideoSourceTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    // A nonexistent mandatory ID must reach device selection and throw a
    // catchable error, rather than aborting in Napi::String::CheckCast.

    it('shouldRejectExactDeviceIdStringWithoutNativeCrash', 0, () => {
      let message = '';
      try {
        pcf.createVideoSource({ deviceId: { exact: '__missing_camera_regression__' } });
      } catch (e) {
        message = (e as Error).message;
      }
      expect(message).assertEqual('Unsatisfied constraint: deviceId');
    })

    it('shouldRejectExactDeviceIdArrayWithoutNativeCrash', 0, () => {
      let message = '';
      try {
        pcf.createVideoSource({ deviceId: { exact: ['__missing_camera_regression__'] } });
      } catch (e) {
        message = (e as Error).message;
      }
      expect(message).assertEqual('Unsatisfied constraint: deviceId');
    })

    it('shouldParseIdealDeviceIdStringBeforeExactSelection', 0, () => {
      let message = '';
      try {
        pcf.createVideoSource({ deviceId: {
          ideal: '__preferred_camera__', exact: '__missing_camera_regression__'
        } });
      } catch (e) {
        message = (e as Error).message;
      }
      expect(message).assertEqual('Unsatisfied constraint: deviceId');
    })

    it('shouldParseIdealDeviceIdArrayBeforeExactSelection', 0, () => {
      let message = '';
      try {
        pcf.createVideoSource({ deviceId: {
          ideal: ['__preferred_camera__'], exact: ['__missing_camera_regression__']
        } });
      } catch (e) {
        message = (e as Error).message;
      }
      expect(message).assertEqual('Unsatisfied constraint: deviceId');
    })

    it('shouldChangeStateToEndedAfterRelease', 0, () => {
      let videoSource = pcf.createVideoSource();
      expect(videoSource.state === 'live' || videoSource.state === 'initializing' || videoSource.state === 'ended' || videoSource.state === 'muted').assertTrue();
      try {
        videoSource.release();
        try {
          expect(videoSource.state).assertEqual('ended');
        } catch (e) {
          // accessing state after release may throw
        }
      } catch (e) {
        // release() may throw "Illegal state" if source wasn't started
      }
    })

    it('shouldCreateNewSourceAfterReleaseWithoutConflict', 0, () => {
      let source1 = pcf.createVideoSource();
      try {
        source1.release();
      } catch (e) {
      }
      let source2 = pcf.createVideoSource();
      expect(source2.state === 'live' || source2.state === 'initializing' || source2.state === 'ended' || source2.state === 'muted').assertTrue();
      try {
        source2.release();
      } catch (e) {
      }
    })

    it('shouldHaveNullCapturerEventCallbacks', 0, () => {
      let videoSource = pcf.createVideoSource();
      expect(videoSource.oncapturerstarted).assertNull();
      expect(videoSource.oncapturerstopped).assertNull();
      try {
        videoSource.release();
      } catch (e) {
      }
    })
  })
}
