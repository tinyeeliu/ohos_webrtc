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

import { Event, EventTarget } from './event'
import { MediaStreamTrack, MediaStream } from './media'

export type RTCErrorDetailType = "data-channel-failure" | "dtls-failure" | "fingerprint-failure" | "hardware-encoder-error" | "hardware-encoder-not-available" | "sctp-failure" | "sdp-syntax-error";
export type RTCIceProtocol = "tcp" | "udp";
export type RTCIceCandidateType = "host" | "prflx" | "relay" | "srflx";
export type RTCIceTcpCandidateType = "active" | "passive" | "so";
export type RTCIceComponent = "rtp" | "rtcp";
export type RTCIceGathererState = "complete" | "gathering" | "new";
export type RTCIceTransportState = "checking" | "closed" | "completed" | "connected" | "disconnected" | "failed" | "new";
export type RTCIceRole = "unknown" | "controlling" | "controlled";
export type RTCSdpType = 'offer' | 'answer' | "pranswer" | "rollback";
export type BinaryType = "blob" | "arraybuffer";
export type DataChannelState = "closed" | "closing" | "connecting" | "open";
export type RTCDtlsTransportState = "new" | "connecting" | "connected" | "closed" | "failed";
export type RTCIceGatheringState = "new" | "gathering" | "complete";
export type RTCIceConnectionState = "checking" | "closed" | "completed" | "connected" | "disconnected" | "failed" | "new";
export type RTCSignalingState = "closed" | "have-local-offer" | "have-local-pranswer" | "have-remote-offer" | "have-remote-pranswer" | "stable";
export type RTCPeerConnectionState = "closed" | "connected" | "connecting" | "disconnected" | "failed" | "new";
export type RTCBundlePolicy = "balanced" | "max-bundle" | "max-compat";
export type RTCRtcpMuxPolicy = "require";
export type RTCIceTransportPolicy = "all" | "relay";
export type DegradationPreference = "balanced" | "maintain-framerate" | "maintain-resolution";
// export type RTCPriorityType = "high" | "low" | "medium" | "very-low";
export type RTCRtpTransceiverDirection = "inactive" | "recvonly" | "sendonly" | "sendrecv" | "stopped";
export type RTCSctpTransportState = "connecting" | "connected" | "closed";
export type RTCStatsType = "candidate-pair" | "certificate" | "codec" | "data-channel" | "inbound-rtp" | "local-candidate" | "media-playout" | "media-source" | "outbound-rtp" | "peer-connection" | "remote-candidate" | "remote-inbound-rtp" | "remote-outbound-rtp" | "transport";
export type RTCStatsIceCandidatePairState = "failed" | "frozen" | "in-progress" | "inprogress" | "succeeded" | "waiting";

export type AlgorithmIdentifier = Algorithm | string;
export type HighResTimeStamp = number;
export type EpochTimeStamp = number;

// error
// https://www.w3.org/TR/webrtc/#rtcerrorinit-dictionary
export interface RTCErrorInit {
  errorDetail: RTCErrorDetailType;
  sdpLineNumber?: number;
  sctpCauseCode?: number;
  receivedAlert?: number;
  sentAlert?: number;
}

// https://www.w3.org/TR/webrtc/#rtcerror-interface
export interface RTCError extends /*DOMException*/ Error {
  readonly errorDetail: RTCErrorDetailType;
  readonly sdpLineNumber?: number;
  readonly sctpCauseCode?: number;
  readonly receivedAlert?: number;
  readonly sentAlert?: number;
}

// https://www.w3.org/TR/webrtc/#rtcerrorevent-interface
export interface RTCErrorEvent extends Event {
  readonly error: RTCError;
}

// https://www.w3.org/TR/webrtc/#rtctrackevent
export interface RTCTrackEvent extends Event {
  readonly receiver: RTCRtpReceiver;
  readonly track: MediaStreamTrack;
  readonly streams: ReadonlyArray<MediaStream>;
  readonly transceiver: RTCRtpTransceiver;
}

// https://www.w3.org/TR/webrtc/#rtcpeerconnectioniceevent
export interface RTCPeerConnectionIceEvent extends Event {
  readonly candidate?: RTCIceCandidate;
  readonly url?: string | null;
}

// https://www.w3.org/TR/webrtc/#rtcpeerconnectioniceerrorevent
export interface RTCPeerConnectionIceErrorEvent extends Event {
  readonly address?: string;
  readonly port?: number;
  readonly url?: string;
  readonly errorCode?: number;
  readonly errorText?: string;
}

// https://www.w3.org/TR/webrtc/#rtcdatachannelevent
export interface RTCDataChannelEvent extends Event {
  readonly channel: RTCDataChannel;
}

// https://www.w3.org/TR/webrtc/#rtcdtmftonechangeevent
export interface RTCDTMFToneChangeEvent extends Event {
  readonly tone: string;
}

// https://html.spec.whatwg.org/multipage/comms.html#the-messageevent-interface
export interface MessageEvent<T = any> extends Event {
  readonly data: T;
}

// https://www.w3.org/TR/WebCryptoAPI/#algorithm
export interface Algorithm {
  name: string;
}

// https://www.w3.org/TR/webrtc/#dom-rtcrtptransceiverinit
export interface RTCRtpTransceiverInit {
  direction?: RTCRtpTransceiverDirection;
  streams?: MediaStream[];
  sendEncodings?: RTCRtpEncodingParameters[];
}

// https://www.w3.org/TR/webrtc/#dom-rtcsessiondescriptioninit
export interface RTCSessionDescriptionInit {
  sdp?: string;
  type: RTCSdpType;
}

// https://www.w3.org/TR/webrtc/#rtcsessiondescription-class
export interface RTCSessionDescription {
  readonly sdp: string;
  readonly type: RTCSdpType;

  toJSON(): RTCSessionDescriptionInit;
}

declare var RTCSessionDescription: {
  prototype: RTCSessionDescription;
  new(descriptionInitDict: RTCSessionDescriptionInit): RTCSessionDescription;
};

// https://www.w3.org/TR/webrtc/#dom-rtcicecandidateinit
export interface RTCIceCandidateInit {
  candidate: string;
  sdpMLineIndex?: number;
  sdpMid?: string;
  usernameFragment?: string;
}

// https://www.w3.org/TR/webrtc/#rtcicecandidate-interface
export interface RTCIceCandidate {
  readonly candidate: string;
  readonly sdpMid?: string;
  readonly sdpMLineIndex?: number;
  readonly foundation?: string;
  readonly component?: RTCIceComponent;
  readonly priority?: number;
  readonly address?: string;
  readonly protocol?: RTCIceProtocol;
  readonly port?: number;
  readonly type?: RTCIceCandidateType;
  readonly tcpType?: RTCIceTcpCandidateType;
  readonly relatedAddress?: string;
  readonly relatedPort?: number;
  readonly usernameFragment?: string;

  constructor(candidateInitDict?: RTCIceCandidateInit);

  toJSON(): RTCIceCandidateInit;
}

declare var RTCIceCandidate: {
  prototype: RTCIceCandidate;
  new(candidateInitDict?: RTCIceCandidateInit): RTCIceCandidate;
};

// https://www.w3.org/TR/webrtc/#rtcdatachannel
export interface RTCDataChannel {
  readonly label: string;
  readonly ordered: boolean;
  readonly maxPacketLifeTime?: number;
  readonly maxRetransmits?: number;
  readonly protocol: string;
  readonly negotiated: boolean;
  readonly id?: number;
  readonly readyState: DataChannelState;
  readonly bufferedAmount: number;
  bufferedAmountLowThreshold: number;
  binaryType: BinaryType;

  onbufferedamountlow: ((this: RTCDataChannel, ev: Event) => any) | null;
  onclose: ((this: RTCDataChannel, ev: Event) => any) | null;
  onclosing: ((this: RTCDataChannel, ev: Event) => any) | null;
  onopen: ((this: RTCDataChannel, ev: Event) => any) | null;
  onmessage: ((this: RTCDataChannel, ev: MessageEvent<any>) => any) | null;
  onerror: ((this: RTCDataChannel, ev: RTCErrorEvent) => any) | null;

  close(): void;
  send(data: string): void;
  send(data: ArrayBuffer): void;
}

declare var RTCDataChannel: {
  prototype: RTCDataChannel;
  new(): RTCDataChannel;
};

// https://www.w3.org/TR/webrtc/#dom-rtcdatachannelinit
export interface RTCDataChannelInit {
  ordered?: boolean;
  maxPacketLifeTime?: number;
  maxRetransmits?: number;
  protocol?: string;
  negotiated?: boolean;
  id?: number;
}

// https://www.w3.org/TR/webrtc/#configuration
export interface RTCConfiguration {
  iceServers?: RTCIceServer[];
  iceTransportPolicy?: RTCIceTransportPolicy;
  bundlePolicy?: RTCBundlePolicy;
  rtcpMuxPolicy?: RTCRtcpMuxPolicy;
  certificates?: RTCCertificate[];
  iceCandidatePoolSize?: number;
  // tcpCandidatePolicy
  // continualGatheringPolicy
}

// https://www.w3.org/TR/webrtc/#dom-rtciceserver
export interface RTCIceServer {
  urls: string | string[];
  username?: string;
  credential?: string;
}

// https://www.w3.org/TR/webrtc/#dom-rtcofferansweroptions
export interface RTCOfferAnswerOptions {
}

// https://www.w3.org/TR/webrtc/#dom-rtcofferoptions
export interface RTCOfferOptions extends RTCOfferAnswerOptions {
  iceRestart?: boolean;
}

// https://www.w3.org/TR/webrtc/#dom-rtcansweroptions
export interface RTCAnswerOptions extends RTCOfferAnswerOptions {
}

// https://www.w3.org/TR/webrtc/#rtcpeerconnection-interface
// https://www.w3.org/TR/webrtc/#rtcpeerconnection-interface-extensions
// https://www.w3.org/TR/webrtc/#rtcpeerconnection-interface-extensions-0
// https://www.w3.org/TR/webrtc/#rtcpeerconnection-interface-extensions-1
export interface RTCPeerConnection extends EventTarget {
  readonly canTrickleIceCandidates?: boolean;
  readonly signalingState: RTCSignalingState;
  readonly iceGatheringState: RTCIceGatheringState;
  readonly iceConnectionState: RTCIceConnectionState;
  readonly connectionState: RTCPeerConnectionState;
  readonly localDescription?: RTCSessionDescription;
  readonly remoteDescription?: RTCSessionDescription;
  readonly currentLocalDescription?: RTCSessionDescription;
  readonly currentRemoteDescription?: RTCSessionDescription;
  readonly pendingLocalDescription?: RTCSessionDescription;
  readonly pendingRemoteDescription?: RTCSessionDescription;
  readonly sctp?: RTCSctpTransport;

  onnegotiationneeded: ((this: RTCPeerConnection, ev: Event) => any) | null;
  onicecandidate: ((this: RTCPeerConnection, ev: RTCPeerConnectionIceEvent) => any) | null;
  onicecandidateerror: ((this: RTCPeerConnection, ev: RTCPeerConnectionIceErrorEvent) => any) | null;
  oniceconnectionstatechange: ((this: RTCPeerConnection, ev: Event) => any) | null;
  onicegatheringstatechange: ((this: RTCPeerConnection, ev: Event) => any) | null;
  onsignalingstatechange: ((this: RTCPeerConnection, ev: Event) => any) | null;
  onconnectionstatechange: ((this: RTCPeerConnection, ev: Event) => any) | null;
  ontrack: ((this: RTCPeerConnection, ev: RTCTrackEvent) => any) | null;
  ondatachannel: ((this: RTCPeerConnection, ev: RTCDataChannelEvent) => any) | null;

  addTrack(track: MediaStreamTrack, ...streams: MediaStream[]): RTCRtpSender;
  removeTrack(sender: RTCRtpSender): void;
  setLocalDescription(description?: RTCSessionDescriptionInit): Promise<void>;
  setRemoteDescription(description: RTCSessionDescriptionInit): Promise<void>;
  createOffer(options?: RTCOfferOptions): Promise<RTCSessionDescriptionInit>;
  createAnswer(options?: RTCAnswerOptions): Promise<RTCSessionDescriptionInit>;
  createDataChannel(label: string, dataChannelDict?: RTCDataChannelInit): RTCDataChannel;
  addIceCandidate(candidate?: RTCIceCandidateInit): Promise<void>;
  getSenders(): RTCRtpSender[];
  getReceivers(): RTCRtpReceiver[];
  getTransceivers(): RTCRtpTransceiver[];
  getConfiguration(): RTCConfiguration;
  restartIce(): void;
  setConfiguration(configuration?: RTCConfiguration): void;
  addTransceiver(trackOrKind: MediaStreamTrack | string, init?: RTCRtpTransceiverInit): RTCRtpTransceiver;
  close(): void;
  getStats(selector?: MediaStreamTrack): Promise<RTCStatsReport>;
}

declare var RTCPeerConnection: {
  prototype: RTCPeerConnection;
  new(configuration?: RTCConfiguration): RTCPeerConnection;
  // https://www.w3.org/TR/webrtc/#sec.cert-mgmt
  generateCertificate(keygenAlgorithm: AlgorithmIdentifier): Promise<RTCCertificate>;
};

// https://www.w3.org/TR/webrtc/#rtcrtpreceiver-interface
export interface RTCRtpReceiver {
  readonly track: MediaStreamTrack;
  readonly transport: RTCDtlsTransport | null;

  getParameters(): RTCRtpReceiveParameters;
  getStats(): Promise<RTCStatsReport>;
  getContributingSources(): RTCRtpContributingSource[];
  getSynchronizationSources(): RTCRtpSynchronizationSource[];
}

declare var RTCRtpReceiver: {
  prototype: RTCRtpReceiver;
  new(): RTCRtpReceiver;
  getCapabilities(kind: string): RTCRtpCapabilities | null;
};

// https://www.w3.org/TR/webrtc/#dom-rtcrtpcodingparameters
export interface RTCRtpCodingParameters {
  rid?: string;
}

// https://www.w3.org/TR/webrtc/#dom-rtcrtpencodingparameters
export interface RTCRtpEncodingParameters extends RTCRtpCodingParameters {
  active?: boolean;
  maxBitrate?: number;
  maxFramerate?: number;
  scaleResolutionDownBy?: number;
  // networkPriority?: RTCPriorityType;
  // priority?: RTCPriorityType;
}

// https://www.w3.org/TR/webrtc/#dom-rtcrtpcodecparameters
export interface RTCRtpCodecParameters {
  clockRate: number;
  channels?: number;
  mimeType: string;
  sdpFmtpLine: string;
  payloadType: number;
}

// https://www.w3.org/TR/webrtc/#dom-rtcrtpheaderextensionparameters
export interface RTCRtpHeaderExtensionParameters {
  id: number;
  uri: string;
  encrypted?: boolean;
}

// https://www.w3.org/TR/webrtc/#dom-rtcrtcpparameters
export interface RTCRtcpParameters {
  cname?: string;
  reducedSize?: boolean;
}

// https://www.w3.org/TR/webrtc/#dom-rtcrtpparameters
export interface RTCRtpParameters {
  codecs: RTCRtpCodecParameters[];
  headerExtensions: RTCRtpHeaderExtensionParameters[];
  rtcp: RTCRtcpParameters;
}

// https://www.w3.org/TR/webrtc/#dom-rtcrtpsendparameters
export interface RTCRtpSendParameters extends RTCRtpParameters {
  // degradationPreference?: DegradationPreference;
  encodings: RTCRtpEncodingParameters[];
  transactionId: string;
}

// https://www.w3.org/TR/webrtc/#dom-rtcrtpreceiveparameters
export interface RTCRtpReceiveParameters extends RTCRtpParameters {
}

// https://www.w3.org/TR/webrtc/#dom-rtcrtpcontributingsource
export interface RTCRtpContributingSource {
  timestamp: HighResTimeStamp;
  source: number;
  audioLevel?: number;
  rtpTimestamp: number;
}

// https://www.w3.org/TR/webrtc/#dom-rtcrtpsynchronizationsource
export interface RTCRtpSynchronizationSource extends RTCRtpContributingSource {
}

// https://www.w3.org/TR/webrtc/#rtcrtpsender-interface
// https://www.w3.org/TR/webrtc/#rtcrtpsender-interface-extensions
export interface RTCRtpSender {
  readonly track: MediaStreamTrack | null;
  readonly transport: RTCDtlsTransport | null;
  readonly dtmf: RTCDTMFSender | null;

  setParameters(parameters: RTCRtpSendParameters): Promise<void>;
  getParameters(): RTCRtpSendParameters;
  replaceTrack(withTrack: MediaStreamTrack | null): Promise<void>;
  setStreams(...streams: MediaStream[]): void;
  getStats(): Promise<RTCStatsReport>;
}

declare var RTCRtpSender: {
  prototype: RTCRtpSender;
  new(): RTCRtpSender;
  /**
   * get the most optimistic view of the capabilities of the system for sending media of the given kind.
   * @param kind 'audio' or 'video'.
   * @returns instance of RTCRtpCapabilities, or null if has no capabilities corresponding to the value of the kind argument.
   */
  getCapabilities(kind: string): RTCRtpCapabilities | null;
};

// https://www.w3.org/TR/webrtc/#dom-rtcrtpcodec
export interface RTCRtpCodec {
  mimeType: string;
  clockRate: number;
  channels?: number;
  sdpFmtpLine?: string;
}

// https://www.w3.org/TR/webrtc/#rtcrtpheaderextensioncapability
export interface RTCRtpHeaderExtensionCapability {
  uri: string;
}

// https://www.w3.org/TR/webrtc/#rtcrtpcapabilities
export interface RTCRtpCapabilities {
  codecs: RTCRtpCodec[];
  headerExtensions: RTCRtpHeaderExtensionCapability[];
}

// https://www.w3.org/TR/webrtc/#rtcdtmfsender
export interface RTCDTMFSender extends EventTarget {
  readonly canInsertDTMF: boolean;
  readonly toneBuffer: string;

  ontonechange: ((this: RTCDTMFSender, ev: RTCDTMFToneChangeEvent) => any) | null;

  insertDTMF(tones: string, duration?: number, interToneGap?: number): void;
}

declare var RTCDTMFSender: {
  prototype: RTCDTMFSender;
  new(): RTCDTMFSender;
};

// https://www.w3.org/TR/webrtc/#rtcrtptransceiver-interface
export interface RTCRtpTransceiver {
  readonly mid: string | null;
  readonly sender: RTCRtpSender;
  readonly receiver: RTCRtpReceiver;
  direction: RTCRtpTransceiverDirection;
  readonly currentDirection: RTCRtpTransceiverDirection | null;

  stop(): void;
  setCodecPreferences(codecs: RTCRtpCodec[]): void;
}

declare var RTCRtpTransceiver: {
  prototype: RTCRtpTransceiver;
  new(): RTCRtpTransceiver;
};

// https://www.w3.org/TR/webrtc/#rtcdtlstransport-interface
export interface RTCDtlsTransport extends EventTarget {
  readonly iceTransport: RTCIceTransport;
  readonly state: RTCDtlsTransportState;

  onstatechange: ((this: RTCDtlsTransport, ev: Event) => any) | null;
  onerror: ((this: RTCDtlsTransport, ev: RTCErrorEvent) => any) | null;

  getRemoteCertificates(): ArrayBuffer[];
}

declare var RTCDtlsTransport: {
  prototype: RTCDtlsTransport;
  new(): RTCDtlsTransport;
};

// https://www.w3.org/TR/webrtc/#rtcdtlsfingerprint
export interface RTCDtlsFingerprint {
  algorithm?: string;
  value?: string;
}

// https://www.w3.org/TR/webrtc/#rtccertificate-interface
export interface RTCCertificate {
  readonly expires: EpochTimeStamp;

  getFingerprints(): RTCDtlsFingerprint[];
}

declare var RTCCertificate: {
  prototype: RTCCertificate;
  new(): RTCCertificate;
};

// https://www.w3.org/TR/webrtc/#rtcicetransport
export interface RTCIceTransport extends EventTarget {
  readonly role: RTCIceRole;
  readonly component: RTCIceComponent;
  readonly state: RTCIceTransportState;
  readonly gatheringState: RTCIceGathererState;

  onstatechange: ((this: RTCIceTransport, ev: Event) => any) | null;
  ongatheringstatechange: ((this: RTCIceTransport, ev: Event) => any) | null;
  onselectedcandidatepairchange: ((this: RTCIceTransport, ev: Event) => any) | null;

  getSelectedCandidatePair(): RTCIceCandidatePair | null;
}

declare var RTCIceTransport: {
  prototype: RTCIceTransport;
  new(): RTCIceTransport;
};

// https://www.w3.org/TR/webrtc/#rtcicecandidatepair
export interface RTCIceCandidatePair {
  local?: RTCIceCandidate;
  remote?: RTCIceCandidate;
}

// https://www.w3.org/TR/webrtc/#rtcsctptransport-interface
export interface RTCSctpTransport extends EventTarget {
  readonly maxChannels?: number;
  readonly maxMessageSize: number;
  readonly state: RTCSctpTransportState;
  readonly transport: RTCDtlsTransport;

  onstatechange: ((this: RTCSctpTransport, ev: Event) => any) | null;
}

declare var RTCSctpTransport: {
  prototype: RTCSctpTransport;
  new(): RTCSctpTransport;
};

// https://www.w3.org/TR/webrtc/#rtcstats-dictionary
export interface RTCStats {
  timestamp: HighResTimeStamp;
  type: RTCStatsType;
  id: string;
}

// https://www.w3.org/TR/webrtc/#rtcstatsreport-object
export interface RTCStatsReport {
  readonly stats: Map<string, RTCStats>;
  // readonly timestamp: HighResTimeStamp; // android
  // forEach(callback: (value: RTCStats, key: string, parent: RTCStatsReport) => void): void;
}

// https://www.w3.org/TR/webrtc-stats/#dom-rtctransportstats
export interface RTCTransportStats extends RTCStats {
  packetsSent?: number;
  packetsReceived?: number;
  bytesSent?: number;
  bytesReceived?: number;
  iceRole?: RTCIceRole;
  iceLocalUsernameFragment?: string;
  dtlsState: RTCDtlsTransportState;
  iceState?: RTCIceTransportState;
  selectedCandidatePairId?: string;
  localCertificateId?: string;
  remoteCertificateId?: string;
  tlsVersion?: string;
  dtlsCipher?: string;
  // dtlsRole?: RTCDtlsRole;
  srtpCipher?: string;
  selectedCandidatePairChanges: number;
}

// https://www.w3.org/TR/webrtc-stats/#dom-rtcrtpstreamstats
export interface RTCRtpStreamStats extends RTCStats {
  ssrc: number;
  kind: string;
  transportId?: string;
  codecId?: string;
}

// https://www.w3.org/TR/webrtc-stats/#dom-rtcicecandidatepairstats
export interface RTCIceCandidatePairStats extends RTCStats {
  transportId: string;
  localCandidateId: string;
  remoteCandidateId: string;
  state: RTCStatsIceCandidatePairState;
  nominated?: boolean;
  packetsSent?: number;
  packetsReceived?: number;
  bytesSent?: number;
  bytesReceived?: number;
  lastPacketSentTimestamp?: HighResTimeStamp;
  lastPacketReceivedTimestamp?: HighResTimeStamp;
  totalRoundTripTime?: number;
  currentRoundTripTime?: number;
  availableOutgoingBitrate?: number;
  availableIncomingBitrate?: number;
  requestsReceived?: number;
  requestsSent?: number;
  responsesReceived?: number;
  responsesSent?: number;
  consentRequestsSent?: number;
  packetsDiscardedOnSend?: number;
  bytesDiscardedOnSend?: number;
}
