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
