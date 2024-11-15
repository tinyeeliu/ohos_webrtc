import abilityTest from './Ability.test';
import PeerConnectionTest from './PeerConnection.test';
import RTCPeerConnectionGetStatsTest from './RTCPeerConnection-getStats.test';
import RTCRtpReceiverTest from './RTCRtpReceiver.test';
import RTCRtpSenderTest from './RTCRtpSender.test';
import RTCRtpTransceiverTest from './RTCRtpTransceiver.test';
import DataChannelTest from './DataChannel.test';
import RTCDtmfSenderTest from './RTCDtmfSender.test';
import RTCSctpTransportTest from './RTCSctpTransport.test';
import RTCCertificateTest from './RTCCertificate.test';
import RTCIceTransportTest from './RTCIceTransport.test';
import RTCDtlsTransportTest from './RTCDtlsTransport.test';

export default function testsuite() {
  abilityTest();
  PeerConnectionTest();
  RTCRtpSenderTest();
  RTCRtpReceiverTest();
  RTCRtpTransceiverTest();
  RTCPeerConnectionGetStatsTest();
  DataChannelTest();
  RTCDtmfSenderTest();
  RTCSctpTransportTest();
  RTCCertificateTest();
  RTCIceTransportTest();
  RTCDtlsTransportTest();
}