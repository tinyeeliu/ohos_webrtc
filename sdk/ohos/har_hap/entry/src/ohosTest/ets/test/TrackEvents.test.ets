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

import { describe, beforeAll, it, expect } from '@ohos/hypium'
import webrtc, { Logging, LoggingSeverity } from '@ohos/webrtc';

const TAG: string = '[TrackEventsTest]';

const pcf = new webrtc.PeerConnectionFactory();

function sleep(ms: number): Promise<void> {
  return new Promise<void>((resolve) => {
    setTimeout(resolve, ms);
  });
}

export default function TrackEventsTest() {
  describe('TrackEventsTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    // E-01: event properties default to null and accept handlers.
    it('eventPropertiesDefaultNullAndAcceptHandlers', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack.onended).assertNull();
      expect(audioTrack.onmute).assertNull();
      expect(audioTrack.onunmute).assertNull();

      let endedHandler = (ev: webrtc.Event): void => {
        console.info(TAG, 'ended: ' + ev.type);
      };
      let muteHandler = (ev: webrtc.Event): void => {
        console.info(TAG, 'mute: ' + ev.type);
      };
      let unmuteHandler = (ev: webrtc.Event): void => {
        console.info(TAG, 'unmute: ' + ev.type);
      };
      audioTrack.onended = endedHandler;
      audioTrack.onmute = muteHandler;
      audioTrack.onunmute = unmuteHandler;
      expect(audioTrack.onended).not().assertNull();
      expect(audioTrack.onmute).not().assertNull();
      expect(audioTrack.onunmute).not().assertNull();
      audioSource.release();
    })

    // E-02: replacing a handler keeps only the latest; null unbinds.
    it('replaceHandlerAndNullUnbinds', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);

      let first = (ev: webrtc.Event): void => {
      };
      let second = (ev: webrtc.Event): void => {
      };
      audioTrack.onended = first;
      audioTrack.onended = second;
      // Read-back returns the most recently assigned function reference.
      expect(audioTrack.onended === second).assertTrue();

      audioTrack.onended = null;
      expect(audioTrack.onended).assertNull();
      audioSource.release();
    })

    // E-07: muted is readonly and false for a live local track without
    // interruption signals.
    it('mutedDefaultsToFalse', 0, () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      expect(audioTrack.muted).assertFalse();
      audioTrack.enabled = false;
      expect(audioTrack.muted).assertFalse();
      audioSource.release();
    })

    // E-04 + E-05: an explicit stop() must not fire ended; enabled toggles must
    // not fire mute/unmute.
    it('activeStopDoesNotFireEndedAndEnabledDoesNotFireMute', 0, async () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);

      let endedCount = 0;
      let muteCount = 0;
      let unmuteCount = 0;
      audioTrack.onended = (): void => {
        endedCount++;
      };
      audioTrack.onmute = (): void => {
        muteCount++;
      };
      audioTrack.onunmute = (): void => {
        unmuteCount++;
      };

      audioTrack.enabled = false;
      audioTrack.enabled = true;
      await sleep(200);
      expect(muteCount).assertEqual(0);
      expect(unmuteCount).assertEqual(0);
      expect(audioTrack.muted).assertFalse();

      audioTrack.stop();
      expect(audioTrack.readyState).assertEqual('ended');
      await sleep(200);
      expect(endedCount).assertEqual(0);
      audioSource.release();
    })

    // E-02 (unbind verification): after null unbind, later state changes reach
    // no handler. Stops the track and waits to confirm no callback fires.
    it('unboundHandlerNeverCalledAfterStop', 0, async () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack('audio', audioSource);
      let callCount = 0;
      audioTrack.onended = (): void => {
        callCount++;
      };
      audioTrack.onended = null;
      audioTrack.stop();
      await sleep(200);
      expect(callCount).assertEqual(0);
      audioSource.release();
    })
  })
}
