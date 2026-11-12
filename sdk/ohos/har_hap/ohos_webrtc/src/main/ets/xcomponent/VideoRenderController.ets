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

/** Frame scaling applied to the rendered video. */
export enum ScalingMode { Fill, AspectFill, AspectFit }

/**
 * XComponent controller that owns a NativeVideoRenderer for video track display.
 *
 * All native renderer calls are failure-tolerant: the XComponent framework
 * invokes {@link onSurfaceCreated}/{@link onSurfaceChanged}/{@link onSurfaceDestroyed}
 * directly, so a native exception there would crash the application; it is
 * caught and logged instead.
 */
export class VideoRenderController extends XComponentController {
  /** 对外控制器通知：旋转后的显示帧宽高（像素），不是渲染缓冲区尺寸。 */
  public onFrameSizeChanged?: (width: number, height: number) => void;
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

  /**
   * Opts new camera preview sessions into the window rendering buffer transform.
   * Existing views keep their explicit rotation lock and rendering buffer behavior.
   */
  enableCameraPreviewAdaptation(): void {
    this.cameraPreviewAdaptation = true;
    this.lockSurfaceRotation(!this.cameraPreviewAdaptation);
    try {
      const rect = this.getXComponentSurfaceRect();
      if (this.renderer && rect.surfaceWidth > 0 && rect.surfaceHeight > 0) {
        this.updateNativeSurfaceSize(Math.round(rect.surfaceWidth), Math.round(rect.surfaceHeight));
      }
    } catch (e) {
      Logging.e(TAG, 'enableCameraPreviewAdaptation failed: ' + (e instanceof Error ? e.message : 'unknown error'));
    }
  }

  /**
   * Attaches the track to render, or detaches with null.
   *
   * @param track track to render, or null to clear the current one.
   */
  setVideoTrack(track: MediaStreamTrack | null): void {
    if (track !== null && !(track instanceof MediaStreamTrack)) {
      Logging.e(TAG, 'setVideoTrack ignored: track is not a MediaStreamTrack');
      return;
    }
    if (this.cameraPreviewAdaptation && this.track === track) {
      return;
    }
    try {
      this.renderer?.setVideoTrack(track);
    } catch (e) {
      Logging.e(TAG, 'setVideoTrack failed: ' + (e instanceof Error ? e.message : 'unknown error'));
      return;
    }
    this.track = track;
    this.displayedFrameWidth = 0;
    this.displayedFrameHeight = 0;
  }

  /** Enables or disables horizontal mirroring of the rendered frame. */
  setMirror(mirror: boolean): void {
    try {
      this.renderer?.setMirror(mirror);
    } catch (e) {
      Logging.e(TAG, 'setMirror failed: ' + (e instanceof Error ? e.message : 'unknown error'));
      return;
    }
    this.mirror = mirror;
  }

  /** Enables or disables vertical mirroring of the rendered frame. */
  setMirrorVertically(mirrorVertically: boolean): void {
    try {
      this.renderer?.setMirrorVertically(mirrorVertically);
    } catch (e) {
      Logging.e(TAG, 'setMirrorVertically failed: ' + (e instanceof Error ? e.message : 'unknown error'));
      return;
    }
    this.mirrorVertically = mirrorVertically;
  }

  /**
   * Sets the frame scaling mode.
   *
   * @param mode one of {@link ScalingMode}; out-of-range values are ignored.
   */
  setScalingMode(mode: ScalingMode): void {
    if (mode < ScalingMode.Fill || mode > ScalingMode.AspectFit) {
      Logging.e(TAG, 'setScalingMode ignored: invalid mode ' + mode);
      return;
    }
    try {
      this.renderer?.setScalingMode(mode);
    } catch (e) {
      Logging.e(TAG, 'setScalingMode failed: ' + (e instanceof Error ? e.message : 'unknown error'));
      return;
    }
    this.mode = mode;
  }

  /**
   * Locks or unlocks the surface rotation.
   *
   * Existing callers keep the explicit lock by default. Adapted WebRTC frames
   * already carry their orientation and must follow the normal window rendering buffer
   * transform, rather than retaining the camera-direct-preview rotation lock.
   */
  lockSurfaceRotation(lock: boolean = true): void {
    try {
      let rotationOptions: SurfaceRotationOptions = { lock: lock };
      this.setXComponentSurfaceRotation(rotationOptions);
    } catch (e) {
      Logging.e(TAG, 'lockSurfaceRotation failed: ' + (e instanceof Error ? e.message : 'unknown error'));
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
    // Older prebuilt libraries must not crash the rendering buffer lifecycle callback.
    if (typeof this.renderer.setSurfaceSize !== 'function') {
      if (!this.reportedMissingResizeApi) {
        Logging.e(TAG, 'Preview resize requires rebuilding libohos_webrtc.so with setSurfaceSize');
        this.reportedMissingResizeApi = true;
      }
      return;
    }
    try {
      this.renderer.setSurfaceSize(width, height);
    } catch (e) {
      Logging.e(TAG, 'setSurfaceSize failed: ' + (e instanceof Error ? e.message : 'unknown error'));
    }
  }

  /**
   * Sets the EGL surface size, skipping non-positive values and no-op repeats.
   *
   * @param surfaceWidth target surface width in pixels.
   * @param surfaceHeight target surface height in pixels.
   */
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
      Logging.e(TAG, 'setPreviewSurfaceSize failed: ' + (e instanceof Error ? e.message : 'unknown error'));
    }
  }

  /**
   * 通话页小窗：按采集分辨率做 AspectFit，不对调宽高。
   * 小窗不是相机全屏预览，套官方 0°/180° 对调会把画面裁错。
   *
   * @param windowWidth available window width in pixels.
   * @param windowHeight available window height in pixels.
   * @param streamWidth captured stream width in pixels.
   * @param streamHeight captured stream height in pixels.
   */
  applyPreviewSurfaceRect(windowWidth: number, windowHeight: number, streamWidth: number, streamHeight: number): void {
    if (streamWidth <= 0 || streamHeight <= 0 || windowWidth <= 0 || windowHeight <= 0) {
      return;
    }
    let scale: number = Math.min(windowWidth / streamWidth, windowHeight / streamHeight);
    this.setPreviewSurfaceSize(Math.floor(streamWidth * scale), Math.floor(streamHeight * scale));
  }

  /**
   * Fits the actual oriented frame without assuming a device natural
   * orientation; the current display dimensions take precedence over the
   * nominal stream size.
   *
   * @param windowWidth available window width in pixels.
   * @param windowHeight available window height in pixels.
   * @param streamWidth captured stream width in pixels, used as a fallback
   * when no frame has been displayed yet.
   * @param streamHeight captured stream height in pixels, used as a fallback
   * when no frame has been displayed yet.
   */
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

  /**
   * Creates and initializes the native renderer for a new surface.
   * Framework callback: native failures are caught, logged, and cleaned up
   * so the render pipeline stays usable.
   */
  onSurfaceCreated(surfaceId: string): void {
    Logging.d(TAG, 'onSurfaceCreated surfaceId: ' + surfaceId);
    this.appliedSurfaceWidth = 0;
    this.appliedSurfaceHeight = 0;
    if (this.cameraPreviewAdaptation) { this.lockSurfaceRotation(!this.cameraPreviewAdaptation); }
    try {
      this.renderer = new NativeVideoRenderer();
      const renderer = this.renderer;
      renderer.init(surfaceId);
      renderer.onFrameResolutionChanged = (rawEvent: WebRtcEvent) => {
        if (this.renderer !== renderer) { return; }
        const event = rawEvent as FrameResolutionChangedEvent;
        if (!Number.isFinite(event.width) || !Number.isFinite(event.height) ||
          event.width <= 0 || event.height <= 0 ||
          (event.rotation !== 0 && event.rotation !== 90 && event.rotation !== 180 && event.rotation !== 270)) {
          Logging.e(TAG, 'Invalid native frame resolution event');
          return;
        }
        const swap = event.rotation === 90 || event.rotation === 270;
        Logging.d(TAG, `[VideoFrameGeometry] renderTarget=${surfaceId} track=${this.track?.id ?? ''} ` +
          `buffer=${event.width}x${event.height} rotation=${event.rotation}`);
        this.displayedFrameWidth = swap ? event.height : event.width;
        this.displayedFrameHeight = swap ? event.width : event.height;
        this.onFrameSizeChanged?.(this.displayedFrameWidth, this.displayedFrameHeight);
      };
      renderer.setVideoTrack(null);
      renderer.setVideoTrack(this.track);
      renderer.setMirror(this.mirror);
      renderer.setMirrorVertically(this.mirrorVertically);
      renderer.setScalingMode(this.mode);
      const rect = this.getXComponentSurfaceRect();
      if (this.cameraPreviewAdaptation && rect.surfaceWidth > 0 && rect.surfaceHeight > 0) {
        this.updateNativeSurfaceSize(Math.round(rect.surfaceWidth), Math.round(rect.surfaceHeight));
      }
    } catch (e) {
      Logging.e(TAG, 'onSurfaceCreated failed: ' + (e instanceof Error ? e.message : 'unknown error'));
      try {
        this.renderer?.release();
      } catch (ignored) {
        // The renderer may be partially initialized; the reference is dropped below.
        Logging.d(TAG, 'renderer cleanup during failed onSurfaceCreated: ' +
          (ignored instanceof Error ? ignored.message : 'unknown error'));
      }
      this.renderer = undefined;
    }
  }

  /** Framework callback: forwards the new surface size to the native renderer. */
  onSurfaceChanged(surfaceId: string, rect: SurfaceRect): void {
    if (this.cameraPreviewAdaptation) { this.lockSurfaceRotation(!this.cameraPreviewAdaptation); }
    Logging.d(TAG, 'onSurfaceChanged ' + rect.surfaceWidth + 'x' + rect.surfaceHeight);
    // ArkUI bounds and EGL buffer size can change at different times.
    // Send the display size without detaching the capture sink.
    if (this.cameraPreviewAdaptation && rect.surfaceWidth > 0 && rect.surfaceHeight > 0) {
      this.updateNativeSurfaceSize(Math.round(rect.surfaceWidth), Math.round(rect.surfaceHeight));
    }
  }

  /** Framework callback: releases the native renderer; always clears the reference. */
  onSurfaceDestroyed(surfaceId: string): void {
    Logging.d(TAG, 'onSurfaceDestroyed surfaceId: ' + surfaceId);
    this.appliedSurfaceWidth = 0;
    this.appliedSurfaceHeight = 0;
    const renderer = this.renderer;
    this.renderer = undefined;
    try {
      renderer?.setVideoTrack(null);
      renderer?.release();
    } catch (e) {
      Logging.e(TAG, 'onSurfaceDestroyed failed: ' + (e instanceof Error ? e.message : 'unknown error'));
    }
  }
}
