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

import { describe, beforeAll, afterEach, it, expect } from '@ohos/hypium'
import webrtc from 'libohos_webrtc.so';
import { Logging, LoggingSeverity } from '../../../main/ets/log/Logging';

const TAG: string = '[LoggingTest]';

export default function LoggingTest() {
  describe('LoggingTest', () => {
    beforeAll(() => {
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    afterEach(() => {
      Logging.deleteInjectedLoggable();
      Logging.enableLogToDebugOutput(LoggingSeverity.VERBOSE);
    })

    it('dShorthandProducesLogWithInfoSeverity', 0, () => {
      let receivedSeverities: number[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (_message: string, severity: number, _tag: string) => {
          receivedSeverities.push(severity);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.d(TAG, 'test d');
      expect(receivedSeverities.length).assertLarger(0);
      expect(receivedSeverities[0]).assertEqual(LoggingSeverity.INFO);
    })

    it('eShorthandProducesLogWithErrorSeverity', 0, () => {
      let receivedSeverities: number[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (_message: string, severity: number, _tag: string) => {
          receivedSeverities.push(severity);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.e(TAG, 'test e');
      expect(receivedSeverities.length).assertLarger(0);
      expect(receivedSeverities[0]).assertEqual(LoggingSeverity.ERROR);
    })

    it('eShorthandIncludesErrorMessage', 0, () => {
      let receivedMessages: string[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (message: string, _severity: number, _tag: string) => {
          receivedMessages.push(message);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.e(TAG, 'error with detail', new Error('my error detail'));
      let allMessages = receivedMessages.join(' ');
      expect(allMessages).assertContain('my error detail');
    })

    it('wShorthandProducesLogWithWarningSeverity', 0, () => {
      let receivedSeverities: number[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (_message: string, severity: number, _tag: string) => {
          receivedSeverities.push(severity);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.w(TAG, 'test w', new Error('test'));
      expect(receivedSeverities.length).assertLarger(0);
      expect(receivedSeverities[0]).assertEqual(LoggingSeverity.WARNING);
    })

    it('vShorthandProducesLogWithVerboseSeverity', 0, () => {
      let receivedSeverities: number[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (_message: string, severity: number, _tag: string) => {
          receivedSeverities.push(severity);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.v(TAG, 'test v');
      expect(receivedSeverities.length).assertLarger(0);
      expect(receivedSeverities[0]).assertEqual(LoggingSeverity.VERBOSE);
    })

    it('logPassesCorrectSeverity', 0, () => {
      let receivedSeverities: number[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (_message: string, severity: number, _tag: string) => {
          receivedSeverities.push(severity);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.log(LoggingSeverity.WARNING, TAG, 'warning message');
      expect(receivedSeverities.length).assertEqual(1);
      expect(receivedSeverities[0]).assertEqual(LoggingSeverity.WARNING);
    })

    it('logPassesCorrectTag', 0, () => {
      let receivedTags: string[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (_message: string, _severity: number, tag: string) => {
          receivedTags.push(tag);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.d('MyCustomTag', 'test');
      expect(receivedTags.length).assertLarger(0);
      expect(receivedTags[0]).assertEqual('MyCustomTag');
    })

    it('vPassesCorrectTag', 0, () => {
      let receivedTags: string[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (_message: string, _severity: number, tag: string) => {
          receivedTags.push(tag);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.v('VerboseTag', 'test');
      expect(receivedTags.length).assertLarger(0);
      expect(receivedTags[0]).assertEqual('VerboseTag');
    })

    it('ePassesCorrectTag', 0, () => {
      let receivedTags: string[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (_message: string, _severity: number, tag: string) => {
          receivedTags.push(tag);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.e('ErrorTag', 'test');
      expect(receivedTags.length).assertLarger(0);
      expect(receivedTags[0]).assertEqual('ErrorTag');
    })

    it('injectLoggableReceivesMessages', 0, () => {
      let receivedMessages: string[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (message: string, _severity: number, _tag: string) => {
          receivedMessages.push(message);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.d(TAG, 'hello from test');
      expect(receivedMessages.length).assertLarger(0);
      expect(receivedMessages[0]).assertContain('hello from test');
    })

    it('deleteInjectedLoggableStopsCallbacks', 0, () => {
      let receivedCount = 0;
      let loggable: webrtc.Loggable = {
        logMessage: (_message: string, _severity: number, _tag: string) => {
          receivedCount++;
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.d(TAG, 'before delete');
      let countBeforeDelete = receivedCount;
      Logging.deleteInjectedLoggable();
      Logging.d(TAG, 'after delete');
      expect(countBeforeDelete).assertLarger(0);
      expect(receivedCount).assertEqual(countBeforeDelete);
    })

    it('enableLogToDebugOutputAfterInjectLoggableThrows', 0, () => {
      let loggable: webrtc.Loggable = {
        logMessage: (_message: string, _severity: number, _tag: string) => {}
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      try {
        Logging.enableLogToDebugOutput(LoggingSeverity.INFO);
        expect().assertFail();
      } catch (e) {
        expect(e).assertInstanceOf('Error');
      }
    })

    it('enableLogToDebugOutputWorksAfterDeleteInjectedLoggable', 0, () => {
      let loggable: webrtc.Loggable = {
        logMessage: (_message: string, _severity: number, _tag: string) => {}
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.deleteInjectedLoggable();
      Logging.enableLogToDebugOutput(LoggingSeverity.INFO);
      let receivedSeverities: number[] = [];
      let loggable2: webrtc.Loggable = {
        logMessage: (_message: string, severity: number, _tag: string) => {
          receivedSeverities.push(severity);
        }
      };
      Logging.injectLoggable(loggable2, LoggingSeverity.VERBOSE);
      Logging.d(TAG, 'after re-enable');
      expect(receivedSeverities.length).assertLarger(0);
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
    })

    it('wShorthandIncludesErrorMessage', 0, () => {
      let receivedMessages: string[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (message: string, _severity: number, _tag: string) => {
          receivedMessages.push(message);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.w(TAG, 'warning with error', new Error('my error detail'));
      let allMessages = receivedMessages.join(' ');
      expect(allMessages).assertContain('my error detail');
    })

    it('enableLogThreadsDoesNotBreakLogging', 0, () => {
      Logging.enableLogThreads();
      let receivedSeverities: number[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (_message: string, severity: number, _tag: string) => {
          receivedSeverities.push(severity);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.d(TAG, 'after enableLogThreads');
      expect(receivedSeverities.length).assertLarger(0);
    })

    it('enableLogTimeStampsDoesNotBreakLogging', 0, () => {
      Logging.enableLogTimeStamps();
      let receivedSeverities: number[] = [];
      let loggable: webrtc.Loggable = {
        logMessage: (_message: string, severity: number, _tag: string) => {
          receivedSeverities.push(severity);
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.VERBOSE);
      Logging.d(TAG, 'after enableLogTimeStamps');
      expect(receivedSeverities.length).assertLarger(0);
    })

    it('noneSeverityFiltersAllMessages', 0, () => {
      let receivedCount = 0;
      let loggable: webrtc.Loggable = {
        logMessage: (_message: string, _severity: number, _tag: string) => {
          receivedCount++;
        }
      };
      Logging.injectLoggable(loggable, LoggingSeverity.NONE);
      Logging.d(TAG, 'info should be filtered');
      Logging.e(TAG, 'error should be filtered');
      Logging.v(TAG, 'verbose should be filtered');
      expect(receivedCount).assertEqual(0);
    })
  })
}
