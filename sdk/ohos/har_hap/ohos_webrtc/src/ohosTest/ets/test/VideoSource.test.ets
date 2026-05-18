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
import webrtc from 'libohos_webrtc.so';
import { Logging, LoggingSeverity } from '../../../main/ets/log/Logging';

const TAG: string = '[VideoSourceTest]';

const pcf = new webrtc.PeerConnectionFactory();

export default function VideoSourceTest() {
  describe('VideoSourceTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('releaseChangesStateToEnded', 0, () => {
      let videoSource = pcf.createVideoSource();
      expect(videoSource.state).assertEqual('live');
      videoSource.release();
      expect(videoSource.state).assertEqual('ended');
    })

    it('releaseThenCreateNewSourceDoesNotConflict', 0, () => {
      let source1 = pcf.createVideoSource();
      source1.release();
      let source2 = pcf.createVideoSource();
      expect(source2.state).assertEqual('live');
      source2.release();
    })
  })
}
