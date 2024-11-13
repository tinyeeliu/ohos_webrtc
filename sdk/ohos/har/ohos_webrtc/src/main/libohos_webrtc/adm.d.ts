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

import { Event } from './event';

export type AudioErrorType = 'init' | 'start-exception' | 'start-state-mismatch' | 'general';

export type AudioState = 'start' | 'stop';

export interface AudioError extends Error {
  readonly type: AudioErrorType;
}

export interface AudioErrorEvent extends Event {
  readonly error: AudioError;
}

export interface AudioStateChangeEvent extends Event {
  readonly state: AudioState;
}

export interface AudioCapturerSamplesReadyEvent extends Event {
  readonly samples: AudioSamples;
}

export interface AudioDeviceModuleOptions {
  // input source. see ohos.multimedia.audio.SourceType, default is SOURCE_TYPE_VOICE_COMMUNICATION.
  audioSource?: number;

  // input format. see ohos.multimedia.audio.AudioSampleFormat, default SAMPLE_FORMAT_S16LE.
  audioFormat?: number;

  // input sample rate, default is 48000.
  inputSampleRate?: number;

  // Control if stereo input should be used or not. The default is mono.
  useStereoInput?: boolean;

  // output sample rate, default is 48000.
  outputSampleRate?: number;

  // Control if stereo output should be used or not. The default is mono.
  useStereoOutput?: boolean;

  // output audio usage. see ohos.multimedia.audio.StreamUsage, default is STREAM_USAGE_VOICE_COMMUNICATION
  rendererUsage?: number;

  // enable low latency capturing and rendering, default is false
  useLowLatency?: number;

  // Control if the built-in HW acoustic echo canceler should be used or not, default is false.
  // It is possible to query support by calling AudioDeviceModule.isBuiltInAcousticEchoCancelerSupported()
  useHardwareAcousticEchoCanceler?: boolean;

  // Control if the built-in HW noise suppressor should be used or not, default is false.
  // It is possible to query support by calling AudioDeviceModule.isBuiltInNoiseSuppressorSupported()
  useHardwareNoiseSuppressor?: boolean;
}

export interface AudioSamples {
  // See ohos.multimedia.audio.AudioSampleFormat
  readonly audioFormat: number;

  // See ohos.multimedia.audio.AudioChannel
  readonly channelCount: number;

  // See ohos.multimedia.audio.AudioSamplingRate
  readonly sampleRate: number;

  // audio data
  readonly data: ArrayBuffer;
}

export interface AudioDeviceModule {
  oncapturererror: ((this: any, event: AudioErrorEvent) => void) | null;
  oncapturerstatechange: ((this: any, event: AudioStateChangeEvent) => void) | null;
  // Called when new audio samples are ready. This should only be set for debug purposes
  oncapturersamplesready: ((this: any, event: AudioCapturerSamplesReadyEvent) => void) | null;
  onrenderererror: ((this: any, event: AudioErrorEvent) => void) | null;
  onrendererstatechange: ((this: any, event: AudioStateChangeEvent) => void) | null;

  setSpeakerMute(mute: boolean): void;
  setMicrophoneMute(mute: boolean): void;
  setNoiseSuppressorEnabled(enabled: boolean): boolean;
}

declare var AudioDeviceModule: {
  prototype: AudioDeviceModule;
  new(options?: AudioDeviceModuleOptions): AudioDeviceModule;
  isBuiltInAcousticEchoCancelerSupported(): boolean;
  isBuiltInNoiseSuppressorSupported(): boolean;
};