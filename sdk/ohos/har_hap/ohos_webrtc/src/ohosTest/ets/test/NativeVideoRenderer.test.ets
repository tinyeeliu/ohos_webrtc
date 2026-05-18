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

const TAG: string = '[NativeVideoRendererTest]';

export default function NativeVideoRendererTest() {
  describe('NativeVideoRendererTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('constructor', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      expect(renderer).not().assertNull();
    })

    it('surfaceIdInitiallyUndefined', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      expect(renderer.surfaceId).assertUndefined();
    })

    it('videoTrackInitiallyUndefined', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      expect(renderer.videoTrack).assertUndefined();
    })

    it('setMirror', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      renderer.setMirror(true);
      renderer.setMirror(false);
    })

    it('setMirrorVertically', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      renderer.setMirrorVertically(true);
      renderer.setMirrorVertically(false);
    })

    it('setScalingMode', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      renderer.setScalingMode(0);
      renderer.setScalingMode(1);
      renderer.setScalingMode(2);
    })

    it('release', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      renderer.release();
    })

    it('releaseCanBeCalledMultipleTimes', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      renderer.release();
      renderer.release();
    })

    it('onFrameResolutionChangedCanBeSet', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      renderer.onFrameResolutionChanged = (event) => {
        console.info(TAG, 'onFrameResolutionChanged: ' + JSON.stringify(event));
      };
      expect(renderer.onFrameResolutionChanged).not().assertNull();
    })

    it('onFrameResolutionChangedCanBeSetToNull', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      renderer.onFrameResolutionChanged = (event) => {};
      renderer.onFrameResolutionChanged = null;
      expect(renderer.onFrameResolutionChanged).assertNull();
    })
  })
}
