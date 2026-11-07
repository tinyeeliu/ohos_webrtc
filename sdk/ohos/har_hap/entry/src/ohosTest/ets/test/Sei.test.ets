// Copyright (c) 2026. Licensed under the Apache License, Version 2.0.
import { describe, it, expect } from '@ohos/hypium';
import webrtc from '@ohos/webrtc';
import { pollUntilOrFail, sleep } from './Helper';

function assertBytes(actual: ArrayBuffer, expected: Uint8Array): void {
  const bytes = new Uint8Array(actual);
  expect(bytes.length).assertEqual(expected.length);
  expect(bytes.every((value: number, index: number) => value === expected[index])).assertTrue();
}

export default function SeiTest() {
  describe('H264Sei', () => {
    it('constantsAndAudioSenderValidation', 0, () => {
      expect(webrtc.EncoderWrapper.maxDataBytes).assertEqual(4096);
      expect(webrtc.EncoderWrapper.maxPendingMessages).assertEqual(64);
      const pc = new webrtc.RTCPeerConnection({ iceServers: [] });
      try {
        const transceiver = pc.addTransceiver('audio');
        let rejected = false;
        try { transceiver.sender.getEncoderWrapper(); } catch (error) { rejected = true; }
        expect(rejected).assertTrue();
      } finally { pc.close(); }
    });

    // Requires camera permission and a device/build with H.264 support.
    // Uses real video RTP; SDP/ICE are exchanged locally, with no SEI side channel.
    it('binaryRtpRoundTripRepeatedReceiverWrappersAndClose', 0, async () => {
      const factory = new webrtc.PeerConnectionFactory();
      webrtc.PeerConnectionFactory.setDefault(factory);
      const caller = factory.createPeerConnection({ iceServers: [] });
      const callee = factory.createPeerConnection({ iceServers: [] });
      let stream: webrtc.MediaStream | undefined = undefined;
      let receiver1: webrtc.RTCRtpReceiver | undefined = undefined;
      let receiver2: webrtc.RTCRtpReceiver | undefined = undefined;
      const received1: webrtc.RTCSeiEvent[] = [];
      const received2: webrtc.RTCSeiEvent[] = [];
      const customUuid = '00112233-4455-6677-8899-aabbccddeeff';
      const record = (events: webrtc.RTCSeiEvent[], sei: webrtc.RTCSeiEvent): void => {
        // Encoders may emit their own type=5 metadata; isolate this test's protocol.
        if (sei.uuid === webrtc.EncoderWrapper.defaultUuid || sei.uuid === customUuid) { events.push(sei); }
      };
      const pendingCaller: webrtc.RTCIceCandidateInit[] = [];
      const pendingCallee: webrtc.RTCIceCandidateInit[] = [];
      let iceError: string = '';
      try {
        const codecs = webrtc.RTCRtpSender.getCapabilities('video')?.codecs.filter(
          (codec: webrtc.RTCRtpCodec) => codec.mimeType.toLowerCase() === 'video/h264' &&
            (codec.sdpFmtpLine ?? '').split(';').some((part: string) => part.trim() === 'packetization-mode=1')) ?? [];
        expect(codecs.length > 0).assertTrue();
        stream = await new webrtc.MediaDevices().getUserMedia({ audio: false,
          video: { width: 640, height: 480, frameRate: 15 } });
        const transceiver = caller.addTransceiver(stream.getVideoTracks()[0], { direction: 'sendonly' });
        transceiver.setCodecPreferences(codecs);
        const encoder = transceiver.sender.getEncoderWrapper();
        callee.ontrack = (event: webrtc.RTCTrackEvent): void => {
          receiver1 = event.receiver;
          receiver1.onsei = (sei: webrtc.RTCSeiEvent): void => { record(received1, sei); };
        };
        caller.onicecandidate = (event: webrtc.RTCPeerConnectionIceEvent): void => {
          if (!event.candidate) { return; }
          const ice = event.candidate.toJSON();
          if (callee.remoteDescription) {
            callee.addIceCandidate(ice).catch((error: Error) => { iceError = error.message; });
          } else { pendingCallee.push(ice); }
        };
        callee.onicecandidate = (event: webrtc.RTCPeerConnectionIceEvent): void => {
          if (!event.candidate) { return; }
          const ice = event.candidate.toJSON();
          if (caller.remoteDescription) {
            caller.addIceCandidate(ice).catch((error: Error) => { iceError = error.message; });
          } else { pendingCaller.push(ice); }
        };
        const offer = await caller.createOffer();
        await caller.setLocalDescription(offer);
        await callee.setRemoteDescription(offer);
        for (const ice of pendingCallee.splice(0)) { await callee.addIceCandidate(ice); }
        const answer = await callee.createAnswer();
        await callee.setLocalDescription(answer);
        await caller.setRemoteDescription(answer);
        for (const ice of pendingCaller.splice(0)) { await caller.addIceCandidate(ice); }
        await pollUntilOrFail(() => encoder.ready && receiver1 !== undefined &&
          caller.connectionState === 'connected' && callee.connectionState === 'connected',
          'H.264 encoder/receiver readiness', 20000);
        expect(iceError).assertEqual('');
        // A separate N-API wrapper must share the existing native receiver transformer.
        receiver2 = callee.getReceivers().find((receiver: webrtc.RTCRtpReceiver) => receiver.track.kind === 'video');
        expect(receiver2 !== undefined).assertTrue();
        receiver2!.onsei = (sei: webrtc.RTCSeiEvent): void => { record(received2, sei); };
        for (const length of [1, 239, 240, 4096]) {
          const payload = new Uint8Array(length);
          for (let i = 0; i < length; i++) { payload[i] = i % 256; }
          const expected = new Uint8Array(payload);
          const before = received1.length;
          encoder.writeSei(payload.buffer);
          payload.fill(0xff);
          await pollUntilOrFail(() => received1.length > before && received2.length > before, 'SEI RTP delivery');
          const event = received1[before];
          assertBytes(event.data, expected);
          assertBytes(received2[before].data, expected);
          expect(event.uuid).assertEqual(webrtc.EncoderWrapper.defaultUuid);
          expect(event.payloadType).assertEqual(5);
          expect(event.codec).assertEqual('video/H264');
          expect(Number.isInteger(event.ssrc)).assertTrue();
          expect(Number.isInteger(event.rtpTimestamp)).assertTrue();
        }
        receiver1!.onsei = null;
        const firstCount = received1.length;
        const secondCount = received2.length;
        encoder.writeSei(new Uint8Array([0, 0, 1, 0, 0, 3]).buffer, customUuid);
        await pollUntilOrFail(() => received2.length > secondCount, 'second receiver listener remains active');
        expect(received1.length).assertEqual(firstCount);
        expect(received2[secondCount].uuid).assertEqual(customUuid);
        for (const length of [0, 4097]) {
          let rejected = false;
          try { encoder.writeSei(new ArrayBuffer(length)); } catch (error) { rejected = true; }
          expect(rejected).assertTrue();
        }
        let rejected = false;
        try { encoder.writeSei(new Uint8Array([1]).buffer, 'invalid'); } catch (error) { rejected = true; }
        expect(rejected).assertTrue();
        encoder.clearSei();
        const beforeClose = received2.length;
        encoder.writeSei(new Uint8Array([123]).buffer);
        callee.close();
        await sleep(200);
        expect(received2.length).assertEqual(beforeClose);
        caller.close();
        expect(encoder.ready).assertFalse();
        rejected = false;
        try { encoder.writeSei(new Uint8Array([1]).buffer); } catch (error) { rejected = true; }
        expect(rejected).assertTrue();
      } finally {
        if (receiver1) { receiver1.onsei = null; }
        if (receiver2) { receiver2.onsei = null; }
        caller.onicecandidate = null; callee.onicecandidate = null; callee.ontrack = null;
        caller.close(); callee.close();
        stream?.getTracks().forEach((track: webrtc.MediaStreamTrack) => {
          const source = track.getSource(); track.stop(); source?.release();
        });
      }
    });
  });
}
