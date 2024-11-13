import { expect } from '@ohos/hypium';
import webrtc from 'libohos_webrtc.so';
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
