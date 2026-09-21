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
import { sleep } from './Helper';

const TAG: string = '[MediaStreamTrackTest]';


const pcf = new webrtc.PeerConnectionFactory();

export default function MediaStreamTrackTest() {
  describe('MediaStreamTrackTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('enabledFalseDoesNotChangeReadyState', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack.readyState).assertEqual('live');
      audioTrack.enabled = false;
      expect(audioTrack.readyState).assertEqual('live');
      audioSource.release();
    })

    it('stopChangesReadyStateToEnded', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack.readyState).assertEqual('live');
      audioTrack.stop();
      expect(audioTrack.readyState).assertEqual('ended');
      audioSource.release();
    })

    it('getSourceAfterStopStillReturnsSource', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      audioTrack.stop();
      let source = audioTrack.getSource();
      expect(source).not().assertNull();
      audioSource.release();
    })

    it('addTrackToPeerConnectionCreatesSenderWithTrack', 0, () => {
      let pc = pcf.createPeerConnection({});
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let sender = pc.addTrack(audioTrack);
      expect(sender.track).not().assertNull();
      expect(sender.track!.id).assertEqual(audioTrack.id);
      expect(sender.track!.kind).assertEqual('audio');
      pc.close();
      audioSource.release();
    })

    it('stopTrackRemovesItFromSender', 0, () => {
      let pc = pcf.createPeerConnection({});
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let sender = pc.addTrack(audioTrack);
      expect(sender.track).not().assertNull();
      pc.removeTrack(sender);
      expect(sender.track).assertNull();
      pc.close();
      audioSource.release();
    })

    it('replaceTrackUpdatesSenderTrack', 0, async () => {
      let pc = pcf.createPeerConnection({});
      let audioSource1 = pcf.createAudioSource();
      let audioTrack1 = pcf.createAudioTrack('audio1', audioSource1);
      let audioSource2 = pcf.createAudioSource();
      let audioTrack2 = pcf.createAudioTrack('audio2', audioSource2);
      let sender = pc.addTrack(audioTrack1);
      expect(sender.track!.id).assertEqual(audioTrack1.id);
      await sender.replaceTrack(audioTrack2);
      expect(sender.track!.id).assertEqual(audioTrack2.id);
      pc.close();
      audioSource1.release();
      audioSource2.release();
    })

    it('calleeReceivesTrackViaOntrack', 0, async () => {
      let caller = pcf.createPeerConnection({
        iceServers: []
      });
      let callee = pcf.createPeerConnection({
        iceServers: []
      });

      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      caller.addTrack(audioTrack);

      caller.onicecandidate = (event) => {
        if (callee.signalingState !== 'closed' && callee.remoteDescription) {
          callee.addIceCandidate(event.candidate).catch(() => {});
        }
      };
      callee.onicecandidate = (event) => {
        if (caller.signalingState !== 'closed' && caller.remoteDescription) {
          caller.addIceCandidate(event.candidate).catch(() => {});
        }
      };

      let ontrackResolve: ((track: webrtc.MediaStreamTrack) => void) | null = null;
      let ontrackPromise = new Promise<webrtc.MediaStreamTrack>((resolve) => {
        ontrackResolve = resolve;
      });
      callee.ontrack = (event) => {
        if (ontrackResolve) {
          ontrackResolve(event.track);
          ontrackResolve = null;
        }
      };

      await caller.setLocalDescription(await caller.createOffer());
      await callee.setRemoteDescription(caller.localDescription);
      await callee.setLocalDescription(await callee.createAnswer());
      await caller.setRemoteDescription(callee.localDescription);

      let timeoutPromise = new Promise<null>((resolve) => {
        setTimeout(() => resolve(null), 10000);
      });
      let track = await Promise.race([ontrackPromise, timeoutPromise]);
      expect(track).not().assertNull();
      expect(track!.kind).assertEqual('audio');
      expect(track!.readyState).assertEqual('live');

      caller.close();
      callee.close();
      audioSource.release();
    })

    it('applyConstraintsWithWidthHeightFrameRate', 0, () => {
      let videoSource = pcf.createVideoSource({});
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();

      videoTrack.applyConstraints({
        width: 1280,
        height: 720,
        frameRate: 30
      });

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsWithFacingModeUser', 0, () => {
      let videoSource = pcf.createVideoSource({});
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();

      videoTrack.applyConstraints({
        facingMode: 'user'
      });

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsWithFacingModeEnvironment', 0, () => {
      let videoSource = pcf.createVideoSource({});
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();

      videoTrack.applyConstraints({
        facingMode: 'environment'
      });

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsWithAllParams', 0, () => {
      let videoSource = pcf.createVideoSource({});
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();

      videoTrack.applyConstraints({
        width: 1920,
        height: 1080,
        frameRate: 60,
        facingMode: 'user'
      });

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsAudioTrackNotSupported', 0, async () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack).not().assertNull();

      try {
        await audioTrack.applyConstraints({
          echoCancellation: true
        });
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }

      audioSource.release();
    })

    it('applyConstraintsWithNegativeWidthShouldReject', 0, async () => {
      let videoSource = pcf.createVideoSource({});
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();

      try {
        await videoTrack.applyConstraints({
          width: -1
        });
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsWithZeroWidthShouldReject', 0, async () => {
      let videoSource = pcf.createVideoSource({});
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();

      try {
        await videoTrack.applyConstraints({
          width: 0
        });
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsWithNegativeHeightShouldReject', 0, async () => {
      let videoSource = pcf.createVideoSource({});
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();

      try {
        await videoTrack.applyConstraints({
          height: -1
        });
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsWithZeroHeightShouldReject', 0, async () => {
      let videoSource = pcf.createVideoSource({});
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();

      try {
        await videoTrack.applyConstraints({
          height: 0
        });
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsWithNegativeFrameRateShouldReject', 0, async () => {
      let videoSource = pcf.createVideoSource({});
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();

      try {
        await videoTrack.applyConstraints({
          frameRate: -1
        });
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsWithZeroFrameRateShouldReject', 0, async () => {
      let videoSource = pcf.createVideoSource({});
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();

      try {
        await videoTrack.applyConstraints({
          frameRate: 0
        });
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsWithInvalidFacingModeShouldReject', 0, async () => {
      let videoSource = pcf.createVideoSource({});
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();

      try {
        await videoTrack.applyConstraints({
          facingMode: 'invalid_facing_mode'
        });
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsWithEmptyFacingModeShouldReject', 0, async () => {
      let videoSource = pcf.createVideoSource({});
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();

      try {
        await videoTrack.applyConstraints({
          facingMode: ''
        });
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }

      videoTrack.stop();
      videoSource.release();
    })

    it('applyConstraintsToEndedTrackShouldReject', 0, async () => {
      let videoSource = pcf.createVideoSource({});
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      expect(videoTrack).not().assertNull();
      videoTrack.stop();

      try {
        await videoTrack.applyConstraints({
          width: 1280,
          height: 720
        });
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }

      videoSource.release();
    })

    // ---- #6: captureFrame ----

    it('captureFrameOnAudioTrackRejects', 0, async () => {
      const audioSource = pcf.createAudioSource();
      const audioTrack = pcf.createAudioTrack('cap-audio', audioSource);
      let rejected = false;
      try {
        await audioTrack.captureFrame();
      } catch (e) {
        rejected = true;
      }
      expect(rejected).assertTrue();
      audioSource.release();
    })

    it('captureFrameOnEndedVideoTrackRejects', 0, async () => {
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('cap-ended', videoSource);
      videoTrack.stop();
      let rejected = false;
      try {
        await videoTrack.captureFrame();
      } catch (e) {
        rejected = true;
      }
      expect(rejected).assertTrue();
      videoSource.release();
    })

    it('captureFrameOnLiveVideoTrackResolvesWithRgba', 0, async () => {
      const videoSource = pcf.createVideoSource({ width: 320, height: 240, facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('cap-live', videoSource);
      await sleep(1000); // 等待首帧

      const frame = await videoTrack.captureFrame();
      expect(frame.width).assertLarger(0);
      expect(frame.height).assertLarger(0);
      expect(frame.format).assertEqual('rgba');
      expect(frame.data.byteLength).assertEqual(frame.width * frame.height * 4);

      videoTrack.stop();
      videoSource.release();
    })

    it('captureFrameRejectsConcurrentSecondCall', 0, async () => {
      const videoSource = pcf.createVideoSource({ width: 320, height: 240, facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('cap-busy', videoSource);
      await sleep(500);

      const first = videoTrack.captureFrame();
      let secondRejected = false;
      try {
        await videoTrack.captureFrame();
      } catch (e) {
        secondRejected = true;
      }
      expect(secondRejected).assertTrue();

      const frame = await first; // 第一个请求应正常完成
      expect(frame.width).assertLarger(0);

      videoTrack.stop();
      videoSource.release();
    })

    // ---- #5: onaudiosamplesready(播放侧逐样本) ----

    it('audioSamplesHandlerDefaultsToNullAndRejectsNonFunction', 0, () => {
      const audioSource = pcf.createAudioSource();
      const audioTrack = pcf.createAudioTrack('samp-args', audioSource);
      expect(audioTrack.onaudiosamplesready).assertNull();

      let thrown = false;
      try {
        const bad: ESObject = audioTrack as ESObject;
        bad.onaudiosamplesready = 'not-a-function';
      } catch (e) {
        thrown = true;
      }
      expect(thrown).assertTrue();

      audioSource.release();
    })

    it('audioSamplesEventsArriveOnLocalMicTrackAndStopAfterNull', 0, async () => {
      const audioSource = pcf.createAudioSource();
      const audioTrack = pcf.createAudioTrack('samp-live', audioSource);

      let events = 0;
      let lastByteLength = 0;
      audioTrack.onaudiosamplesready = (ev: webrtc.AudioTrackSamplesEvent) => {
        events++;
        lastByteLength = ev.samples.data.byteLength;
      };

      let elapsed = 0;
      while (elapsed < 5000 && events === 0) {
        await sleep(200);
        elapsed += 200;
      }
      expect(events).assertLarger(0);
      expect(lastByteLength).assertLarger(0);

      audioTrack.onaudiosamplesready = null;
      const countAtStop = events;
      await sleep(1000);
      // 置 null 后不再增长(允许停止前在途的一帧)
      expect(events - countAtStop).assertLessOrEqual(1);

      audioSource.release();
    })

    it('audioSamplesHandlerOnVideoTrackStaysSilent', 0, async () => {
      const videoSource = pcf.createVideoSource({ facingMode: 'environment' });
      const videoTrack = pcf.createVideoTrack('samp-video', videoSource);

      let events = 0;
      videoTrack.onaudiosamplesready = (ev: webrtc.AudioTrackSamplesEvent) => {
        events++;
      };
      await sleep(1000);
      expect(events).assertEqual(0);

      videoTrack.onaudiosamplesready = null;
      videoTrack.stop();
      videoSource.release();
    })
  })
}
