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
import { sleep } from './Helper';

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
      let flag = false;
      let promise = new Promise<void>((resolve) => {
        channel.onbufferedamountlow = () => {
          flag = true;
          resolve();
        };
      });

      channel.bufferedAmountLowThreshold = 1024;
      channel.onopen = () => {
        const largeData = new Array(1000).join('A');
        channel.send(largeData);
      };
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await promise;
      expect(flag).assertTrue();
    })

    it("theCloseEventBeTriggeredWhenTheConversationClose", 0, async () => {
      let channel = caller.createDataChannel("onclose", dataChannel);
      console.debug(TAG, "onclose: " + JSON.stringify(channel))
      let flag: boolean = false;
      let promise = new Promise<void>((resolve) => {
        channel.onclose = () => {
          flag = true;
          resolve();
        };
      });
      channel.close();
      await promise;
      expect(flag).assertEqual(true);
    })

    it("onclosing", 0, async () => {
      let channel = caller.createDataChannel("onclosing", dataChannel);
      console.debug(TAG, "onclosing: " + JSON.stringify(channel));
      let flag = false;
      let promise = new Promise<void>((resolve) => {
        channel.onclosing = () => {
          console.info(TAG, "channel.readyState:", channel.readyState);
          flag = true;
          resolve();
        };
      })
      channel.close();
      await promise;
      expect(flag).assertTrue();
    })

    it("afterConnectionOnopenWillTriggered", 0, async () => {
      addTracks();
      let channel = caller.createDataChannel("onopen");
      let flag = false;
      let promise = new Promise<void>((resolve) => {
        channel.onopen = () => {
          flag = true;
          resolve();
        }
      });

      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await promise;
      expect(flag).assertTrue();
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
      let promise = new Promise<void>((resolve) => {
        callee.ondatachannel = (event) => {
          console.info(TAG, "ondatachannel");
          let receiveDataChannel = event.channel;
          receiveDataChannel.onopen = () => {
            console.info(TAG, "The channel from callee is open");
            receiveDataChannel.send("channel from callee");
          }
          receiveDataChannel.onmessage = (event: webrtc.MessageEvent<String>) => {
            console.info(TAG, "message from caller", event.data);
            expect(event.data).assertEqual("message from caller");
            resolve();
          }
        }
      });
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await promise;
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

    it("afterConnectionOncloseWillTriggered", 0, async () => {
      let channel = caller.createDataChannel("18", dataChannel);
      console.debug(TAG, "close: " + JSON.stringify(channel));
      let flag = false;
      let promise = new Promise<void>((resolve) => {
        channel.onclose = () => {
          flag = true;
          resolve();
        };
      });
      channel.close();
      await promise;
      expect(flag).assertTrue();
    })

    it("sendString", 0, async () => {
      addTracks();
      let channel = caller.createDataChannel("sendString");
      channel.onopen = () => {
        console.info(TAG, "channel.readyState:", channel.readyState);
        channel.send("a");
      }
      let promise = new Promise<void>((resolve) => {
        callee.ondatachannel = (event) => {
          let receiveDataChannel = event.channel;
          receiveDataChannel.onmessage = (event: webrtc.MessageEvent<String>) => {
            console.info(TAG, "message from caller", event.data);
            expect(event.data).assertEqual("a");
            resolve();
          }
        }
      });
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await promise;
    })

    it("sendArrayBuffer", 0, async () => {
      addTracks();
      let channel = caller.createDataChannel("sendArrayBuffer");
      let promise = new Promise<void>((resolve) => {
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
          resolve();
        }
      });
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await promise;
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
      let promise = new Promise<void>((resolve) => {
        callee.ondatachannel = (event) => {
          let receiveDataChannel = event.channel;
          receiveDataChannel.onmessage = (event: webrtc.MessageEvent<String>) => {
            console.info(TAG, "message from caller:", event.data);
            if (event.data instanceof ArrayBuffer) {
              const uint8View = new Uint8Array(event.data);
              console.info(TAG, "ArrayBuffer length:", uint8View.length);
              expect(uint8View.length).assertEqual(10);
              resolve();
            }
          }
        }
      });

      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await promise;
    })

    it("sendUnicodeString", 0, async () => {
      addTracks();
      let channel = caller.createDataChannel("sendUnicodeString");
      channel.onopen = () => {
        console.info(TAG, "channel.readyState:", channel.readyState);
        const unicodeString = "你好";
        channel.send(unicodeString);
      }
      let promise = new Promise<void>((resolve) => {
        callee.ondatachannel = (event) => {
          let receiveDataChannel = event.channel;
          receiveDataChannel.onmessage = (event: webrtc.MessageEvent<String>) => {
            console.info(TAG, "message from caller: ", event.data);
            expect(event.data).assertEqual("你好");
            resolve();
          }
        }
      })
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await promise;
    })

    it("sendBeforeCloseChannel", 0, async () => {
      addTracks();
      let channel1 = caller.createDataChannel("sendBeforeCloseChannel");
      let channel2: webrtc.RTCDataChannel;
      let receivedSize = 0, sentSize = 0;
      const largeString = " ".repeat(64 * 1024);
      // channel2.binaryType = "arraybuffer";

      let promise = new Promise<void>((resolve) => {
        callee.ondatachannel = async (event) => {
          channel2 = event.channel;
          channel2.onmessage = (event: webrtc.MessageEvent<String>) => {
            receivedSize += event.data.length;
            resolve();
          }
          channel2.onclose = () => {
            console.info(TAG, "channel.readyState:", channel2.readyState);
          }
        }
      })
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);

      channel1.onopen = () => {
        channel1.send(largeString);
        sentSize += largeString.length;
        channel1.close();
      }
      await promise;
      console.info(TAG, "after close receivedSize: " + receivedSize, " sentSize: " + sentSize);
      expect(receivedSize).assertEqual(sentSize);
    })

    it("setNegotiatedFalse", 0, async () => {
      let channel1 = caller.createDataChannel('', {
        negotiated: false,
        id: 42
      });

      let promise = new Promise<void>((resolve) => {
        channel1.onopen = () => {
          channel1.send("a");
          resolve();
        }
      })

      let channel2 = callee.createDataChannel('', {
        negotiated: false,
        id: 42
      });

      expect(channel1.id).assertUndefined();
      expect(channel2.id).assertUndefined();

      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await promise;
      expect(channel1.id == 42 && channel2.id == 42).assertFalse();
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

      let promise = new Promise<void>((resolve) => {
        callee.ondatachannel = (event) => {
          event.channel.onmessage = (e: webrtc.MessageEvent<String>) => {
            console.info(TAG, "e: ", e.data.toString());
            expect(e.data.toString()).assertEqual("a");
            resolve();
          }
        }
      });
      await promise;
    })

    it("afterPcCloseChannelWillClose", 0, async () => {
      addTracks();
      let channel1 = caller.createDataChannel("");
      channel1.onopen = () => {
        console.info(TAG, "channel.readyState:", channel1.readyState);
      }
      let closed = false;
      let promise = new Promise<void>((resolve) => {
        channel1.onclose = () => {
          closed = true;
          resolve();
        }
      });
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      callee.close();
      await promise;
      expect(closed).assertEqual(true);
    })

    it("beforePcCloseChannelWillNotClose", 0, async () => {
      addTracks();
      let channel1 = caller.createDataChannel("");
      let closed = false;
      channel1.onclose = () => {
        closed = true;
      }
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await listenToConnected(callee);
      await sleep(1000);
      expect(closed).assertEqual(false);
    })

    it("changeBinaryType", 0, () => {
      let channel = caller.createDataChannel("");
      console.info(TAG, "BinaryType: ", channel.binaryType);
      channel.binaryType = "arraybuffer";
      expect(channel.binaryType).assertEqual("arraybuffer");
    })
  })
}