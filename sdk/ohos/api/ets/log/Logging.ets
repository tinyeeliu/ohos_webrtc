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

import {Loggable, NativeLogging} from 'libohos_webrtc.so'

/** Log severity levels, ordered from most verbose to silent. */
export enum LoggingSeverity { VERBOSE, INFO, WARNING, ERROR, NONE }

/**
 * Static logging facade with three delivery targets, in priority order:
 * an injected Loggable (filtered by its severity), the native debug output
 * (after {@link enableLogToDebugOutput}), and the system console fallback.
 *
 * Injecting a Loggable and enabling native debug output are mutually exclusive:
 * {@link enableLogToDebugOutput} throws while a Loggable is injected.
 *
 * All methods are failure-tolerant: a logging error is never propagated to
 * the caller, because this class is the facility used to report errors.
 */
export class Logging {
  private static loggingEnabled: boolean = false;
  private static loggable: Loggable | null = null;
  private static loggableSeverity: LoggingSeverity = LoggingSeverity.ERROR;

  /**
   * Injects a Loggable that receives subsequent log messages at or above the
   * given severity. The Loggable is held by a static strong reference (and by
   * the native side via a persistent reference) until removed: if it captures
   * UI component context, that context stays alive. Call
   * {@link deleteInjectedLoggable} from the host component's aboutToDisappear.
   *
   * @param injectedLoggable receiver for log messages; null is ignored.
   * @param severity minimum severity delivered to the Loggable.
   */
  static injectLoggable(injectedLoggable: Loggable, severity: LoggingSeverity) {
    if (injectedLoggable != null) {
      Logging.loggable = injectedLoggable;
      Logging.loggableSeverity = severity;
      try {
        NativeLogging.injectLoggable(injectedLoggable, severity);
      } catch (e) {
        // Keep the ArkTS-side Loggable active; native injection must not crash the caller.
        console.error('Logging: injectLoggable failed:', e);
      }
    }
  }

  /**
   * Removes the injected Loggable, if any. Must be called before the host
   * component disappears if the injected Loggable captures component context.
   */
  static deleteInjectedLoggable() {
    Logging.loggable = null;
    try {
      NativeLogging.deleteLoggable();
    } catch (e) {
      // Logging teardown must never crash the caller.
      console.error('Logging: deleteInjectedLoggable failed:', e);
    }
  }

  /** Enables thread id printing in native log output. */
  static enableLogThreads() {
    try {
      NativeLogging.enableLogThreads();
    } catch (e) {
      // Logging configuration must never crash the caller.
      console.error('Logging: enableLogThreads failed:', e);
    }
  }

  /** Enables timestamps in native log output. */
  static enableLogTimeStamps() {
    try {
      NativeLogging.enableLogTimeStamps();
    } catch (e) {
      // Logging configuration must never crash the caller.
      console.error('Logging: enableLogTimeStamps failed:', e);
    }
  }

  /**
   * Enables forwarding native logs to the debug output at or above the given
   * severity. Must not be called while a Loggable is injected.
   *
   * @param severity minimum severity forwarded to the debug output.
   * @throws Error if a Loggable is currently injected.
   */
  static enableLogToDebugOutput(severity: LoggingSeverity) {
    if (severity < LoggingSeverity.VERBOSE || severity > LoggingSeverity.NONE) {
      return;
    }
    if (Logging.loggable != null) {
      throw new Error("Logging to native debug output not supported while Loggable is injected."
        + "Delete the Loggable before calling this method.");
    }
    try {
      NativeLogging.enableLogToDebugOutput(severity);
      Logging.loggingEnabled = true;
    } catch (e) {
      // Leave debug output disabled; configuration must never crash the caller.
      console.error('Logging: enableLogToDebugOutput failed:', e);
    }
  }

  /**
   * Dispatches a log message to the active delivery target.
   *
   * @param severity severity of the message.
   * @param tag origin tag of the message.
   * @param message log body.
   */
  static log(severity: LoggingSeverity, tag: string, message: string) {
    if (severity < LoggingSeverity.VERBOSE || severity > LoggingSeverity.NONE) {
      return;
    }
    if (tag == null || message == null) {
      return;
    }
    try {
      if (Logging.loggable) {
        // Filter log messages below loggableSeverity.
        if (severity < Logging.loggableSeverity) {
          return;
        }

        Logging.loggable.logMessage(message, severity, tag);
        return;
      }

      // Try native logging if no loggable is injected.
      if (Logging.loggingEnabled) {
        NativeLogging.log(message, severity, tag);
        return;
      }

      // Fallback to system
      switch (severity) {
        case LoggingSeverity.ERROR:
          console.error(tag, message);
          break;
        case LoggingSeverity.WARNING:
          console.warn(tag, message);
          break;
        case LoggingSeverity.INFO:
          console.info(tag, message);
          break;
        default:
          console.debug(tag, message);
          break;
      }
    } catch (e) {
      // A logging failure must never propagate to the caller.
      console.error('Logging: log delivery failed:', e);
    }
  }

  /** Logs an INFO message. */
  static d(tag: string, message: string) {
    Logging.log(LoggingSeverity.INFO, tag, message);
  }

  /** Logs an ERROR message, plus the message of the optional Error. */
  static e(tag: string, message: string, e?: Error) {
    Logging.log(LoggingSeverity.ERROR, tag, message);
    if (e != undefined) {
      Logging.log(LoggingSeverity.ERROR, tag, e.message);
      // Logging.log(LoggingSeverity.ERROR, tag, e.stack);
    }
  }

  /** Logs a WARNING message, plus the message of the given Error when defined. */
  static w(tag: string, message: string, e: Error) {
    Logging.log(LoggingSeverity.WARNING, tag, message);
    if (e != undefined) {
      Logging.log(LoggingSeverity.WARNING, tag, e.message);
      // Logging.log(LoggingSeverity.WARNING, tag, e.stack);
    }
  }

  /** Logs a VERBOSE message. */
  static v(tag: string, message: string) {
    Logging.log(LoggingSeverity.VERBOSE, tag, message);
  }

}
