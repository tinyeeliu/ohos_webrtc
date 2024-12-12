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
 
import { expect } from '@ohos/hypium';
import webrtc from 'libohos_webrtc.so';
import * as helper from './Helper';

export function validateRtpCapabilities(capabilities: webrtc.RTCRtpCapabilities) {
  helper.assertArray(capabilities.codecs);
  for(const codec of capabilities.codecs) {
    validateCodecCapability(codec);
  }
  expect(capabilities.codecs.length).assertLarger(0);

  helper.assertArray(capabilities.headerExtensions);
  for(const headerExt of capabilities.headerExtensions) {
    validateHeaderExtensionCapability(headerExt);
  }
}

function validateCodecCapability(codec: webrtc.RTCRtpCodec) {
  if (codec.mimeType !== undefined) {
    expect(typeof codec.mimeType).assertEqual('string');
  }
  if (codec.clockRate !== undefined) {
    expect(Number.isInteger(codec.clockRate) && codec.clockRate > 0).assertTrue();
  }
  if (codec.channels !== undefined) {
    expect(Number.isInteger(codec.channels) && codec.channels > 0).assertTrue();
  }
  if (codec.sdpFmtpLine !== undefined) {
    expect(typeof codec.sdpFmtpLine).assertEqual('string');
  }
}

function validateHeaderExtensionCapability(headerExt: webrtc.RTCRtpHeaderExtensionCapability) {
  helper.assertOptionalString(headerExt.uri);
}
