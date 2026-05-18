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

const TAG: string = '[MediaStreamTrackTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function MediaStreamTrackTest() {
  describe('MediaStreamTrackTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('audioTrackKind', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack.kind).assertEqual('audio');
    })

    it('videoTrackKind', 0, () => {
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack.kind).assertEqual('video');
    })

    it('audioTrackIdIsString', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(typeof audioTrack.id).assertEqual('string');
      expect(audioTrack.id.length > 0).assertTrue();
    })

    it('videoTrackIdIsString', 0, () => {
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(typeof videoTrack.id).assertEqual('string');
      expect(videoTrack.id.length > 0).assertTrue();
    })

    it('differentTracksHaveDifferentIds', 0, () => {
      let audioSource = pcf.createAudioSource();
      let track1 = pcf.createAudioTrack('audio1', audioSource);
      let track2 = pcf.createAudioTrack('audio2', audioSource);
      expect(track1.id).not().assertEqual(track2.id);
    })

    it('audioTrackEnabledInitiallyTrue', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack.enabled).assertTrue();
    })

    it('videoTrackEnabledInitiallyTrue', 0, () => {
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack.enabled).assertTrue();
    })

    it('setEnabledToFalse', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      audioTrack.enabled = false;
      expect(audioTrack.enabled).assertFalse();
    })

    it('setEnabledToTrueAfterFalse', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      audioTrack.enabled = false;
      expect(audioTrack.enabled).assertFalse();
      audioTrack.enabled = true;
      expect(audioTrack.enabled).assertTrue();
    })

    it('audioTrackReadyStateIsLive', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack.readyState).assertEqual('live');
    })

    it('videoTrackReadyStateIsLive', 0, () => {
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack.readyState).assertEqual('live');
    })

    it('stopChangesReadyStateToEnded', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack.readyState).assertEqual('live');
      audioTrack.stop();
      expect(audioTrack.readyState).assertEqual('ended');
    })

    it('stopVideoTrackChangesReadyStateToEnded', 0, () => {
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack.readyState).assertEqual('live');
      videoTrack.stop();
      expect(videoTrack.readyState).assertEqual('ended');
    })

    it('stopCanBeCalledMultipleTimes', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      audioTrack.stop();
      audioTrack.stop();
      expect(audioTrack.readyState).assertEqual('ended');
    })

    it('getSourceReturnsAudioSourceForAudioTrack', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let source = audioTrack.getSource();
      expect(source).not().assertNull();
      expect(source!.state).assertEqual('live');
    })

    it('getSourceReturnsVideoSourceForVideoTrack', 0, () => {
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      let source = videoTrack.getSource();
      expect(source).not().assertNull();
    })

    it('enabledStillSettableAfterStop', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      audioTrack.stop();
      audioTrack.enabled = false;
      expect(audioTrack.enabled).assertFalse();
    })

    it('audioTrackIsInstanceOfMediaStreamTrack', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack instanceof webrtc.MediaStreamTrack).assertTrue();
    })

    it('videoTrackIsInstanceOfMediaStreamTrack', 0, () => {
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack instanceof webrtc.MediaStreamTrack).assertTrue();
    })
  })
}
