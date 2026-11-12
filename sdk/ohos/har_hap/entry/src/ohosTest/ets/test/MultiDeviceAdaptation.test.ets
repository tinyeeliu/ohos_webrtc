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

/** 测试类型： integration（集成测试） */

import { describe, beforeAll, it, expect } from '@ohos/hypium'
import webrtc, { Logging, LoggingSeverity, VideoRenderController, ScalingMode,
  CameraPreviewSession, CameraPreviewOptions, CameraPreviewStatus } from '@ohos/webrtc';
import abilityDelegatorRegistry from '@ohos.app.ability.abilityDelegatorRegistry';
import common from '@ohos.app.ability.common';
import window from '@ohos.window';
import { sleep } from './Helper';

const TAG: string = '[MultiDeviceAdaptationTest]';

const pcf = new webrtc.PeerConnectionFactory();

function getTestContext(): common.Context | undefined {
  try {
    const delegator = abilityDelegatorRegistry.getAbilityDelegator();
    return delegator.getAppContext();
  } catch (e) {
    console.warn(TAG, 'getAppContext threw: ' + JSON.stringify(e));
    return undefined;
  }
}

/** 优雅降级构造：环境缺少相机服务时返回 undefined。 */
function tryCreateSession(context: common.Context,
  options?: CameraPreviewOptions): CameraPreviewSession | undefined {
  try {
    const controller = new VideoRenderController();
    return new CameraPreviewSession(context, pcf, controller, options);
  } catch (e) {
    console.warn(TAG, 'CameraPreviewSession construction threw: ' + JSON.stringify(e));
    return undefined;
  }
}

function assertStatusShape(status: CameraPreviewStatus): void {
  expect(typeof status.state).assertEqual('string');
  expect(typeof status.cameraId).assertEqual('string');
  expect(typeof status.front).assertEqual('boolean');
  expect(typeof status.error).assertEqual('string');
}

export default function MultiDeviceAdaptationTest() {
  describe('MultiDeviceAdaptationTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    // 多设备场景（对应 demo MultiDeviceRtcLabPage 的会话层流程自动化）：
    // 折叠/显示器迁移会触发相机重选与 window 重绑，这里自动化会话层迁移行为。

    it('shouldMigrateCameraPreferenceAcrossDevices', 0, () => {
      const context = getTestContext();
      if (!context) {
        console.warn(TAG, 'test context unavailable, skip');
        return;
      }
      const session = tryCreateSession(context, {
        front: true,
        autoRecover: false,
        maxRetries: 0,
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
      // 模拟多设备相机拓扑变化：前置 → 后置 → 指定不存在的设备ID → 回退。
      session.setCameraPreference(false, '');
      session.setCameraPreference(true, '');
      session.setCameraPreference(true, 'nonexistent-camera-id');
      session.retry();
      for (const status of received) {
        assertStatusShape(status);
      }
      expect(session.track).assertUndefined();
      session.release();
      expect(received.length).assertLarger(0);
    })

    it('shouldSurviveWindowRebindAndVisibilityCycle', 0, async () => {
      const context = getTestContext();
      if (!context) {
        console.warn(TAG, 'test context unavailable, skip');
        return;
      }
      const session = tryCreateSession(context, {
        front: true,
        allowFallback: true,
        autoRecover: false,
        maxRetries: 0
      });
      if (!session) {
        console.warn(TAG, 'camera service unavailable, skip deep assertions');
        return;
      }
      const received: CameraPreviewStatus[] = [];
      session.onStatusChanged = (status: CameraPreviewStatus): void => {
        received.push(status);
      };
      let hostWindow: window.Window | undefined = undefined;
      try {
        hostWindow = await window.getLastWindow(context);
      } catch (e) {
        console.warn(TAG, 'getLastWindow threw: ' + JSON.stringify(e));
      }
      if (hostWindow) {
        // 显示器迁移路径：重绑同一 window（幂等）→ 激活 → 去激活 → 重绑。
        session.attachWindow(hostWindow);
        session.attachWindow(hostWindow);
        session.setActive(true);
        await sleep(500);
        session.setActive(false);
        session.attachWindow(hostWindow);
      } else {
        // 无 window 环境：仅验证不可见状态下的激活/去激活不崩溃。
        session.setActive(true);
        session.setActive(false);
      }
      for (const status of received) {
        assertStatusShape(status);
      }
      session.release();
      // release 后回调被清空，状态机不得再投递事件。
      session.setActive(true);
      const countAfterRelease = received.length;
      await sleep(300);
      expect(received.length).assertEqual(countAfterRelease);
    })
  })
}
