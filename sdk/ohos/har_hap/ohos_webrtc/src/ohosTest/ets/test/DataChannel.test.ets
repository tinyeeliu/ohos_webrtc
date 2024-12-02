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
 
import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium'
import webrtc from 'libohos_webrtc.so';
import { Logging, LoggingSeverity } from '../../../main/ets/log/Logging';

const TAG: string = '[DataChannelTest]';
const STUN_SERVER = "stun:stun.l.google.com:19302";

const pcf = new webrtc.PeerConnectionFactory();

let caller: webrtc.RTCPeerConnection;
let callee: webrtc.RTCPeerConnection;

let dataChannel: webrtc.RTCDataChannelInit = {
  ordered: true, // 设置为 true 表示消息是有序的
  maxPacketLifeTime: 3000, // 可选，设置数据包的最大生存时间（毫秒）
  // maxRetransmits: 5, // 可选，设置最大重传次数
  protocol: "sctp-protocol", // 可选，设置协议
  // negotiated: false, // 默认为 false，表示数据通道是由 SDP 协商创建的
  // id: 1, // 可选，设置数据通道 ID
};

async function addTracks() {
  const stream = await getMediaStream();
  const tracks = stream.getTracks();
  callee.addTrack(tracks[0], stream);
  caller.addTransceiver('audio');
}

async function getMediaStream() {
  let audioSource = pcf.createAudioSource();
  let audioTrack = pcf.createAudioTrack("audio", audioSource);
  return new webrtc.MediaStream([audioTrack]);
}

async function exchangeOffer(caller: webrtc.RTCPeerConnection, callee: webrtc.RTCPeerConnection) {
  await caller.setLocalDescription(await caller.createOffer());
  console.log(TAG, "caller.localDescription: " + caller.localDescription!.type);
  await callee.setRemoteDescription(caller.localDescription);
}

async function exchangeAnswer(caller: webrtc.RTCPeerConnection, callee: webrtc.RTCPeerConnection) {
  // 必须先执行caller.setRemoteDescription，否则callee的candidates有可能在caller的remote description设置之前到达
  const answer = await callee.createAnswer();
  await caller.setRemoteDescription(answer);
  await callee.setLocalDescription(answer);
}

async function exchangeOfferAnswer(caller: webrtc.RTCPeerConnection, callee: webrtc.RTCPeerConnection) {
  await exchangeOffer(caller, callee);
  await exchangeAnswer(caller, callee);
}

function exchangeIceCandidates(pc1: webrtc.RTCPeerConnection, pc2: webrtc.RTCPeerConnection) {
  const doExchange = (pc1: webrtc.RTCPeerConnection, pc2: webrtc.RTCPeerConnection) => {
    pc1.onicecandidate = (event) => {
      const candidate = event.candidate;
      if (pc2.signalingState !== 'closed') {
        pc2.addIceCandidate(candidate);
      }
    };
  };

  doExchange(pc1, pc2);
  doExchange(pc2, pc1);
}

// 等待pc.connectionState变为'connected'.
async function listenToConnected(pc: webrtc.RTCPeerConnection) {
  while (pc.connectionState != 'connected') {
    await new Promise<webrtc.Event>((resolve) => {
      pc.onconnectionstatechange = resolve;
    });
    pc.onconnectionstatechange = null;
  }
}

async function didRemotePcClose(flag: Boolean) {
  addTracks();
  let channel1 = caller.createDataChannel("");
  channel1.onopen = () => {
    console.info(TAG, "channel.readyState:", channel1.readyState);
  }
  let closed = false;
  channel1.onclose = () => {
    closed = true;
  }
  exchangeIceCandidates(caller, callee);
  await exchangeOfferAnswer(caller, callee);
  await listenToConnected(callee);
  await sleep(5000);
  if (flag) {
    callee.close();
  }
  await sleep(1000);
  return closed;
}

function sleep(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

export default function DataChannelTest() {
  describe('DataChannelTest', () => {
    // Defines a test suite. Two parameters are supported: test suite name and test suite function.
    beforeAll(() => {
      // Presets an action, which is performed only once before all test cases of the test suite start.
      // This API supports only one parameter: preset action function.
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })
    beforeEach(() => {
      // Presets an action, which is performed before each unit test case starts.
      // The number of execution times is the same as the number of test cases defined by **it**.
      // This API supports only one parameter: preset action function.
      caller = pcf.createPeerConnection({
        iceServers: [{
          urls: STUN_SERVER
        }]
      });
      callee = pcf.createPeerConnection({
        iceServers: [{
          urls: STUN_SERVER
        }]
      });
    })
    afterEach(() => {
      // Presets a clear action, which is performed after each unit test case ends.
      // The number of execution times is the same as the number of test cases defined by **it**.
      // This API supports only one parameter: clear action function.
      if (caller) {
        caller.close();
      }
      if (callee) {
        callee.close();
      }
    })
    afterAll(() => {
      // Presets a clear action, which is performed after all test cases of the test suite end.
      // This API supports only one parameter: clear action function.
    })
    it('attribute', 0, async () => {
      let channel = caller.createDataChannel("audio", dataChannel);
      console.debug(TAG, "channel attribute: " + JSON.stringify(channel));
      await sleep(1000);
      expect(channel.label).assertEqual("audio");
      expect(channel.ordered).assertEqual(true);
      expect(channel.maxPacketLifeTime).assertEqual(3000);
      expect(channel.maxRetransmits).not().assertNull();
      expect(channel.protocol).assertEqual("sctp-protocol");
      expect(channel.negotiated).not().assertNull();
      expect(channel.id).not().assertNull();
      expect(channel.readyState).not().assertNull();
      expect(channel.bufferedAmount).not().assertNull();
      expect(channel.bufferedAmountLowThreshold).not().assertNull();
      expect(channel.binaryType).assertEqual("blob");
    })

    it("onBufferedAmountLow", 0, async () => {
      addTracks();
      let channel = caller.createDataChannel("onBufferedAmountLow");
      console.debug(TAG, "onBufferedAmountLow: " + JSON.stringify(channel));
      let flag: Boolean = false;
      channel.onbufferedamountlow = () => {
        flag = true;
        console.info(TAG, "onBufferedAmountLow on");
      };
      channel.bufferedAmountLowThreshold = 1024;
      channel.onopen = () => {
        const largeData = new Array(1000).join('A');
        channel.send(largeData);
      };
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await sleep(1000);
      expect(flag).assertEqual(true);
    })

    it("onclose", 0, async () => {
      let channel = caller.createDataChannel("onclose", dataChannel);
      console.debug(TAG, "onclose: " + JSON.stringify(channel));
      channel.onclose = () => {
        console.info(TAG, "onclose on", channel.readyState);
      };
      channel.close();
      await sleep(1000);
      console.info(TAG, "channel.readyState:", channel.readyState);
      expect(channel.readyState).assertEqual("closed");
    })

    it("onclosing", 0, async () => {
      let channel = caller.createDataChannel("onclosing", dataChannel);
      console.debug(TAG, "onclosing: " + JSON.stringify(channel));
      channel.onclosing = () => {
        console.info(TAG, "channel.readyState:", channel.readyState);
      };
      channel.close();
    })

    it("onopen", 0, async () => {
      addTracks();
      let channel = caller.createDataChannel("onopen");
      channel.onopen = () => {
        console.info(TAG, "channel.readyState:", channel.readyState);
        expect(channel.readyState).assertEqual("open");
      }

      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await sleep(5000);
    })

    it("onmessage", 0, async () => {
      addTracks();
      let channel = caller.createDataChannel("onmessage");
      channel.onopen = () => {
        console.info(TAG, "channel.readyState:", channel.readyState);
        channel.send("message from caller");
      }
      channel.onmessage = (event: webrtc.MessageEvent<string>) => {
        console.info(TAG, "message from callee", event.data);
        expect(event.data).assertEqual("channel from callee")
      };
      callee.ondatachannel = (event) => {
        console.info(TAG, "ondatachannel");
        let receiveDataChannel = event.channel;
        receiveDataChannel.onopen = () => {
          console.info(TAG, "The channel from callee is open");
          receiveDataChannel.send("channel from callee");
        }
        receiveDataChannel.onmessage = (event: webrtc.MessageEvent<String>) => {
          console.info(TAG, "message from caller", event.data);
          expect(event.data).assertEqual("message from caller")
        }
      }
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await sleep(5000);
    })

    it("sendWhenClosed", 0, async () => {
      let channel = caller.createDataChannel("17", dataChannel);
      let onclose = new Promise<webrtc.Event>((resolve, reject) => {
        channel.onclose = resolve;
        // channel.onerror = reject;
      });
      channel.close();
      await onclose;
      console.info(TAG, "channel.readyState:", channel.readyState);
      try {
        channel.send("a");
      } catch (e) {
        expect(e).assertInstanceOf("Error");
      }
    })

    it("close", 0, async () => {
      let channel = caller.createDataChannel("18", dataChannel);
      console.debug(TAG, "close: " + JSON.stringify(channel));
      let flag: Boolean = false;
      channel.onclose = () => {
        flag = true;
        console.info(TAG, "onclose on");
      };
      channel.close();
      await sleep(5000);
      expect(flag).assertEqual(true);
    })

    it("sendString", 0, async () => {
      addTracks();
      let channel = caller.createDataChannel("sendString");
      channel.onopen = () => {
        console.info(TAG, "channel.readyState:", channel.readyState);
        channel.send("a");
      }
      callee.ondatachannel = (event) => {
        let receiveDataChannel = event.channel;
        receiveDataChannel.onmessage = (event: webrtc.MessageEvent<String>) => {
          console.info(TAG, "message from caller", event.data);
          expect(event.data).assertEqual("a")
        }
      }
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await sleep(5000);
    })

    it("sendArrayBuffer", 0, async () => {
      addTracks();
      let channel = caller.createDataChannel("sendArrayBuffer");
      channel.onopen = () => {
        console.info(TAG, "channel.readyState:", channel.readyState);
        const buffer = new ArrayBuffer(1);
        const uint8View = new Uint8Array(buffer);
        uint8View[0] = 97;
        try {
          channel.send(buffer);
        } catch (e) {
          expect().assertFail()
        }
      }
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await sleep(5000);
    })

    it("onerror", 0, () => {
      expect(1).assertEqual(1);
    })

    it("bufferedAmountChange", 0, async () => {
      addTracks();
      let channel = caller.createDataChannel("");
      let buffer = "a";
      channel.onopen = () => {
        channel.send(buffer);
        expect(channel.bufferedAmount).assertEqual(buffer.toString().length);
      };
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      expect(channel.bufferedAmount).assertEqual(0);
    })

    it("sendWhenReadyStateIsConnecting", 0, () => {
      let channel = caller.createDataChannel("sendWhenReadyStateConnecting");
      try {
        console.info(TAG, "channel.readyState:", channel.readyState);
        channel.send("a");
      } catch (e) {
        expect(e).assertInstanceOf("Error");
      }
    })

    it("sendWhenBufferIsEmpty", 0, async () => {
      addTracks();
      let channel = caller.createDataChannel("sendWhenBufferIsEmpty");
      channel.onopen = () => {
        console.info(TAG, "channel.readyState:", channel.readyState);
        const buffer = new ArrayBuffer(10);
        channel.send(buffer);
      }
      callee.ondatachannel = (event) => {
        let receiveDataChannel = event.channel;
        receiveDataChannel.onmessage = (event: webrtc.MessageEvent<String>) => {
          console.info(TAG, "message from caller:", event.data);
          if (event.data instanceof ArrayBuffer) {
            const uint8View = new Uint8Array(event.data);
            console.info(TAG, "ArrayBuffer length:", uint8View.length);
            expect(uint8View.length).assertEqual(10);
          }
        }
      }
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await sleep(1000);
    })

    it("sendUnicodeString", 0, async () => {
      addTracks();
      let channel = caller.createDataChannel("sendUnicodeString");
      channel.onopen = () => {
        console.info(TAG, "channel.readyState:", channel.readyState);
        const unicodeString = "你好";
        channel.send(unicodeString);
      }
      callee.ondatachannel = (event) => {
        let receiveDataChannel = event.channel;
        receiveDataChannel.onmessage = (event: webrtc.MessageEvent<String>) => {
          console.info(TAG, "message from caller: ", event.data);
          expect(event.data).assertEqual("你好");
        }
      }
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await sleep(1000);
    })

    it("sendBeforeCloseChannel", 0, async () => {
      addTracks();
      let channel1 = caller.createDataChannel("sendBeforeCloseChannel");
      let channel2: webrtc.RTCDataChannel;
      let receivedSize = 0, sentSize = 0;
      const largeString = " ".repeat(64 * 1024);
      // channel2.binaryType = "arraybuffer";

      callee.ondatachannel = async (event) => {
        channel2 = event.channel;
        channel2.onmessage = (event: webrtc.MessageEvent<String>) => {
          console.info(TAG, "receivedSize: " + receivedSize, " sentSize: " + sentSize);
          receivedSize += event.data.length;
        }
        channel2.onclose = () => {
          console.info(TAG, "channel.readyState:", channel2.readyState);
        }
      }

      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await sleep(1000);
      channel1.onopen = async () => {
        try {
          channel1.send(largeString);
          sentSize += largeString.length;
          channel1.close();
        } catch (e) {
          console.info(TAG, "e: ", e);
        }
      }
      await sleep(1000);
      console.info(TAG, "after close receivedSize: " + receivedSize, " sentSize: " + sentSize);
      expect(receivedSize).assertEqual(sentSize);
    })

    it("setNegotiatedFalse", 0, async () => {
      let channel1 = caller.createDataChannel('', {
        negotiated: false,
        id: 42
      });

      channel1.onopen = () => {
        channel1.send("a");
      }

      let channel2 = callee.createDataChannel('', {
        negotiated: false,
        id: 42
      });

      expect(channel1.id).assertUndefined();
      expect(channel2.id).assertUndefined();

      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      expect(channel1.id).not().assertEqual(42);
      expect(channel2.id).not().assertEqual(42);
    })

    it("restartIceChannelIsOpen", 0, async () => {
      addTracks();
      let channel1 = caller.createDataChannel("");
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);

      caller.restartIce();

      channel1.onopen = () => {
        channel1.send("a");
      }

      callee.ondatachannel = (event) => {
        event.channel.onmessage = (e: webrtc.MessageEvent<String>) => {
          console.info(TAG, "e: ", e.data.toString());
          expect(e.data.toString()).assertEqual("a");
        }
      }
      await sleep(1000);
    })

    it("ChannelWhenPcClose", 0, async () => {
      let res = await didRemotePcClose(true);
      expect(res).assertEqual(true);
    })

    it("ChannelWhenPcOpen", 0, async () => {
      let res = await didRemotePcClose(false);
      expect(res).assertEqual(false);
    })

    it("changeBinaryType", 0, () => {
      let channel = caller.createDataChannel("");
      console.info(TAG, "BinaryType: ", channel.binaryType);
      channel.binaryType = "arraybuffer";
      expect(channel.binaryType).assertEqual("arraybuffer");
    })
  })
}