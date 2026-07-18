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
import { VideoRenderController, ScalingMode } from '../../../main/ets/xcomponent/VideoRenderController';

const TAG: string = '[VideoRenderControllerTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function VideoRenderControllerTest() {
  describe('VideoRenderControllerTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('setVideoTrackWithAudioTrackHandled', 0, () => {
      let controller = new VideoRenderController();
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      try {
        controller.setVideoTrack(audioTrack);
      } catch (e) {
        // NAPI may throw for non-video tracks
      }
      audioSource.release();
    })

    it('setVideoTrackWithNullDoesNotCrash', 0, () => {
      let controller = new VideoRenderController();
      controller.setVideoTrack(null);
    })

    it('setVideoTrackWithVideoTrackDoesNotCrash', 0, () => {
      let controller = new VideoRenderController();
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      controller.setVideoTrack(videoTrack);
      videoSource.release();
    })

    it('setMirrorDoesNotCrash', 0, () => {
      let controller = new VideoRenderController();
      controller.setMirror(true);
      controller.setMirror(false);
    })

    it('setMirrorVerticallyDoesNotCrash', 0, () => {
      let controller = new VideoRenderController();
      controller.setMirrorVertically(true);
      controller.setMirrorVertically(false);
    })

    it('setScalingModeAllValuesDoesNotCrash', 0, () => {
      let controller = new VideoRenderController();
      controller.setScalingMode(ScalingMode.Fill);
      controller.setScalingMode(ScalingMode.AspectFill);
      controller.setScalingMode(ScalingMode.AspectFit);
    })

    it('onSurfaceCreatedWithEmptyStringThrows', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.onSurfaceCreated('');
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('onSurfaceCreatedWithNonNumericStringThrows', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.onSurfaceCreated('not-a-number');
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('onSurfaceChangedDoesNotCrash', 0, () => {
      let controller = new VideoRenderController();
      let rect: SurfaceRect = { offsetX: 0, offsetY: 0, surfaceWidth: 640, surfaceHeight: 480 };
      controller.onSurfaceChanged('0', rect);
    })

    it('onSurfaceDestroyedDoesNotCrash', 0, () => {
      let controller = new VideoRenderController();
      controller.onSurfaceDestroyed('0');
    })

    it('onSurfaceDestroyedAfterReleaseDoesNotCrash', 0, () => {
      let controller = new VideoRenderController();
      controller.onSurfaceDestroyed('0');
      controller.onSurfaceDestroyed('0');
    })
  })
}
