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

import { NativeVideoRenderer, MediaStreamTrack, Event as WebRtcEvent,
  FrameResolutionChangedEvent } from 'libohos_webrtc.so';
import { Logging } from '../log/Logging'

const TAG: string = '[XComponentController1]';

export enum ScalingMode { Fill, AspectFill, AspectFit }

export class VideoRenderController extends XComponentController {
  /** 对外控制器通知：旋转后的显示帧宽高（像素），不是 Surface 尺寸。 */
  onFrameSizeChanged?: (width: number, height: number) => void;
  private renderer?: NativeVideoRenderer;
  private track: MediaStreamTrack | null = null;
  private mirror: boolean = false;
  private mirrorVertically: boolean = false;
  private mode: ScalingMode = ScalingMode.Fill;
  private appliedSurfaceWidth: number = 0;
  private appliedSurfaceHeight: number = 0;
  private reportedMissingResizeApi: boolean = false;
  private displayedFrameWidth: number = 0;
  private displayedFrameHeight: number = 0;
  private cameraPreviewAdaptation: boolean = false;

  /** New camera sessions opt in; existing views retain their Surface behavior. */
  enableCameraPreviewAdaptation(): void {
    this.cameraPreviewAdaptation = true;
    this.lockSurfaceRotation(!this.cameraPreviewAdaptation);
    if (this.renderer) {
      const rect = this.getXComponentSurfaceRect();
      if (rect.surfaceWidth > 0 && rect.surfaceHeight > 0) {
        this.updateNativeSurfaceSize(Math.round(rect.surfaceWidth), Math.round(rect.surfaceHeight));
      }
    }
  }

  setVideoTrack(track: MediaStreamTrack | null): void {
    if (this.cameraPreviewAdaptation && this.track === track) {
      return;
    }
    this.renderer?.setVideoTrack(track);
    this.track = track;
    this.displayedFrameWidth = 0;
    this.displayedFrameHeight = 0;
  }

  setMirror(mirror: boolean): void {
    this.mirror = mirror;
    this.renderer?.setMirror(mirror);
  }

  setMirrorVertically(mirrorVertically: boolean): void {
    this.mirrorVertically = mirrorVertically;
    this.renderer?.setMirrorVertically(mirrorVertically);
  }

  setScalingMode(mode: ScalingMode): void {
    this.mode = mode;
    this.renderer?.setScalingMode(mode);
  }

  /**
   * Existing callers keep the explicit lock by default. Adapted WebRTC frames
   * already carry their orientation and must follow the normal window Surface
   * transform, rather than retaining the camera-direct-preview rotation lock.
   */
  lockSurfaceRotation(lock: boolean = true): void {
    try {
      let rotationOptions: SurfaceRotationOptions = { lock: lock };
      this.setXComponentSurfaceRotation(rotationOptions);
    } catch (e) {
      Logging.e(TAG, 'lockSurfaceRotation failed');
    }
  }

  /** 旋转结束后清缓存，避免 setPreviewSurfaceSize 因尺寸巧合相同而跳过真正刷新。 */
  resetAppliedSurfaceSize(): void {
    this.appliedSurfaceWidth = 0;
    this.appliedSurfaceHeight = 0;
  }

  private updateNativeSurfaceSize(width: number, height: number): void {
    if (!this.renderer) {
      return;
    }
    // Older prebuilt libraries must not crash the Surface lifecycle callback.
    if (typeof this.renderer.setSurfaceSize !== 'function') {
      if (!this.reportedMissingResizeApi) {
        Logging.e(TAG, 'Preview resize requires rebuilding libohos_webrtc.so with setSurfaceSize');
        this.reportedMissingResizeApi = true;
      }
      return;
    }
    this.renderer.setSurfaceSize(width, height);
  }

  setPreviewSurfaceSize(surfaceWidth: number, surfaceHeight: number): void {
    if (surfaceWidth <= 0 || surfaceHeight <= 0) {
      return;
    }
    if (surfaceWidth === this.appliedSurfaceWidth && surfaceHeight === this.appliedSurfaceHeight) {
      return;
    }
    try {
      let rect: SurfaceRect = {
        surfaceWidth: surfaceWidth,
        surfaceHeight: surfaceHeight
      };
      this.setXComponentSurfaceRect(rect);
      this.updateNativeSurfaceSize(surfaceWidth, surfaceHeight);
      this.appliedSurfaceWidth = surfaceWidth;
      this.appliedSurfaceHeight = surfaceHeight;
    } catch (e) {
      Logging.e(TAG, 'setPreviewSurfaceSize failed');
    }
  }

  /**
   * 通话页小窗：按采集分辨率做 AspectFit，不对调宽高。
   * 小窗不是相机全屏预览，套官方 0°/180° 对调会把画面裁错。
   */
  applyPreviewSurfaceRect(windowWidth: number, windowHeight: number, streamWidth: number, streamHeight: number): void {
    if (streamWidth <= 0 || streamHeight <= 0 || windowWidth <= 0 || windowHeight <= 0) {
      return;
    }
    let scale: number = Math.min(windowWidth / streamWidth, windowHeight / streamHeight);
    this.setPreviewSurfaceSize(Math.floor(streamWidth * scale), Math.floor(streamHeight * scale));
  }

  /** Fit the actual oriented frame; no device natural-orientation assumptions. */
  applyOrientedPreviewSurfaceRect(windowWidth: number, windowHeight: number, streamWidth: number,
    streamHeight: number): void {
    this.lockSurfaceRotation(!this.cameraPreviewAdaptation);
    let srcW: number = this.displayedFrameWidth || streamWidth;
    let srcH: number = this.displayedFrameHeight || streamHeight;
    if (srcW <= 0 || srcH <= 0 || windowWidth <= 0 || windowHeight <= 0) {
      return;
    }
    let scale: number = Math.min(windowWidth / srcW, windowHeight / srcH);
    this.setPreviewSurfaceSize(Math.floor(srcW * scale), Math.floor(srcH * scale));
  }

  onSurfaceCreated(surfaceId: string): void {
    Logging.d(TAG, 'onSurfaceCreated surfaceId: ' + surfaceId);
    this.appliedSurfaceWidth = 0;
    this.appliedSurfaceHeight = 0;
    if (this.cameraPreviewAdaptation) { this.lockSurfaceRotation(!this.cameraPreviewAdaptation); }
    this.renderer = new NativeVideoRenderer();
    this.renderer.init(surfaceId);
    const renderer = this.renderer;
    this.renderer.onFrameResolutionChanged = (rawEvent: WebRtcEvent) => {
      if (this.renderer !== renderer) { return; }
      const event = rawEvent as FrameResolutionChangedEvent;
      if (!Number.isFinite(event.width) || !Number.isFinite(event.height) ||
        event.width <= 0 || event.height <= 0 ||
        (event.rotation !== 0 && event.rotation !== 90 && event.rotation !== 180 && event.rotation !== 270)) {
        Logging.e(TAG, 'Invalid native frame resolution event');
        return;
      }
      const swap = event.rotation === 90 || event.rotation === 270;
      Logging.d(TAG, `[VideoFrameGeometry] surface=${surfaceId} track=${this.track?.id ?? ''} ` +
        `buffer=${event.width}x${event.height} rotation=${event.rotation}`);
      this.displayedFrameWidth = swap ? event.height : event.width;
      this.displayedFrameHeight = swap ? event.width : event.height;
      this.onFrameSizeChanged?.(this.displayedFrameWidth, this.displayedFrameHeight);
    };
    this.renderer.setVideoTrack(null);
    this.renderer.setVideoTrack(this.track);
    this.renderer.setMirror(this.mirror);
    this.renderer.setMirrorVertically(this.mirrorVertically);
    this.renderer.setScalingMode(this.mode);
    const rect = this.getXComponentSurfaceRect();
    if (this.cameraPreviewAdaptation && rect.surfaceWidth > 0 && rect.surfaceHeight > 0) {
      this.updateNativeSurfaceSize(Math.round(rect.surfaceWidth), Math.round(rect.surfaceHeight));
    }
  }

  onSurfaceChanged(surfaceId: string, rect: SurfaceRect): void {
    if (this.cameraPreviewAdaptation) { this.lockSurfaceRotation(!this.cameraPreviewAdaptation); }
    Logging.d(TAG, 'onSurfaceChanged ' + rect.surfaceWidth + 'x' + rect.surfaceHeight);
    // ArkUI bounds and EGL buffer size can change at different times.
    // Send the display size without detaching the capture sink.
    if (this.cameraPreviewAdaptation && rect.surfaceWidth > 0 && rect.surfaceHeight > 0) {
      this.updateNativeSurfaceSize(Math.round(rect.surfaceWidth), Math.round(rect.surfaceHeight));
    }
  }

  onSurfaceDestroyed(surfaceId: string): void {
    Logging.d(TAG, 'onSurfaceDestroyed surfaceId: ' + surfaceId);
    this.appliedSurfaceWidth = 0;
    this.appliedSurfaceHeight = 0;
    this.renderer?.setVideoTrack(null);
    this.renderer?.release();
    this.renderer = undefined;
  }
}
