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

    // ---- #9: 强软/禁硬 codec 名单 ----

    describe('CodecForceLists', () => {
      it('constructEncoderFactoryWithForceSWList', 0, () => {
        const encoderFactory = new webrtc.HardwareVideoEncoderFactory({ forceSWCodecList: ['H264'] });
        expect(encoderFactory).not().assertNull();
        expect(encoderFactory.enableH264HighProfile).assertFalse();
      })

      it('constructEncoderFactoryWithBothListsAndHighProfile', 0, () => {
        const encoderFactory = new webrtc.HardwareVideoEncoderFactory({
          enableH264HighProfile: true,
          forceSWCodecList: ['h264'],
          forceHWCodecList: ['VP8'],
        });
        expect(encoderFactory.enableH264HighProfile).assertTrue();
      })

      it('forceSWH264StillNegotiatesVideoOffer', 0, async () => {
        // H264 被强软后应由软编兜底,视频协商仍然成立
        const encoderFactory = new webrtc.HardwareVideoEncoderFactory({ forceSWCodecList: ['H264'] });
        const factory = new webrtc.PeerConnectionFactory({ videoEncoderFactory: encoderFactory });
        const videoSource = factory.createVideoSource();
        const videoTrack = factory.createVideoTrack('force-sw-video', videoSource);
        const pc = factory.createPeerConnection({});
        pc.addTrack(videoTrack);
        const offer = await pc.createOffer();
        expect(offer.sdp ?? '').assertContain('m=video');
        pc.close();
        try {
          videoSource.release();
        } catch (e) {
        }
      })

      it('constructDecoderFactoryWithLists', 0, () => {
        const decoderFactory = new webrtc.HardwareVideoDecoderFactory({ forceSWCodecList: ['H265'] });
        expect(decoderFactory).not().assertNull();
      })

      it('nonArrayForceSWListThrows', 0, () => {
        let thrown = false;
        try {
          const options: ESObject = {};
          options.forceSWCodecList = 'H264';
          new webrtc.HardwareVideoEncoderFactory(options);
        } catch (e) {
          thrown = true;
        }
        expect(thrown).assertTrue();
      })

      it('nonStringEntryInForceHWListThrows', 0, () => {
        let thrown = false;
        try {
          const options: ESObject = {};
          options.forceHWCodecList = ['H264', 42];
          new webrtc.HardwareVideoEncoderFactory(options);
        } catch (e) {
          thrown = true;
        }
        expect(thrown).assertTrue();
      })

      it('nonArrayDecoderForceSWListThrows', 0, () => {
        let thrown = false;
        try {
          const options: ESObject = {};
          options.forceSWCodecList = { codec: 'H264' };
          new webrtc.HardwareVideoDecoderFactory(options);
        } catch (e) {
          thrown = true;
        }
        expect(thrown).assertTrue();
      })
    })
  })
}
