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

  // 可修改的测试参数
  @State videoWidth: string = '640';
  @State videoHeight: string = '480';
  @State videoFrameRate: string = '30';
  @State exactWidth: string = '640';
  @State exactHeight: string = '480';
  @State exactFrameRate: string = '30';
  @State idealWidth: string = '640';
  @State idealHeight: string = '480';
  @State idealFrameRate: string = '30';
  @State invalidWidth: string = '-1';
  @State facingMode: string = 'user';
  @State echoCancellation: boolean = true;

  private pcf?: webrtc.PeerConnectionFactory;
  private localVideoSource?: webrtc.VideoSource;
  private localVideoTrack?: webrtc.MediaStreamTrack;
  private localAudioSource?: webrtc.AudioSource;
  private localAudioTrack?: webrtc.MediaStreamTrack;
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
    this.localAudioTrack?.stop();
    this.localAudioSource?.release();
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
      this.localVideoSource = this.pcf?.createVideoSource({ width: 1280, height: 720, facingMode: 'environment' }, false);
      this.localVideoTrack = this.pcf?.createVideoTrack("video", this.localVideoSource);
      if (this.localVideoTrack) {
        this.mXComponentController.setVideoTrack(this.localVideoTrack);
        this.mXComponentController.setScalingMode(ScalingMode.AspectFit);
      }
    }
  }

  async prepareAudioTrack() {
    if (!this.localAudioTrack) {
      this.localAudioSource = this.pcf?.createAudioSource({ echoCancellation: true, noiseSuppression: true });
      this.localAudioTrack = this.pcf?.createAudioTrack("audio", this.localAudioSource);
    }
  }

  // 测试1: 视频约束 resolve
  async testVideoConstraintsResolve() {
    this.setStatus('测试: 视频约束 resolve');
    await this.prepareVideoTrack();

    const width = parseInt(this.videoWidth, 10);
    const height = parseInt(this.videoHeight, 10);
    const frameRate = parseInt(this.videoFrameRate, 10);

    try {
      await this.localVideoTrack!.applyConstraints({
        width: width,
        height: height,
        frameRate: frameRate
      });
      this.addResult(`✓ testVideoConstraintsResolve(${width}x${height}@${frameRate}): 通过 (Promise resolved)`);
    } catch (e) {
      this.addResult(`✗ testVideoConstraintsResolve(${width}x${height}@${frameRate}): 失败 - ${e}`);
    }
  }

  // 测试2: exact 约束 resolve
  async testVideoExactConstraint() {
    this.setStatus('测试: exact 约束');
    await this.prepareVideoTrack();

    const width = parseInt(this.exactWidth, 10);
    const height = parseInt(this.exactHeight, 10);
    const frameRate = parseInt(this.exactFrameRate, 10);

    try {
      await this.localVideoTrack!.applyConstraints({
        width: { exact: width },
        height: { exact: height },
        frameRate: { exact: frameRate }
      });
      this.addResult(`✓ testVideoExactConstraint(${width}x${height}@${frameRate}): 通过 (Promise resolved)`);
    } catch (e) {
      this.addResult(`✗ testVideoExactConstraint(${width}x${height}@${frameRate}): 失败 - ${e}`);
    }
  }

  // 测试3: ideal 约束 resolve
  async testVideoIdealConstraint() {
    this.setStatus('测试: ideal 约束');
    await this.prepareVideoTrack();

    const width = parseInt(this.idealWidth, 10);
    const height = parseInt(this.idealHeight, 10);
    const frameRate = parseInt(this.idealFrameRate, 10);

    try {
      await this.localVideoTrack!.applyConstraints({
        width: { ideal: width },
        height: { ideal: height },
        frameRate: { ideal: frameRate }
      });
      this.addResult(`✓ testVideoIdealConstraint(${width}x${height}@${frameRate}): 通过 (Promise resolved)`);
    } catch (e) {
      this.addResult(`✗ testVideoIdealConstraint(${width}x${height}@${frameRate}): 失败 - ${e}`);
    }
  }

  // 测试4: 音频约束 resolve
  async testAudioConstraintsResolve() {
    this.setStatus('测试: 音频约束 resolve');
    await this.prepareAudioTrack();

    try {
      await this.localAudioTrack!.applyConstraints({
        echoCancellation: this.echoCancellation
      });
      this.addResult(`✓ testAudioConstraintsResolve(echoCancellation=${this.echoCancellation}): 通过 (Promise resolved)`);
    } catch (e) {
      this.addResult(`✗ testAudioConstraintsResolve: 失败 - ${e}`);
    }
  }

  // 测试5: facingMode 切换
  async testFacingModeSwitch() {
    this.setStatus('测试: facingMode 切换');
    await this.prepareVideoTrack();

    try {
      await this.localVideoTrack!.applyConstraints({
        facingMode: this.facingMode
      });
      this.addResult(`✓ testFacingModeSwitch(facingMode=${this.facingMode}): 通过 (Promise resolved)`);
    } catch (e) {
      this.addResult(`✗ testFacingModeSwitch(facingMode=${this.facingMode}): 失败 - ${e}`);
    }
  }

  // 测试6: 无效值约束
  async testInvalidConstraintValue() {
    this.setStatus('测试: 无效值约束');
    await this.prepareVideoTrack();

    const width = parseInt(this.invalidWidth, 10);

    try {
      await this.localVideoTrack!.applyConstraints({
        width: width
      });
      this.addResult(`✗ testInvalidConstraintValue(width=${width}): 失败 - 应该 reject 但 resolve 了`);
    } catch (e) {
      this.addResult(`✓ testInvalidConstraintValue(width=${width}): 通过 (Promise rejected)`);
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

        // 测试1: 视频约束
        Text('测试1: 视频约束 (基本数字值)')
          .fontSize(18)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 8 })

        Row() {
          TextInput({ text: this.videoWidth, placeholder: '宽' })
            .layoutWeight(1)
            .height(44)
            .fontSize(14)
            .type(InputType.Number)
            .onChange((value: string) => { this.videoWidth = value; })

          TextInput({ text: this.videoHeight, placeholder: '高' })
            .layoutWeight(1)
            .height(44)
            .fontSize(14)
            .type(InputType.Number)
            .margin({ left: 8 })
            .onChange((value: string) => { this.videoHeight = value; })

          TextInput({ text: this.videoFrameRate, placeholder: '帧率' })
            .layoutWeight(1)
            .height(44)
            .fontSize(14)
            .type(InputType.Number)
            .margin({ left: 8 })
            .onChange((value: string) => { this.videoFrameRate = value; })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Button('测试视频约束')
          .width('100%')
          .height(44)
          .fontSize(16)
          .onClick(() => { this.testVideoConstraintsResolve(); })
          .margin({ bottom: 16 })

        // 测试2: exact 约束
        Text('测试2: exact 约束')
          .fontSize(18)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 8 })

        Row() {
          TextInput({ text: this.exactWidth, placeholder: '宽' })
            .layoutWeight(1)
            .height(44)
            .fontSize(14)
            .type(InputType.Number)
            .onChange((value: string) => { this.exactWidth = value; })

          TextInput({ text: this.exactHeight, placeholder: '高' })
            .layoutWeight(1)
            .height(44)
            .fontSize(14)
            .type(InputType.Number)
            .margin({ left: 8 })
            .onChange((value: string) => { this.exactHeight = value; })

          TextInput({ text: this.exactFrameRate, placeholder: '帧率' })
            .layoutWeight(1)
            .height(44)
            .fontSize(14)
            .type(InputType.Number)
            .margin({ left: 8 })
            .onChange((value: string) => { this.exactFrameRate = value; })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Button('测试 exact 约束')
          .width('100%')
          .height(44)
          .fontSize(16)
          .onClick(() => { this.testVideoExactConstraint(); })
          .margin({ bottom: 16 })

        // 测试3: ideal 约束
        Text('测试3: ideal 约束')
          .fontSize(18)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 8 })

        Row() {
          TextInput({ text: this.idealWidth, placeholder: '宽' })
            .layoutWeight(1)
            .height(44)
            .fontSize(14)
            .type(InputType.Number)
            .onChange((value: string) => { this.idealWidth = value; })

          TextInput({ text: this.idealHeight, placeholder: '高' })
            .layoutWeight(1)
            .height(44)
            .fontSize(14)
            .type(InputType.Number)
            .margin({ left: 8 })
            .onChange((value: string) => { this.idealHeight = value; })

          TextInput({ text: this.idealFrameRate, placeholder: '帧率' })
            .layoutWeight(1)
            .height(44)
            .fontSize(14)
            .type(InputType.Number)
            .margin({ left: 8 })
            .onChange((value: string) => { this.idealFrameRate = value; })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Button('测试 ideal 约束')
          .width('100%')
          .height(44)
          .fontSize(16)
          .onClick(() => { this.testVideoIdealConstraint(); })
          .margin({ bottom: 16 })

        // 测试4: 音频约束
        Text('测试4: 音频约束')
          .fontSize(18)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 8 })

        Row() {
          Text('echoCancellation:')
            .fontSize(16)
            .layoutWeight(1)

          Toggle({ type: ToggleType.Switch, isOn: this.echoCancellation })
            .selectedColor(0x39a2db)
            .onChange((isOn: boolean) => {
              this.echoCancellation = isOn;
            })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Button('测试音频约束')
          .width('100%')
          .height(44)
          .fontSize(16)
          .onClick(() => { this.testAudioConstraintsResolve(); })
          .margin({ bottom: 16 })

        // 测试5: facingMode 切换
        Text('测试5: facingMode 切换 (切换前后摄像头)')
          .fontSize(18)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 8 })

        Row() {
          Text('facingMode:')
            .fontSize(16)
            .layoutWeight(1)

          Select([{ value: 'user' }, { value: 'environment' }])
            .selected(this.facingMode === 'user' ? 0 : 1)
            .value(this.facingMode)
            .font({ size: 16 })
            .onSelect((index: number, text: string) => {
              this.facingMode = text;
              console.info(TAG, 'facingMode changed to: ' + text);
            })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Button('测试 facingMode 切换')
          .width('100%')
          .height(44)
          .fontSize(16)
          .onClick(() => { this.testFacingModeSwitch(); })
          .margin({ bottom: 16 })

        // 测试6: 无效值约束
        Text('测试6: 无效值约束')
          .fontSize(18)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 8 })

        TextInput({ text: this.invalidWidth, placeholder: '无效宽度' })
          .width('100%')
          .height(44)
          .fontSize(14)
          .margin({ bottom: 8 })
          .onChange((value: string) => { this.invalidWidth = value; })

        Button('测试无效值约束')
          .width('100%')
          .height(44)
          .fontSize(16)
          .onClick(() => { this.testInvalidConstraintValue(); })
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
