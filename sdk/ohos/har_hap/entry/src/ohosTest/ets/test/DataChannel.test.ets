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
 
/** 测试类型： unit（单元测试） */

import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium'
import webrtc, { Logging, LoggingSeverity } from '@ohos/webrtc';
import { sleep } from './Helper';

const TAG: string = '[DataChannelTest]';

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
  const doExchange = (source: webrtc.RTCPeerConnection, target: webrtc.RTCPeerConnection) => {
    const pending: webrtc.RTCIceCandidateInit[] = [];
    source.onicecandidate = (event) => {
      if (!event.candidate) return;
      if (target.signalingState !== 'closed' && target.remoteDescription) {
        target.addIceCandidate(event.candidate).catch(() => {});
      } else {
        pending.push(event.candidate);
      }
    };
    const timer = setInterval(() => {
      if (source.signalingState === 'closed' || target.signalingState === 'closed') {
        clearInterval(timer);
        return;
      }
      if (pending.length > 0 && target.remoteDescription) {
        const toFlush = pending.splice(0);
        toFlush.forEach(c => target.addIceCandidate(c).catch(() => {}));
      }
      if (source.iceGatheringState === 'complete' && pending.length === 0) {
        clearInterval(timer);
      }
    }, 50);
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

async function createDataChannelPair(options: webrtc.RTCDataChannelInit, pc1: webrtc.RTCPeerConnection, pc2: webrtc.RTCPeerConnection): Promise<webrtc.RTCDataChannel[]> {
  let pair = [pc1.createDataChannel('', options)];
  let bothOpen = Promise.all([
    new Promise<webrtc.Event>((r, e) => {
      pair[0].onopen = r;
      pair[0].onerror = (event) => e(event.error);
    }),
    new Promise<webrtc.Event>((r, e) => {
      pc2.ondatachannel = (event) => {
        console.log(TAG, "ondatachannel");
        pair[1] = event.channel;
        pair[1].onopen = r;
        pair[1].onerror = (event) => e(event.error);
      }
    })
  ]);

  exchangeIceCandidates(pc1, pc2);
  await exchangeOfferAnswer(pc1, pc2);
  await bothOpen;
  return pair;
}

async function checkCanPassData(channel1: webrtc.RTCDataChannel, channel2: webrtc.RTCDataChannel) {
  let promise = new Promise<string>((resolve) => {
    channel2.onmessage = (event: webrtc.MessageEvent<string>) => {
      resolve(event.data);
    };
  });
  channel1.send('hello');
  let message = await promise;
  expect(message).assertEqual('hello');
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
      // The API supports only one parameter: preset action function.
      caller = pcf.createPeerConnection({
        iceServers: []
      });
      callee = pcf.createPeerConnection({
        iceServers: []
      });
    })
    afterEach(async () => {
      if (caller) {
        caller.close();
      }
      if (callee) {
        callee.close();
      }
      await sleep(500);
    })
    afterAll(() => {
      // Presets a clear action, which is performed after all test cases of the test suite end.
      // This API supports only one parameter: clear action function.
    })

    it('shouldExposeDataChannelAttributes', 0, async () => {
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

    it('shouldTriggerOnBufferedAmountLow', 0, async () => {
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
      await promise;
      expect(flag).assertTrue();
    })

    it('shouldTriggerCloseEventWhenConversationCloses', 0, async () => {
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

    it('shouldTriggerOnclosing', 0, async () => {
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

    it('shouldTriggerOnopenAfterConnection', 0, async () => {
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
      await promise;
      expect(flag).assertTrue();
    })

    it('shouldTriggerOnmessageWhenMessageSent', 0, async () => {
      let pair = await createDataChannelPair({}, caller, callee);
      let dc1 = pair[0];
      let dc2 = pair[1];

      let promise = new Promise<void>((resolve) => {
        dc2.onmessage = (event: webrtc.MessageEvent<String>) => {
          console.info(TAG, "message from caller", event.data);
          expect(typeof event.data).assertEqual('string');
          expect(event.data).assertEqual("message from caller");
          resolve();
        };
      });

      dc1.send("message from caller");
      await promise;
    })

    it('shouldRejectSendWhenChannelClosed', 0, async () => {
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

    it('shouldTriggerOncloseAfterConnection', 0, async () => {
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

    it('shouldSendString', 0, async () => {
      let channel1 = caller.createDataChannel("sendString");
      let channel2: webrtc.RTCDataChannel;
      let promiseOpen1 = new Promise<void>((resolve) => {
        channel1.onopen = () => {
          console.info(TAG, "channel.readyState:", channel1.readyState);
          resolve();
        }
      });
      let promiseOpen2 = new Promise<void>((resolve) => {
        callee.ondatachannel = (event) => {
          console.info(TAG, "ondatachannel: ", JSON.stringify(event.channel));
          channel2 = event.channel;
          channel2.onopen = () => {
            resolve();
          }
        }
      });
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await Promise.all([promiseOpen1, promiseOpen2]);

      let promiseReceive = new Promise<void>((resolve) => {
        channel2.onmessage = (event: webrtc.MessageEvent<String>) => {
          console.info(TAG, "message from caller", event.data);
          expect(typeof event.data).assertEqual('string');
          expect(event.data).assertEqual("a");
          resolve();
        }
      });
      channel1.send("a");
      await promiseReceive;
    })

    it('shouldSendArrayBuffer', 0, async () => {
      let pair = await createDataChannelPair({}, caller, callee);
      let dc1 = pair[0];
      let dc2 = pair[1];

      let promise = new Promise<ArrayBuffer>((resolve) => {
        dc2.onmessage = (event: webrtc.MessageEvent<ArrayBuffer>) => {
          resolve(event.data);
        };
      });

      try {
        let buffer = new Uint8Array(1);
        buffer.fill(97);
        dc1.send(buffer.buffer);
      } catch (e) {
        expect().assertFail()
      }

      let data = await promise;
      expect(new Uint8Array(data)[0]).assertEqual(97);
    })

    it('shouldRejectSendWhenReadyStateIsConnecting', 0, () => {
      let channel = caller.createDataChannel("sendWhenReadyStateConnecting");
      try {
        console.info(TAG, "channel.readyState:", channel.readyState);
        channel.send("a");
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf("Error");
      }
    })

    it('shouldSendWhenBufferIsEmpty', 0, async () => {
      let pair = await createDataChannelPair({}, caller, callee);
      let dc1 = pair[0];
      let dc2 = pair[1];

      let promise = new Promise<ArrayBuffer>((resolve) => {
        dc2.onmessage = (event: webrtc.MessageEvent<ArrayBuffer>) => {
          console.info(TAG, "message from caller", event.data);
          resolve(event.data);
        };
      });

      dc1.send(new ArrayBuffer(10));
      let data = await promise;

      expect(data instanceof ArrayBuffer).assertTrue();
      console.info(TAG, "ArrayBuffer length:", data.byteLength);
      expect(data.byteLength).assertEqual(10);
    })

    it('shouldSendUnicodeString', 0, async () => {
      let pair = await createDataChannelPair({}, caller, callee);
      let dc1 = pair[0];
      let dc2 = pair[1];

      let promise = new Promise<void>((resolve) => {
        dc2.onmessage = (event: webrtc.MessageEvent<String>) => {
          console.info(TAG, "message from caller", event.data);
          expect(typeof event.data).assertEqual('string');
          expect(event.data).assertEqual("你好");
          resolve();
        };
      });

      const unicodeString = "你好";
      dc1.send(unicodeString);
      await promise;
    })

    it('shouldSendBeforeCloseChannel', 0, async () => {
      let pair = await createDataChannelPair({}, caller, callee);
      let dc1 = pair[0];
      let dc2 = pair[1];

      let receivedSize = 0, sentSize = 0;
      const largeString = " ".repeat(64 * 1024);

      let promise = new Promise<void>((resolve) => {
        dc2.onmessage = (event: webrtc.MessageEvent<string>) => {
          receivedSize += event.data.length;
          resolve();
        };
      });

      dc1.send(largeString);
      sentSize += largeString.length;
      dc1.close();

      await promise;
      console.info(TAG, "after close receivedSize: " + receivedSize, " sentSize: " + sentSize);
      expect(receivedSize).assertEqual(sentSize);
    })

    it('shouldHaveNegotiatedFalse', 0, async () => {
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

    it('shouldReopenChannelAfterIceRestart', 0, async () => {
      let pair = await createDataChannelPair({}, caller, callee);
      let dc1 = pair[0];
      let dc2 = pair[1];

      await checkCanPassData(dc1, dc2);
      await checkCanPassData(dc2, dc1);

      caller.restartIce();
      await exchangeOfferAnswer(caller, callee);

      await checkCanPassData(dc1, dc2);
      await checkCanPassData(dc2, dc1);

      dc1.close();
      dc2.close();
    })

    it('shouldCloseChannelWhenPcCloses', 0, async () => {
      let channel1 = caller.createDataChannel("");
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

    it('shouldNotCloseChannelBeforePcCloses', 0, async () => {
      let channel1 = caller.createDataChannel("");
      let closed = false;
      channel1.onclose = () => {
        closed = true;
      }
      exchangeIceCandidates(caller, callee);
      await exchangeOfferAnswer(caller, callee);
      await sleep(1000);
      expect(closed).assertEqual(false);
    })

    it('shouldChangeBinaryType', 0, () => {
      let channel = caller.createDataChannel("");
      console.info(TAG, "BinaryType: ", channel.binaryType);
      channel.binaryType = "arraybuffer";
      expect(channel.binaryType).assertEqual("arraybuffer");
    })
  })
}
