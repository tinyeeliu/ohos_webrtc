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

import { describe, beforeAll, afterAll, it, expect } from '@ohos/hypium'
import { Logging, LoggingSeverity } from '../../../main/ets/log/Logging';

const TAG: string = '[LoggingTest]';

export default function LoggingTest() {
  describe('LoggingTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    afterAll(() => {
      Logging.deleteInjectedLoggable();
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('enableLogToDebugOutputDoesNotCrash', 0, () => {
      Logging.enableLogToDebugOutput(LoggingSeverity.INFO);
      Logging.enableLogToDebugOutput(LoggingSeverity.WARNING);
      Logging.enableLogToDebugOutput(LoggingSeverity.ERROR);
      Logging.enableLogToDebugOutput(LoggingSeverity.NONE);
    })

    it('enableLogToDebugOutputWithVerboseDoesNotCrash', 0, () => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('enableLogThreadsDoesNotCrash', 0, () => {
      Logging.enableLogThreads();
    })

    it('enableLogTimeStampsDoesNotCrash', 0, () => {
      Logging.enableLogTimeStamps();
    })

    it('logWithInfoSeverityDoesNotCrash', 0, () => {
      Logging.log(LoggingSeverity.INFO, TAG, 'test info log');
    })

    it('logWithWarningSeverityDoesNotCrash', 0, () => {
      Logging.log(LoggingSeverity.WARNING, TAG, 'test warning log');
    })

    it('logWithErrorSeverityDoesNotCrash', 0, () => {
      Logging.log(LoggingSeverity.ERROR, TAG, 'test error log');
    })

    it('logWithVerboseSeverityDoesNotCrash', 0, () => {
      Logging.log(LoggingSeverity.VERBOSE, TAG, 'test verbose log');
    })

    it('dShorthandDoesNotCrash', 0, () => {
      Logging.d(TAG, 'test d shorthand');
    })

    it('eShorthandDoesNotCrash', 0, () => {
      Logging.e(TAG, 'test e shorthand');
    })

    it('wShorthandDoesNotCrash', 0, () => {
      Logging.w(TAG, 'test w shorthand', new Error('test error'));
    })

    it('vShorthandDoesNotCrash', 0, () => {
      Logging.v(TAG, 'test v shorthand');
    })

    it('injectLoggableDoesNotCrash', 0, () => {
      let loggable: webrtc.Loggable = {
        logMessage: (message: string, severity: number, tag: string) => {
          console.info('[CustomLoggable]', tag, message);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.d(TAG, 'log after injectLoggable');
    })

    it('logAfterInjectLoggableUsesCustomLoggable', 0, () => {
      let receivedMessages: string[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (message: string, _severity: number, _tag: string) => {
          receivedMessages.push(message);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.d(TAG, 'test custom log message');
      expect(receivedMessages.length).assertLarger(0);
      expect(receivedMessages[0]).assertContain('test custom log message');
    })

    it('deleteInjectedLoggableDoesNotCrash', 0, () => {
      let loggable: webrtc.Loggable = {
        logMessage: (message: string, severity: number, tag: string) => {}
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.deleteInjectedLoggable();
      Logging.d(TAG, 'log after deleteInjectedLoggable');
    })

    it('enableLogToDebugOutputAfterInjectLoggableThrows', 0, () => {
      let loggable: webrtc.Loggable = {
        logMessage: (message: string, severity: number, tag: string) => {}
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      try {
        Logging.enableLogToDebugOutput(LoggingSeverity.INFO);
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
      Logging.deleteInjectedLoggable();
    })

    it('severityFilterFiltersBelowThreshold', 0, () => {
      let receivedMessages: string[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (message: string, _severity: number, _tag: string) => {
          receivedMessages.push(message);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.ERROR);
      Logging.d(TAG, 'info message should be filtered');
      Logging.e(TAG, 'error message should pass');
      expect(receivedMessages.length).assertEqual(1);
      expect(receivedMessages[0]).assertContain('error message should pass');
      Logging.deleteInjectedLoggable();
    })
  })
}
