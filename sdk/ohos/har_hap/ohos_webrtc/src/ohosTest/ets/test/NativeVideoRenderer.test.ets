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

const pcf = new webrtc.PeerConnectionFactory();

export default function NativeVideoRendererTest() {
  describe('NativeVideoRendererTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('setVideoTrackWithAudioTrackHandled', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      try {
        renderer.setVideoTrack(audioTrack);
      } catch (e) {
        // NAPI may throw for non-video tracks
      }
      audioSource.release();
    })

    it('initWithEmptyStringThrows', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      try {
        renderer.init('');
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('initWithNonNumericStringThrows', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      try {
        renderer.init('not-a-number');
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('initWithPartialNumericStringThrows', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      try {
        renderer.init('123abc');
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('setVideoTrackAfterReleaseDoesNotCrash', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      renderer.release();
      renderer.setVideoTrack(videoTrack);
      videoSource.release();
    })

    it('setMirrorDoesNotCrash', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      renderer.setMirror(true);
      renderer.setMirror(false);
    })

    it('setMirrorVerticallyDoesNotCrash', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      renderer.setMirrorVertically(true);
      renderer.setMirrorVertically(false);
    })

    it('setScalingModeAllValuesDoesNotCrash', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      renderer.setScalingMode(0);
      renderer.setScalingMode(1);
      renderer.setScalingMode(2);
    })
  })
}
