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

/** 测试类型： unit（单元测试） */

import { describe, beforeAll, it, expect } from '@ohos/hypium'
import webrtc, { Logging, LoggingSeverity } from '@ohos/webrtc';

const TAG: string = '[NativeVideoRendererTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function NativeVideoRendererTest() {
  describe('NativeVideoRendererTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('shouldHandleAudioTrackWhenSetVideoTrack', 0, () => {
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

    it('shouldThrowWhenInitWithEmptyString', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      try {
        renderer.init('');
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('shouldThrowWhenInitWithNonNumericString', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      try {
        renderer.init('not-a-number');
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('shouldThrowWhenInitWithPartialNumericString', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      try {
        renderer.init('123abc');
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('shouldNotCrashWhenSetVideoTrackAfterRelease', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      try {
        renderer.release();
        renderer.setVideoTrack(videoTrack);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
      videoSource.release();
    })

    it('shouldNotCrashWhenSetMirror', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      try {
        renderer.setMirror(true);
        renderer.setMirror(false);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldNotCrashWhenSetMirrorVertically', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      try {
        renderer.setMirrorVertically(true);
        renderer.setMirrorVertically(false);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldNotCrashWhenSetScalingModeAllValues', 0, () => {
      let renderer = new webrtc.NativeVideoRenderer();
      try {
        renderer.setScalingMode(0);
        renderer.setScalingMode(1);
        renderer.setScalingMode(2);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })
  })
}
