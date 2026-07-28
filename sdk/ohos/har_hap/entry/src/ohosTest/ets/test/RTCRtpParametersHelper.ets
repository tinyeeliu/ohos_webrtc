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
import webrtc from '@ohos/webrtc';
import * as helper from './Helper';

export function validateReceiverRtpParameters(param: webrtc.RTCRtpReceiveParameters) {
  validateRtpParameters(param);
}

function validateRtpParameters(param: webrtc.RTCRtpParameters) {
  helper.assertArray(param.headerExtensions);
  for(const headerExt of param.headerExtensions) {
    validateHeaderExtensionParameters(headerExt);
  }

  helper.assertObject(param.rtcp);
  validateRtcpParameters(param.rtcp);

  helper.assertArray(param.codecs);
  for(const codec of param.codecs) {
    validateCodecParameters(codec);
  }
}

function validateHeaderExtensionParameters(headerExt: webrtc.RTCRtpHeaderExtensionParameters) {
  helper.assertString(headerExt.uri);
  helper.assertUnsignedInt(headerExt.id);
  helper.assertOptionalBoolean(headerExt.encrypted);
}

function validateCodecParameters(codec: webrtc.RTCRtpCodecParameters) {
  helper.assertUnsignedInt(codec.payloadType);
  helper.assertString(codec.mimeType);
  helper.assertUnsignedInt(codec.clockRate);
  helper.assertOptionalUnsignedInt(codec.channels);
  helper.assertString(codec.sdpFmtpLine);
}

function validateRtcpParameters(rtcp: webrtc.RTCRtcpParameters) {
  helper.assertOptionalString(rtcp.cname);
  helper.assertOptionalBoolean(rtcp.reducedSize);
}
