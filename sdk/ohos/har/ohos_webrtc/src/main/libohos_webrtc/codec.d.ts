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

export interface VideoEncoderFactory {}

export interface VideoDecoderFactory {}

export interface HardwareVideoEncoderFactory extends VideoEncoderFactory {
  readonly enableH264HighProfile: boolean;
}

declare var HardwareVideoEncoderFactory: {
  prototype: HardwareVideoEncoderFactory;
  new(enableH264HighProfile?: boolean): HardwareVideoEncoderFactory;
}

export interface SoftwareVideoEncoderFactory extends VideoEncoderFactory {
}

declare var SoftwareVideoEncoderFactory: {
  prototype: SoftwareVideoEncoderFactory;
  new(): SoftwareVideoEncoderFactory;
}

export interface HardwareVideoDecoderFactory extends VideoDecoderFactory {
}

declare var HardwareVideoDecoderFactory: {
  prototype: HardwareVideoDecoderFactory;
  new(): HardwareVideoDecoderFactory;
}

export interface SoftwareVideoDecoderFactory extends VideoDecoderFactory {
}

declare var SoftwareVideoDecoderFactory: {
  prototype: SoftwareVideoDecoderFactory;
  new(): SoftwareVideoDecoderFactory;
}
