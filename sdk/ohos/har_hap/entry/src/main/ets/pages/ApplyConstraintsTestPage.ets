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
          this.setStatus('权限未授予');
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
      this.localVideoSource = this.pcf?.createVideoSource({});
      this.localVideoTrack = this.pcf?.createVideoTrack('video', this.localVideoSource);
      if (this.localVideoTrack) {
        this.mXComponentController.setVideoTrack(this.localVideoTrack);
        this.mXComponentController.setScalingMode(ScalingMode.AspectFit);
      }
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
          id: 'test-video',
          type: XComponentType.SURFACE,
          controller: this.mXComponentController
        })
          .width('100%')
          .height(200)
          .margin({ bottom: 16 })

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
