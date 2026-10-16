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
import { pollUntilOrFail } from './Helper';

const TAG: string = '[PeerConnectionStreamEventsTest]';

const pcf = new webrtc.PeerConnectionFactory();

class AudioStreamLoopback {
  caller: webrtc.RTCPeerConnection = pcf.createPeerConnection({ iceServers: [] });
  callee: webrtc.RTCPeerConnection = pcf.createPeerConnection({ iceServers: [] });
  audioSource: webrtc.AudioSource = pcf.createAudioSource();
  audioTrack: webrtc.AudioTrack = pcf.createAudioTrack('audio', this.audioSource);
  stream: webrtc.MediaStream = new webrtc.MediaStream([this.audioTrack]);
  sender: webrtc.RTCRtpSender | null = null;

  async connect(): Promise<void> {
    this.sender = this.caller.addTrack(this.audioTrack, this.stream);
    this.caller.onicecandidate = (event) => {
      if (this.callee.signalingState !== 'closed') {
        this.callee.addIceCandidate(event.candidate);
      }
    };
    this.callee.onicecandidate = (event) => {
      if (this.caller.signalingState !== 'closed') {
        this.caller.addIceCandidate(event.candidate);
      }
    };
    await this.caller.setLocalDescription(await this.caller.createOffer());
    await this.callee.setRemoteDescription(this.caller.localDescription);
    const answer = await this.callee.createAnswer();
    await this.caller.setRemoteDescription(answer);
    await this.callee.setLocalDescription(answer);
  }

  async renegotiateAfterRemoveTrack(): Promise<void> {
    this.caller.removeTrack(this.sender!);
    await this.caller.setLocalDescription(await this.caller.createOffer());
    await this.callee.setRemoteDescription(this.caller.localDescription);
    const answer = await this.callee.createAnswer();
    await this.caller.setRemoteDescription(answer);
    await this.callee.setLocalDescription(answer);
  }

  close(): void {
    this.caller.close();
    this.callee.close();
    this.audioSource.release();
  }
}

export default function PeerConnectionStreamEventsTest() {
  describe('PeerConnectionStreamEventsTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('streamEventPropertiesInitiallyNull', 0, () => {
      const pc = pcf.createPeerConnection({ iceServers: [] });
      expect(pc.onaddstream).assertNull();
      expect(pc.onremovestream).assertNull();
      pc.close();
    })

    it('onAddStreamFiresWithStreamAndStreamId', 0, async () => {
      const loopback = new AudioStreamLoopback();
      let addFired = false;
      let gotStreamId = '';
      loopback.callee.onaddstream = (ev: webrtc.MediaStreamEvent) => {
        addFired = true;
        gotStreamId = ev.streamId;
        console.info(TAG, 'onaddstream streamId: ' + ev.streamId);
        expect(ev.stream).not().assertNull();
        expect(typeof ev.stream.getAudioTracks).assertEqual('function');
      };

      await loopback.connect();
      await pollUntilOrFail(() => addFired, 'onaddstream not fired');
      expect(gotStreamId.length).assertLarger(0);

      loopback.close();
    })

    it('onRemoveStreamFiresAfterTrackRemoval', 0, async () => {
      const loopback = new AudioStreamLoopback();
      let removeFired = false;
      loopback.callee.onremovestream = (ev: webrtc.MediaStreamEvent) => {
        removeFired = true;
        console.info(TAG, 'onremovestream streamId: ' + ev.streamId);
        expect(ev.stream).not().assertNull();
      };

      await loopback.connect();
      await loopback.renegotiateAfterRemoveTrack();
      await pollUntilOrFail(() => removeFired, 'onremovestream not fired');

      loopback.close();
    })

    it('addAndRemoveStreamDeliverSameStreamObject', 0, async () => {
      const loopback = new AudioStreamLoopback();
      let addedStream: webrtc.MediaStream | null = null;
      let removedStream: webrtc.MediaStream | null = null;
      loopback.callee.onaddstream = (ev: webrtc.MediaStreamEvent) => {
        if (addedStream === null) {
          addedStream = ev.stream;
        }
      };
      loopback.callee.onremovestream = (ev: webrtc.MediaStreamEvent) => {
        removedStream = ev.stream;
      };

      await loopback.connect();
      await loopback.renegotiateAfterRemoveTrack();
      await pollUntilOrFail(() => removedStream !== null, 'onremovestream not fired');
      // 契约: remove 回传当初 add 的同一对象
      expect(removedStream === addedStream).assertTrue();

      loopback.close();
    })

    // ===== 异常参数: null 处理器与抛异常处理器不得中断协商 =====

    it('nullStreamHandlersDoNotBreakNegotiation', 0, async () => {
      const loopback = new AudioStreamLoopback();
      loopback.callee.onaddstream = null;
      loopback.callee.onremovestream = null;

      await loopback.connect();
      expect(loopback.callee.signalingState).assertEqual('stable');

      loopback.close();
    })

    it('throwingStreamHandlerDoesNotBreakNegotiation', 0, async () => {
      const loopback = new AudioStreamLoopback();
      loopback.callee.onaddstream = (ev: webrtc.MediaStreamEvent) => {
        throw new Error('handler error on purpose');
      };

      await loopback.connect();
      expect(loopback.callee.signalingState).assertEqual('stable');

      loopback.close();
    })
  })
}
