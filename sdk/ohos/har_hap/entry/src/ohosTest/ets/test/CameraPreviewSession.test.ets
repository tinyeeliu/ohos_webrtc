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

import { describe, beforeAll, afterEach, it, expect } from '@ohos/hypium'
import webrtc, { Logging, LoggingSeverity, VideoRenderController, ScalingMode,
  CameraPreviewSession, CameraPreviewOptions, CameraPreviewStatus } from '@ohos/webrtc';
import abilityDelegatorRegistry from '@ohos.app.ability.abilityDelegatorRegistry';
import common from '@ohos.app.ability.common';
import window from '@ohos.window';
import { sleep } from './Helper';

const TAG: string = '[CameraPreviewSessionTest]';

const pcf = new webrtc.PeerConnectionFactory();

// CameraPreviewSession 构造器先做 options 校验（非法值直接抛 Error），
// 之后才调用 camera.getCameraManager(context)，因此非法 options 用例不依赖真实 context。
const PLACEHOLDER_CONTEXT = {} as common.Context;

/** 惰性获取测试 Ability 的应用级 Context；无法获取（如异常环境）时返回 undefined。 */
function getTestContext(): common.Context | undefined {
  try {
    const delegator = abilityDelegatorRegistry.getAbilityDelegator();
    return delegator.getAppContext();
  } catch (e) {
    console.warn(TAG, 'getAppContext threw: ' + JSON.stringify(e));
    return undefined;
  }
}

/** 直接构造：options 非法时抛出库内 Error（校验先于 getCameraManager，不触达 context）。 */
function constructSession(context: common.Context,
  options?: CameraPreviewOptions): CameraPreviewSession {
  const controller = new VideoRenderController();
  return new CameraPreviewSession(context, pcf, controller, options);
}

/** 优雅降级构造：环境缺少相机服务（getCameraManager 抛错）时返回 undefined。 */
function tryCreateSession(context: common.Context,
  options?: CameraPreviewOptions): CameraPreviewSession | undefined {
  try {
    return constructSession(context, options);
  } catch (e) {
    console.warn(TAG, 'CameraPreviewSession construction threw: ' + JSON.stringify(e));
    return undefined;
  }
}

function assertInvalidOptionsError(e: Object): void {
  const err = e as Error;
  expect(err instanceof Error).assertTrue();
  expect(err.message).assertContain('Invalid camera preview options');
}

function assertStatusShape(status: CameraPreviewStatus): void {
  expect(typeof status.state).assertEqual('string');
  expect(typeof status.cameraId).assertEqual('string');
  expect(typeof status.front).assertEqual('boolean');
  expect(typeof status.error).assertEqual('string');
}

export default function CameraPreviewSessionTest() {
  describe('CameraPreviewSessionTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    afterEach(async () => {
      await sleep(300);
    })

    // ========== CameraPreviewOptions 全字段构造 ==========

    it('shouldConstructWithAllOptionsFields', 0, () => {
      const context = getTestContext();
      const session = context ? tryCreateSession(context, {
        width: 640,
        height: 480,
        front: true,
        deviceId: '',
        allowFallback: false,
        autoRecover: false,
        maxRetries: 5,
        scalingMode: ScalingMode.Fill,
        frameStallTimeoutMs: 2000
      }) : undefined;
      if (!session) {
        console.warn(TAG, 'camera service unavailable, skip deep assertions');
        return;
      }
      expect(session instanceof CameraPreviewSession).assertTrue();
      expect(session.track).assertUndefined();
      session.release();
    })

    it('shouldConstructWithDefaultOptions', 0, () => {
      const context = getTestContext();
      const session = context ? tryCreateSession(context) : undefined;
      if (!session) {
        console.warn(TAG, 'camera service unavailable, skip deep assertions');
        return;
      }
      expect(session instanceof CameraPreviewSession).assertTrue();
      expect(session.track).assertUndefined();
      session.release();
    })

    it('shouldConstructWithBoundaryFrameStallTimeoutMsValues', 0, () => {
      const context = getTestContext();
      if (!context) {
        console.warn(TAG, 'test context unavailable, skip');
        return;
      }
      const boundaries: number[] = [0, 1000, 60000];
      for (const frameStallTimeoutMs of boundaries) {
        const session = tryCreateSession(context, { frameStallTimeoutMs: frameStallTimeoutMs });
        expect(session !== undefined).assertTrue();
        session?.release();
      }
    })

    it('shouldConstructWithBoundaryMaxRetriesZero', 0, () => {
      const context = getTestContext();
      if (!context) {
        console.warn(TAG, 'test context unavailable, skip');
        return;
      }
      const session = tryCreateSession(context, { maxRetries: 0, autoRecover: true });
      expect(session !== undefined).assertTrue();
      session?.release();
    })

    // ========== 无效 options 校验（校验先于 getCameraManager，无需真实 context）==========

    it('shouldThrowWhenConstructWithZeroWidth', 0, () => {
      try {
        constructSession(PLACEHOLDER_CONTEXT, { width: 0, height: 480 });
        expect().assertFail();
      } catch (e) {
        assertInvalidOptionsError(e as Object);
      }
    })

    it('shouldThrowWhenConstructWithNegativeHeight', 0, () => {
      try {
        constructSession(PLACEHOLDER_CONTEXT, { width: 640, height: -1 });
        expect().assertFail();
      } catch (e) {
        assertInvalidOptionsError(e as Object);
      }
    })

    it('shouldThrowWhenConstructWithFractionalWidth', 0, () => {
      try {
        constructSession(PLACEHOLDER_CONTEXT, { width: 1.5, height: 480 });
        expect().assertFail();
      } catch (e) {
        assertInvalidOptionsError(e as Object);
      }
    })

    it('shouldThrowWhenConstructWithFractionalHeight', 0, () => {
      try {
        constructSession(PLACEHOLDER_CONTEXT, { width: 640, height: 2.5 });
        expect().assertFail();
      } catch (e) {
        assertInvalidOptionsError(e as Object);
      }
    })

    it('shouldThrowWhenConstructWithNegativeMaxRetries', 0, () => {
      try {
        constructSession(PLACEHOLDER_CONTEXT, { maxRetries: -1 });
        expect().assertFail();
      } catch (e) {
        assertInvalidOptionsError(e as Object);
      }
    })

    it('shouldThrowWhenConstructWithFractionalMaxRetries', 0, () => {
      try {
        constructSession(PLACEHOLDER_CONTEXT, { maxRetries: 1.5 });
        expect().assertFail();
      } catch (e) {
        assertInvalidOptionsError(e as Object);
      }
    })

    it('shouldThrowWhenConstructWithLowFrameStallTimeoutMs', 0, () => {
      try {
        constructSession(PLACEHOLDER_CONTEXT, { frameStallTimeoutMs: 999 });
        expect().assertFail();
      } catch (e) {
        assertInvalidOptionsError(e as Object);
      }
    })

    it('shouldThrowWhenConstructWithHighFrameStallTimeoutMs', 0, () => {
      try {
        constructSession(PLACEHOLDER_CONTEXT, { frameStallTimeoutMs: 61001 });
        expect().assertFail();
      } catch (e) {
        assertInvalidOptionsError(e as Object);
      }
    })

    // ========== onStatusChanged 'suspended' 状态强断言 ==========

    it('shouldEmitSuspendedStatusWhenSetActiveWithoutWindow', 0, () => {
      const context = getTestContext();
      if (!context) {
        console.warn(TAG, 'test context unavailable, skip');
        return;
      }
      const session = tryCreateSession(context,
        { allowFallback: false, autoRecover: false, maxRetries: 0 });
      if (!session) {
        console.warn(TAG, 'camera service unavailable, skip deep assertions');
        return;
      }
      const received: CameraPreviewStatus[] = [];
      session.onStatusChanged = (status: CameraPreviewStatus): void => {
        received.push(status);
      };
      // 未 attachWindow 时 visible=false，setActive(true) → reconcile 同步 emit 'suspended'
      session.setActive(true);
      expect(received.length).assertLarger(0);
      assertStatusShape(received[0]);
      expect(received[0].state).assertEqual('suspended');
      expect(received[0].cameraId).assertEqual('');
      expect(received[0].front).assertFalse();
      expect(received[0].error).assertEqual('');
      session.release();
    })

    it('shouldEmitSuspendedOnlyOnChangeWhenSetActiveToggled', 0, () => {
      const context = getTestContext();
      if (!context) {
        console.warn(TAG, 'test context unavailable, skip');
        return;
      }
      const session = tryCreateSession(context);
      if (!session) {
        console.warn(TAG, 'camera service unavailable, skip deep assertions');
        return;
      }
      const received: CameraPreviewStatus[] = [];
      session.onStatusChanged = (status: CameraPreviewStatus): void => {
        received.push(status);
      };
      session.setActive(true);
      // 同值 setActive 不重复触发
      session.setActive(true);
      const countAfterDuplicate = received.length;
      session.setActive(false);
      const countAfterDeactivate = received.length;
      expect(countAfterDuplicate).assertEqual(1);
      expect(countAfterDeactivate).assertEqual(2);
      assertStatusShape(received[0]);
      expect(received[0].state).assertEqual('suspended');
      assertStatusShape(received[1]);
      expect(received[1].state).assertEqual('suspended');
      session.release();
    })

    // ========== onTrackChanged 与 track getter ==========

    it('shouldSupportTrackGetterAndOnTrackChangedCallbacks', 0, () => {
      const context = getTestContext();
      if (!context) {
        console.warn(TAG, 'test context unavailable, skip');
        return;
      }
      const session = tryCreateSession(context);
      if (!session) {
        console.warn(TAG, 'camera service unavailable, skip deep assertions');
        return;
      }
      expect(session.track).assertUndefined();
      let trackChangedCount = 0;
      session.onTrackChanged = (track: webrtc.MediaStreamTrack | undefined): void => {
        trackChangedCount++;
        expect(track).assertUndefined();
      };
      // 未打开过采集时 release 不触发 onTrackChanged（仅已有 track 时回调）
      session.release();
      expect(trackChangedCount).assertEqual(0);
      // release 后回调被清空
      expect(session.onStatusChanged).assertUndefined();
      expect(session.onTrackChanged).assertUndefined();
    })

    it('shouldClearCallbacksAndBeIdempotentWhenRelease', 0, () => {
      const context = getTestContext();
      if (!context) {
        console.warn(TAG, 'test context unavailable, skip');
        return;
      }
      const session = tryCreateSession(context);
      if (!session) {
        console.warn(TAG, 'camera service unavailable, skip deep assertions');
        return;
      }
      session.onStatusChanged = (status: CameraPreviewStatus): void => {
        console.info(TAG, `onStatusChanged: state=${status.state} cameraId=${status.cameraId} ` +
          `front=${status.front} error=${status.error}`);
      };
      session.onTrackChanged = (track: webrtc.MediaStreamTrack | undefined): void => {
        console.info(TAG, 'onTrackChanged: ' + (track ? track.id : 'undefined'));
      };
      expect(session.onStatusChanged).not().assertUndefined();
      expect(session.onTrackChanged).not().assertUndefined();
      session.release();
      expect(session.onStatusChanged).assertUndefined();
      expect(session.onTrackChanged).assertUndefined();
      // release 幂等
      session.release();
      expect(session.onStatusChanged).assertUndefined();
      expect(session.onTrackChanged).assertUndefined();
    })

    // ========== attachWindow / setActive / setCameraPreference / retry 组合 ==========

    it('shouldAttachWindowAndLifecycleCombination', 0, async () => {
      const context = getTestContext();
      if (!context) {
        console.warn(TAG, 'test context unavailable, skip');
        return;
      }
      const session = tryCreateSession(context, {
        front: true,
        allowFallback: true,
        autoRecover: false,
        maxRetries: 1,
        scalingMode: ScalingMode.AspectFit
      });
      if (!session) {
        console.warn(TAG, 'camera service unavailable, skip deep assertions');
        return;
      }
      const received: CameraPreviewStatus[] = [];
      session.onStatusChanged = (status: CameraPreviewStatus): void => {
        received.push(status);
      };
      session.setCameraPreference(true, '');
      session.setCameraPreference(false, '');
      session.setCameraPreference(true, 'nonexistent-camera-id');
      session.retry();

      let hostWindow: window.Window | undefined = undefined;
      try {
        hostWindow = await window.getLastWindow(context);
      } catch (e) {
        console.warn(TAG, 'getLastWindow threw: ' + JSON.stringify(e));
      }
      if (hostWindow) {
        session.attachWindow(hostWindow);
        expect(session.track).assertUndefined();
      }
      session.setActive(true);
      // 立即去激活，取消 300ms 后的 openCapture 定时器，保持用例确定性
      session.setActive(false);
      session.release();
      expect(received.length).assertLarger(0);
      for (const status of received) {
        assertStatusShape(status);
      }
      expect(received[0].state).assertEqual('suspended');
    })

    it('shouldSetCameraPreferenceWithAllParameterCombinations', 0, () => {
      const context = getTestContext();
      if (!context) {
        console.warn(TAG, 'test context unavailable, skip');
        return;
      }
      const session = tryCreateSession(context);
      if (!session) {
        console.warn(TAG, 'camera service unavailable, skip deep assertions');
        return;
      }
      const received: CameraPreviewStatus[] = [];
      session.onStatusChanged = (status: CameraPreviewStatus): void => {
        received.push(status);
      };
      // (front=true, deviceId 省略)、(front=true, deviceId 非空)、(front=false, deviceId 省略)、重复同值幂等
      session.setCameraPreference(true);
      session.setCameraPreference(true, 'id-0');
      session.setCameraPreference(false);
      session.setCameraPreference(false);
      session.retry();
      // 均未 attachWindow/激活，不应产生任何状态回调
      expect(received.length).assertEqual(0);
      expect(session.track).assertUndefined();
      session.release();
    })

    it('shouldIgnoreSetActiveAfterRelease', 0, () => {
      const context = getTestContext();
      if (!context) {
        console.warn(TAG, 'test context unavailable, skip');
        return;
      }
      const session = tryCreateSession(context);
      if (!session) {
        console.warn(TAG, 'camera service unavailable, skip deep assertions');
        return;
      }
      const received: CameraPreviewStatus[] = [];
      session.onStatusChanged = (status: CameraPreviewStatus): void => {
        received.push(status);
      };
      session.release();
      expect(received.length).assertEqual(0);
      session.setActive(true);
      session.setCameraPreference(true);
      session.retry();
      expect(received.length).assertEqual(0);
    })

    it('shouldReceiveWellFormedStatusOnEveryEmission', 0, () => {
      const context = getTestContext();
      if (!context) {
        console.warn(TAG, 'test context unavailable, skip');
        return;
      }
      const session = tryCreateSession(context, {
        autoRecover: true,
        maxRetries: 2,
        allowFallback: true
      });
      if (!session) {
        console.warn(TAG, 'camera service unavailable, skip deep assertions');
        return;
      }
      const received: CameraPreviewStatus[] = [];
      session.onStatusChanged = (status: CameraPreviewStatus): void => {
        received.push(status);
      };
      session.setActive(true);
      session.setActive(false);
      expect(received.length).assertLarger(0);
      for (const status of received) {
        assertStatusShape(status);
      }
      session.release();
    })
  })
}
