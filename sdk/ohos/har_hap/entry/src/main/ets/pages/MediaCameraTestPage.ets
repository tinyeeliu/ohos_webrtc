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

import webrtc, { Logging, LoggingSeverity, VideoRenderController, ScalingMode } from '@ohos/webrtc';
import abilityAccessCtrl, { PermissionRequestResult } from '@ohos.abilityAccessCtrl';
import { CollapsibleResults } from './common/CollapsibleResults';
import promptAction from '@ohos.promptAction';
import image from '@ohos.multimedia.image';

const TAG: string = '[MediaCameraTest]';

// 值域见 ohos.multimedia.camera.FocusMode / ExposureMode
const FOCUS_MODE_MANUAL: number = 0;
const FOCUS_MODE_CONTINUOUS_AUTO: number = 1;
const FOCUS_MODE_AUTO: number = 2;
const FOCUS_MODE_LOCKED: number = 3;
const EXPOSURE_MODE_LOCKED: number = 0;
const EXPOSURE_MODE_AUTO: number = 1;
const FOCUS_POINT_MIN: number = 0.0;
const FOCUS_POINT_MAX: number = 1.0;
const INVALID_FOCUS_MODE_DEMO: number = 999;
// I420: 每像素 1.5 字节(Y 全平面 + 两个 1/4 尺寸色度平面)
const I420_BYTES_PER_PIXEL: number = 3 / 2;
// 抓帧画面显示下采样步长(原帧尺寸较大, 步长 2 即 1/4 像素量)
const DISPLAY_DOWNSAMPLE_STEP: number = 2;
const SHARPNESS_SAMPLE_STEP: number = 2;
const LUMA_SAMPLE_STEP: number = 4;
// 场景点位: 近景目标置于画面中心, 远景目标置于画面中上部
const FOCUS_SCENE_NEAR_X: number = 0.5;
const FOCUS_SCENE_NEAR_Y: number = 0.5;
const FOCUS_SCENE_FAR_X: number = 0.5;
const FOCUS_SCENE_FAR_Y: number = 0.18;
const FOCUS_SCENE_RADIUS_RATIO: number = 0.25;
const FOCUS_SETTLE_MS: number = 2000;
const EXPOSURE_SETTLE_MS: number = 1500;
// 判定余量: 清晰度需高出 10% 以上、亮度差需超过 8(0~255 尺度) 才算观察到效果
const SHARPNESS_PASS_MARGIN: number = 1.1;
const LUMA_PASS_DELTA: number = 8;
const INTENSITY_MAX: number = 255;

@Entry
@Component
struct MediaCameraTestPage {
  @State statusText: string = '等待权限...';
  @State capResults: string[] = [];
  @State torchResults: string[] = [];
  @State focusResults: string[] = [];
  @State captureResults: string[] = [];
  @State capabilityText: string[] = [];
  @State torchSupported: boolean = false;
  @State torchOn: boolean = false;
  @State selectedFocusMode: number = FOCUS_MODE_CONTINUOUS_AUTO;
  @State previewWidth: number = 1;
  @State previewHeight: number = 1;
  @State frameInfo: string = '尚未抓帧';
  @State capturedCount: number = 0;
  @State captureImage: image.PixelMap | undefined = undefined;
  @State focusShotA: image.PixelMap | undefined = undefined;
  @State focusShotB: image.PixelMap | undefined = undefined;
  @State focusSharpA: number = -1;
  @State focusSharpB: number = -1;
  @State focusSceneVerdict: string = '待抓取近景/远景两帧';
  @State exposureShotA: image.PixelMap | undefined = undefined;
  @State exposureShotB: image.PixelMap | undefined = undefined;
  @State exposureLumaA: number = -1;
  @State exposureLumaB: number = -1;
  @State exposureSceneVerdict: string = '待抓取锁定/自动两帧';

  private pcf?: webrtc.PeerConnectionFactory;
  private videoSource?: webrtc.VideoSource;
  private videoTrack?: webrtc.MediaStreamTrack;
  private audioSource?: webrtc.AudioSource;
  private audioTrack?: webrtc.AudioTrack;
  private mXComponentController: VideoRenderController = new VideoRenderController();

  aboutToAppear(): void {
    Logging.enableLogThreads();
    Logging.enableLogTimeStamps();
    Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);

    abilityAccessCtrl.createAtManager()
      .requestPermissionsFromUser(getContext(), ['ohos.permission.CAMERA', 'ohos.permission.MICROPHONE'])
      .then(async (result: PermissionRequestResult) => {
        let cameraGranted = false;
        let micGranted = false;
        for (let index = 0; index < result.permissions.length; index++) {
          if (result.permissions[index] === 'ohos.permission.CAMERA' &&
            result.authResults[index] === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
            cameraGranted = true;
          }
          if (result.permissions[index] === 'ohos.permission.MICROPHONE' &&
            result.authResults[index] === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
            micGranted = true;
          }
        }
        if (!cameraGranted) {
          this.setStatus('相机权限未授予');
          return;
        }
        this.setupCamera();
        this.queryCameraCapabilities();
        if (micGranted) {
          // 供音频轨异常场景使用
          this.audioSource = this.pcf!.createAudioSource();
          this.audioTrack = this.pcf!.createAudioTrack('audio', this.audioSource);
        }
      });
  }

  aboutToDisappear(): void {
    this.videoTrack?.stop();
    this.videoSource?.release();
    this.audioSource?.release();
    this.pcf = undefined;
  }

  setStatus(text: string) {
    console.info(TAG, text);
    this.statusText = text;
  }

  pushResult(list: string[], result: string) {
    console.info(TAG, result);
    list.push(result);
  }

  sleep(ms: number): Promise<void> {
    return new Promise<void>((resolve: () => void) => {
      setTimeout(resolve, ms);
    });
  }

  clamp255(value: number): number {
    return Math.min(INTENSITY_MAX, Math.max(0, value));
  }

  // ===== I420 -> RGBA8888: 下采样后逐像素转换, 供页面显示 =====
  i420ToRgba(frame: webrtc.CapturedVideoFrame, outW: number, outH: number): ArrayBuffer {
    const width = frame.width;
    const src = new Uint8Array(frame.data);
    const rgba = new Uint8Array(outW * outH * 4);
    const uOffset = width * frame.height;
    const vOffset = uOffset + (width >> 1) * (frame.height >> 1);
    const uvStride = width >> 1;
    let dst = 0;
    for (let y = 0; y < outH; y++) {
      const sy = y * DISPLAY_DOWNSAMPLE_STEP;
      for (let x = 0; x < outW; x++) {
        const sx = x * DISPLAY_DOWNSAMPLE_STEP;
        const luma = src[sy * width + sx];
        const uvIndex = (sy >> 1) * uvStride + (sx >> 1);
        const u = src[uOffset + uvIndex] - 128;
        const v = src[vOffset + uvIndex] - 128;
        rgba[dst] = this.clamp255(luma + 1.402 * v);
        rgba[dst + 1] = this.clamp255(luma - 0.344 * u - 0.714 * v);
        rgba[dst + 2] = this.clamp255(luma + 1.772 * u);
        rgba[dst + 3] = INTENSITY_MAX;
        dst += 4;
      }
    }
    return rgba.buffer;
  }

  async frameToPixelMap(frame: webrtc.CapturedVideoFrame): Promise<image.PixelMap> {
    const outW = Math.max(1, Math.floor(frame.width / DISPLAY_DOWNSAMPLE_STEP));
    const outH = Math.max(1, Math.floor(frame.height / DISPLAY_DOWNSAMPLE_STEP));
    const options: image.InitializationOptions = {
      size: { width: outW, height: outH },
      srcPixelFormat: image.PixelMapFormat.RGBA_8888,
      pixelFormat: image.PixelMapFormat.RGBA_8888
    };
    const pixelMap = await image.createPixelMap(this.i420ToRgba(frame, outW, outH), options);
    if (frame.rotation !== 0) {
      await pixelMap.rotate(frame.rotation);
    }
    return pixelMap;
  }

  // ===== 中心区域清晰度: 拉普拉斯响应方差, 越大越清晰; 中心点旋转后仍为中心, 无需坐标映射 =====
  centerSharpness(frame: webrtc.CapturedVideoFrame): number {
    const width = frame.width;
    const height = frame.height;
    const src = new Uint8Array(frame.data);
    const rx = Math.max(8, Math.floor(width * FOCUS_SCENE_RADIUS_RATIO));
    const ry = Math.max(8, Math.floor(height * FOCUS_SCENE_RADIUS_RATIO));
    const x0 = Math.max(1, (width >> 1) - rx);
    const x1 = Math.min(width - 2, (width >> 1) + rx);
    const y0 = Math.max(1, (height >> 1) - ry);
    const y1 = Math.min(height - 2, (height >> 1) + ry);
    let sum = 0;
    let sumSq = 0;
    let count = 0;
    for (let y = y0; y <= y1; y += SHARPNESS_SAMPLE_STEP) {
      for (let x = x0; x <= x1; x += SHARPNESS_SAMPLE_STEP) {
        const i = y * width + x;
        const lap = 4 * src[i] - src[i - 1] - src[i + 1] - src[i - width] - src[i + width];
        sum += lap;
        sumSq += lap * lap;
        count++;
      }
    }
    if (count === 0) {
      return 0;
    }
    const mean = sum / count;
    return sumSq / count - mean * mean;
  }

  // ===== 全帧平均亮度: Y 均值(0~255), 曝光效果的量化指标 =====
  frameMeanLuma(frame: webrtc.CapturedVideoFrame): number {
    const width = frame.width;
    const src = new Uint8Array(frame.data);
    let sum = 0;
    let count = 0;
    for (let y = 0; y < frame.height; y += LUMA_SAMPLE_STEP) {
      for (let x = 0; x < width; x += LUMA_SAMPLE_STEP) {
        sum += src[y * width + x];
        count++;
      }
    }
    return count === 0 ? 0 : sum / count;
  }

  setupCamera() {
    this.pcf = new webrtc.PeerConnectionFactory({});
    this.videoSource = this.pcf.createVideoSource({ facingMode: 'environment' });
    this.videoTrack = this.pcf.createVideoTrack('video', this.videoSource);
    this.mXComponentController.setVideoTrack(this.videoTrack);
    this.mXComponentController.setScalingMode(ScalingMode.AspectFit);
    this.torchSupported = this.videoSource.hasTorch();
    this.setStatus(`相机就绪, 补光灯: ${this.torchSupported ? '支持' : '不支持'}`);
  }

  // ===== getCapabilities: 摄像头硬件能力区间 =====
  async queryCameraCapabilities() {
    this.setStatus('查询摄像头能力');
    this.capabilityText = [];
    try {
      const mediaDevices = new webrtc.MediaDevices();
      const devices = await mediaDevices.enumerateDevices();
      const cameras = devices.filter(d => d.kind === 'videoinput');
      this.pushResult(this.capResults, `✓ 枚举到 ${cameras.length} 个 videoinput 设备`);
      for (const camera of cameras) {
        const caps = camera.getCapabilities?.();
        if (caps) {
          const width = caps.width ? `${caps.width.min}~${caps.width.max}` : '未知';
          const height = caps.height ? `${caps.height.min}~${caps.height.max}` : '未知';
          const frameRate = caps.frameRate ? `${caps.frameRate.min}~${caps.frameRate.max}` : '未知';
          this.capabilityText.push(`${camera.label || camera.deviceId}: ${width} x ${height} @ ${frameRate}fps`);
        } else {
          this.capabilityText.push(`${camera.label || camera.deviceId}: 无能力信息`);
        }
      }
    } catch (e) {
      this.pushResult(this.capResults, `✗ 查询失败 - ${e}`);
    }
  }

  // ===== torch: 补光灯, 画面亮度直接可见 =====
  toggleTorch() {
    if (!this.videoSource) {
      return;
    }
    if (!this.torchSupported) {
      this.pushResult(this.torchResults, '✗ 本设备摄像头不带补光灯');
      return;
    }
    try {
      const next = !this.torchOn;
      this.videoSource.setTorch(next);
      this.torchOn = next;
      this.setStatus(`补光灯已${next ? '开' : '关'}`);
      this.pushResult(this.torchResults, `✓ setTorch(${next})`);
    } catch (e) {
      this.pushResult(this.torchResults, `✗ setTorch 失败 - ${e}`);
    }
  }

  // ===== 对焦/测光: 点按预览画面即对焦该位置 =====
  tapToFocus(x: number, y: number) {
    if (!this.videoSource || this.previewWidth <= 0 || this.previewHeight <= 0) {
      return;
    }
    const nx = Math.min(FOCUS_POINT_MAX, Math.max(FOCUS_POINT_MIN, x / this.previewWidth));
    const ny = Math.min(FOCUS_POINT_MAX, Math.max(FOCUS_POINT_MIN, y / this.previewHeight));
    try {
      this.videoSource.setFocusPoint(nx, ny);
      this.videoSource.setFocusMode(this.selectedFocusMode);
      this.setStatus(`点按对焦 (${nx.toFixed(2)}, ${ny.toFixed(2)})`);
      this.pushResult(this.focusResults, `✓ setFocusPoint(${nx.toFixed(2)}, ${ny.toFixed(2)})` +
        ` + setFocusMode(${this.selectedFocusMode})`);
    } catch (e) {
      this.pushResult(this.focusResults, `✗ 点按对焦失败 - ${e}`);
    }
  }

  applyFocusMode() {
    try {
      this.videoSource?.setFocusMode(this.selectedFocusMode);
      this.pushResult(this.focusResults, `✓ setFocusMode(${this.selectedFocusMode})`);
    } catch (e) {
      this.pushResult(this.focusResults, `✗ setFocusMode(${this.selectedFocusMode}) 失败 - ${e}`);
    }
  }

  invalidFocusModeDemo() {
    try {
      this.videoSource?.setFocusMode(INVALID_FOCUS_MODE_DEMO);
      this.pushResult(this.focusResults, '✗ setFocusMode(999) 未按预期抛错');
    } catch (e) {
      promptAction.showToast({ message: `非法 focusMode 已被拒绝: ${e}` });
      this.pushResult(this.focusResults, `✓ setFocusMode(999) 抛错 - ${e}`);
    }
  }

  // ===== captureFrame: 抓帧并直接显示抓取画面 =====
  async captureFromVideoTrack() {
    this.setStatus('抓取一帧');
    try {
      const frame = await this.videoTrack!.captureFrame();
      this.capturedCount++;
      const expected = frame.width * frame.height * I420_BYTES_PER_PIXEL;
      const sizeOk = frame.data.byteLength === expected;
      this.frameInfo = `第${this.capturedCount}帧: ${frame.width}x${frame.height} rotation=${frame.rotation} ` +
        `${frame.data.byteLength}字节(I420) ${sizeOk ? '✓' : '✗ 应为' + expected}`;
      this.captureImage = await this.frameToPixelMap(frame);
      this.pushResult(this.captureResults, `✓ captureFrame: ${frame.width}x${frame.height} rotation=${frame.rotation}` +
        ` bytes=${frame.data.byteLength} 画面已显示`);
    } catch (e) {
      this.pushResult(this.captureResults, `✗ captureFrame 失败 - ${e}`);
    }
  }

  // ===== 对焦效果场景: 近景帧中心区域应比远景帧清晰 =====
  async runFocusScene(focusNear: boolean) {
    if (!this.videoSource || !this.videoTrack) {
      this.setStatus('相机未就绪');
      return;
    }
    this.setStatus(focusNear ? '对焦近景目标并抓帧...' : '对焦远景目标并抓帧...');
    try {
      this.videoSource.setFocusMode(FOCUS_MODE_AUTO);
      this.videoSource.setFocusPoint(
        focusNear ? FOCUS_SCENE_NEAR_X : FOCUS_SCENE_FAR_X,
        focusNear ? FOCUS_SCENE_NEAR_Y : FOCUS_SCENE_FAR_Y);
      await this.sleep(FOCUS_SETTLE_MS);
      const frame = await this.videoTrack.captureFrame();
      const shot = await this.frameToPixelMap(frame);
      const sharpness = this.centerSharpness(frame);
      if (focusNear) {
        this.focusShotA = shot;
        this.focusSharpA = sharpness;
      } else {
        this.focusShotB = shot;
        this.focusSharpB = sharpness;
      }
      this.pushResult(this.focusResults,
        `✓ 抓帧(${focusNear ? '近景' : '远景'}): 中心区域清晰度 ${sharpness.toFixed(0)}`);
      this.evaluateFocusScene();
    } catch (e) {
      this.pushResult(this.focusResults, `✗ 对焦场景抓帧失败 - ${e}`);
    }
  }

  evaluateFocusScene() {
    if (this.focusSharpA < 0 || this.focusSharpB < 0) {
      this.focusSceneVerdict = '待抓取近景/远景两帧';
      return;
    }
    const a = this.focusSharpA.toFixed(0);
    const b = this.focusSharpB.toFixed(0);
    if (this.focusSharpA > this.focusSharpB * SHARPNESS_PASS_MARGIN) {
      this.focusSceneVerdict = `✓ 对焦生效: 近景帧中心清晰度 ${a} 明显高于远景帧 ${b}`;
      this.pushResult(this.focusResults, `✓ 对焦效果判定通过: ${a} > ${b}`);
    } else {
      this.focusSceneVerdict = `✗ 未见对焦效果差异: 近景 ${a} 未明显高于远景 ${b}, 检查近景目标是否在画面中心`;
      this.pushResult(this.focusResults, `✗ 对焦效果判定未通过: ${a} 未明显高于 ${b}`);
    }
  }

  // 锁定须在遮挡前完成: 锁住亮光下的曝光参数, 遮挡后不补偿才会变暗
  lockExposureForScene() {
    if (!this.videoSource) {
      this.setStatus('相机未就绪');
      return;
    }
    try {
      this.videoSource.setExposureMode(EXPOSURE_MODE_LOCKED);
      this.setStatus('已在当前光照下锁定曝光');
      this.pushResult(this.focusResults, '✓ 亮光下曝光已锁定, 请遮住镜头后抓帧 A');
    } catch (e) {
      this.pushResult(this.focusResults, `✗ 锁定曝光失败 - ${e}`);
    }
  }

  // ===== 曝光效果场景: 遮挡后锁定档应保持暗, 自动档应被拉亮 =====
  async runExposureScene(locked: boolean) {
    if (!this.videoSource || !this.videoTrack) {
      this.setStatus('相机未就绪');
      return;
    }
    this.setStatus(locked ? '锁定档下抓帧...' : '切换自动档并抓帧...');
    try {
      if (!locked) {
        this.videoSource.setExposureMode(EXPOSURE_MODE_AUTO);
        await this.sleep(EXPOSURE_SETTLE_MS);
      }
      const frame = await this.videoTrack.captureFrame();
      const shot = await this.frameToPixelMap(frame);
      const luma = this.frameMeanLuma(frame);
      if (locked) {
        this.exposureShotA = shot;
        this.exposureLumaA = luma;
      } else {
        this.exposureShotB = shot;
        this.exposureLumaB = luma;
      }
      this.pushResult(this.focusResults,
        `✓ 抓帧(${locked ? '锁定' : '自动'}): 平均亮度 ${luma.toFixed(1)}`);
      this.evaluateExposureScene();
    } catch (e) {
      this.pushResult(this.focusResults, `✗ 曝光场景抓帧失败 - ${e}`);
    }
  }

  evaluateExposureScene() {
    if (this.exposureLumaA < 0 || this.exposureLumaB < 0) {
      this.exposureSceneVerdict = '待抓取锁定/自动两帧';
      return;
    }
    const a = this.exposureLumaA.toFixed(1);
    const b = this.exposureLumaB.toFixed(1);
    if (this.exposureLumaB - this.exposureLumaA > LUMA_PASS_DELTA) {
      this.exposureSceneVerdict = `✓ 曝光生效: 自动档亮度 ${b} 明显高于锁定档 ${a}(遮挡下自动补偿)`;
      this.pushResult(this.focusResults, `✓ 曝光效果判定通过: 自动 ${b} > 锁定 ${a}`);
    } else {
      this.exposureSceneVerdict = `✗ 未见曝光效果差异: 锁定 ${a} 与自动 ${b} 接近, 确认抓帧时遮挡了镜头且设备支持该曝光模式`;
      this.pushResult(this.focusResults, `✗ 曝光效果判定未通过: ${b} 与 ${a} 无明显差异`);
    }
  }

  // 异常场景: 音频轨应拒绝
  async captureFromAudioTrack() {
    if (!this.audioTrack) {
      this.pushResult(this.captureResults, '✗ 麦克风权限未授予, 无法创建音频轨演示');
      return;
    }
    this.setStatus('音频轨抓帧 (应拒绝)');
    try {
      await this.audioTrack.captureFrame();
      this.pushResult(this.captureResults, '✗ 音频轨 captureFrame 未按预期拒绝');
    } catch (e) {
      this.pushResult(this.captureResults, `✓ 音频轨 captureFrame 已拒绝 - ${e}`);
    }
  }

  // 异常场景: 已停止轨道应拒绝
  async captureFromStoppedTrack() {
    this.setStatus('已停轨抓帧 (应拒绝)');
    const source = this.pcf!.createVideoSource({ facingMode: 'environment' });
    const track = this.pcf!.createVideoTrack('video2', source);
    track.stop();
    try {
      await track.captureFrame();
      this.pushResult(this.captureResults, '✗ 已停轨 captureFrame 未按预期拒绝');
    } catch (e) {
      this.pushResult(this.captureResults, `✓ 已停轨 captureFrame 已拒绝 - ${e}`);
    }
    source.release();
  }

  build() {
    Scroll() {
      Column() {
        Text('相机功能测试')
          .fontSize(22)
          .fontWeight(FontWeight.Bold)
          .margin({ bottom: 12 })

        Text(`状态: ${this.statusText}`)
          .fontSize(15)
          .fontColor(0x007DFF)
          .margin({ bottom: 12 })

        XComponent({
          id: 'media-camera-preview',
          type: XComponentType.SURFACE,
          controller: this.mXComponentController
        })
          .width('100%')
          .height(240)
          .margin({ bottom: 6 })
          .onAreaChange((oldArea: Area, newArea: Area) => {
            this.previewWidth = Number(newArea.width);
            this.previewHeight = Number(newArea.height);
          })
          .onClick((event: ClickEvent) => {
            this.tapToFocus(event.x, event.y);
          })

        Text('提示: 点击预览画面对该位置对焦; 开补光灯后观察画面亮度变化')
          .fontSize(12)
          .fontColor(0x999999)
          .margin({ bottom: 12 })

        Text('摄像头能力 (进入页面自动展示)')
          .fontSize(16)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 6 })

        CollapsibleResults({ items: this.capResults })

        CollapsibleResults({ items: this.capabilityText, maxVisible: 5 })

        Button(`补光灯 ${this.torchOn ? '关' : '开'} (${this.torchSupported ? '支持' : '不支持'})`)
          .width('100%')
          .height(44)
          .fontSize(15)
          .enabled(this.torchSupported)
          .onClick(() => { this.toggleTorch(); })
          .margin({ top: 8, bottom: 8 })

        CollapsibleResults({ items: this.torchResults })

        Text('对焦效果场景验证')
          .fontSize(16)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 6 })

        Text('场景: 把一张印有文字的纸放在近处并对准画面中心(占满取景框中部)。先按①对焦近景抓帧, 再按②对焦远处背景抓帧, 对比两帧中心区域清晰度。')
          .fontSize(12)
          .fontColor(0x999999)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 8 })

        Row() {
          Button('① 对焦近景并抓帧')
            .height(40)
            .fontSize(14)
            .layoutWeight(1)
            .onClick(() => { this.runFocusScene(true); })
          Button('② 对焦远景并抓帧')
            .height(40)
            .fontSize(14)
            .layoutWeight(1)
            .margin({ left: 8 })
            .onClick(() => { this.runFocusScene(false); })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Row() {
          Column() {
            Text('A 近景对焦').fontSize(12).fontColor(0x666666)
            if (this.focusShotA) {
              Image(this.focusShotA!)
                .width('100%')
                .height(110)
                .objectFit(ImageFit.Contain)
                .border({ width: 1, color: 0xCCCCCC })
            } else {
              Text('未抓帧').fontSize(12).fontColor(0x999999).height(110)
            }
            Text(this.focusSharpA < 0 ? '清晰度 -' : `清晰度 ${this.focusSharpA.toFixed(0)}`)
              .fontSize(12)
              .fontColor(0x333333)
          }
          .width('48%')

          Column() {
            Text('B 远景对焦').fontSize(12).fontColor(0x666666)
            if (this.focusShotB) {
              Image(this.focusShotB!)
                .width('100%')
                .height(110)
                .objectFit(ImageFit.Contain)
                .border({ width: 1, color: 0xCCCCCC })
            } else {
              Text('未抓帧').fontSize(12).fontColor(0x999999).height(110)
            }
            Text(this.focusSharpB < 0 ? '清晰度 -' : `清晰度 ${this.focusSharpB.toFixed(0)}`)
              .fontSize(12)
              .fontColor(0x333333)
          }
          .width('48%')
        }
        .width('100%')
        .justifyContent(FlexAlign.SpaceBetween)
        .margin({ bottom: 6 })

        Text(this.focusSceneVerdict)
          .fontSize(13)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 12 })

        Text('曝光效果场景验证')
          .fontSize(16)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 6 })

        Text('场景: 在亮光下按①锁定曝光 → 用手遮住镜头(画面应变暗) → 按②抓帧 A → 保持遮挡按③切自动, 画面应被自动拉亮。')
          .fontSize(12)
          .fontColor(0x999999)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 8 })

        Button('① 亮光下锁定曝光')
          .width('100%')
          .height(40)
          .fontSize(14)
          .onClick(() => { this.lockExposureForScene(); })
          .margin({ bottom: 8 })

        Row() {
          Button('② 遮住镜头后抓帧 A')
            .height(40)
            .fontSize(14)
            .layoutWeight(1)
            .onClick(() => { this.runExposureScene(true); })
          Button('③ 保持遮挡切自动抓帧 B')
            .height(40)
            .fontSize(13)
            .layoutWeight(1)
            .margin({ left: 8 })
            .onClick(() => { this.runExposureScene(false); })
        }
        .width('100%')
        .margin({ bottom: 8 })

        Row() {
          Column() {
            Text('A 曝光锁定').fontSize(12).fontColor(0x666666)
            if (this.exposureShotA) {
              Image(this.exposureShotA!)
                .width('100%')
                .height(110)
                .objectFit(ImageFit.Contain)
                .border({ width: 1, color: 0xCCCCCC })
            } else {
              Text('未抓帧').fontSize(12).fontColor(0x999999).height(110)
            }
            Text(this.exposureLumaA < 0 ? '亮度 -' : `亮度 ${this.exposureLumaA.toFixed(1)}`)
              .fontSize(12)
              .fontColor(0x333333)
          }
          .width('48%')

          Column() {
            Text('B 曝光自动').fontSize(12).fontColor(0x666666)
            if (this.exposureShotB) {
              Image(this.exposureShotB!)
                .width('100%')
                .height(110)
                .objectFit(ImageFit.Contain)
                .border({ width: 1, color: 0xCCCCCC })
            } else {
              Text('未抓帧').fontSize(12).fontColor(0x999999).height(110)
            }
            Text(this.exposureLumaB < 0 ? '亮度 -' : `亮度 ${this.exposureLumaB.toFixed(1)}`)
              .fontSize(12)
              .fontColor(0x333333)
          }
          .width('48%')
        }
        .width('100%')
        .justifyContent(FlexAlign.SpaceBetween)
        .margin({ bottom: 6 })

        Text(this.exposureSceneVerdict)
          .fontSize(13)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 12 })

        Text('对焦模式切换与异常分支')
          .fontSize(16)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 6 })

        Row() {
          Radio({ value: 'manual', group: 'focus' })
            .checked(this.selectedFocusMode === FOCUS_MODE_MANUAL)
            .onChange((checked: boolean) => { if (checked) { this.selectedFocusMode = FOCUS_MODE_MANUAL; } })
          Text('手动').fontSize(13).margin({ right: 10 })
          Radio({ value: 'continuous', group: 'focus' })
            .checked(this.selectedFocusMode === FOCUS_MODE_CONTINUOUS_AUTO)
            .onChange((checked: boolean) => { if (checked) { this.selectedFocusMode = FOCUS_MODE_CONTINUOUS_AUTO; } })
          Text('连续').fontSize(13).margin({ right: 10 })
          Radio({ value: 'auto', group: 'focus' })
            .checked(this.selectedFocusMode === FOCUS_MODE_AUTO)
            .onChange((checked: boolean) => { if (checked) { this.selectedFocusMode = FOCUS_MODE_AUTO; } })
          Text('单次').fontSize(13).margin({ right: 10 })
          Radio({ value: 'locked', group: 'focus' })
            .checked(this.selectedFocusMode === FOCUS_MODE_LOCKED)
            .onChange((checked: boolean) => { if (checked) { this.selectedFocusMode = FOCUS_MODE_LOCKED; } })
          Text('锁定').fontSize(13)
        }
        .width('100%')
        .margin({ bottom: 8 })

        Row() {
          Button('应用对焦模式')
            .height(40)
            .fontSize(14)
            .layoutWeight(1)
            .onClick(() => { this.applyFocusMode(); })
          Button('非法值演示(999)应拒绝')
            .height(40)
            .fontSize(13)
            .layoutWeight(1)
            .margin({ left: 8 })
            .onClick(() => { this.invalidFocusModeDemo(); })
        }
        .width('100%')
        .margin({ bottom: 8 })

        CollapsibleResults({ items: this.focusResults })

        Divider().margin({ bottom: 8 })

        Text('抓帧 (captureFrame)')
          .fontSize(16)
          .fontWeight(FontWeight.Bold)
          .alignSelf(ItemAlign.Start)
          .margin({ bottom: 6 })

        Text(this.frameInfo)
          .fontSize(15)
          .fontColor(0x333333)
          .margin({ bottom: 8 })

        Button('抓取一帧（视频轨道）')
          .width('100%')
          .height(40)
          .fontSize(14)
          .onClick(() => { this.captureFromVideoTrack(); })
          .margin({ bottom: 8 })

        if (this.captureImage) {
          Image(this.captureImage!)
            .width('100%')
            .height(180)
            .objectFit(ImageFit.Contain)
            .border({ width: 1, color: 0xCCCCCC })
            .margin({ bottom: 8 })
        }

        Row() {
          Button('音频轨道抓帧(应拒绝)')
            .height(40)
            .fontSize(13)
            .layoutWeight(1)
            .onClick(() => { this.captureFromAudioTrack(); })
          Button('已停止轨道抓帧(应拒绝)')
            .height(40)
            .fontSize(13)
            .layoutWeight(1)
            .margin({ left: 8 })
            .onClick(() => { this.captureFromStoppedTrack(); })
        }
        .width('100%')
        .margin({ bottom: 8 })

        CollapsibleResults({ items: this.captureResults })

        Text('判定口径: 抓帧后页面直接显示抓到的画面(已按 rotation 转正), 帧信息行 ✓ 表示字节数=w*h*3/2 校验通过; 对焦场景看两帧中心区域清晰度(A 应明显高于 B); 曝光场景按①锁定→②遮挡抓帧A→③自动抓帧B, B 亮度应明显高于 A。')
          .fontSize(12)
          .fontColor(0x999999)
          .margin({ bottom: 16 })
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
