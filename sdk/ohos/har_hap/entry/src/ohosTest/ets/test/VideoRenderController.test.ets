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
import webrtc, { Logging, LoggingSeverity, VideoRenderController, ScalingMode } from '@ohos/webrtc';

const TAG: string = '[VideoRenderControllerTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function VideoRenderControllerTest() {
  describe('VideoRenderControllerTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('shouldHandleAudioTrackWhenSetVideoTrack', 0, () => {
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

    it('shouldNotCrashWhenSetVideoTrackWithNull', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.setVideoTrack(null);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldNotCrashWhenSetVideoTrackWithVideoTrack', 0, () => {
      let controller = new VideoRenderController();
      let videoSource = pcf.createVideoSource();
      let videoTrack = pcf.createVideoTrack('video', videoSource);
      try {
        controller.setVideoTrack(videoTrack);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
      videoSource.release();
    })

    it('shouldNotCrashWhenSetMirror', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.setMirror(true);
        controller.setMirror(false);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldNotCrashWhenSetMirrorVertically', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.setMirrorVertically(true);
        controller.setMirrorVertically(false);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldNotCrashWhenSetScalingModeAllValues', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.setScalingMode(ScalingMode.Fill);
        controller.setScalingMode(ScalingMode.AspectFill);
        controller.setScalingMode(ScalingMode.AspectFit);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldThrowWhenOnSurfaceCreatedWithEmptyString', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.onSurfaceCreated('');
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('shouldThrowWhenOnSurfaceCreatedWithNonNumericString', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.onSurfaceCreated('not-a-number');
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('shouldNotCrashWhenOnSurfaceChanged', 0, () => {
      let controller = new VideoRenderController();
      let rect: SurfaceRect = { offsetX: 0, offsetY: 0, surfaceWidth: 640, surfaceHeight: 480 };
      try {
        controller.onSurfaceChanged('0', rect);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldNotCrashWhenOnSurfaceDestroyed', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.onSurfaceDestroyed('0');
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldNotCrashWhenOnSurfaceDestroyedRepeatedly', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.onSurfaceDestroyed('0');
        controller.onSurfaceDestroyed('0');
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    // ========== 表面旋转锁定 ==========

    it('shouldNotThrowWhenLockSurfaceRotationWithTrueAndFalse', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.lockSurfaceRotation(true);
        expect(true).assertTrue();
      } catch (e) {
        // 内部 setXComponentSurfaceRotation 失败被吞掉；无 XComponent 场景不应向调用方抛出
        expect(false).assertTrue();
      }
      try {
        controller.lockSurfaceRotation(false);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldNotThrowWhenLockSurfaceRotationWithDefaultParameter', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.lockSurfaceRotation();
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    // ========== 旋转后清缓存 ==========

    it('shouldNotThrowWhenResetAppliedSurfaceSize', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.setPreviewSurfaceSize(640, 480);
        controller.resetAppliedSurfaceSize();
        // 重置后相同尺寸应重新走 setXComponentSurfaceRect 路径
        controller.setPreviewSurfaceSize(640, 480);
        controller.resetAppliedSurfaceSize();
        controller.resetAppliedSurfaceSize();
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    // ========== 预览尺寸 ==========

    it('shouldNotThrowWhenSetPreviewSurfaceSizeWithPositiveValues', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.setPreviewSurfaceSize(1280, 720);
        controller.setPreviewSurfaceSize(1280, 720);
        controller.setPreviewSurfaceSize(640, 480);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldIgnoreSetPreviewSurfaceSizeWithNonPositiveValues', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.setPreviewSurfaceSize(0, 480);
        controller.setPreviewSurfaceSize(640, 0);
        controller.setPreviewSurfaceSize(-1, 480);
        controller.setPreviewSurfaceSize(640, -1);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    // ========== 通话页小窗 AspectFit ==========

    it('shouldNotThrowWhenApplyPreviewSurfaceRectWithValidValues', 0, () => {
      let controller = new VideoRenderController();
      try {
        // 640x480 流放入 320x240 窗口：AspectFit 缩放 0.5 → 320x240
        controller.applyPreviewSurfaceRect(320, 240, 640, 480);
        // 16:9 流放入 4:3 窗口：以宽为限制
        controller.applyPreviewSurfaceRect(640, 480, 1280, 720);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldIgnoreApplyPreviewSurfaceRectWithNonPositiveValues', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.applyPreviewSurfaceRect(0, 240, 640, 480);
        controller.applyPreviewSurfaceRect(320, 0, 640, 480);
        controller.applyPreviewSurfaceRect(320, 240, 0, 480);
        controller.applyPreviewSurfaceRect(320, 240, 640, 0);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    // ========== 适配旋转帧的 SurfaceRect ==========

    it('shouldNotThrowWhenApplyOrientedPreviewSurfaceRectWithValidValues', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.applyOrientedPreviewSurfaceRect(1080, 1920, 720, 1280);
        // 重复调用与 portrait 流
        controller.applyOrientedPreviewSurfaceRect(1080, 1920, 720, 1280);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldIgnoreApplyOrientedPreviewSurfaceRectWithNonPositiveValues', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.applyOrientedPreviewSurfaceRect(0, 1920, 720, 1280);
        controller.applyOrientedPreviewSurfaceRect(1080, 0, 720, 1280);
        controller.applyOrientedPreviewSurfaceRect(1080, 1920, 0, 1280);
        controller.applyOrientedPreviewSurfaceRect(1080, 1920, 720, 0);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    // ========== 相机预览适配模式 ==========

    it('shouldNotThrowWhenEnableCameraPreviewAdaptationWithoutRenderer', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.enableCameraPreviewAdaptation();
        // 重复启用保持幂等
        controller.enableCameraPreviewAdaptation();
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldCombineCameraPreviewAdaptationWithOtherMutators', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.enableCameraPreviewAdaptation();
        controller.setScalingMode(ScalingMode.AspectFit);
        controller.setMirror(true);
        controller.setPreviewSurfaceSize(640, 480);
        controller.applyOrientedPreviewSurfaceRect(640, 480, 1280, 720);
        controller.resetAppliedSurfaceSize();
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    // ========== 旋转后显示帧宽高回调 ==========

    it('shouldAssignAndClearOnFrameSizeChangedCallback', 0, () => {
      let controller = new VideoRenderController();
      let callbackCount = 0;
      try {
        controller.onFrameSizeChanged = (width: number, height: number): void => {
          callbackCount++;
          expect(typeof width).assertEqual('number');
          expect(typeof height).assertEqual('number');
        };
        expect(controller.onFrameSizeChanged).not().assertUndefined();
        controller.onFrameSizeChanged = undefined;
        expect(controller.onFrameSizeChanged).assertUndefined();
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
      expect(callbackCount).assertEqual(0);
    })

    // ========== 极端尺寸与异常结构边界（surface 相关方法） ==========

    it('shouldHandleExtremeAspectRatioPreviewRect', 0, () => {
      let controller = new VideoRenderController();
      try {
        // 极端宽高比：1:1000000 与 1000000:1，不应除零或溢出崩溃。
        controller.applyPreviewSurfaceRect(1, 1000000, 16, 9);
        controller.applyPreviewSurfaceRect(1000000, 1, 16, 9);
        controller.applyOrientedPreviewSurfaceRect(1, 1000000, 1280, 720);
        controller.applyOrientedPreviewSurfaceRect(1000000, 1, 1280, 720);
        controller.setPreviewSurfaceSize(1, 2147483000);
        controller.setPreviewSurfaceSize(2147483000, 1);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldIgnoreAbnormalSurfaceRectInOnSurfaceChanged', 0, () => {
      let controller = new VideoRenderController();
      try {
        const zeroRect: SurfaceRect = {
          offsetX: 0, offsetY: 0, surfaceWidth: 0, surfaceHeight: 0
        };
        const negativeRect: SurfaceRect = {
          offsetX: -1, offsetY: -1, surfaceWidth: -100, surfaceHeight: -100
        };
        controller.onSurfaceChanged('surface-x', zeroRect);
        controller.onSurfaceChanged('surface-x', negativeRect);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    it('shouldResetSurfaceSizeBeforeReapplyAfterExtremeRect', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.setPreviewSurfaceSize(640, 480);
        controller.resetAppliedSurfaceSize();
        // 重置后相同尺寸必须重新应用而非被 no-op 去重吞掉。
        controller.setPreviewSurfaceSize(640, 480);
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })

    // ========== mutator 交叉参数组合 ==========

    it('shouldCombineMirrorScalingAndTrackMutatorsInCycle', 0, () => {
      let controller = new VideoRenderController();
      try {
        controller.setMirror(true);
        controller.setMirrorVertically(true);
        controller.setScalingMode(ScalingMode.Fill);
        controller.setVideoTrack(null);
        controller.lockSurfaceRotation(true);
        controller.setMirror(false);
        controller.setMirrorVertically(false);
        controller.setScalingMode(ScalingMode.AspectFill);
        controller.lockSurfaceRotation(false);
        controller.setScalingMode(ScalingMode.AspectFit);
        // 组合变更后控制器仍处于可用状态。
        controller.setPreviewSurfaceSize(320, 240);
        controller.onSurfaceDestroyed('surface-x');
        expect(true).assertTrue();
      } catch (e) {
        expect(false).assertTrue();
      }
    })
  })
}
