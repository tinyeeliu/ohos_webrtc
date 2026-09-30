/**
 * Copyright (c) 2026 Archermind Technology (Nanjing) Co. Ltd.
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

import window from '@ohos.window';
import display from '@ohos.display';

// Local structural types keep optional API 14/19/20 names out of SDK 12 declarations.
// Values follow the system rotation event protocol (API 19).
export const PREVIEW_WILL_ROTATE: number = 0;
export const PREVIEW_RECT_SCREEN: number = 0;
export interface PreviewRotationInfo { type: number; }
export interface PreviewRotationResult { rectType: number; windowRect: window.Rect; }
interface RotationWindow {
  on(type: 'rotationChange', callback: (info: PreviewRotationInfo) => PreviewRotationResult | void): void;
  off(type: 'rotationChange', callback: (info: PreviewRotationInfo) => PreviewRotationResult | void): void;
}
interface DisplayIdWindow {
  on(type: 'displayIdChange', callback: () => void): void;
  off(type: 'displayIdChange', callback: () => void): void;
}
interface LiveCreaseDisplay {
  getLiveCreaseRegion?: () => display.FoldCreaseRegion;
}

export function tryPreviewPlatformAction(action: () => void, operation: string = 'optional platform action'): boolean {
  try { action(); return true; } catch (e) {
    console.warn(`[PreviewPlatformCompat] ${operation} failed: ${(e as Error).message}`);
    return false;
  }
}

export class PreviewOptionalWindowEvents {
  private rotationBound: boolean = false;
  private displayBound: boolean = false;

  bind(host: window.Window, rotation: (info: PreviewRotationInfo) => PreviewRotationResult | void,
    displayChanged: () => void): void {
    this.rotationBound = tryPreviewPlatformAction(() => {
      (host as Object as RotationWindow).on('rotationChange', rotation);
    }, 'register rotationChange');
    this.displayBound = tryPreviewPlatformAction(() => {
      (host as Object as DisplayIdWindow).on('displayIdChange', displayChanged);
    }, 'register displayIdChange');
  }

  unbind(host: window.Window, rotation: (info: PreviewRotationInfo) => PreviewRotationResult | void,
    displayChanged: () => void): void {
    if (this.rotationBound) {
      tryPreviewPlatformAction(() => { (host as Object as RotationWindow).off('rotationChange', rotation); }, 'remove rotationChange');
    }
    if (this.displayBound) {
      tryPreviewPlatformAction(() => { (host as Object as DisplayIdWindow).off('displayIdChange', displayChanged); }, 'remove displayIdChange');
    }
    this.rotationBound = false;
    this.displayBound = false;
  }
}

export function previewLiveCreases(screen: display.Display): display.Rect[] {
  try {
    const optional = screen as Object as LiveCreaseDisplay;
    if (typeof optional.getLiveCreaseRegion === 'function') {
      const region = optional.getLiveCreaseRegion();
      if (region.displayId === screen.id) { return region.creaseRects; }
    }
  } catch (e) {
    console.warn(`[PreviewPlatformCompat] Live crease query failed: ${(e as Error).message}`);
  }
  // Older static crease geometry does not guarantee current rotated coordinates.
  // An empty result activates the page's existing safe waiting layout.
  return [];
}
