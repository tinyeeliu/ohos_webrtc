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

import webrtc from '@ohos/webrtc';

/**
 * 测试数据集中定义（fixtures）：
 * 将原先硬编码在各测试文件内的 SDP 字符串、ICE candidate、测试参数
 * 统一提取为常量与工厂函数，供各测试套件引用。
 */

// ========== SDP 会话描述常量 ==========

/** audio-only 完整 SDP（含 ICE/DTLS/RTP 属性），用于需要真实会话结构的用例 */
export const AUDIO_ONLY_SESSION_SDP: string = 'v=0\r\n' +
  'o=- 166855176514521964 2 IN IP4 127.0.0.1\r\n' +
  's=-\r\n' +
  't=0 0\r\n' +
  'a=ice-options:trickle\r\n' +
  'm=audio 9 UDP/TLS/RTP/SAVPF 111\r\n' +
  'c=IN IP4 0.0.0.0\r\n' +
  'a=rtcp:9 IN IP4 0.0.0.0\r\n' +
  'a=ice-ufrag:someufrag\r\n' +
  'a=ice-pwd:somelongpwdwithenoughrandomness\r\n' +
  'a=fingerprint:sha-256 8C:71:B3:8D:A5:38:FD:8F:A4:2E:A2:65:6C:86:52:BC:E0:6E:94:F2:9F:7C:4D:B5:DF:AF:AA:6F:44:90:8D:F4\r\n' +
  'a=setup:actpass\r\n' +
  'a=rtcp-mux\r\n' +
  'a=mid:mid1\r\n' +
  'a=sendonly\r\n' +
  'a=msid:stream1 track1\r\n' +
  'a=ssrc:1001 cname:some\r\n' +
  'a=rtpmap:111 opus/48000/2\r\n';

/** 最小合法 offer SDP（仅会话级属性） */
export const MINIMAL_OFFER_SDP: string =
  'v=0\r\no=- 8926124112408190582 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=extmap-allow-mixed\r\na=msid-semantic: WMS\r\n';

/** 最小合法 answer SDP（仅会话级属性） */
export const MINIMAL_ANSWER_SDP: string =
  'v=0\r\no=- 1824345164880213868 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=extmap-allow-mixed\r\na=msid-semantic: WMS\r\n';

// ========== 非法 SDP 输入 ==========

export const INVALID_SDP_BOGUS: string = 'bogus';
export const INVALID_SDP_MARKED_CONTENT: string = '!<Invalid SDP Content>;';
export const INVALID_SDP_PLAIN_TEXT: string = 'Invalid SDP';

// ========== ICE candidate 字符串 ==========

/** UDP host candidate */
export const UDP_HOST_CANDIDATE_STRING: string =
  'candidate:1905690388 1 udp 2113937151 192.168.0.1 58041 typ host generation 0 ufrag thC8 network-cost 50';

/** TCP srflx candidate（含 raddr/rport/tcptype） */
export const TCP_SRFLX_CANDIDATE_STRING: string =
  'candidate:435653019 2 tcp 1845501695 192.168.0.196 4444 typ srflx raddr www.example.com rport 22222 tcptype active';

/** 任意字符串（非法 sdpMid 等边界用例输入） */
export const ARBITRARY_CANDIDATE_STRING: string = '<arbitrary string[0] content>;';

// ========== ICE 服务器与 DataChannel 测试参数 ==========

export const STUN_SERVER_URL: string = 'stun:stun.example.com:19302';
export const ICE_CANDIDATE_POOL_SIZE: number = 10;

export const DATA_CHANNEL_PROTOCOL: string = 'sctp-protocol';
export const DATA_CHANNEL_MAX_PACKET_LIFETIME_MS: number = 3000;
export const DATA_CHANNEL_TEST_LABEL: string = 'fire-me!';
export const DATA_CHANNEL_TEST_MESSAGE: string = 'meow meow!';

// ========== 工厂函数（每次调用返回新对象，避免用例间共享可变状态） ==========

export function createOfferInit(): webrtc.RTCSessionDescriptionInit {
  return {
    sdp: MINIMAL_OFFER_SDP,
    type: 'offer'
  } as webrtc.RTCSessionDescriptionInit;
}

export function createAnswerInit(): webrtc.RTCSessionDescriptionInit {
  return {
    sdp: MINIMAL_ANSWER_SDP,
    type: 'answer'
  } as webrtc.RTCSessionDescriptionInit;
}

export function createTransceiverInit(): webrtc.RTCRtpTransceiverInit {
  return {
    direction: 'sendrecv',
    streams: [],
    sendEncodings: [{
      maxBitrate: 100000,
    }],
  } as webrtc.RTCRtpTransceiverInit;
}

export function createDataChannelInit(): webrtc.RTCDataChannelInit {
  let init: webrtc.RTCDataChannelInit = {
    ordered: true,
    maxPacketLifeTime: DATA_CHANNEL_MAX_PACKET_LIFETIME_MS,
    protocol: DATA_CHANNEL_PROTOCOL,
  };
  return init;
}

export function createStunServerConfiguration(): webrtc.RTCConfiguration {
  let config: webrtc.RTCConfiguration = {
    iceServers: [{ urls: STUN_SERVER_URL }],
    iceCandidatePoolSize: ICE_CANDIDATE_POOL_SIZE
  };
  return config;
}
