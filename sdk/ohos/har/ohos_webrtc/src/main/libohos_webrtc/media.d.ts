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

import { Event, EventTarget } from './event'

export type ConstrainBoolean = boolean | ConstrainBooleanParameters;
export type ConstrainULong = number | ConstrainULongRange;
export type ConstrainDouble = number | ConstrainDoubleRange;
export type ConstrainString = string | string[] | ConstrainStringParameters;

export type MediaStreamTrackState = 'live' | 'ended';
export type MediaDeviceKind = 'audioinput' | 'audiooutput' | 'videoinput';
export type MediaSourceState = 'initializing' | 'live' | 'ended' | 'muted';
export type VideoFacingModeEnum = "user" | "environment" | "left" | "right";
export type VideoResizeModeEnum = "none" | "crop-and-scale";

// https://www.w3.org/TR/mediacapture-streams/#mediastreamtrackevent
export interface MediaStreamTrackEvent extends Event {
  readonly track: MediaStreamTrack;
}

export interface VideoCapturerStartedEvent extends Event {
  readonly success: boolean;
}

export interface ULongRange {
  max?: number;
  min?: number;
}

export interface DoubleRange {
  max?: number;
  min?: number;
}

export interface ConstrainBooleanParameters {
  exact?: boolean;
  ideal?: boolean;
}

export interface ConstrainStringParameters {
  exact?: string | string[];
  ideal?: string | string[];
}

export interface ConstrainDoubleRange extends DoubleRange {
  exact?: number;
  ideal?: number;
}

export interface ConstrainULongRange extends ULongRange {
  exact?: number;
  ideal?: number;
}

// https://www.w3.org/TR/mediacapture-streams/#media-track-capabilities
export interface MediaTrackCapabilities {
  width?: ULongRange;
  height?: ULongRange;
  aspectRatio?: DoubleRange;
  frameRate?: DoubleRange;
  facingMode?: string[];
  resizeMode?: string[];
  sampleRate?: ULongRange;
  sampleSize?: ULongRange;
  echoCancellation?: boolean[];
  autoGainControl?: boolean[];
  noiseSuppression?: boolean[];
  latency?: DoubleRange;
  channelCount?: ULongRange;
  deviceId?: string;
  groupId?: string;
}

export interface MediaTrackConstraintSet {
  width?: ConstrainULong;
  height?: ConstrainULong;
  aspectRatio?: ConstrainDouble;
  frameRate?: ConstrainDouble;
  facingMode?: ConstrainString;
  resizeMode?: ConstrainString;
  sampleRate?: ConstrainULong;
  sampleSize?: ConstrainULong;
  echoCancellation?: ConstrainBoolean;
  autoGainControl?: ConstrainBoolean;
  noiseSuppression?: ConstrainBoolean;
  latency?: ConstrainDouble;
  channelCount?: ConstrainULong;
  deviceId?: ConstrainString;
  groupId?: ConstrainString;
}

// https://www.w3.org/TR/mediacapture-streams/#media-track-constraints
export interface MediaTrackConstraints extends MediaTrackConstraintSet {
  advanced?: MediaTrackConstraintSet[];
}

// https://www.w3.org/TR/mediacapture-streams/#media-track-settings
export interface MediaTrackSettings {
  width?: number;
  height?: number;
  aspectRatio?: number;
  frameRate?: number;
  facingMode?: string;
  resizeMode?: string;
  sampleRate?: number;
  sampleSize?: number;
  echoCancellation?: boolean;
  autoGainControl?: boolean;
  noiseSuppression?: boolean;
  latency?: number;
  channelCount?: number;
  deviceId?: string;
  groupId?: string;
}

// https://www.w3.org/TR/mediacapture-streams/#media-track-supported-constraints
export interface MediaTrackSupportedConstraints {
  width?: boolean;
  height?: boolean;
  aspectRatio?: boolean;
  frameRate?: boolean;
  facingMode?: boolean;
  resizeMode?: boolean;
  sampleRate?: boolean;
  sampleSize?: boolean;
  echoCancellation?: boolean;
  autoGainControl?: boolean;
  noiseSuppression?: boolean;
  latency?: boolean;
  channelCount?: boolean;
  deviceId?: boolean;
  groupId?: boolean;
}

export interface MediaStreamConstraints {
  // default is false
  video?: boolean | MediaTrackConstraints;
  // default is false
  audio?: boolean | MediaTrackConstraints;
}

// https://www.w3.org/TR/screen-capture/#displaymediastreamoptions
export interface DisplayMediaStreamOptions {
  video?: boolean | MediaTrackConstraints;
  audio?: boolean | MediaTrackConstraints;
}

export interface MediaSource {
  readonly state: MediaSourceState;
}

export interface AudioSource extends MediaSource {
  setVolume(volume: number);
}

export interface VideoSource extends MediaSource {
  oncapturerstarted: ((this: VideoSource, ev: VideoCapturerStartedEvent) => any) | null;
  oncapturerstopped: ((this: VideoSource, ev: Event) => any) | null;
}

// https://www.w3.org/TR/mediacapture-streams/#mediastreamtrack
export interface MediaStreamTrack extends EventTarget {
  readonly kind: string;
  readonly id: string;
  enabled: boolean;
  readonly readyState: MediaStreamTrackState;

  stop(): void;
}

export declare var MediaStreamTrack: {
  prototype: MediaStreamTrack;
  new(): MediaStreamTrack;
};

export interface AudioTrack extends MediaStreamTrack {
}

export interface VideoTrack extends MediaStreamTrack {
}

// https://www.w3.org/TR/mediacapture-streams/#mediastream
export interface MediaStream extends EventTarget {
  readonly id: string;
  readonly active: boolean;

  onaddtrack: ((this: MediaStream, ev: MediaStreamTrackEvent) => any) | null;
  onremovetrack: ((this: MediaStream, ev: MediaStreamTrackEvent) => any) | null;

  addTrack(track: MediaStreamTrack): void;
  removeTrack(track: MediaStreamTrack): void;
  getTrackById(trackId: string): MediaStreamTrack | null;
  getTracks(): MediaStreamTrack[];
  getAudioTracks(): MediaStreamTrack[];
  getVideoTracks(): MediaStreamTrack[];
  clone(): MediaStream;
}

declare var MediaStream: {
  prototype: MediaStream;
  new(): MediaStream;
  new(stream: MediaStream): MediaStream;
  new(tracks: MediaStreamTrack[]): MediaStream;
};

export interface MediaDeviceInfo {
  readonly deviceId: string;
  readonly kind:MediaDeviceKind;
  readonly label: string;
  readonly groupId: string;
}

export interface DeviceChangeEvent extends Event {
  readonly devices: ReadonlyArray<MediaDeviceInfo>;
  readonly userInsertedDevices: ReadonlyArray<MediaDeviceInfo>;
}

// https://www.w3.org/TR/mediacapture-streams/#mediadevices
export interface MediaDevices extends EventTarget {
  ondevicechange: (this: MediaDevices, event: DeviceChangeEvent) => void | null;

  enumerateDevices(): Promise<MediaDeviceInfo[]>;
  getSupportedConstraints(): MediaTrackSupportedConstraints;
  getUserMedia(constraints?: MediaStreamConstraints): Promise<MediaStream>;
  getDisplayMedia(options?: DisplayMediaStreamOptions): Promise<MediaStream>;
}

declare var MediaDevices: {
  prototype: MediaDevices;
  new(): MediaDevices;
};

export interface NativeVideoRenderer {
  readonly surfaceId?: string;
  readonly videoTrack?: MediaStreamTrack;

  init(surfaceId: string): void;
  setVideoTrack(videoTrack: MediaStreamTrack | null): void;
  release(): void;

  // onSurfaceCreated(surfaceId: string): void;
  // onSurfaceChanged(surfaceId: string, x: number, y: number, width: number, height: number): void;
  // onSurfaceDestroyed(surfaceId: string);
}

declare var NativeVideoRenderer: {
  prototype: NativeVideoRenderer;
  new(): NativeVideoRenderer;
};
