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
import RTCIceCandidateTest from './RTCIceCandidate.test';

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
  RTCIceCandidateTest();
}