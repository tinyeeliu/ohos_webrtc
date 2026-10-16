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

export function previewAspectRatio(ratio: number, windowWidth: number, windowHeight: number): number {
  return windowHeight > windowWidth ? 1 / ratio : ratio;
}

// Rotation must be stable when width and height exchange. A wide but short
// phone window is still a small window; layout breakpoints only use width.
export function canRotateCameraWindow(widthPx: number, heightPx: number, density: number): boolean {
  return Number.isFinite(widthPx) && Number.isFinite(heightPx) && Number.isFinite(density) &&
    density > 0 && Math.min(widthPx, heightPx) / density >= 600;
}

export interface PreviewGeometry {
  width: number;
  height: number;
  surfaceWidth: number;
  surfaceHeight: number;
}

// Fit the viewfinder in the available area, then uniformly scale the video
// rendering buffer to cover it. Only the outer viewfinder clips; never stretch the video.
export function computePreviewGeometry(availableWidth: number, availableHeight: number,
  frameWidth: number, frameHeight: number, viewfinderRatio: number): PreviewGeometry {
  let width = Math.max(1, availableWidth);
  let height = Math.max(1, availableHeight);
  if (viewfinderRatio > 0) {
    width = Math.min(width, height * viewfinderRatio);
    height = width / viewfinderRatio;
  }
  width = Math.max(1, Math.floor(width));
  height = Math.max(1, Math.floor(height));
  const scale = Math.max(width / frameWidth, height / frameHeight);
  return { width: width, height: height,
    surfaceWidth: Math.max(1, Math.round(frameWidth * scale)),
    surfaceHeight: Math.max(1, Math.round(frameHeight * scale)) };
}
