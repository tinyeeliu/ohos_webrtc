import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium';
import webrtc from 'libohos_webrtc.so';
import { Logging, LoggingSeverity } from '../../../main/ets/log/Logging';

const TAG: string = '[RTCPeerConnectionGetStatsTest]';

const pcf = new webrtc.PeerConnectionFactory();
let caller: webrtc.RTCPeerConnection;

export default function RTCPeerConnectionGetStatsTest() {
  describe('RTCPeerConnectionGetStatsTest', () => {
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
      caller = pcf.createPeerConnection({});
    })
    afterEach(() => {
      // Presets a clear action, which is performed after each unit test case ends.
      // The number of execution times is the same as the number of test cases defined by **it**.
      // This API supports only one parameter: clear action function.
      caller.close();
    })
    afterAll(() => {
      // Presets a clear action, which is performed after all test cases of the test suite end.
      // This API supports only one parameter: clear action function.
    })

    it('getStatsWithNoArgument', 0, async () => {
      await caller.getStats().then((statsReport) => {
        let found = false;
        statsReport.stats.forEach((value: webrtc.RTCStats, _key: string) => {
          if (value.type === 'peer-connection') {
            found = true;
          }
        });
        expect(found).assertTrue();
      }).catch(() => {
        expect().assertFail();
      });
    })

    it('getStatsWithNull', 0, async () => {
      await caller.getStats(null).then(() => {
      }).catch(() => {
        expect().assertFail();
      });
    })

    it('getStatsWithUndefined', 0, async () => {
      await caller.getStats(undefined).then(() => {
      }).catch(() => {
        expect().assertFail();
      });
    })

    it('getStatsWithNotAddedTrack', 0, async () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack("audio", audioSource);

      await caller.getStats(audioTrack).then(() => {
        expect().assertFail();
      }).catch((e: Error) => {
        // TODO: should reject width InvalidAccessError
        console.log(TAG, 'get stats error: ' + e);
        expect(e).assertInstanceOf('Error');
      });
    })

    it('getStatsWithAddedTrack', 0, async () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack("audio", audioSource);

      caller.addTrack(audioTrack);

      await caller.getStats(audioTrack).then(() => {
      }).catch(() => {
        expect().assertFail();
      });
    })

    it('getStatsWithTrackViaAddTransceiver', 0, async () => {
      let audioSource = pcf.createAudioSource();
      let audioTrack = pcf.createAudioTrack("audio", audioSource);

      caller.addTransceiver(audioTrack);

      await caller.getStats(audioTrack).then(() => {
      }).catch(() => {
        expect().assertFail();
      });
    })

    it('getStatsWithTrackAssociatedWithBothSenderAndReceiver', 0, async () => {

      const transceiver1 = caller.addTransceiver('audio');

      // Create another transceiver that resends what
      // is being received, kind of like echo
      const transceiver2 = caller.addTransceiver(transceiver1.receiver!.track);

      expect(transceiver1.receiver!.track.id).assertEqual(transceiver2.sender!.track!.id);

      await caller.getStats(transceiver1.receiver!.track).then(() => {
        expect().assertFail();
      }).catch((e: Error) => {
        // TODO: should reject width InvalidAccessError
        console.log(TAG, 'get stats error: ' + e);
        expect(e).assertInstanceOf('Error');
      });
    })
  })
}
