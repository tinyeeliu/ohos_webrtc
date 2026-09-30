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

export interface HoverRect {
  left: number;
  top: number;
  width: number;
  height: number;
}

export interface HoverGeometry {
  active: boolean;
  fallback: boolean;
  preview: HoverRect;
  controls: HoverRect;
  gap: HoverRect;
  gaps: HoverRect[];
}

export function resolveHoverPreviewSide(saved: string): string {
  if (saved === 'left' || saved === 'right') {
    return saved;
  }
  // Physical holding direction is handled by the window rotation policy,
  // not by changing the logical side for a particular device model.
  return 'right';
}

interface HoverPartitions {
  regions: HoverRect[];
  gaps: HoverRect[];
}

function splitHoverRegions(regions: HoverRect[], horizontal: boolean, top: number, left: number,
  bottom: number, right: number): HoverRect[] {
  const next: HoverRect[] = [];
  for (const region of regions) {
    const start = horizontal ? region.top : region.left;
    const end = start + (horizontal ? region.height : region.width);
    const cutStart = horizontal ? top : left;
    const cutEnd = horizontal ? bottom : right;
    if (cutEnd <= start || cutStart >= end) {
      next.push(region);
      continue;
    }
    if (cutStart > start) {
      next.push(horizontal ? { left: region.left, top: start, width: region.width, height: cutStart - start } :
        { left: start, top: region.top, width: cutStart - start, height: region.height });
    }
    if (cutEnd < end) {
      next.push(horizontal ? { left: region.left, top: cutEnd, width: region.width, height: end - cutEnd } :
        { left: cutEnd, top: region.top, width: end - cutEnd, height: region.height });
    }
  }
  return next;
}

function partitionHoverRegions(content: HoverRect, creases: HoverRect[], padding: number): HoverPartitions {
  // Split every remaining region at every crease. Project each crease across
  // its axis, matching the single-crease layout while keeping all panes safe.
  let regions: HoverRect[] = [{ left: 0, top: 0, width: content.width, height: content.height }];
  const gaps: HoverRect[] = [];
  for (const crease of creases) {
    const left = Math.max(0, crease.left - content.left - padding);
    const top = Math.max(0, crease.top - content.top - padding);
    const right = Math.min(content.width, crease.left + crease.width - content.left + padding);
    const bottom = Math.min(content.height, crease.top + crease.height - content.top + padding);
    if (right <= left || bottom <= top) {
      continue;
    }
    const horizontal = crease.width >= crease.height;
    gaps.push(horizontal ? { left: 0, top: top, width: content.width, height: bottom - top } :
      { left: left, top: 0, width: right - left, height: content.height });
    regions = splitHoverRegions(regions, horizontal, top, left, bottom, right);
  }
  return { regions: regions, gaps: gaps };
}

function selectHoverPanes(regions: HoverRect[], previewSide: string, result: HoverGeometry): void {
  regions.sort((a: HoverRect, b: HoverRect): number => a.top - b.top || a.left - b.left);
  // Prefer two useful panes; on equal areas keep them far apart. Input crease
  // ordering must not decide which pane the user sees.
  let bestArea = -1;
  let bestTotal = -1;
  let bestDistance = -1;
  for (let i = 0; i < regions.length; i++) {
    for (let j = i + 1; j < regions.length; j++) {
      const a = regions[i];
      const b = regions[j];
      const area = Math.min(a.width * a.height, b.width * b.height);
      const total = a.width * a.height + b.width * b.height;
      const dx = a.left + a.width / 2 - b.left - b.width / 2;
      const dy = a.top + a.height / 2 - b.top - b.height / 2;
      const distance = dx * dx + dy * dy;
      if (area < bestArea || (area === bestArea && total < bestTotal) ||
        (area === bestArea && total === bestTotal && distance <= bestDistance)) {
        continue;
      }
      bestArea = area;
      bestTotal = total;
      bestDistance = distance;
      const sideBySide = a.left + a.width <= b.left || b.left + b.width <= a.left;
      const first = sideBySide ? (a.left < b.left ? a : b) : (a.top < b.top ? a : b);
      const second = first === a ? b : a;
      result.preview = sideBySide && previewSide !== 'left' ? second : first;
      result.controls = result.preview === first ? second : first;
    }
  }
}

// All inputs are screen pixels. Results are relative to the page content origin.
// Use the API's screen coordinates; do not infer the hinge from window aspect.
export function computeHoverLayout(content: HoverRect, creases: HoverRect[], halfFolded: boolean,
  padding: number, previewSide: string = 'right'): HoverGeometry {
  const full: HoverRect = { left: 0, top: 0, width: content.width, height: content.height };
  const empty: HoverRect = { left: 0, top: 0, width: 0, height: 0 };
  const result: HoverGeometry = { active: false, fallback: false, preview: full, controls: full, gap: empty, gaps: [] };
  if (!halfFolded || content.width <= 0 || content.height <= 0) {
    return result;
  }
  if (creases.length === 0) {
    // No guessed midline: wait for live geometry before placing either region.
    return { active: true, fallback: true, preview: empty, controls: empty, gap: empty, gaps: [] };
  }
  const partitions = partitionHoverRegions(content, creases, padding);
  const regions = partitions.regions;
  const gaps = partitions.gaps;
  if (gaps.length === 0) {
    return result;
  }
  result.active = true;
  result.gaps = gaps;
  result.gap = gaps[0];
  if (regions.length < 2) {
    result.fallback = true;
    result.preview = empty;
    result.controls = regions.length === 1 ? regions[0] : empty;
    return result;
  }
  selectHoverPanes(regions, previewSide, result);
  return result;
}
