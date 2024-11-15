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

import { RTCConfiguration, RTCPeerConnection } from './core'
import { MediaTrackConstraints, AudioSource, AudioTrack, VideoSource, VideoTrack } from './media'
import { AudioDeviceModule } from './adm'
import { VideoEncoderFactory, VideoDecoderFactory } from './codec'
import { AudioProcessing } from './audio_processing';

export interface PeerConnectionFactoryOptions {
  adm?: AudioDeviceModule;
  videoEncoderFactory?: VideoEncoderFactory;
  videoDecoderFactory?: VideoDecoderFactory;
  audioProcessing?: AudioProcessing;
}

export interface PeerConnectionFactory {
  createPeerConnection(config: RTCConfiguration): RTCPeerConnection;
  createAudioSource(constraints?: MediaTrackConstraints): AudioSource;
  createAudioTrack(id: string, source: AudioSource): AudioTrack;
  createVideoSource(constraints?: MediaTrackConstraints, isScreencast?: boolean): VideoSource;
  createVideoTrack(id: string, source: VideoSource): VideoTrack;
  startAecDump(fd: number, max_size_bytes: number): boolean;
  stopAecDump(): void;
}

declare var PeerConnectionFactory: {
  prototype: PeerConnectionFactory;
  new(options?: PeerConnectionFactoryOptions): PeerConnectionFactory;
  getDefault(): PeerConnectionFactory;
  setDefault(factory: PeerConnectionFactory): void;
}
