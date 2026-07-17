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

const TAG: string = '[VideoEncoderFactoryTest]';

export default function VideoEncoderFactoryTest() {
  describe('VideoEncoderFactoryTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    describe('HardwareVideoEncoderFactory', () => {
      it('constructWithDefaultOptions', 0, () => {
        let factory = new webrtc.HardwareVideoEncoderFactory();
        expect(factory).not().assertNull();
      })

      it('constructWithH264HighProfileTrue', 0, () => {
        let factory = new webrtc.HardwareVideoEncoderFactory(true);
        expect(factory.enableH264HighProfile).assertTrue();
      })

      it('constructWithH264HighProfileFalse', 0, () => {
        let factory = new webrtc.HardwareVideoEncoderFactory(false);
        expect(factory.enableH264HighProfile).assertFalse();
      })

      it('hardwareEncoderProducesOfferWithVideoMline', 0, async () => {
        let encoderFactory = new webrtc.HardwareVideoEncoderFactory();
        let factory = new webrtc.PeerConnectionFactory({ videoEncoderFactory: encoderFactory });
        let videoSource = factory.createVideoSource();
        let videoTrack = factory.createVideoTrack('video', videoSource);
        let pc = factory.createPeerConnection({});
        pc.addTrack(videoTrack);
        let offer = await pc.createOffer();
        expect(offer.type).assertEqual('offer');
        expect(offer.sdp).assertContain('m=video');
        pc.close();
        try {
          videoSource.release();
        } catch (e) {
        }
      })
    })

    describe('SoftwareVideoEncoderFactory', () => {
      it('construct', 0, () => {
        let factory = new webrtc.SoftwareVideoEncoderFactory();
        expect(factory).not().assertNull();
      })

      it('softwareEncoderProducesOfferWithVideoMline', 0, async () => {
        let encoderFactory = new webrtc.SoftwareVideoEncoderFactory();
        let factory = new webrtc.PeerConnectionFactory({ videoEncoderFactory: encoderFactory });
        let videoSource = factory.createVideoSource();
        let videoTrack = factory.createVideoTrack('video', videoSource);
        let pc = factory.createPeerConnection({});
        pc.addTrack(videoTrack);
        let offer = await pc.createOffer();
        expect(offer.type).assertEqual('offer');
        expect(offer.sdp).assertContain('m=video');
        pc.close();
        try {
          videoSource.release();
        } catch (e) {
        }
      })
    })

    describe('HardwareVideoDecoderFactory', () => {
      it('construct', 0, () => {
        let factory = new webrtc.HardwareVideoDecoderFactory();
        expect(factory).not().assertNull();
      })
    })

    describe('SoftwareVideoDecoderFactory', () => {
      it('construct', 0, () => {
        let factory = new webrtc.SoftwareVideoDecoderFactory();
        expect(factory).not().assertNull();
      })
    })

    describe('AudioProcessingFactory', () => {
      it('constructAndCreate', 0, () => {
        let factory = new webrtc.AudioProcessingFactory();
        expect(factory).not().assertNull();
        let ap = factory.create();
        expect(ap).not().assertNull();
      })
    })
  })
}
