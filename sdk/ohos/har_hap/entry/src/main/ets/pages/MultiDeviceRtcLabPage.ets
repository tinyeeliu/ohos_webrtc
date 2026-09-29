/**
 * Copyright (c) 2024 Archermind Technology (Nanjing) Co. Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import webrtc, { Logging, LoggingSeverity, VideoRenderController, ScalingMode, CameraPreviewSession, CameraPreviewStatus } from '@ohos/webrtc';
import abilityAccessCtrl, { PermissionRequestResult } from '@ohos.abilityAccessCtrl';
import router from '@ohos.router';
import display from '@ohos.display';
import window from '@ohos.window';
import deviceInfo from '@ohos.deviceInfo';
import preferences from '@ohos.data.preferences';
import { BusinessError } from '@ohos.base';
import { previewAspectRatio, computePreviewGeometry,
  canRotateCameraWindow } from './PreviewPolicy';
import { computeHoverLayout, resolveHoverPreviewSide, HoverGeometry, HoverRect } from './HoverLayout';
import { savePreviewPhoto, removePreviewPhoto } from './PreviewPhoto';
import { PreviewPeerCall } from './PreviewPeerCall';
import { PreviewOptionalWindowEvents, PreviewRotationInfo, PreviewRotationResult, PREVIEW_WILL_ROTATE,
  PREVIEW_RECT_SCREEN, tryPreviewPlatformAction, previewLiveCreases } from './PreviewPlatformCompat';

const TAG: string = '[MultiDeviceRtcLab]';
const STREAM_W: number = 1280;
const STREAM_H: number = 720;
const FEATURE_CODES: string[] = ['A', 'B', 'C', 'D', 'E', 'F'];
const FEATURE_NAMES: string[] = ['断点布局', '相机设备', '预览比例', '预览方向验证', '悬停页面', '本地与对端视频'];
const PREVIEW_RATIOS: string[] = ['全屏', '16:9', '4:3', '1:1'];

@Entry
@Component
struct MultiDeviceRtcLabPage {
  @State featureIndex: number = 0;
  @State telemetry: string = '等待相机...';
  @State breakpointName: string = 'sm';
  @State orientationPolicy: string = 'auto';
  @State autoRotationAllowed: boolean = false;
  private appliedOrientation: window.Orientation | undefined = undefined;
  @State useFront: boolean = false;
  @State deviceLines: string[] = [];
  private deviceListRequest: number = 0;
  @State isHalfFolded: boolean = false;
  @State isWideLayout: boolean = false;
  @State checkedA: boolean = false;
  @State checkedB: boolean = false;
  @State checkedC: boolean = false;
  @State checkedD: boolean = false;
  @State checkedE: boolean = false;
  @State checkedF: boolean = false;
  @State busy: boolean = false;
  @State previewRatio: string = '全屏';
  @State panelVisible: boolean = false;
  @State hoverPreviewSide: string = 'right';
  private hoverPreferences?: preferences.Preferences;
  private hoverPreferenceKey: string = '';
  @State hover: HoverGeometry = computeHoverLayout({ left: 0, top: 0, width: 1, height: 1 }, [], false, 0);
  private contentRect: HoverRect = { left: 0, top: 0, width: 0, height: 0 };
  private contentArea: Area | undefined = undefined;
  @State viewfinderWidth: number = 1;
  @State viewfinderHeight: number = 1;
  @State videoSurfaceWidth: number = 1;
  @State videoSurfaceHeight: number = 1;
  private frameWidth: number = STREAM_H;
  private frameHeight: number = STREAM_W;
  private cropWidthFraction: number = 1;
  private cropHeightFraction: number = 1;
  @State previewSizeText: string = '';
  @State previewReady: boolean = false;
  @State takingPhoto: boolean = false;
  @State photoUri: string = '';
  @State photoDetails: string = '';
  @State photoStatus: string = '';
  private photoPath: string = '';
  private pageActive: boolean = false;
  private pageVisible: boolean = false;
  private windowVisible: boolean = false;
  private previewSuspended: boolean = true;
  private foldStatus: number = 0;
  @State actualFront: boolean = false;
  @State selectedCameraId: string = '';
  private pcf?: webrtc.PeerConnectionFactory;
  private cameraSession?: CameraPreviewSession;
  private controller: VideoRenderController = new VideoRenderController();
  private remoteController: VideoRenderController = new VideoRenderController();
  private peerCall?: PreviewPeerCall;
  @State signalServer: string = 'wss://youzhi.life:8443';
  @State localPeerId: string = `${Math.floor(Math.random() * 9000) + 1000}`;
  @State remotePeerId: string = '';
  @State callActive: boolean = false;
  @State callRegistered: boolean = false;
  @State callRequested: boolean = false;
  @State callStatus: string = '未连接';
  private hostWindow: window.Window | undefined = undefined;
  private optionalWindowEvents: PreviewOptionalWindowEvents = new PreviewOptionalWindowEvents();
  private isRotating: boolean = false;
  private previewWidthPx: number = 0;
  private previewHeightPx: number = 0;
  private savedOrientation: window.Orientation | undefined = undefined;
  private rotateSettleTimer: number = -1;

  aboutToAppear(): void {
    this.pageActive = true;
    this.hoverPreferenceKey = `previewSide_${deviceInfo.productModel}`;
    let savedSide = '';
    try {
      this.hoverPreferences = preferences.getPreferencesSync(getContext(this), { name: 'rtcLabHover' });
      savedSide = this.hoverPreferences.getSync(this.hoverPreferenceKey, '') as string;
    } catch (e) {
      console.warn(TAG, 'Unable to load hover preference');
    }
    this.hoverPreviewSide = resolveHoverPreviewSide(savedSide);
    this.controller.onFrameSizeChanged = (width: number, height: number) => {
      if (!this.canRunPreview() || !this.cameraSession?.track || width <= 0 || height <= 0) {
        return;
      }
      this.frameWidth = width;
      this.frameHeight = height;
      this.previewReady = true;
      this.refreshPreviewSurface();
      this.refreshTelemetry();
    };
    Logging.enableLogThreads();
    Logging.enableLogTimeStamps();
    Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    tryPreviewPlatformAction(() => { display.on('change', this.onDisplayChange); });
    tryPreviewPlatformAction(() => { display.on('foldStatusChange', this.onFoldStatusChange); });
    try {
      this.isHalfFolded = display.isFoldable() &&
        display.getFoldStatus() === display.FoldStatus.FOLD_STATUS_HALF_FOLDED;
      this.foldStatus = display.getFoldStatus();
      this.panelVisible = this.isHalfFolded;
    } catch (e) {
      console.error(TAG, 'register display listener failed');
    }
    this.bindWindowEvents();
    abilityAccessCtrl.createAtManager()
      .requestPermissionsFromUser(getContext(), ['ohos.permission.CAMERA'])
      .then((result: PermissionRequestResult) => {
        if (!this.pageActive) {
          return;
        }
        let granted = true;
        for (let index = 0; index < result.permissions.length; index++) {
          if (result.authResults[index] != abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
            granted = false;
          }
        }
        if (!granted) {
          this.telemetry = '相机权限未授予';
          return;
        }
        this.pcf = new webrtc.PeerConnectionFactory({});
        webrtc.PeerConnectionFactory.setDefault(this.pcf);
        this.cameraSession = new CameraPreviewSession(getContext(this), this.pcf, this.controller,
          { width: STREAM_W, height: STREAM_H, front: this.useFront,
            frameStallTimeoutMs: (webrtc.NativeVideoRenderer.cameraPreviewApiVersion ?? 0) >= 2 ? 8000 : 0 });
        this.cameraSession.onTrackChanged = (track: webrtc.MediaStreamTrack | undefined): void => {
          this.peerCall?.replaceTrack(track);
        };
        this.cameraSession.onStatusChanged = (status: CameraPreviewStatus) => {
          if (!this.pageActive) { return; }
          const refreshDevices = status.state === 'running' &&
            (!this.previewReady || this.selectedCameraId !== status.cameraId);
          this.actualFront = status.front;
          this.selectedCameraId = status.cameraId;
          this.previewReady = status.state === 'running';
          this.busy = status.state === 'starting' || status.state === 'recovering';
          if (refreshDevices) { this.refreshDeviceList(); }
          if (status.error.length > 0) {
            this.telemetry = `${status.state}: ${status.error}`;
          } else {
            this.refreshTelemetry();
          }
        };
        if (this.hostWindow) { this.cameraSession.attachWindow(this.hostWindow); }
        this.cameraSession.setActive(this.pageVisible);
        this.refreshDeviceList();
      });
  }

  onPageShow(): void {
    this.pageVisible = true;
    this.resumePreview();
  }

  onPageHide(): void {
    this.pageVisible = false;
    this.suspendPreview();
  }

  private canRunPreview(): boolean {
    return this.pageActive && this.pageVisible && this.windowVisible && !this.previewSuspended;
  }

  private suspendPreview(): void {
    this.previewSuspended = true;
    this.clearRotateSettleTimer();
    this.isRotating = false;
    this.cameraSession?.setActive(false);
    console.info(TAG, 'preview suspended');
  }

  private resumePreview(): void {
    if (!this.pageActive || !this.pageVisible || !this.windowVisible || !this.previewSuspended) {
      return;
    }
    this.previewSuspended = false;
    try {
      // Folding can happen while the app is hidden and its callbacks are paused.
      this.foldStatus = display.getFoldStatus();
      this.isHalfFolded = this.foldStatus === display.FoldStatus.FOLD_STATUS_HALF_FOLDED;
    } catch (e) {
      console.warn(TAG, 'Unable to refresh fold state on resume');
    }
    this.telemetry = '正在恢复相机预览…';
    this.cameraSession?.setActive(true);
    this.scheduleLayoutRefresh(0);
    console.info(TAG, 'preview resume scheduled');
  }

  private onWindowEvent = (event: window.WindowEventType): void => {
    if (!this.pageActive) {
      return;
    }
    if (event === window.WindowEventType.WINDOW_HIDDEN) {
      this.windowVisible = false;
      this.suspendPreview();
    } else if (event === window.WindowEventType.WINDOW_SHOWN) {
      this.windowVisible = true;
      this.resumePreview();
    }
  }

  aboutToDisappear(): void {
    this.peerCall?.stop();
    this.peerCall = undefined;
    this.pageActive = false;
    this.deviceListRequest++;
    this.pageVisible = false;
    this.windowVisible = false;
    this.suspendPreview();
    this.controller.onFrameSizeChanged = undefined;
    this.clearPhoto();
    this.clearRotateSettleTimer();
    tryPreviewPlatformAction(() => { display.off('change', this.onDisplayChange); });
    tryPreviewPlatformAction(() => { display.off('foldStatusChange', this.onFoldStatusChange); });
    this.cameraSession?.release();
    this.cameraSession = undefined;
    this.restoreOrientation();
    this.unbindWindowEvents();
    this.pcf = undefined;
  }

  private bindWindowEvents(): void {
    window.getLastWindow(getContext()).then((win: window.Window) => {
      if (!this.pageActive) {
        return;
      }
      this.hostWindow = win;
      this.cameraSession?.attachWindow(win);
      tryPreviewPlatformAction(() => { win.on('windowEvent', this.onWindowEvent); });
      this.windowVisible = win.isWindowShowing();
      this.resumePreview();
      try {
        this.savedOrientation = win.getWindowProperties().windowRect !== undefined ?
          window.Orientation.UNSPECIFIED : window.Orientation.UNSPECIFIED;
      } catch (e) {
      }
      tryPreviewPlatformAction(() => { win.on('windowSizeChange', this.onWindowSizeChange); });
      this.optionalWindowEvents.bind(win, this.onRotationChange, this.onDisplayChange);
      this.updateBreakpoint(win.getWindowProperties().windowRect.width,
        win.getWindowProperties().windowRect.height);
      this.applyOrientationPolicy(this.orientationPolicy);
      this.updateHoverLayout();
      this.refreshPreviewSurface();
    }).catch((e: Error) => {
      console.error(TAG, 'bind window failed: ' + e);
    });
  }

  private unbindWindowEvents(): void {
    const host = this.hostWindow;
    if (host !== undefined) {
      tryPreviewPlatformAction(() => { host.off('windowEvent', this.onWindowEvent); });
      tryPreviewPlatformAction(() => { host.off('windowSizeChange', this.onWindowSizeChange); });
      this.optionalWindowEvents.unbind(host, this.onRotationChange, this.onDisplayChange);
    }
    this.hostWindow = undefined;
  }

  private restoreOrientation(): void {
    try {
      if (this.hostWindow !== undefined) {
        this.hostWindow.setPreferredOrientation(window.Orientation.UNSPECIFIED);
      }
    } catch (e) {
    }
  }

  private applyOrientationPolicy(policy: string, recover: boolean = true): void {
    if (this.hostWindow === undefined) {
      return;
    }
    try {
      let requestedOrientation: window.Orientation;
      if (policy === 'portrait') {
        // A user direction request must remain rotatable on expanded devices.
        // Fixed PORTRAIT lets the system place this ability in a narrow
        // compatibility window on a landscape tablet / unfolded tri-fold.
        requestedOrientation = window.Orientation.USER_ROTATION_PORTRAIT;
      } else if (policy === 'landscape') {
        requestedOrientation = window.Orientation.USER_ROTATION_LANDSCAPE;
      } else {
        // Use both dimensions: rotating a narrow phone must not opt it into
        // large-window rotation just because its current width becomes >=600vp.
        requestedOrientation = this.autoRotationAllowed ? window.Orientation.AUTO_ROTATION_RESTRICTED :
          window.Orientation.PORTRAIT;
      }
      this.orientationPolicy = policy;
      if (requestedOrientation === this.appliedOrientation) {
        return;
      }
      this.appliedOrientation = requestedOrientation;
      this.hostWindow.setPreferredOrientation(requestedOrientation).catch((error: BusinessError) => {
        if (this.appliedOrientation === requestedOrientation) {
          this.appliedOrientation = undefined;
        }
        console.error(TAG, `set orientation failed: ${error.code}`);
      });
      // 窗口策略变化后只重新计算页面布局。
      if (recover) {
        this.scheduleLayoutRefresh();
      }
    } catch (e) {
      console.error(TAG, 'set orientation failed');
    }
  }

  private clearRotateSettleTimer(): void {
    if (this.rotateSettleTimer >= 0) {
      clearTimeout(this.rotateSettleTimer);
      this.rotateSettleTimer = -1;
    }
  }

  private scheduleLayoutRefresh(delayMs: number = 100): void {
    if (!this.canRunPreview()) { return; }
    this.clearRotateSettleTimer();
    this.rotateSettleTimer = setTimeout(() => {
      this.rotateSettleTimer = -1;
      this.finishLayoutRefresh();
    }, delayMs);
  }

  private finishLayoutRefresh(): void {
    this.clearRotateSettleTimer();
    this.isRotating = false;
    if (!this.canRunPreview()) { return; }
    this.syncWindowGeometry(false);
    this.updateHoverLayout();
    this.refreshPreviewSurface();
    this.refreshTelemetry();
  }

  /** 从当前窗口矩形刷新断点/布局状态（含文案依赖的 @State）。 */
  private syncWindowGeometry(scheduleRecoverOnChange: boolean = true): void {
    try {
      let widthPx = display.getDefaultDisplaySync().width;
      let heightPx = display.getDefaultDisplaySync().height;
      if (this.hostWindow !== undefined) {
        let rect = this.hostWindow.getWindowProperties().windowRect;
        if (rect.width > 0 && rect.height > 0) {
          widthPx = rect.width;
          heightPx = rect.height;
        }
      }
      this.updateBreakpoint(widthPx, heightPx, scheduleRecoverOnChange);
    } catch (e) {
    }
  }

  private updateBreakpoint(widthPx: number, heightPx: number, scheduleRecoverOnChange: boolean = true): void {
    let density = this.currentDisplay().densityPixels;
    if (density <= 0) {
      density = 1;
    }
    const nextAutoRotation = canRotateCameraWindow(widthPx, heightPx, density);
    const rotationPolicyChanged = nextAutoRotation !== this.autoRotationAllowed;
    this.autoRotationAllowed = nextAutoRotation;
    let widthVp = widthPx / density;
    let next = 'sm';
    if (widthVp >= 840) {
      next = 'lg';
    } else if (widthVp >= 600) {
      next = 'md';
    }
    let wideChanged = (next !== 'sm') !== this.isWideLayout;
    let bpChanged = next !== this.breakpointName;
    this.breakpointName = next;
    this.isWideLayout = next !== 'sm';
    if (this.orientationPolicy === 'auto' && (rotationPolicyChanged || this.appliedOrientation === undefined)) {
      // Height-only changes (split-screen/floating windows) matter as well.
      this.applyOrientationPolicy('auto', false);
    }
    if (bpChanged || wideChanged) {
      this.refreshTelemetry();
      if (scheduleRecoverOnChange) {
        this.scheduleLayoutRefresh(520);
      }
    }
  }

  private onRotationChange = (info: PreviewRotationInfo): PreviewRotationResult | void => {
    try {
      if (info.type === PREVIEW_WILL_ROTATE) {
        this.isRotating = true;
        // Do not send a future HAL angle ahead of the native display event.
        this.refreshPreviewSurface();
        this.scheduleLayoutRefresh(800);
        return this.makeRotationResult();
      }
      // Keep the session alive and update geometry immediately, as CameraView does.
      this.finishLayoutRefresh();
    } catch (e) {
      this.isRotating = false;
      this.scheduleLayoutRefresh(200);
    }
    // Some window implementations parse the result for both event phases.
    // DID_ROTATE ignores its geometry, but a complete result avoids NAPI errors.
    return this.makeRotationResult();
  }

  private onWindowSizeChange = (windowSize: window.Size): void => {
    this.updateBreakpoint(windowSize.width, windowSize.height);
    this.updateHoverLayout();
    this.refreshTelemetry();
    // 旋转过程中不改 Buffer 尺寸；结束后由 finishLayoutRefresh 统一刷新。
    if (this.isRotating) {
      this.scheduleLayoutRefresh(600);
      return;
    }
    this.scheduleLayoutRefresh(100);
  }

  private makeRotationResult(): PreviewRotationResult {
    let rect: window.Rect = { left: 0, top: 0, width: 1, height: 1 };
    try {
      if (this.hostWindow !== undefined) {
        rect = this.hostWindow.getWindowProperties().windowRect;
      }
    } catch (e) {
    }
    if (rect.width <= 0 || rect.height <= 0) {
      let d = display.getDefaultDisplaySync();
      rect.width = d.width;
      rect.height = d.height;
    }
    return { rectType: PREVIEW_RECT_SCREEN, windowRect: rect };
  }

  private onDisplayChange = (): void => {
    this.syncWindowGeometry();
    this.updateHoverLayout();
    this.refreshTelemetry();
    if (this.isRotating) {
      this.scheduleLayoutRefresh(600);
      return;
    }
    this.scheduleLayoutRefresh(100);
  }

  private currentDisplay(): display.Display {
    if (this.hostWindow !== undefined) {
      return display.getDisplayByIdSync(this.hostWindow.getWindowProperties().displayId);
    }
    return display.getDefaultDisplaySync();
  }

  private onFoldStatusChange = (foldStatus: display.FoldStatus): void => {
    this.refreshDeviceList();
    this.foldStatus = foldStatus;
    this.isHalfFolded = (foldStatus === display.FoldStatus.FOLD_STATUS_HALF_FOLDED);
    if (this.isHalfFolded) {
      this.panelVisible = true;
    }
    this.updateHoverLayout();
    this.syncWindowGeometry();
    this.refreshTelemetry();
    // 页面处理折痕和布局；设备选择与会话恢复由库处理。
    this.scheduleLayoutRefresh(520);
  }

  private updateHoverLayout(): void {
    let creases: HoverRect[] = [];
    try {
      if (this.hostWindow !== undefined && this.contentArea !== undefined) {
        const win = this.hostWindow.getWindowProperties();
        const area = this.contentArea;
        this.contentRect = { left: win.windowRect.left + this.areaToPx(area.globalPosition.x ?? 0),
          top: win.windowRect.top + this.areaToPx(area.globalPosition.y ?? 0),
          width: this.areaToPx(area.width), height: this.areaToPx(area.height) };
        if (this.isHalfFolded) {
          creases = previewLiveCreases(this.currentDisplay());
        }
      }
    } catch (e) {
    }
    this.hover = computeHoverLayout(this.contentRect, creases, this.isHalfFolded, 0, this.hoverPreviewSide);
  }

  private selectHoverPreviewSide(side: string): void {
    this.hoverPreviewSide = side;
    this.updateHoverLayout();
    this.checkedE = false;
    try {
      this.hoverPreferences?.putSync(this.hoverPreferenceKey, side);
      this.hoverPreferences?.flush().catch(() => console.warn(TAG, 'Unable to save hover preference'));
    } catch (e) {
      console.warn(TAG, 'Unable to save hover preference');
    }
  }

  private pxToVp(value: number): number {
    return this.getUIContext().px2vp(value);
  }

  private refreshTelemetry(): void {
    let rotation = this.currentDisplay().rotation * 90;
    let fold = this.isHalfFolded ? '半折' : '普通';
    this.telemetry =
      `${this.actualFront ? '前置' : '后置'} · 断点 ${this.breakpointName} · 屏角 ${rotation}° · ${fold} · 帧 ${this.frameWidth}×${this.frameHeight}`;
  }

  private refreshPreviewSurface(): void {
    if (this.previewWidthPx <= 0 || this.previewHeightPx <= 0) {
      return;
    }
    let ratio = 0;
    if (this.previewRatio === '16:9') {
      ratio = 16 / 9;
    } else if (this.previewRatio === '4:3') {
      ratio = 4 / 3;
    } else if (this.previewRatio === '1:1') {
      ratio = 1;
    }
    if (ratio > 0) {
      // Tri-fold panels can have different natural orientations. Use the actual
      // app window, not the default display rotation or the hover preview half.
      const bounds = this.hostWindow?.getWindowProperties().windowRect;
      ratio = previewAspectRatio(ratio, bounds?.width ?? this.previewWidthPx,
        bounds?.height ?? this.previewHeightPx);
    }
    const geometry = computePreviewGeometry(this.previewWidthPx, this.previewHeightPx,
      this.frameWidth, this.frameHeight, ratio);
    this.viewfinderWidth = this.pxToVp(geometry.width);
    this.viewfinderHeight = this.pxToVp(geometry.height);
    this.videoSurfaceWidth = this.pxToVp(geometry.surfaceWidth);
    this.videoSurfaceHeight = this.pxToVp(geometry.surfaceHeight);
    this.cropWidthFraction = geometry.width / geometry.surfaceWidth;
    this.cropHeightFraction = geometry.height / geometry.surfaceHeight;
    this.previewSizeText = `${geometry.width}×${geometry.height} px`;
    this.controller.setScalingMode(ScalingMode.AspectFit);
  }

  private selectPreviewRatio(ratio: string): void {
    this.previewRatio = ratio;
    this.checkedC = false;
    this.refreshPreviewSurface();
  }

  private selectCamera(): void {
    this.cameraSession?.setCameraPreference(this.useFront);
  }

  private refreshDeviceList(): void {
    if (!this.pageActive) { return; }
    const request = ++this.deviceListRequest;
    let lines: string[] = [];
    try {
      let mediaDevices = new webrtc.MediaDevices();
      mediaDevices.enumerateDevices().then((devices: webrtc.MediaDeviceInfo[]) => {
        if (!this.pageActive || request !== this.deviceListRequest) { return; }
        let next: string[] = [];
        for (let i = 0; i < devices.length; i++) {
          let d = devices[i];
          if (d.kind === 'videoinput') {
            next.push(`${d.label || 'camera'} · ${d.deviceId}`);
          }
        }
        if (next.length === 0) {
          next.push(this.useFront ? '前置 · facing=user' : '后置 · facing=environment');
        }
        this.deviceLines = next;
      }).catch(() => {
        if (!this.pageActive || request !== this.deviceListRequest) { return; }
        this.deviceLines = [this.useFront ? '前置 · facing=user' : '后置 · facing=environment'];
      });
      return;
    } catch (e) {
      if (!this.pageActive || request !== this.deviceListRequest) { return; }
      lines.push(this.useFront ? '前置 · facing=user' : '后置 · facing=environment');
      this.deviceLines = lines;
    }
  }

  private clearPhoto(): void {
    this.photoUri = '';
    this.photoDetails = '';
    this.photoStatus = '';
    removePreviewPhoto(this.photoPath);
    this.photoPath = '';
  }

  private async takePhoto(): Promise<void> {
    if (this.takingPhoto || !this.previewReady || this.isRotating || this.busy) {
      return;
    }
    this.takingPhoto = true;
    this.photoStatus = '正在保存照片…';
    const source = this.cameraSession?.track;
    let pendingPath = '';
    try {
      const rotation = display.getDefaultDisplaySync().rotation;
      const cameraName = this.actualFront ? '前置' : '后置';
      const cameraId = this.selectedCameraId;
      const ratio = this.previewRatio;
      const surfaceId = this.controller.getXComponentSurfaceId();
      const cropWidth = this.cropWidthFraction;
      const cropHeight = this.cropHeightFraction;
      const viewWidth = this.viewfinderWidth;
      const viewHeight = this.viewfinderHeight;
      const side = this.hoverPreviewSide;
      const pixels = await this.getUIContext().getComponentSnapshot().get('rtc-lab-viewfinder',
        { waitUntilRenderFinished: true });
      const photo = await savePreviewPhoto(pixels, getContext(this).cacheDir);
      pendingPath = photo.path;
      if (!this.pageActive || source !== this.cameraSession?.track || !this.previewReady ||
        surfaceId !== this.controller.getXComponentSurfaceId() ||
        rotation !== display.getDefaultDisplaySync().rotation || this.isRotating || ratio !== this.previewRatio ||
        cropWidth !== this.cropWidthFraction || cropHeight !== this.cropHeightFraction ||
        viewWidth !== this.viewfinderWidth || viewHeight !== this.viewfinderHeight || side !== this.hoverPreviewSide) {
        if (this.pageActive) {
          this.photoStatus = '拍照期间画面发生变化，请稳定设备后重试';
        }
        return;
      }
      const previousPath = this.photoPath;
      this.photoPath = photo.path;
      this.photoUri = photo.uri;
      pendingPath = '';
      this.photoDetails = `${photo.width}×${photo.height} · ${cameraName} ${cameraId} · 屏角 ${rotation * 90}° · ${ratio}`;
      this.photoStatus = `拍摄于 ${new Date().toLocaleTimeString()} · 在 D 项查看照片`;
      this.checkedD = false;
      removePreviewPhoto(previousPath);
    } catch (e) {
      if (this.pageActive) {
        this.photoStatus = `拍照失败：${(e as Error).message}`;
      }
    } finally {
      removePreviewPhoto(pendingPath);
      this.takingPhoto = false;
    }
  }

  private startPeerCall(): void {
    const track = this.cameraSession?.track;
    if (!this.pcf || !track || !this.previewReady) { return; }
    if (!/^\d{2,4}$/.test(this.localPeerId) || !/^wss?:\/\//.test(this.signalServer)) {
      this.callStatus = '请填写信令服务器地址和 2–4 位本机 ID';
      return;
    }
    this.peerCall?.stop();
    const call = new PreviewPeerCall(this.remoteController);
    this.peerCall = call;
    this.checkedF = false;
    call.onStatus = (message: string, registered: boolean, active: boolean): void => {
      if (this.peerCall !== call || !this.pageActive) { return; }
      this.callStatus = message;
      this.callRegistered = registered;
      this.callActive = active;
      if (!active) { this.callRequested = false; }
    };
    call.start(this.pcf, track, this.signalServer, this.localPeerId);
  }

  private markCurrentChecked(value: boolean): void {
    if (this.featureIndex === 0) {
      this.checkedA = value;
    } else if (this.featureIndex === 1) {
      this.checkedB = value;
    } else if (this.featureIndex === 2) {
      this.checkedC = value;
    } else if (this.featureIndex === 3) {
      this.checkedD = value;
    } else if (this.featureIndex === 4) {
      this.checkedE = value;
    } else {
      this.checkedF = value;
    }
  }

  private isCurrentChecked(): boolean {
    if (this.featureIndex === 0) {
      return this.checkedA;
    }
    if (this.featureIndex === 1) {
      return this.checkedB;
    }
    if (this.featureIndex === 2) {
      return this.checkedC;
    }
    if (this.featureIndex === 3) {
      return this.checkedD;
    }
    return this.featureIndex === 4 ? this.checkedE : this.checkedF;
  }

  private featureHint(): string {
    if (this.featureIndex === 5) {
      return '核对：两端在 F 项注册，一端输入另一端 ID 并呼叫。检查本地取景与下方对端视频的连续性、方向和比例，再测试旋转、折叠、切换镜头及前后台恢复。';
    }
    if (this.featureIndex === 0) {
      return '核对：sm（宽度<600vp）按钮分布在上下两侧；md（600–839vp）、lg（≥840vp）分布在左右两侧。调整窗口或折叠设备检查切换；自动模式下短边达到600vp支持系统旋转。';
    }
    if (this.featureIndex === 1) {
      return '核对：切换前后置；折叠/展开后设备列表刷新并保持前后置偏好。';
    }
    if (this.featureIndex === 2) {
      return '核对：全屏和固定比例等比裁切，物体不变形。';
    }
    if (this.featureIndex === 3) {
      return '核对：四向持握后拍照，在下方照片预览中检查方向；后置画面不镜像。';
    }
    return '核对：预览与操作区分居折痕两侧，避让带不可操作；横竖屏及展开后布局恢复。';
  }

  private areaToPx(value: Length): number {
    let density = display.getDefaultDisplaySync().densityPixels;
    if (density <= 0) {
      density = 1;
    }
    if (typeof value === 'number') {
      return this.getUIContext().vp2px(value);
    }
    return 0;
  }

  private usesSideControls(): boolean {
    // Hover controls stay inside their crease-free region.
    return this.isWideLayout && !this.hover.active;
  }

  @Builder
  FeatureRail() {
    Flex({ direction: this.usesSideControls() ? FlexDirection.Column : FlexDirection.Row,
      wrap: FlexWrap.Wrap, justifyContent: FlexAlign.SpaceBetween, alignItems: ItemAlign.Center }) {
      ForEach(FEATURE_CODES, (code: string, index: number) => {
        Button(`${code}`)
          .width(36)
          .height(36)
          .padding(0)
          .flexShrink(0)
          .fontSize(14)
          .fontWeight(FontWeight.Bold)
          .backgroundColor(this.featureIndex === index ? '#7CFFB2' : '#1E2A24')
          .fontColor(this.featureIndex === index ? '#0B1210' : '#A8B5AD')
          .onClick(() => {
            this.featureIndex = index;
            this.panelVisible = true;
          })
      }, (code: string, index: number) => `${code}_${index}`)
      Button('←')
        .width(44)
        .height(36)
        .padding(0)
        .flexShrink(0)
        .backgroundColor('#2A3530')
        .fontColor(Color.White)
        .onClick(() => {
          router.back();
        })
    }
    .width(this.usesSideControls() ? 64 : '100%')
    .height(this.usesSideControls() ? '100%' : 'auto')
    .padding(8)
    .flexShrink(0)
    .backgroundColor('#990B1210')
  }

  @Builder
  CameraActions() {
    Flex({ direction: this.usesSideControls() ? FlexDirection.Column : FlexDirection.Row,
      wrap: FlexWrap.Wrap, justifyContent: FlexAlign.SpaceAround, alignItems: ItemAlign.Center }) {
      Button(this.actualFront ? '切后置' : '切前置')
        .width(64)
        .fontSize(12)
        .height(40)
        .padding({ left: 8, right: 8 })
        .margin(4)
        .enabled(!this.busy && !this.takingPhoto)
        .backgroundColor('#CC24302A')
        .fontColor(Color.White)
        .onClick(() => {
          this.useFront = !this.actualFront;
          this.selectCamera();
          this.refreshDeviceList();
        })
      Button(this.takingPhoto ? '拍摄中' : '拍照')
        .width(64)
        .height(64)
        .padding(0)
        .margin(4)
        .fontSize(16)
        .backgroundColor('#7CFFB2')
        .fontColor('#0B1210')
        .enabled(this.previewReady && !this.takingPhoto && !this.busy)
        .onClick(() => { this.takePhoto(); })
      Button(this.panelVisible ? '收起' : '设置')
        .width(64)
        .fontSize(12)
        .height(40)
        .margin(4)
        .backgroundColor('#CC24302A')
        .fontColor(Color.White)
        .onClick(() => {
          this.panelVisible = !this.panelVisible;
        })
    }
    .width(this.usesSideControls() ? 88 : '100%')
    .height(this.usesSideControls() ? '100%' : 'auto')
    .padding(8)
    .flexShrink(0)
    .backgroundColor('#990B1210')
  }

  @Builder
  ControlPanel() {
    Column({ space: 10 }) {
      Text(`${FEATURE_CODES[this.featureIndex]} · ${FEATURE_NAMES[this.featureIndex]}`)
        .fontSize(18)
        .fontColor(Color.White)
        .fontWeight(FontWeight.Medium)
      Text(this.featureHint())
        .fontSize(13)
        .fontColor('#B7C4BC')
      if (this.featureIndex === 0 || this.featureIndex === 2) {
        Row({ space: 8 }) {
          Button('自动(断点)')
            .fontSize(12)
            .backgroundColor(this.orientationPolicy === 'auto' ? '#7CFFB2' : '#24302A')
            .fontColor(this.orientationPolicy === 'auto' ? '#0B1210' : '#DCE6E0')
            .onClick(() => {
              this.applyOrientationPolicy('auto');
            })
          Button('竖屏')
            .fontSize(12)
            .backgroundColor(this.orientationPolicy === 'portrait' ? '#7CFFB2' : '#24302A')
            .fontColor(this.orientationPolicy === 'portrait' ? '#0B1210' : '#DCE6E0')
            .onClick(() => {
              this.applyOrientationPolicy('portrait');
            })
          Button('横屏')
            .fontSize(12)
            .backgroundColor(this.orientationPolicy === 'landscape' ? '#7CFFB2' : '#24302A')
            .fontColor(this.orientationPolicy === 'landscape' ? '#0B1210' : '#DCE6E0')
            .onClick(() => {
              this.applyOrientationPolicy('landscape');
            })
        }
        Text(`当前断点 ${this.breakpointName} · ${this.hover.active ? '悬停分区' :
          (this.isWideLayout ? '按钮左右布局' : '按钮上下布局')}`)
          .fontSize(12)
          .fontColor('#7CFFB2')
        Text(this.orientationPolicy === 'auto' ?
          (this.autoRotationAllowed ? '自动策略：允许旋转，遵循系统旋转开关' : '自动策略：保持竖屏') :
          '手动方向测试：已覆盖自动旋转策略')
          .fontSize(12)
          .fontColor('#A8B5AD')
      }
      if (this.featureIndex === 1) {
        Button(this.actualFront ? '切换到后置' : '切换到前置')
          .enabled(!this.busy)
          .backgroundColor('#24302A')
          .fontColor('#7CFFB2')
          .onClick(() => {
            this.useFront = !this.actualFront;
            this.selectCamera();
            this.refreshDeviceList();
          })
        Text(`当前：${this.actualFront ? '前置' : '后置'} · ${this.selectedCameraId}`).fontSize(12).fontColor('#A8B5AD')
        if (this.useFront !== this.actualFront && this.selectedCameraId.length > 0) {
          Text('已自动使用当前可用镜头；形态恢复后优先使用原选择').fontSize(12).fontColor('#A8B5AD')
        }
        ForEach(this.deviceLines, (line: string) => {
          Text(line).fontSize(12).fontColor('#A8B5AD')
        }, (line: string) => line)
      }
      if (this.featureIndex === 2) {
        Text('取景框比例（横屏标记，竖屏自动对调）')
          .fontSize(13)
          .fontColor(Color.White)
        Flex({ wrap: FlexWrap.Wrap }) {
          ForEach(PREVIEW_RATIOS, (ratio: string) => {
            Button(ratio)
              .fontSize(12)
              .height(34)
              .margin({ right: 6, bottom: 6 })
              .backgroundColor(this.previewRatio === ratio ? '#7CFFB2' : '#24302A')
              .fontColor(this.previewRatio === ratio ? '#0B1210' : '#DCE6E0')
              .onClick(() => this.selectPreviewRatio(ratio))
          }, (ratio: string) => ratio)
        }
        Text(`${this.previewRatio} · ${this.previewSizeText} · 等比裁切`)
          .fontSize(12)
          .fontColor('#7CFFB2')
        Text('全屏及固定比例只裁切边缘，不改变物体比例；切换比例不更换镜头或采集分辨率。')
          .fontSize(12)
          .fontColor('#A8B5AD')
        Button('恢复全屏比例')
          .fontSize(12)
          .onClick(() => {
            this.selectPreviewRatio('全屏');
          })
        Button('刷新预览布局')
          .backgroundColor('#24302A')
          .fontColor('#7CFFB2')
          .onClick(() => {
            this.refreshPreviewSurface();
          })
      }
      if (this.featureIndex === 3) {
        Text((webrtc.NativeVideoRenderer.cameraPreviewApiVersion ?? 0) >= 2 ?
          '已启用采集停帧检测（8 秒采样）' : '当前库支持预览；更新 Native 库后可启用采集停帧检测')
          .fontSize(12).fontColor('#A8B5AD')
        Button(this.takingPhoto ? '正在保存…' : '保存预览截图')
          .enabled(this.previewReady && !this.takingPhoto && !this.busy)
          .backgroundColor('#7CFFB2')
          .fontColor('#0B1210')
          .onClick(() => { this.takePhoto(); })
        Text('保存屏幕合成后的预览为 JPEG，包含裁切和镜像。用于验证预览效果，不代表原始帧、编码输出或远端画面方向。')
          .fontSize(12)
          .fontColor('#A8B5AD')
        if (!this.previewReady) {
          Text('等待相机出帧；可到 B 页切换可用摄像头').fontSize(12).fontColor('#E8C27A')
        }
        if (this.photoUri.length > 0) {
          Image(this.photoUri)
            .width('100%')
            .height(200)
            .objectFit(ImageFit.ScaleDown)
            .backgroundColor(Color.Black)
          Text(this.photoDetails).fontSize(12).fontColor('#7CFFB2')
          Button('清除照片')
            .fontSize(12)
            .enabled(!this.takingPhoto)
            .onClick(() => { this.clearPhoto(); })
        } else {
          Text('照片预览区域\n拍照后在此查看结果')
            .width('100%')
            .height(160)
            .textAlign(TextAlign.Center)
            .fontColor('#A8B5AD')
            .backgroundColor(Color.Black)
        }
        if (this.photoStatus.length > 0) {
          Text(this.photoStatus).fontSize(12).fontColor('#E8C27A')
        }
        Button('重开采集')
          .enabled(!this.busy)
          .backgroundColor('#24302A')
          .fontColor('#7CFFB2')
          .onClick(() => {
            this.cameraSession?.retry();
          })
      }
      if (this.featureIndex === 5) {
        Text('本地视频：当前页面取景区；对端视频：下方区域')
          .fontSize(12).fontColor('#A8B5AD')
        TextInput({ text: this.signalServer, placeholder: '信令服务器 wss://…' })
          .fontColor(Color.White).placeholderColor('#A8B5AD').backgroundColor('#24302A')
          .enabled(!this.callActive)
          .onChange((value: string) => { this.signalServer = value.trim(); })
        TextInput({ text: this.localPeerId, placeholder: '本机 ID（2–4 位数字）' })
          .fontColor(Color.White).placeholderColor('#A8B5AD').backgroundColor('#24302A')
          .type(InputType.Number).enabled(!this.callActive)
          .onChange((value: string) => { this.localPeerId = value.trim(); })
        Button(this.callActive ? '断开连接' : '注册 / 等待呼叫')
          .enabled(this.callActive || (this.previewReady && !this.busy))
          .backgroundColor('#7CFFB2')
          .fontColor('#0B1210')
          .onClick(() => {
            if (this.callActive) { this.peerCall?.stop(); }
            else { this.startPeerCall(); }
          })
        TextInput({ text: this.remotePeerId, placeholder: '对端 ID' })
          .fontColor(Color.White).placeholderColor('#A8B5AD').backgroundColor('#24302A')
          .type(InputType.Number).enabled(!this.callRequested)
          .onChange((value: string) => { this.remotePeerId = value.trim(); })
        Button('呼叫对端')
          .enabled(this.callRegistered && !this.callRequested)
          .onClick(() => {
            if (!/^\d{2,4}$/.test(this.remotePeerId) || this.remotePeerId === this.localPeerId) {
              this.callStatus = '请输入与本机不同的 2–4 位对端 ID';
              return;
            }
            this.callRequested = true;
            this.peerCall?.call(this.remotePeerId);
          })
        Text(this.callStatus).fontSize(12).fontColor('#7CFFB2')
        XComponent({ id: 'rtc-lab-remote', type: XComponentType.SURFACE, controller: this.remoteController })
          .width('100%').height(200).backgroundColor(Color.Black)
        Text('仅测试视频。两端使用相同信令服务；只需一端发起呼叫。断开后可重新注册。')
          .fontSize(12).fontColor('#A8B5AD')
        Text('信令协议与原测试页相同。当前使用 STUN，未配置 TURN；受限网络下可能无法建立连接。')
          .fontSize(12).fontColor('#A8B5AD')
      }
      if (this.featureIndex === 4) {
        Text('左右分区的预览位置（上下分区保持不变）')
          .fontSize(12)
          .fontColor('#A8B5AD')
        Row({ space: 8 }) {
          Button('预览居左')
            .fontSize(12)
            .height(34)
            .fontColor(this.hoverPreviewSide === 'left' ? '#0B1210' : '#DCE6E0')
            .backgroundColor(this.hoverPreviewSide === 'left' ? '#7CFFB2' : '#24302A')
            .onClick(() => this.selectHoverPreviewSide('left'))
          Button('预览居右')
            .fontSize(12)
            .height(34)
            .fontColor(this.hoverPreviewSide === 'right' ? '#0B1210' : '#DCE6E0')
            .backgroundColor(this.hoverPreviewSide === 'right' ? '#7CFFB2' : '#24302A')
            .onClick(() => this.selectHoverPreviewSide('right'))
        }
        Text(`折叠状态：${this.isHalfFolded ? '半折叠（悬停）' : '非悬停'}`)
          .fontSize(12)
          .fontColor('#7CFFB2')
        Text(this.hover.active ? (this.hover.fallback ? '等待实时折痕位置' :
          (this.hover.gaps.length > 1 ? '多折痕分区 · 全部避让' :
            (this.hover.gap.width > this.hover.gap.height ? '上下分区 · 折痕避让' : '左右分区 · 折痕避让'))) : '整窗取景')
          .fontSize(12)
          .fontColor('#A8B5AD')
      }
      Row() {
        Text(this.isCurrentChecked() ? '已目视通过' : '待核对')
          .fontColor(this.isCurrentChecked() ? '#7CFFB2' : '#E8C27A')
          .fontSize(14)
        Toggle({ type: ToggleType.Switch, isOn: this.isCurrentChecked() })
          .selectedColor('#7CFFB2')
          .onChange((isOn: boolean) => {
            this.markCurrentChecked(isOn);
          })
      }
      .width('100%')
      .justifyContent(FlexAlign.SpaceBetween)
      Text(this.telemetry)
        .fontSize(11)
        .fontColor('#809088')
    }
    .padding(16)
    .width('100%')
    .backgroundColor('#121A17')
  }

  @Builder
  PreviewSurface() {
    // 保持同一预览节点，减少布局切换时的 Surface 创建开销。
    Stack() {
      Stack() {
        XComponent({
          id: 'rtc-lab-preview',
          type: XComponentType.SURFACE,
          controller: this.controller
        })
          .onLoad(() => {
            this.controller.setScalingMode(ScalingMode.AspectFit);
            this.refreshPreviewSurface();
          })
          .onAreaChange((_oldArea: Area, newArea: Area) => {
            this.controller.setPreviewSurfaceSize(Math.round(this.areaToPx(newArea.width)),
              Math.round(this.areaToPx(newArea.height)));
          })
          .width(this.videoSurfaceWidth)
          .height(this.videoSurfaceHeight)
        if (this.featureIndex === 2) {
          Rect()
            .width(this.viewfinderWidth)
            .height(this.viewfinderHeight)
            .fill(Color.Transparent)
            .stroke('#7CFFB2')
            .strokeWidth(1)
            .hitTestBehavior(HitTestMode.None)
        }
      }
      .width(this.viewfinderWidth)
      .height(this.viewfinderHeight)
      .clip(true)
      .id('rtc-lab-viewfinder')
    }
    .width(this.hover.active ? this.pxToVp(this.hover.preview.width) : '100%')
    .height(this.hover.active ? this.pxToVp(this.hover.preview.height) : '100%')
    .position({ x: this.hover.active ? this.pxToVp(this.hover.preview.left) : 0,
      y: this.hover.active ? this.pxToVp(this.hover.preview.top) : 0 })
    .backgroundColor('#000000')
    .clip(true)
    .onAreaChange((_oldArea: Area, newArea: Area) => {
      // Measure the available container, never the ratio-constrained child.
      this.previewWidthPx = Math.round(this.areaToPx(newArea.width));
      this.previewHeightPx = Math.round(this.areaToPx(newArea.height));
      this.refreshPreviewSurface();
    })
  }

  build() {
    // A single persistent Surface fills the window. Controls overlay the preview
    // and never participate in its measurement or resize it when opened.
    Stack({ alignContent: Alignment.TopStart }) {
      this.PreviewSurface()
      if (this.hover.fallback) {
        Column({ space: 12 }) {
          Text(this.hover.gaps.length > 0 ? '折痕外空间不足，请展开设备后重试' :
            '暂未获取实时折痕位置，请展开设备后重试')
            .fontSize(14)
            .fontColor(Color.White)
          Button('返回').onClick(() => router.back())
        }
        .padding(16)
        .width(this.hover.gaps.length > 0 ? this.pxToVp(this.hover.controls.width) : '100%')
        .height(this.hover.gaps.length > 0 ? this.pxToVp(this.hover.controls.height) : '100%')
        .position({ x: this.pxToVp(this.hover.controls.left), y: this.pxToVp(this.hover.controls.top) })
        .clip(true)
      }
      if (this.hover.active) {
        ForEach(this.hover.gaps, (gap: HoverRect) => {
          Rect()
            .width(this.pxToVp(gap.width))
            .height(this.pxToVp(gap.height))
            .position({ x: this.pxToVp(gap.left), y: this.pxToVp(gap.top) })
            .fill('#252525')
            .hitTestBehavior(HitTestMode.None)
        })
      }
      if (!this.hover.fallback) {
        Flex({ direction: this.usesSideControls() ? FlexDirection.Row : FlexDirection.Column,
          alignItems: ItemAlign.Center }) {
          this.FeatureRail()
          if (!this.hover.active) {
            Blank().layoutWeight(1)
          }
          if (this.panelVisible) {
            Scroll() {
              this.ControlPanel()
            }
            .width(this.hover.active ? '100%' : (this.isWideLayout ? 340 : '100%'))
            .height(this.hover.active ? '100%' : (this.isWideLayout ? '55%' : '42%'))
            .layoutWeight(this.hover.active ? 1 : 0)
            .flexShrink(1)
          }
          this.CameraActions()
        }
        .width(this.hover.active ? this.pxToVp(this.hover.controls.width) : '100%')
        .height(this.hover.active ? this.pxToVp(this.hover.controls.height) : '100%')
        .position({ x: this.hover.active ? this.pxToVp(this.hover.controls.left) : 0,
          y: this.hover.active ? this.pxToVp(this.hover.controls.top) : 0 })
        .backgroundColor(this.hover.active ? '#0B1210' : Color.Transparent)
        .clip(true)
        .hitTestBehavior(HitTestMode.Transparent)
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#000000')
    .onAreaChange((_oldArea: Area, area: Area) => {
      this.contentArea = area;
      this.updateHoverLayout();
    })
  }
}
