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

import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium'
import webrtc from 'libohos_webrtc.so';
import { Logging, LoggingSeverity } from '../../../main/ets/log/Logging';

const TAG: string = '[MediaStreamTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function MediaStreamTest() {
  describe('MediaStreamTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('constructorWithNoArguments', 0, () => {
      let stream = new webrtc.MediaStream();
      expect(stream).not().assertNull();
      expect(stream.id).not().assertUndefined();
      expect(stream.active).assertFalse();
    })

    it('constructorWithStream', 0, () => {
      let stream1 = new webrtc.MediaStream();
      let stream2 = new webrtc.MediaStream(stream1);
      expect(stream2).not().assertNull();
      expect(stream2.id).not().assertEqual(stream1.id);
    })

    it('constructorWithTrackArray', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let stream = new webrtc.MediaStream([audioTrack]);
      expect(stream).not().assertNull();
      expect(stream.getAudioTracks().length).assertEqual(1);
    })

    it('idProperty', 0, () => {
      let stream = new webrtc.MediaStream();
      expect(typeof stream.id).assertEqual('string');
      expect(stream.id.length > 0).assertTrue();
    })

    it('activePropertyWithNoTracks', 0, () => {
      let stream = new webrtc.MediaStream();
      expect(stream.active).assertFalse();
    })

    it('activePropertyWithTracks', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let stream = new webrtc.MediaStream([audioTrack]);
      expect(stream.active).assertTrue();
    })

    it('addTrack', 0, () => {
      let stream = new webrtc.MediaStream();
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      stream.addTrack(audioTrack);
      expect(stream.getAudioTracks().length).assertEqual(1);
    })

    it('addTrackAndActive', 0, () => {
      let stream = new webrtc.MediaStream();
      expect(stream.active).assertFalse();
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      stream.addTrack(audioTrack);
      expect(stream.active).assertTrue();
    })

    it('removeTrack', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let stream = new webrtc.MediaStream([audioTrack]);
      expect(stream.getAudioTracks().length).assertEqual(1);
      stream.removeTrack(audioTrack);
      expect(stream.getAudioTracks().length).assertEqual(0);
    })

    it('removeTrackAndActive', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let stream = new webrtc.MediaStream([audioTrack]);
      expect(stream.active).assertTrue();
      stream.removeTrack(audioTrack);
      expect(stream.active).assertFalse();
    })

    it('getTrackByIdFound', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let stream = new webrtc.MediaStream([audioTrack]);
      let found = stream.getTrackById(audioTrack.id);
      expect(found).not().assertNull();
      expect(found.id).assertEqual(audioTrack.id);
    })

    it('getTrackByIdNotFound', 0, () => {
      let stream = new webrtc.MediaStream();
      let found = stream.getTrackById('nonexistent');
      expect(found).assertNull();
    })

    it('getTracksEmpty', 0, () => {
      let stream = new webrtc.MediaStream();
      expect(stream.getTracks().length).assertEqual(0);
    })

    it('getTracksWithAudioTrack', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let stream = new webrtc.MediaStream([audioTrack]);
      expect(stream.getTracks().length).assertEqual(1);
      expect(stream.getTracks()[0].kind).assertEqual('audio');
    })

    it('getAudioTracks', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let stream = new webrtc.MediaStream([audioTrack]);
      let audioTracks = stream.getAudioTracks();
      expect(audioTracks.length).assertEqual(1);
      expect(audioTracks[0].kind).assertEqual('audio');
    })

    it('getAudioTracksEmpty', 0, () => {
      let stream = new webrtc.MediaStream();
      expect(stream.getAudioTracks().length).assertEqual(0);
    })

    it('getVideoTracksEmpty', 0, () => {
      let stream = new webrtc.MediaStream();
      expect(stream.getVideoTracks().length).assertEqual(0);
    })

    it('toJSON', 0, () => {
      let stream = new webrtc.MediaStream();
      let json = stream.toJSON();
      expect(json).not().assertNull();
      expect(json.id).assertEqual(stream.id);
      expect(json.active).assertEqual(stream.active);
    })

    it('addMultipleTracks', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack1 = pcf.createAudioTrack('audio1', audioSource);
      let audioTrack2 = pcf.createAudioTrack('audio2', audioSource);
      let stream = new webrtc.MediaStream();
      stream.addTrack(audioTrack1);
      stream.addTrack(audioTrack2);
      expect(stream.getAudioTracks().length).assertEqual(2);
      expect(stream.getTracks().length).assertEqual(2);
    })

    it('differentStreamIds', 0, () => {
      let stream1 = new webrtc.MediaStream();
      let stream2 = new webrtc.MediaStream();
      expect(stream1.id).not().assertEqual(stream2.id);
    })

    it('constructorWithStreamCopiesTracks', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let stream1 = new webrtc.MediaStream([audioTrack]);
      let stream2 = new webrtc.MediaStream(stream1);
      expect(stream2.getAudioTracks().length).assertEqual(1);
      expect(stream2.getAudioTracks()[0].id).assertEqual(audioTrack.id);
    })

    it('constructorWithVideoTrack', 0, () => {
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      let stream = new webrtc.MediaStream([videoTrack]);
      expect(stream).not().assertNull();
      expect(stream.getVideoTracks().length).assertEqual(1);
      expect(stream.active).assertTrue();
    })

    it('getVideoTracksWithVideoTrack', 0, () => {
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      let stream = new webrtc.MediaStream([videoTrack]);
      let videoTracks = stream.getVideoTracks();
      expect(videoTracks.length).assertEqual(1);
      expect(videoTracks[0].kind).assertEqual('video');
    })

    it('addVideoTrack', 0, () => {
      let stream = new webrtc.MediaStream();
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      stream.addTrack(videoTrack);
      expect(stream.getVideoTracks().length).assertEqual(1);
      expect(stream.active).assertTrue();
    })

    it('removeVideoTrack', 0, () => {
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      let stream = new webrtc.MediaStream([videoTrack]);
      expect(stream.getVideoTracks().length).assertEqual(1);
      stream.removeTrack(videoTrack);
      expect(stream.getVideoTracks().length).assertEqual(0);
      expect(stream.active).assertFalse();
    })

    it('mixedAudioAndVideoTracks', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      let stream = new webrtc.MediaStream([audioTrack, videoTrack]);
      expect(stream.getTracks().length).assertEqual(2);
      expect(stream.getAudioTracks().length).assertEqual(1);
      expect(stream.getVideoTracks().length).assertEqual(1);
    })

    it('getTrackByIdWithVideoTrack', 0, () => {
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      let stream = new webrtc.MediaStream([videoTrack]);
      let found = stream.getTrackById(videoTrack.id);
      expect(found).not().assertNull();
      expect(found.kind).assertEqual('video');
    })

    it('constructorWithStreamCopiesVideoTrack', 0, () => {
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      let stream1 = new webrtc.MediaStream([videoTrack]);
      let stream2 = new webrtc.MediaStream(stream1);
      expect(stream2.getVideoTracks().length).assertEqual(1);
      expect(stream2.getVideoTracks()[0].id).assertEqual(videoTrack.id);
    })

    it('toJSONWithTracks', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let stream = new webrtc.MediaStream([audioTrack]);
      let json = stream.toJSON();
      expect(json).not().assertNull();
      expect(json.id).assertEqual(stream.id);
      expect(json.active).assertTrue();
    })
  })
}
