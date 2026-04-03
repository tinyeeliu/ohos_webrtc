# <center>ohos_webrtc</center>

This project is based on [WebRTC](https://webrtc.googlesource.com/src).

## Introduction

ohos_webrtc is a real-time communication library for OpenHarmony/HarmonyOS. It provides ArkTS interface bindings and adapts video capture, rendering, and codec modules at the C++ layer. Most interfaces follow the [WebRTC specification](https://www.w3.org/TR/webrtc/), and additional interfaces such as PeerConnectionFactory and AudioDeviceModule are also provided.

**Figure 1** SDK Architecture

![architecture](./img/webrtc-arch.png)


## Installation

```bash
ohpm install @ohos/webrtc
```

For more information on configuring the OpenHarmony ohpm environment, see [How to Install OpenHarmony ohpm Packages](https://gitcode.com/openharmony-tpc/docs/blob/master/OpenHarmony_har_usage.md).

## Constraints

### Compatibility

Verified on the following versions:

- DevEco Studio: NEXT Beta2(5.0.3.900), SDK: API12 Release(5.0.0.71), ROM: 5.0.0.71

> **OHOS SDK API 12 or above is required.**

### Permissions

Open `entry/src/main/module.json5` and add the following permissions as needed:

```json
"requestPermissions": [
  {
    "name": "ohos.permission.MICROPHONE",
    "reason": "$string:microphone_reason",
    "usedScene": {
      "abilities": ["EntryAbility"],
      "when": "inuse"
    }
  },
  {
    "name": "ohos.permission.CAMERA",
    "reason": "$string:camera_reason",
    "usedScene": {
      "abilities": ["EntryAbility"],
      "when": "inuse"
    }
  },
  {
    "name": "ohos.permission.CAPTURE_SCREEN",
    "reason": "$string:screen_capture_reason",
    "usedScene": {
      "abilities": ["EntryAbility"],
      "when": "inuse"
    }
  }
]
```

## Usage Example

The following example demonstrates a basic audio/video call setup flow:

```typescript
import webrtc from '@ohos/webrtc';

// Create a PeerConnectionFactory instance
let peerConnectionFactory = new webrtc.PeerConnectionFactory();

// Create a local audio source and audio track
let audioSource = peerConnectionFactory.createAudioSource();
let audioTrack = peerConnectionFactory.createAudioTrack('audio', audioSource);

// Create a local video source and video track (resolution 640×480, frame rate 15~30fps)
let videoSource = peerConnectionFactory.createVideoSource({
  width: 640,
  height: 480,
  frameRate: { min: 15, max: 30 }
});
let videoTrack = peerConnectionFactory.createVideoTrack('video', videoSource);

// Create an RTCPeerConnection
let peerConnection = peerConnectionFactory.createPeerConnection({
  iceServers: [{ urls: 'stun:stun.example.com' }]
});

// Add media tracks to the connection
peerConnection.addTrack(audioTrack);
peerConnection.addTrack(videoTrack);

// Listen for ICE candidates
peerConnection.onicecandidate = (event) => {
  if (event.candidate) {
    // Send the candidate to the remote peer via the signaling channel
  }
};

// Create and set the local Offer description
let offer = await peerConnection.createOffer();
await peerConnection.setLocalDescription(offer);
// Send the SDP offer to the remote peer via the signaling channel
```

For more examples, see the demo project in the `har_hap` directory. For instructions on compiling the dynamic library, see the [Build Documentation](../../docs/ohos/webrtc_build.md).

The implementation code is in the `src/ohos_webrtc` directory. NAPI interface bindings are built with the open-source project [node-addon-api](https://github.com/nodejs/node-addon-api).

## API Reference

Full type definitions are in `api/libohos_webrtc/webrtc.d.ts`. For detailed documentation, see the [API Documentation](./api/libwebrtc.md).

### PeerConnectionFactory

| Name | Description | Parameters | Return Value |
|------|-------------|------------|--------------|
| new | Constructs a factory instance | options?: PeerConnectionFactoryOptions | PeerConnectionFactory |
| getDefault | Returns the default factory instance (static) | - | PeerConnectionFactory |
| setDefault | Sets the default factory instance (static) | factory: PeerConnectionFactory | void |
| createPeerConnection | Creates an RTCPeerConnection instance | config: RTCConfiguration | RTCPeerConnection |
| createAudioSource | Creates an audio source | constraints?: MediaTrackConstraints | AudioSource |
| createAudioTrack | Creates an audio track | id: string, source: AudioSource | AudioTrack |
| createVideoSource | Creates a video source | constraints?: MediaTrackConstraints, isScreencast?: boolean | VideoSource |
| createVideoTrack | Creates a video track | id: string, source: VideoSource | VideoTrack |
| startAecDump | Starts recording an AEC debug dump | fd: number, max_size_bytes: number | boolean |
| stopAecDump | Stops recording the AEC debug dump | - | void |

#### PeerConnectionFactoryOptions

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| adm | AudioDeviceModule | No | Custom audio device module |
| videoEncoderFactory | VideoEncoderFactory | No | Custom video encoder factory |
| videoDecoderFactory | VideoDecoderFactory | No | Custom video decoder factory |
| audioProcessing | AudioProcessing | No | Custom audio processing instance |

### RTCPeerConnection

| Name | Description | Parameters | Return Value |
|------|-------------|------------|--------------|
| new | Creates a connection instance | configuration?: RTCConfiguration | RTCPeerConnection |
| createOffer | Creates an SDP Offer | options?: RTCOfferOptions | Promise\<RTCSessionDescriptionInit\> |
| createAnswer | Creates an SDP Answer | options?: RTCAnswerOptions | Promise\<RTCSessionDescriptionInit\> |
| setLocalDescription | Sets the local SDP description | description?: RTCSessionDescriptionInit | Promise\<void\> |
| setRemoteDescription | Sets the remote SDP description | description: RTCSessionDescriptionInit | Promise\<void\> |
| addIceCandidate | Adds an ICE candidate | candidate?: RTCIceCandidateInit | Promise\<void\> |
| addTrack | Adds a media track | track: MediaStreamTrack, ...streams: MediaStream[] | RTCRtpSender |
| removeTrack | Removes a media track | sender: RTCRtpSender | void |
| addTransceiver | Adds a transceiver | trackOrKind: MediaStreamTrack \| string, init?: RTCRtpTransceiverInit | RTCRtpTransceiver |
| createDataChannel | Creates a data channel | label: string, dataChannelDict?: RTCDataChannelInit | RTCDataChannel |
| getSenders | Returns all senders | - | RTCRtpSender[] |
| getReceivers | Returns all receivers | - | RTCRtpReceiver[] |
| getTransceivers | Returns all transceivers | - | RTCRtpTransceiver[] |
| getConfiguration | Returns the current connection configuration | - | RTCConfiguration |
| setConfiguration | Updates the connection configuration | configuration?: RTCConfiguration | void |
| restartIce | Restarts ICE negotiation | - | void |
| getStats | Returns call statistics | selector?: MediaStreamTrack | Promise\<RTCStatsReport\> |
| setAudioRecording | Enables/disables audio capture | recording: boolean | void |
| setAudioPlayout | Enables/disables audio playback | playout: boolean | void |
| close | Closes the connection and releases resources | - | void |

### MediaDevices

| Name | Description | Parameters | Return Value |
|------|-------------|------------|--------------|
| getUserMedia | Obtains a camera/microphone media stream | constraints?: MediaStreamConstraints | Promise\<MediaStream\> |
| getDisplayMedia | Obtains a screen capture media stream | options?: DisplayMediaStreamOptions | Promise\<MediaStream\> |
| enumerateDevices | Enumerates available media devices | - | Promise\<MediaDeviceInfo[]\> |
| getSupportedConstraints | Returns supported media constraints | - | MediaTrackSupportedConstraints |

### VideoRenderController

`VideoRenderController` extends `XComponentController` and wraps `NativeVideoRenderer` for rendering video with the XComponent.

| Name | Description | Parameters | Return Value |
|------|-------------|------------|--------------|
| setVideoTrack | Sets the video track to render | track: MediaStreamTrack \| null | void |
| setMirror | Sets horizontal mirror flip | mirror: boolean | void |
| setMirrorVertically | Sets vertical mirror flip | mirrorVertically: boolean | void |
| setScalingMode | Sets the scaling mode | mode: ScalingMode | void |

`ScalingMode` enum: `Fill` (0), `AspectFill` (1), `AspectFit` (2).

### AudioDeviceModule

| Name | Description | Parameters | Return Value |
|------|-------------|------------|--------------|
| new | Constructs an audio device module | options?: AudioDeviceModuleOptions | AudioDeviceModule |
| isBuiltInAcousticEchoCancelerSupported | Queries hardware AEC support (static) | - | boolean |
| isBuiltInNoiseSuppressorSupported | Queries hardware noise suppressor support (static) | - | boolean |
| setSpeakerMute | Mutes/unmutes the speaker | mute: boolean | void |
| setMicrophoneMute | Mutes/unmutes the microphone | mute: boolean | void |
| setNoiseSuppressorEnabled | Enables/disables noise suppression | enabled: boolean | boolean |
| release | Releases resources | - | void |

#### AudioDeviceModuleOptions

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| audioSource | number | No | SOURCE_TYPE_VOICE_COMMUNICATION | Input audio source type. See ohos.multimedia.audio.SourceType |
| audioFormat | number | No | SAMPLE_FORMAT_S16LE | Sample format. See ohos.multimedia.audio.AudioSampleFormat |
| inputSampleRate | number | No | 48000 | Input sample rate (Hz) |
| useStereoInput | boolean | No | false | Whether to use stereo input; defaults to mono |
| outputSampleRate | number | No | 48000 | Output sample rate (Hz) |
| useStereoOutput | boolean | No | false | Whether to use stereo output; defaults to mono |
| rendererUsage | number | No | STREAM_USAGE_VOICE_COMMUNICATION | Output audio usage. See ohos.multimedia.audio.StreamUsage |
| useLowLatency | boolean | No | false | Whether to enable low-latency capture and playback |
| useHardwareAcousticEchoCanceler | boolean | No | false | Whether to use hardware AEC (call isBuiltInAcousticEchoCancelerSupported() first) |
| useHardwareNoiseSuppressor | boolean | No | false | Whether to use hardware noise suppressor (call isBuiltInNoiseSuppressorSupported() first) |

## Code Obfuscation

For details on code obfuscation, see [Code Obfuscation](https://developer.huawei.com/consumer/cn/doc/harmonyos-guides/source-obfuscation).

To prevent `@ohos/webrtc` from being obfuscated, add the following exclusion rule to `obfuscation-rules.txt`:

```
-keep
./oh_modules/@ohos/webrtc
```

## Directory Structure

```shell
sdk/ohos/
├── BUILD.gn            # Build script for src/
├── api                 # Interface files
│   ├── ets
│   │   ├── log
│   │   │   └── Logging.ets             # Logging utility
│   │   └── xcomponent
│   │       └── VideoRenderController.ets # Video render controller (wraps NativeVideoRenderer)
│   ├── libohos_webrtc
│   │   ├── index.d.ts                  # Entry point (re-exports webrtc.d.ts)
│   │   └── webrtc.d.ts                 # Full type definitions
│   └── libwebrtc.md                    # API documentation
├── har_hap             # DevEco Studio demo project (HAR module + demo HAP module)
└── src
    ├── node-addon-api  # Third-party library for NAPI development
    └── ohos_webrtc     # C++ implementation
        ├── async_work      # Async task helpers
        ├── audio_device    # Audio module
        ├── camera          # Camera module
        ├── desktop_capture # Desktop/screen capture
        ├── helper          # NDK interface helpers
        ├── logging         # Logging adapter
        ├── render          # Video rendering module
        ├── user_media      # Media constraints helpers
        ├── utils           # Utilities
        ├── video           # Video module
        ├── video_codec     # Video codec module
        ├── peer_connection.cpp  # RTCPeerConnection implementation
        └── peer_connection.h    # RTCPeerConnection header
```

## Contributing

Feel free to open an [Issue](https://gitcode.com/openharmony-sig/ohos_webrtc/issues) if you encounter any problems, or submit a [PR](https://gitcode.com/openharmony-sig/ohos_webrtc/pulls) to contribute.

## License

This project is licensed under the [BSD License](../../LICENSE).
