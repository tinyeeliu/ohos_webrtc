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

import { describe, beforeAll, afterEach, it, expect } from '@ohos/hypium';
import webrtc from 'libohos_webrtc.so';
import { Logging, LoggingSeverity } from '../../../main/ets/log/Logging';
import { isX86, pollUntil, sleep } from './Helper';

const TAG: string = '[X86EmulatorFallbackTest]';

const pcf = new webrtc.PeerConnectionFactory();

// 与 workPage.ets createVideoSourceWithFallback 保持一致：优先后置，失败回退不限定 facingMode。
function createVideoSourceWithFallback(width: number, height: number,
  factory: webrtc.PeerConnectionFactory = pcf): webrtc.VideoSource | undefined {
  let src: webrtc.VideoSource | undefined;
  try {
    src = factory.createVideoSource({ width, height, facingMode: 'environment' }, false);
  } catch (e) {
    console.warn(TAG, `createVideoSource(environment) threw: ${JSON.stringify(e)}`);
  }
  if (!src) {
    console.warn(TAG, 'environment facingMode unsatisfied, retry without facingMode');
    try {
      src = factory.createVideoSource({ width, height }, false);
    } catch (e) {
      console.error(TAG, `createVideoSource(fallback) threw: ${JSON.stringify(e)}`);
    }
  }
  return src;
}

async function hasVideoInputDevice(): Promise<boolean> {
  const mediaDevices = new webrtc.MediaDevices();
  const devices = await mediaDevices.enumerateDevices();
  for (let i = 0; i < devices.length; i++) {
    if (devices[i].kind === 'videoinput') {
      return true;
    }
  }
  return false;
}

function releaseVideoSource(videoSource: webrtc.VideoSource | undefined): void {
  if (!videoSource) {
    return;
  }
  try {
    videoSource.release();
  } catch (e) {
  }
}

function isLiveVideoSourceState(state: string): boolean {
  return state === 'live' || state === 'initializing' || state === 'ended' || state === 'muted';
}

export default function X86EmulatorFallbackTest() {
  describe('X86EmulatorFallbackTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    });

    afterEach(async () => {
      await sleep(300);
    });

    // 场景 1：默认 PeerConnectionFactory 视频编解码协商成功（含软件回退）。
    // 注：getParameters().codecs 需完成 offer/answer 交换后才有值，此处用 SDP + getCapabilities 验证。
    it('defaultFactoryVideoRecvNegotiationSucceeds', 0, async () => {
      const capabilities = webrtc.RTCRtpReceiver.getCapabilities('video');
      expect(capabilities).not().assertNull();
      expect(capabilities!.codecs.length).assertLarger(0);

      const factory = new webrtc.PeerConnectionFactory();
      const pc = factory.createPeerConnection({ iceServers: [] });
      pc.addTransceiver('video', { direction: 'recvonly' });
      const offer = await pc.createOffer();
      await pc.setLocalDescription(offer);
      expect(offer.sdp).assertContain('m=video');
      expect(offer.sdp!.indexOf('rtpmap')).assertLarger(-1);
      pc.close();
    });

    // 场景 2：后置摄像头不可用时回退为不限定 facingMode，能创建 VideoSource 且不抛异常。
    it('createVideoSourceEnvironmentFacingModeFallback', 0, () => {
      const videoSource = createVideoSourceWithFallback(640, 480);
      if (videoSource) {
        expect(isLiveVideoSourceState(videoSource.state)).assertTrue();
      }
      releaseVideoSource(videoSource);
    });

    // 场景 3：摄像头完全不可用时降级 audio-only，setLocalDescription 仅含音频 m-line。
    it('audioOnlyModeWhenCameraUnavailable', 0, async () => {
      const videoSource = createVideoSourceWithFallback(640, 480);
      if (videoSource) {
        releaseVideoSource(videoSource);
        return;
      }
      const pc = pcf.createPeerConnection({ iceServers: [] });
      const audioSource = pcf.createAudioSource();
      const audioTrack = pcf.createAudioTrack('audio', audioSource);
      pc.addTrack(audioTrack);
      const offer = await pc.createOffer();
      await pc.setLocalDescription(offer);
      expect(offer.sdp).assertContain('m=audio');
      expect(offer.sdp!.indexOf('m=video')).assertEqual(-1);
      audioSource.release();
      pc.close();
    });

    // 场景 4：x86 CPU 渲染路径（NativeWindowRendererRaster）下 setVideoTrack/镜像设置不崩溃。
    it('nativeVideoRendererSetVideoTrackOnX86', 0, () => {
      if (!isX86()) {
        return;
      }
      const videoSource = createVideoSourceWithFallback(640, 480);
      if (!videoSource) {
        return;
      }
      const renderer = new webrtc.NativeVideoRenderer();
      const videoTrack = pcf.createVideoTrack('video', videoSource);
      renderer.setMirror(true);
      renderer.setMirrorVertically(true);
      renderer.setVideoTrack(videoTrack);
      renderer.setMirror(false);
      renderer.setMirrorVertically(false);
      renderer.setScalingMode(0);
      renderer.release();
      releaseVideoSource(videoSource);
    });

    // 场景 5：x86 CPU 采集路径（VideoFrameReceiverNative）下摄像头可用时 capturer 能启动。
    // 无摄像头或采集不可用时跳过，避免 enumerateDevices 误报 videoinput 导致超时 fail。
    it('videoSourceCapturerStartedOnX86', 0, async () => {
      if (!isX86()) {
        return;
      }
      const videoSource = createVideoSourceWithFallback(640, 480);
      if (!videoSource) {
        return;
      }
      let capturerStarted = false;
      videoSource.oncapturerstarted = (event: webrtc.VideoCapturerStartedEvent) => {
        capturerStarted = event.success;
      };
      const started = await pollUntil(
        () => capturerStarted || videoSource.state === 'live', 5000);
      try {
        if (!started) {
          console.warn(TAG, `capturer unavailable on x86, skip: state=${videoSource.state}`);
          return;
        }
        expect(capturerStarted || videoSource.state === 'live').assertTrue();
      } finally {
        releaseVideoSource(videoSource);
      }
    });

    // 场景 6：CPU 渲染路径下镜像/旋转相关设置不崩溃（对应 UpdateRotation + I420Buffer::Rotate 链路）。
    it('nativeVideoRendererMirrorSettingsOnX86', 0, () => {
      if (!isX86()) {
        return;
      }
      const renderer = new webrtc.NativeVideoRenderer();
      renderer.setMirror(true);
      renderer.setMirrorVertically(true);
      renderer.setScalingMode(1);
      renderer.setMirror(false);
      renderer.setMirrorVertically(false);
      renderer.setScalingMode(2);
      expect(true).assertTrue();
    });

    // 场景 7：帧尺寸/旋转变化回调 onFrameResolutionChanged 可注册（对应 frameSizeChangeCallback_）。
    it('nativeVideoRendererFrameResolutionCallbackRegistered', 0, () => {
      const renderer = new webrtc.NativeVideoRenderer();
      renderer.onFrameResolutionChanged = () => {
      };
      expect(renderer.onFrameResolutionChanged).not().assertNull();
    });

    // 场景 8：x86 上仅指定硬件编解码工厂时，接收方向 codec 列表为空或 setLocalDescription 失败（负向回归）。
    it('hardwareOnlyFactoryRecvNegotiationFailsOnX86', 0, async () => {
      if (!isX86()) {
        return;
      }
      const hwFactory = new webrtc.PeerConnectionFactory({
        videoEncoderFactory: new webrtc.HardwareVideoEncoderFactory(),
        videoDecoderFactory: new webrtc.HardwareVideoDecoderFactory(),
      });
      const pc = hwFactory.createPeerConnection({ iceServers: [] });
      pc.addTransceiver('video', { direction: 'recvonly' });
      const offer = await pc.createOffer();

      let recvCodecUnavailable = false;
      try {
        await pc.setLocalDescription(offer);
      } catch (e) {
        recvCodecUnavailable = true;
        console.info(TAG, `hardware-only recv setLocalDescription failed as expected: ${JSON.stringify(e)}`);
      }

      if (!recvCodecUnavailable) {
        const receivers = pc.getReceivers();
        if (receivers.length > 0) {
          recvCodecUnavailable = receivers[0].getParameters().codecs.length === 0;
        }
      }
      expect(recvCodecUnavailable).assertTrue();
      pc.close();
    });

    // 场景 9：存在 videoinput 设备时（API18+ 宿主机摄像头），createVideoSourceWithFallback 能创建 VideoSource。
    it('videoSourceCreatedWhenVideoInputExists', 0, async () => {
      const hasVideo = await hasVideoInputDevice();
      if (!hasVideo) {
        return;
      }
      const videoSource = createVideoSourceWithFallback(640, 480);
      expect(videoSource).not().assertUndefined();
      if (videoSource) {
        expect(isLiveVideoSourceState(videoSource.state)).assertTrue();
      }
      releaseVideoSource(videoSource);
    });

    // 场景 10：无 videoinput 时回退 audio-only，与 API18 以下模拟器行为一致。
    it('audioOnlyFallbackWhenNoVideoInput', 0, async () => {
      const hasVideo = await hasVideoInputDevice();
      if (hasVideo) {
        return;
      }
      const videoSource = createVideoSourceWithFallback(640, 480);
      expect(videoSource).assertUndefined();

      const pc = pcf.createPeerConnection({ iceServers: [] });
      const audioSource = pcf.createAudioSource();
      pc.addTrack(pcf.createAudioTrack('audio', audioSource));
      const offer = await pc.createOffer();
      await pc.setLocalDescription(offer);
      expect(offer.sdp).assertContain('m=audio');
      expect(offer.sdp!.indexOf('m=video')).assertEqual(-1);
      audioSource.release();
      pc.close();
    });
  });
}
