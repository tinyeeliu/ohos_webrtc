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

import webrtc, { Logging, LoggingSeverity, VideoRenderController, ScalingMode } from '@ohos/webrtc';
import abilityAccessCtrl, { PermissionRequestResult } from '@ohos.abilityAccessCtrl';
import promptAction from '@ohos.promptAction';
import { BusinessError } from '@kit.BasicServicesKit';

const TAG: string = '[ApplyConstraintsTest]';

@Entry
@Component
struct ApplyConstraintsTestPage {
  @State statusText: string = '等待测试...';
  @State testResults: string[] = [];

  // 分辨率选择
  @State selectedWidth: number = 1280;
  @State selectedHeight: number = 720;

  // 帧率
  @State videoFrameRate: string = '30';

  // facingMode
  @State facingMode: string = 'user';

  // zoom
  @State currentZoom: number = 1.0;
  @State zoomInput: string = '2.0';
  @State zoomMin: number = 1.0;
  @State zoomMax: number = 6.0;
  @State zoomSupported: boolean = true;
  private zoomAtGestureStart: number = 1.0;


  private pcf?: webrtc.PeerConnectionFactory;
  private localVideoSource?: webrtc.VideoSource;
  private localVideoTrack?: webrtc.MediaStreamTrack;
  private mXComponentController: VideoRenderController = new VideoRenderController();

  aboutToAppear(): void {
    Logging.enableLogThreads();
    Logging.enableLogTimeStamps();
    Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);

    abilityAccessCtrl.createAtManager()
      .requestPermissionsFromUser(getContext(), ['ohos.permission.CAMERA', 'ohos.permission.MICROPHONE'])
      .then(async (result: PermissionRequestResult) => {
        let granted = true;
        for (let index = 0; index < result.permissions.length; index++) {
          if (result.authResults[index] != abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
            granted = false;
          }
        }

        if (!granted) {
          this.setStatus("权限未授予");
          return;
        }

        this.createPeerConnectionFactory();
      });
  }

  aboutToDisappear(): void {
    this.localVideoTrack?.stop();
    this.localVideoSource?.release();
    this.pcf = undefined;
  }

  createPeerConnectionFactory() {
    this.pcf = new webrtc.PeerConnectionFactory({});
    webrtc.PeerConnectionFactory.setDefault(this.pcf);
  }

  setStatus(text: string) {
    console.info(TAG, text);
    this.statusText = text;
  }

  addResult(result: string) {
    console.info(TAG, result);
    this.testResults.push(result);
  }

  async prepareVideoTrack() {
    if (!this.localVideoTrack) {
      this.localVideoSource = this.pcf?.createVideoSource({ facingMode: 'environment' });
      this.localVideoTrack = this.pcf?.createVideoTrack("video", this.localVideoSource);
      if (this.localVideoTrack) {
        this.mXComponentController.setVideoTrack(this.localVideoTrack);
        this.mXComponentController.setScalingMode(ScalingMode.AspectFit);
        this.queryZoomRange();
      }
    }
  }

  queryZoomRange() {
    try {
      let range = this.localVideoTrack?.getZoomRange();
      if (range) {
        this.zoomMin = range.min;
        this.zoomMax = range.max;
        this.zoomSupported = range.supported;
        console.info(TAG, `【mhg】 queryZoomRange: supported=${range.supported}, min=${range.min}, max=${range.max}`);
        this.setStatus(`zoom 范围: ${range.min.toFixed(1)}x ~ ${range.max.toFixed(1)}x ${range.supported ? '(支持)' : '(不支持)'}`);
      }
    } catch (e) {
      console.info(TAG, `【mhg】 queryZoomRange: FAILED - ${e}`);
    }
  }

  // 测试1: 视频约束 resolve
  async testVideoConstraintsResolve() {
    this.setStatus('测试: 视频约束 resolve');
    await this.prepareVideoTrack();

    const frameRate = parseInt(this.videoFrameRate, 10);

    try {
      await this.localVideoTrack!.applyConstraints({
        width: this.selectedWidth,
        height: this.selectedHeight,
        frameRate: frameRate
      });
      this.addResult(`✓ testVideoConstraintsResolve(${this.selectedWidth}x${this.selectedHeight}@${frameRate}): 通过 (Promise resolved)`);
    } catch (e) {
      this.addResult(`✗ testVideoConstraintsResolve(${this.selectedWidth}x${this.selectedHeight}@${frameRate}): 失败 - ${e}`);
    }
  }

  // 测试2: exact 约束 resolve
  async testVideoExactConstraint() {
    this.setStatus('测试: exact 约束');
    await this.prepareVideoTrack();

    const frameRate = parseInt(this.videoFrameRate, 10);

    try {
      await this.localVideoTrack!.applyConstraints({
        width: { exact: this.selectedWidth },
        height: { exact: this.selectedHeight },
        frameRate: { exact: frameRate }
      });
      this.addResult(`✓ testVideoExactConstraint(${this.selectedWidth}x${this.selectedHeight}@${frameRate}): 通过 (Promise resolved)`);
    } catch (e) {
      this.addResult(`✗ testVideoExactConstraint(${this.selectedWidth}x${this.selectedHeight}@${frameRate}): 失败 - ${e}`);
    }
  }

  // 测试3: ideal 约束 resolve
  async testVideoIdealConstraint() {
    this.setStatus('测试: ideal 约束');
    await this.prepareVideoTrack();

    const frameRate = parseInt(this.videoFrameRate, 10);

    try {
      await this.localVideoTrack!.applyConstraints({
        width: { ideal: this.selectedWidth },
        height: { ideal: this.selectedHeight },
        frameRate: { ideal: frameRate }
      });
      this.addResult(`✓ testVideoIdealConstraint(${this.selectedWidth}x${this.selectedHeight}@${frameRate}): 通过 (Promise resolved)`);
    } catch (e) {
      this.addResult(`✗ testVideoIdealConstraint(${this.selectedWidth}x${this.selectedHeight}@${frameRate}): 失败 - ${e}`);
    }
  }

  // 测试5: facingMode 切换
  async testFacingModeSwitch() {
    this.setStatus('测试: facingMode 切换');
    console.info(TAG, `[testFacingModeSwitch] 开始测试, target facingMode=${this.facingMode}`);
    await this.prepareVideoTrack();

    // 记录切换前的状态
    console.info(TAG, `[testFacingModeSwitch] 切换前 facingMode=${this.facingMode}`);

    try {
      console.info(TAG, `[testFacingModeSwitch] 调用 applyConstraints({ facingMode: '${this.facingMode}' })`);
      await this.localVideoTrack!.applyConstraints({
        facingMode: this.facingMode
      });

      // 记录切换后的状态
      console.info(TAG, `[testFacingModeSwitch] 切换完成`);

      this.addResult(`✓ testFacingModeSwitch(facingMode=${this.facingMode}): 通过 (Promise resolved)`);
    } catch (e) {
      console.error(TAG, `[testFacingModeSwitch] 失败: ${e}`);
      this.addResult(`✗ testFacingModeSwitch(facingMode=${this.facingMode}): 失败 - ${e}`);
    }
  }

  // 测试6: zoom 变焦
  async testVideoZoom() {
    this.setStatus('测试: zoom 变焦');
    await this.prepareVideoTrack();

    const zoom = parseFloat(this.zoomInput);
    if (isNaN(zoom) || zoom <= 0) {
      this.addResult(`✗ testVideoZoom: 无效的 zoom 值 (${this.zoomInput})`);
      return;
    }

    try {
      console.info(TAG, `【mhg】 testVideoZoom: calling applyConstraints({ zoom: ${zoom} })`);
      await this.localVideoTrack!.applyConstraints({
        zoom: zoom
      });
      this.currentZoom = zoom;
      console.info(TAG, `【mhg】 testVideoZoom: SUCCESS zoom=${zoom}`);
      this.addResult(`✓ testVideoZoom(zoom=${zoom}): 通过 (Promise resolved)`);
    } catch (e) {
      console.info(TAG, `【mhg】 testVideoZoom: FAILED - ${e}`);
      this.addResult(`✗ testVideoZoom(zoom=${zoom}): 失败 - ${e}`);
    }
  }

  // 测试7: zoom 超界（测试 C++ clamp 保护）
  async testVideoZoomOutOfRange() {
    this.setStatus('测试: zoom 超界 (应被 clamp，不崩溃)');
    await this.prepareVideoTrack();

    try {
      console.info(TAG, `【mhg】 testVideoZoomOutOfRange: calling applyConstraints({ zoom: 9999 })`);
      await this.localVideoTrack!.applyConstraints({
        zoom: 9999
      });
      console.info(TAG, `【mhg】 testVideoZoomOutOfRange: SUCCESS (C++ should have clamped)`);
      this.addResult(`✓ testVideoZoomOutOfRange(zoom=9999): 通过 (Promise resolved, C++ 应已 clamp)`);
    } catch (e) {
      console.info(TAG, `【mhg】 testVideoZoomOutOfRange: FAILED - ${e}`);
      this.addResult(`✗ testVideoZoomOutOfRange(zoom=9999): 失败 - ${e}`);
    }
  }

  // 重置 zoom 到 1.0
  async resetZoom() {
    this.setStatus('重置 zoom 到 1.0');
    await this.prepareVideoTrack();

    try {
      console.info(TAG, `【mhg】 resetZoom: calling applyConstraints({ zoom: 1.0 })`);
      await this.localVideoTrack!.applyConstraints({ zoom: 1.0 });
      this.currentZoom = 1.0;
      console.info(TAG, `【mhg】 resetZoom: SUCCESS`);
      this.addResult(`✓ resetZoom: 已重置到 1.0x`);
    } catch (e) {
      console.info(TAG, `【mhg】 resetZoom: FAILED - ${e}`);
      this.addResult(`✗ resetZoom: 失败 - ${e}`);
    }
  }

  build() {
    Scroll() {
      Column() {
        Text('applyConstraints 测试页面')
          .fontSize(24)
          .fontWeight(FontWeight.Bold)
          .margin({ bottom: 16 })

        Text(`状态: ${this.statusText}`)
          .fontSize(16)
          .fontColor(0x007DFF)
          .margin({ bottom: 12 })

        XComponent({
          id: "test-video",
          type: XComponentType.SURFACE,
          controller: this.mXComponentController
        })
          .width('100%')
          .height(200)
          .margin({ bottom: 16 })
          .gesture(
            PinchGesture({ fingers: 2 })
              .onActionStart(() => {
                this.zoomAtGestureStart = this.currentZoom;
                console.info(TAG, `【mhg】 [PinchZoom] start, base zoom=${this.zoomAtGestureStart}`);
              })
              .onActionUpdate((event: GestureEvent) => {
                let target = this.zoomAtGestureStart * event.scale;
                let prevZoom = this.currentZoom;
                this.currentZoom = Math.max(this.zoomMin, Math.min(target, this.zoomMax));
                if (target > this.zoomMax && prevZoom < this.zoomMax) {
                  promptAction.showToast({ message: `已达到最大变焦 ${this.zoomMax.toFixed(1)}x` });
                } else if (target < this.zoomMin && prevZoom > this.zoomMin) {
                  promptAction.showToast({ message: `已达到最小变焦 ${this.zoomMin.toFixed(1)}x` });
                }
                console.info(TAG, `【mhg】 [PinchZoom] update, scale=${event.scale}, target=${target.toFixed(2)}, clamped=${this.currentZoom.toFixed(2)}`);
                this.localVideoTrack?.applyConstraints({
                  zoom: this.currentZoom
                });
              })
              .onActionEnd(() => {
                console.info(TAG, `【mhg】 [PinchZoom] end, final zoom=${this.currentZoom}`);
              })
          )

        // 分辨率选择 (Radio单选)
        Text('分辨率选择')
          .fontSize(18)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 8 })

        Row() {
          Radio({ value: '1280x720', group: 'resolution' })
            .checked(this.selectedWidth === 1280 && this.selectedHeight === 720)
            .onChange((isChecked: boolean) => {
              if (isChecked) {
                this.selectedWidth = 1280;
                this.selectedHeight = 720;
              }
            })
          Text('1280x720').fontSize(14).margin({ right: 16 })

          Radio({ value: '1920x1080', group: 'resolution' })
            .checked(this.selectedWidth === 1920 && this.selectedHeight === 1080)
            .onChange((isChecked: boolean) => {
              if (isChecked) {
                this.selectedWidth = 1920;
                this.selectedHeight = 1080;
              }
            })
          Text('1920x1080').fontSize(14).margin({ right: 16 })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Row() {
          Radio({ value: '2560x1440', group: 'resolution' })
            .checked(this.selectedWidth === 2560 && this.selectedHeight === 1440)
            .onChange((isChecked: boolean) => {
              if (isChecked) {
                this.selectedWidth = 2560;
                this.selectedHeight = 1440;
              }
            })
          Text('2560x1440').fontSize(14).margin({ right: 16 })

          Radio({ value: '3840x2160', group: 'resolution' })
            .checked(this.selectedWidth === 3840 && this.selectedHeight === 2160)
            .onChange((isChecked: boolean) => {
              if (isChecked) {
                this.selectedWidth = 3840;
                this.selectedHeight = 2160;
              }
            })
          Text('3840x2160').fontSize(14)
        }
        .width('100%')
        .margin({ bottom: 16 })

        // 帧率输入
        Row() {
          Text('帧率:')
            .fontSize(16)
            .width(50)

          TextInput({ text: this.videoFrameRate, placeholder: '帧率' })
            .width(80)
            .height(44)
            .fontSize(14)
            .type(InputType.Number)
            .onChange((value: string) => { this.videoFrameRate = value; })
        }
        .width('100%')
        .margin({ bottom: 16 })

        // 测试1: 视频约束
        Button('测试1: 视频约束')
          .width('100%')
          .height(44)
          .fontSize(16)
          .onClick(() => { this.testVideoConstraintsResolve(); })
          .margin({ bottom: 16 })

        // 测试2: exact 约束
        Button('测试2: exact 约束')
          .width('100%')
          .height(44)
          .fontSize(16)
          .onClick(() => { this.testVideoExactConstraint(); })
          .margin({ bottom: 16 })

        // 测试3: ideal 约束
        Button('测试3: ideal 约束')
          .width('100%')
          .height(44)
          .fontSize(16)
          .onClick(() => { this.testVideoIdealConstraint(); })
          .margin({ bottom: 16 })

        // 测试5: facingMode 切换
        Row() {
          Text('facingMode:')
            .fontSize(16)
            .layoutWeight(1)

          Radio({ value: 'user', group: 'facingMode' })
            .checked(this.facingMode === 'user')
            .onChange((isChecked: boolean) => {
              if (isChecked) {
                this.facingMode = 'user';
              }
            })
          Text('user').fontSize(14).margin({ right: 16 })

          Radio({ value: 'environment', group: 'facingMode' })
            .checked(this.facingMode === 'environment')
            .onChange((isChecked: boolean) => {
              if (isChecked) {
                this.facingMode = 'environment';
              }
            })
          Text('environment').fontSize(14)
        }
        .width('100%')
        .margin({ bottom: 8 })

        Button('测试5: facingMode 切换')
          .width('100%')
          .height(44)
          .fontSize(16)
          .onClick(() => { this.testFacingModeSwitch(); })
          .margin({ bottom: 16 })

        // ===== zoom 变焦测试 =====
        Text('变焦 (Zoom) 测试')
          .fontSize(18)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 8 })

        Text(`当前 zoom: ${this.currentZoom.toFixed(2)}x  (范围 ${this.zoomMin.toFixed(1)}x ~ ${this.zoomMax.toFixed(1)}x)`)
          .fontSize(16)
          .fontColor(0x007DFF)
          .margin({ bottom: 8 })

        Row() {
          Text('zoom:')
            .fontSize(16)
            .width(60)

          TextInput({ text: this.zoomInput, placeholder: 'zoom 值' })
            .width(100)
            .height(44)
            .fontSize(14)
            .type(InputType.Number)
            .onChange((value: string) => { this.zoomInput = value; })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Button('测试6: 设置 zoom')
          .width('100%')
          .height(44)
          .fontSize(16)
          .onClick(() => { this.testVideoZoom(); })
          .margin({ bottom: 8 })

        Button('测试7: zoom 超界 (9999)')
          .width('100%')
          .height(44)
          .fontSize(16)
          .onClick(() => { this.testVideoZoomOutOfRange(); })
          .margin({ bottom: 8 })

        Button('重置 zoom (1.0x)')
          .width('100%')
          .height(44)
          .fontSize(16)
          .onClick(() => { this.resetZoom(); })
          .margin({ bottom: 8 })

        Text('提示: 默认使用后置摄像头。双指捏合实时变焦，已自动限制在设备支持范围内（达到极限会提示）。若 zoom 不生效，查看日志中 "SetZoomRatio" 的 clamp 范围。')
          .fontSize(12)
          .fontColor(0x999999)
          .margin({ bottom: 16 })

        // 测试结果
        Text('测试结果:')
          .fontSize(18)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 8 })

        ForEach(this.testResults, (item: string, index: number) => {
          Text(item)
            .fontSize(14)
            .fontColor(item.startsWith('✓') ? 0x00AA00 : 0xFF0000)
            .margin({ bottom: 4 })
            .alignSelf(ItemAlign.Start)
        }, (item: string, index: number) => index.toString())
      }
      .width('100%')
      .padding(16)
      .alignItems(HorizontalAlign.Center)
    }
    .width('100%')
    .height('100%')
    .scrollBar(BarState.Auto)
    .scrollable(ScrollDirection.Vertical)
  }
}
