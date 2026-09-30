import camera from '@ohos.multimedia.camera';
import display from '@ohos.display';
import window from '@ohos.window';
import common from '@ohos.app.ability.common';
import { BusinessError } from '@ohos.base';
import { PeerConnectionFactory, VideoSource, MediaStreamTrack, VideoCapturerStartedEvent,
  NativeVideoRenderer } from 'libohos_webrtc.so';
import { VideoRenderController, ScalingMode } from './VideoRenderController';
import { Logging } from '../log/Logging';

// Optional API 14 event, isolated from older SDK Window overloads.
// rotationChange belongs to the window layout owner: its return value controls
// the next window rectangle. Capture observes committed display/size events.
interface DisplayIdEventWindow {
  on(type: 'displayIdChange', callback: (id: number) => void): void;
  off(type: 'displayIdChange', callback: (id: number) => void): void;
}

export interface CameraPreviewOptions {
  width?: number;
  height?: number;
  front?: boolean;
  // A nonempty ID is strict: never silently substitute another device.
  deviceId?: string;
  allowFallback?: boolean;
  autoRecover?: boolean;
  maxRetries?: number;
  scalingMode?: ScalingMode;
  // 0 (default) disables monitoring. Explicitly enable with 1000..60000 ms.
  frameStallTimeoutMs?: number;
}

export interface CameraPreviewStatus {
  state: string;
  cameraId: string;
  front: boolean;
  error: string;
}

/** Owns capture recovery, not page layout. Grant camera permission before construction. */
export class CameraPreviewSession {
  public onStatusChanged?: (status: CameraPreviewStatus) => void;
  // Capture recovery can replace a track. Callers publishing it must replace their sender track here.
  public onTrackChanged?: (track: MediaStreamTrack | undefined) => void;
  private manager: camera.CameraManager;
  private hostWindow?: window.Window;
  private source?: VideoSource;
  private videoTrack?: MediaStreamTrack;
  private visible: boolean = false;
  private active: boolean = false;
  private released: boolean = false;
  private retryCount: number = 0;
  private timer: number = -1;
  private startTimer: number = -1;
  private generation: number = 0;
  private selectedId: string = '';
  private actualFront: boolean = false;
  private front: boolean;
  private exactId: string;
  private width: number;
  private height: number;
  private fallback: boolean;
  private recover: boolean;
  private maxRetries: number;
  private factory: PeerConnectionFactory;
  private controller: VideoRenderController;
  private displayListening: boolean = false;
  private foldListening: boolean = false;
  private cameraListening: boolean = false;
  private deviceListening: boolean = false;
  private nativeCompatible: boolean = false;
  private displayIdListening: boolean = false;
  private frameTimer: number = -1;
  private frameStallTimeoutMs: number;

  constructor(context: common.Context, factory: PeerConnectionFactory,
    controller: VideoRenderController, options: CameraPreviewOptions = {}) {
    this.factory = factory;
    this.controller = controller;
    this.nativeCompatible = (NativeVideoRenderer.cameraPreviewApiVersion ?? 0) >= 1;
    if (this.nativeCompatible) {
      this.controller.enableCameraPreviewAdaptation();
      this.controller.setScalingMode(options.scalingMode ?? ScalingMode.AspectFit);
    }
    this.front = options.front ?? false;
    this.exactId = options.deviceId ?? '';
    this.width = options.width ?? 1280;
    this.height = options.height ?? 720;
    this.fallback = options.allowFallback ?? true;
    this.recover = options.autoRecover ?? true;
    this.maxRetries = options.maxRetries ?? 3;
    this.frameStallTimeoutMs = options.frameStallTimeoutMs ?? 0;
    if (!Number.isInteger(this.width) || !Number.isInteger(this.height) || this.width <= 0 || this.height <= 0 ||
      !Number.isInteger(this.maxRetries) || this.maxRetries < 0 ||
      !Number.isInteger(this.frameStallTimeoutMs) ||
      (this.frameStallTimeoutMs !== 0 && (this.frameStallTimeoutMs < 1000 || this.frameStallTimeoutMs > 60000))) {
      throw new Error('Invalid camera preview options');
    }
    this.manager = camera.getCameraManager(context);
    try {
      this.manager.on('cameraStatus', this.onCameraStatus);
      this.deviceListening = true;
    } catch (e) { Logging.e('CameraPreviewSession', 'Camera status events unavailable'); }
    try {
      display.on('change', this.onDisplayChange);
      this.displayListening = true;
    } catch (e) { Logging.e('CameraPreviewSession', 'Display events unavailable; using window events'); }
    try {
      display.on('foldStatusChange', this.onFoldChange);
      this.foldListening = true;
    } catch (e) { Logging.e('CameraPreviewSession', 'Display fold events unavailable'); }
    try {
      this.manager.on('foldStatusChange', this.onCameraFoldChange);
      this.cameraListening = true;
    } catch (e) {
      Logging.e('CameraPreviewSession', 'Camera fold events unavailable; using display fold events');
    }
  }

  get track(): MediaStreamTrack | undefined { return this.videoTrack; }

  attachWindow(host: window.Window): void {
    if (this.released || this.hostWindow === host) { return; }
    this.detachWindow();
    this.hostWindow = host;
    host.on('windowEvent', this.onWindowEvent);
    host.on('windowSizeChange', this.onWindowSizeChange);
    try {
      (host as Object as DisplayIdEventWindow).on('displayIdChange', this.onDisplayIdChange);
      this.displayIdListening = true;
    } catch (e) { Logging.e('CameraPreviewSession', 'Window display migration events unavailable'); }
    this.visible = host.isWindowShowing();
    this.reconcile();
  }

  /** Page visibility and permission state. Window visibility is handled by the library. */
  setActive(active: boolean): void {
    if (this.released || this.active === active) { return; }
    this.active = active;
    this.reconcile();
  }

  setCameraPreference(front: boolean, deviceId: string = ''): void {
    if (this.released || (this.front === front && this.exactId === deviceId)) { return; }
    this.front = front;
    this.exactId = deviceId;
    this.retryCount = 0;
    if (this.canCapture()) { this.schedule(0); }
  }

  retry(): void {
    if (!this.canCapture()) { return; }
    this.retryCount = 0;
    this.schedule(0);
  }

  release(): void {
    if (this.released) { return; }
    this.released = true;
    this.active = false;
    this.clearTimer();
    this.closeCapture();
    this.detachWindow();
    try { if (this.displayListening) { display.off('change', this.onDisplayChange); } } catch (e) {
      Logging.e('CameraPreviewSession', 'Unable to remove display listener');
    }
    try { if (this.foldListening) { display.off('foldStatusChange', this.onFoldChange); } } catch (e) {
      Logging.e('CameraPreviewSession', 'Unable to remove display fold listener');
    }
    try { if (this.cameraListening) { this.manager.off('foldStatusChange', this.onCameraFoldChange); } } catch (e) {
      Logging.e('CameraPreviewSession', 'Unable to remove camera fold listener');
    }
    try { if (this.deviceListening) { this.manager.off('cameraStatus', this.onCameraStatus); } } catch (e) {
      Logging.e('CameraPreviewSession', 'Unable to remove camera status listener');
    }
    this.onStatusChanged = undefined;
    this.onTrackChanged = undefined;
  }

  private canCapture(): boolean { return !this.released && this.active && this.visible; }

  private emit(state: string, error: string = ''): void {
    this.onStatusChanged?.({ state: state, cameraId: this.selectedId, front: this.actualFront, error: error });
  }

  private reconcile(): void {
    this.clearTimer();
    if (!this.canCapture()) {
      this.closeCapture();
      this.emit('suspended');
      return;
    }
    this.retryCount = 0;
    this.controller.resetAppliedSurfaceSize();
    this.schedule(300);
  }

  private clearTimer(): void {
    if (this.timer >= 0) { clearTimeout(this.timer); this.timer = -1; }
  }

  private clearStartTimer(): void {
    if (this.startTimer >= 0) { clearTimeout(this.startTimer); this.startTimer = -1; }
  }

  private schedule(delay: number): void {
    this.clearTimer();
    if (!this.canCapture()) { return; }
    if (!this.nativeCompatible) {
      this.emit('incompatible', 'Camera preview requires a matching libohos_webrtc.so (cameraPreviewApiVersion >= 1)');
      return;
    }
    this.timer = setTimeout(() => { this.timer = -1; this.openCapture(); }, delay);
  }

  private closeCapture(): void {
    this.generation++;
    if (this.frameTimer >= 0) { clearTimeout(this.frameTimer); this.frameTimer = -1; }
    this.clearStartTimer();
    const oldSource = this.source;
    this.source = undefined;
    if (oldSource) {
      oldSource.oncapturerstarted = null;
      oldSource.oncapturerstopped = null;
    }
    this.controller.setVideoTrack(null);
    const oldTrack = this.videoTrack;
    this.videoTrack = undefined;
    if (oldTrack) {
      this.onTrackChanged?.(undefined);
      oldTrack.stop();
    }
    oldSource?.release();
  }

  private failed(message: string): void {
    this.closeCapture();
    if (!this.canCapture()) { return; }
    if (!this.recover || this.retryCount >= this.maxRetries) {
      this.emit('error', message);
      return;
    }
    this.retryCount++;
    this.emit('recovering', message);
    this.schedule(700);
  }

  private openCapture(): void {
    if (!this.canCapture()) { return; }
    this.closeCapture();
    this.selectedId = '';
    try {
      const devices = this.manager.getSupportedCameras();
      const position = this.front ? camera.CameraPosition.CAMERA_POSITION_FRONT :
        camera.CameraPosition.CAMERA_POSITION_BACK;
      let selected: camera.CameraDevice | undefined = undefined;
      for (const device of devices) {
        if (this.exactId.length > 0 ? device.cameraId === this.exactId : device.cameraPosition === position) {
          selected = device;
          break;
        }
      }
      if (!selected && this.exactId.length === 0 && this.fallback && devices.length > 0) { selected = devices[0]; }
      if (!selected) { this.failed('Requested camera unavailable'); return; }
      this.selectedId = selected.cameraId;
      this.actualFront = selected.cameraPosition === camera.CameraPosition.CAMERA_POSITION_FRONT;
      this.emit('starting');
      if (!this.canCapture()) { return; }
      const source = this.factory.createVideoSource({ width: this.width, height: this.height,
        deviceId: { exact: selected.cameraId } }, false);
      this.source = source;
      if (typeof source.applyDisplayRotation !== 'function' ||
        (this.frameStallTimeoutMs > 0 && typeof source.getCaptureFrameCount !== 'function')) {
        this.nativeCompatible = false;
        this.closeCapture();
        this.emit('incompatible', 'Native preview capability declaration does not match its methods');
        return;
      }
      const generation = this.generation;
      source.oncapturerstarted = (event: VideoCapturerStartedEvent) => {
        if (this.source !== source || this.generation !== generation || !this.canCapture()) { return; }
        if (!event.success) { this.failed('Camera failed to start'); return; }
        this.clearStartTimer();
        try { this.syncRotation(); } catch (e) { this.failed('Unable to read window display'); return; }
        if (this.frameStallTimeoutMs > 0 && this.frameTimer < 0) {
          try {
            this.monitorFrames(source, generation, source.getCaptureFrameCount!());
          } catch (e) {
            this.failed('Unable to read capture frame count');
            return;
          }
        }
        this.emit('running');
      };
      source.oncapturerstopped = () => {
        if (this.source === source && this.generation === generation && this.canCapture()) {
          this.failed('Camera capture interrupted');
        }
      };
      // Set the window angle before attaching the first sink, which starts capture.
      this.syncRotation();
      this.videoTrack = this.factory.createVideoTrack('camera-preview-' + generation, source);
      this.controller.setMirror(this.actualFront);
      this.controller.lockSurfaceRotation(false);
      this.controller.setVideoTrack(this.videoTrack);
      this.onTrackChanged?.(this.videoTrack);
      if (!this.canCapture() || this.source !== source || this.generation !== generation) { return; }
      this.startTimer = setTimeout(() => {
        this.startTimer = -1;
        if (this.source === source && this.generation === generation) { this.failed('Camera start timed out'); }
      }, 5000);
    } catch (e) {
      this.failed('Unable to open camera: ' + e);
    }
  }

  private syncRotation(): void {
    if (!this.source || !this.hostWindow || !this.canCapture()) { return; }
    const id = this.hostWindow.getWindowProperties().displayId;
    // Older window implementations may omit displayId. Only then use the
    // default display; never substitute it for a known secondary display.
    const currentDisplay = Number.isFinite(id) ? display.getDisplayByIdSync(id) : display.getDefaultDisplaySync();
    const angle = currentDisplay.rotation * 90;
    this.source.applyDisplayRotation?.(angle);
  }

  private monitorFrames(source: VideoSource, generation: number, previous: number): void {
    this.frameTimer = setTimeout(() => {
      this.frameTimer = -1;
      if (!this.canCapture() || this.source !== source || this.generation !== generation) { return; }
      try {
        const current = source.getCaptureFrameCount!();
        if (!Number.isFinite(current) || current === previous) {
          this.failed('Camera stopped delivering frames');
          return;
        }
        this.monitorFrames(source, generation, current);
      } catch (e) { this.failed('Unable to read capture frame count'); }
    }, this.frameStallTimeoutMs);
  }

  private onDisplayIdChange = (id: number): void => {
    this.controller.resetAppliedSurfaceSize();
    this.controller.lockSurfaceRotation(false);
    this.onDisplayChange();
  };

  private onDisplayChange = (): void => {
    try { this.syncRotation(); } catch (e) { this.failed('Unable to read window display'); }
  };

  private onWindowSizeChange = (): void => { this.onDisplayChange(); };

  private onWindowEvent = (event: window.WindowEventType): void => {
    if (event === window.WindowEventType.WINDOW_HIDDEN) {
      if (!this.visible) { return; }
      this.visible = false;
      this.reconcile();
    } else if (event === window.WindowEventType.WINDOW_SHOWN) {
      if (this.visible) { return; }
      this.visible = true;
      this.reconcile();
    }
  };

  private onFoldChange = (): void => {
    if (!this.canCapture()) { return; }
    this.retryCount = 0;
    this.schedule(520);
  };

  private onCameraFoldChange = (error: BusinessError, info: camera.FoldStatusInfo): void => {
    if (error) { this.emit('error', 'Camera device event failed: ' + error.code); return; }
    this.onFoldChange();
  };

  private onCameraStatus = (error: BusinessError, info: camera.CameraStatusInfo): void => {
    if (error || !this.canCapture()) { return; }
    // Opening our own camera changes AVAILABLE/UNAVAILABLE. Reacting to those
    // would create a close/open loop. Only topology changes trigger re-selection.
    if (info.status === camera.CameraStatus.CAMERA_STATUS_APPEAR ||
      info.status === camera.CameraStatus.CAMERA_STATUS_DISAPPEAR) {
      this.onFoldChange();
    }
  };

  private detachWindow(): void {
    if (!this.hostWindow) { return; }
    const host = this.hostWindow;
    this.hostWindow = undefined;
    this.visible = false;
    try { host.off('windowEvent', this.onWindowEvent); } catch (e) {
      Logging.e('CameraPreviewSession', 'Unable to remove window listener');
    }
    try {
      if (this.displayIdListening) {
        (host as Object as DisplayIdEventWindow).off('displayIdChange', this.onDisplayIdChange);
      }
    } catch (e) { Logging.e('CameraPreviewSession', 'Unable to remove display migration listener'); }
    this.displayIdListening = false;
    try { host.off('windowSizeChange', this.onWindowSizeChange); } catch (e) {
      Logging.e('CameraPreviewSession', 'Unable to remove window size listener');
    }
  }
}
