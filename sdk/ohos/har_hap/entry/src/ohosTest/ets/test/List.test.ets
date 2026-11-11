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
import MediaStreamTest from './MediaStream.test';
import MediaDevicesTest from './MediaDevices.test';
import AudioDeviceModuleTest from './AudioDeviceModule.test';
import NativeVideoRendererTest from './NativeVideoRenderer.test';
import AudioSourceTest from './AudioSource.test';
import PeerConnectionFactoryTest from './PeerConnectionFactory.test';
import MediaStreamTrackTest from './MediaStreamTrack.test';
import LoggingTest from './Logging.test';
import RTCSessionDescriptionTest from './RTCSessionDescription.test';
import VideoSourceTest from './VideoSource.test';
import VideoRenderControllerTest from './VideoRenderController.test';
import CameraPreviewSessionTest from './CameraPreviewSession.test';
import VideoEncoderFactoryTest from './VideoEncoderFactory.test';
import X86EmulatorFallbackTest from './X86EmulatorFallback.test';
import CameraZoomTest from './CameraZoom.test';

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
  MediaStreamTest();
  MediaDevicesTest();
  AudioDeviceModuleTest();
  NativeVideoRendererTest();
  AudioSourceTest();
  PeerConnectionFactoryTest();
  MediaStreamTrackTest();
  LoggingTest();
  RTCSessionDescriptionTest();
  VideoSourceTest();
  VideoRenderControllerTest();
  CameraPreviewSessionTest();
  VideoEncoderFactoryTest();
  X86EmulatorFallbackTest();
  CameraZoomTest();
}