# <center>webrtc</center>

This project is based on [WebRTC](https://webrtc.googlesource.com/src).

## Introduction

webrtc is a real-time communication library for OpenHarmony, providing ArkTS interface bindings for WebRTC, with video capture, rendering, and codec modules adapted at the C++ layer. The interface design follows the [WebRTC specification](https://www.w3.org/TR/webrtc/), with additional interfaces such as PeerConnectionFactory and AudioDeviceModule provided based on practical needs.

**Figure 1** SDK Architecture

![architecture](./img/webrtc-arch.png)

## Installation

```bash
ohpm install @ohos/webrtc
```

For more information on configuring the OpenHarmony ohpm environment, see [How to Install OpenHarmony ohpm Packages](https://gitcode.com/openharmony-tpc/docs/blob/master/OpenHarmony_har_usage.md).

To integrate `libwebrtc.a` directly in an NDK C++ project, see [Integrating the Binary in a Native App](../../docs/ohos/webrtc_adapter_app.md).

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
let peerConnectionFactory: webrtc.PeerConnectionFactory = new webrtc.PeerConnectionFactory();

// Create a local audio source and audio track
let audioSource: webrtc.AudioSource = peerConnectionFactory.createAudioSource();
let audioTrack: webrtc.AudioTrack = peerConnectionFactory.createAudioTrack('audio', audioSource);

// Create a local video source and video track (resolution 640x480, frame rate 15~30fps)
let videoSource: webrtc.VideoSource = peerConnectionFactory.createVideoSource({
  width: 640,
  height: 480,
  frameRate: { min: 15, max: 30 }
});
let videoTrack: webrtc.VideoTrack = peerConnectionFactory.createVideoTrack('video', videoSource);

// Create an RTCPeerConnection
let peerConnection: webrtc.RTCPeerConnection = peerConnectionFactory.createPeerConnection({
  iceServers: [{ urls: 'stun:stun.example.com' }]
});

// Add media tracks to the connection
peerConnection.addTrack(audioTrack);
peerConnection.addTrack(videoTrack);

// Listen for ICE candidates
peerConnection.onicecandidate = (event: webrtc.RTCPeerConnectionIceEvent) => {
  if (event.candidate) {
    // Send the candidate to the remote peer via the signaling channel
  }
};

// Create and set the local Offer description
let offer: webrtc.RTCSessionDescriptionInit = await peerConnection.createOffer();
await peerConnection.setLocalDescription(offer);
// Send the SDP offer to the remote peer via the signaling channel
```

For more examples, see the demo project in the `har_hap` directory. For instructions on compiling the dynamic library, see the [Build Documentation](../../docs/ohos/webrtc_build.md). For common issues during compilation, see the [Build FAQ](../../docs/ohos/webrtc_adapter_questions.md).

## Usage Guide

### 1. Creating a PeerConnectionFactory

PeerConnectionFactory is the entry point for using WebRTC, used to create audio/video sources, tracks, and RTCPeerConnection instances.

```typescript
import webrtc from '@ohos/webrtc';

// Create with default configuration
let factory: webrtc.PeerConnectionFactory = new webrtc.PeerConnectionFactory();

// Create with custom configuration
let factoryWithOptions: webrtc.PeerConnectionFactory = new webrtc.PeerConnectionFactory({
  videoEncoderFactory: new webrtc.HardwareVideoEncoderFactory(true),
  videoDecoderFactory: new webrtc.HardwareVideoDecoderFactory()
});
```

> It is recommended to create a PeerConnectionFactory instance at app startup and call `PeerConnectionFactory.setDefault()` to set it as the default, so it can be retrieved by other modules (such as MediaDevices) at the C++ layer.

### 2. Creating Audio/Video Sources and Tracks

```typescript
// Create audio source and track
let audioSource: webrtc.AudioSource = factory.createAudioSource();
let audioTrack: webrtc.AudioTrack = factory.createAudioTrack('audioTrackId', audioSource);

// Create video source with resolution and frame rate constraints
let videoSource: webrtc.VideoSource = factory.createVideoSource({
  width: 640,
  height: 480,
  frameRate: { min: 15, max: 30 }
});
let videoTrack: webrtc.VideoTrack = factory.createVideoTrack('videoTrackId', videoSource);
```

### 3. Establishing a Peer-to-Peer Connection

```typescript
// Create a PeerConnection
let pc: webrtc.RTCPeerConnection = factory.createPeerConnection({
  iceServers: [{ urls: 'stun:stun.example.com' }]
});

// Add tracks
pc.addTrack(audioTrack);
pc.addTrack(videoTrack);

// Create an Offer and set it as the local description
let offer: webrtc.RTCSessionDescriptionInit = await pc.createOffer();
await pc.setLocalDescription(offer);

// Set the remote description upon receiving the remote Answer
await pc.setRemoteDescription(remoteAnswer);

// Add ICE candidates
await pc.addIceCandidate(remoteCandidate);
```

### 4. Using Data Channels

```typescript
// Create a data channel
let dataChannel: webrtc.RTCDataChannel = pc.createDataChannel('chat', {
  ordered: true
});

// Listen for messages
dataChannel.onmessage = (event: webrtc.MessageEvent) => {
  console.info('Message received: ' + event.data);
};

// Send a message
dataChannel.send('Hello WebRTC');
```

### 5. Capturing Media Devices

```typescript
let mediaDevices: webrtc.MediaDevices = new webrtc.MediaDevices();

// Get user media (camera/microphone)
let stream: webrtc.MediaStream = await mediaDevices.getUserMedia({
  video: true,
  audio: true
});

// Screen capture
let displayStream: webrtc.MediaStream = await mediaDevices.getDisplayMedia({
  video: {
    ohosScreenCaptureMode: 'home-screen'
  }
});
```

### 6. Video Rendering

```typescript
let renderer: webrtc.NativeVideoRenderer = new webrtc.NativeVideoRenderer();

// Initialize the renderer with the XComponent surfaceId
renderer.init(surfaceId);
renderer.setVideoTrack(videoTrack);
renderer.setMirror(true);
renderer.setScalingMode(1); // 0: fit, 1: fill
```

> Call `renderer.release()` after use to release rendering resources and avoid memory leaks.

### 7. Audio Device Management

```typescript
let audioModule: webrtc.AudioDeviceModule = new webrtc.AudioDeviceModule({
  audioSource: 1,          // SOURCE_TYPE_VOICE_COMMUNICATION
  inputSampleRate: 48000,
  outputSampleRate: 48000,
  useHardwareAcousticEchoCanceler: true,
  useHardwareNoiseSuppressor: true
});

// Control speaker and microphone
audioModule.setSpeakerMute(false);
audioModule.setMicrophoneMute(false);
```

> Call `release()` after using the AudioDeviceModule to release resources.

## API Reference

Full type definitions are in `api/libohos_webrtc/webrtc.d.ts`. For detailed documentation, see the [API Documentation](./api/libwebrtc.md).

### Core Interfaces

| Name | Description |
|------|-------------|
| PeerConnectionFactory | Creates and manages RTCPeerConnection, audio/video sources and tracks |
| RTCPeerConnection | Establishes and manages peer-to-peer connections |
| MediaDevices | Accesses user media devices and screen capture |
| MediaStream / MediaStreamTrack | Media stream and track abstractions |
| RTCDataChannel | Peer-to-peer data channel |
| AudioDeviceModule | Audio device management (speaker/microphone control) |
| NativeVideoRenderer | Video renderer for use with XComponent |
| HardwareVideoEncoderFactory / HardwareVideoDecoderFactory | Hardware video codec factories |
| NativeLogging | Logging utility |

## Code Obfuscation

For details on code obfuscation, see [Code Obfuscation](https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/arkts-utils/source-obfuscation-overview.md).

To prevent `@ohos/webrtc` from being obfuscated, add the following exclusion rule to `obfuscation-rules.txt`:

```
-keep
./oh_modules/@ohos/webrtc
```

## FAQ

**Q: NAPI-related errors during compilation?**

Ensure the NDK environment is correctly configured. See the [Build Documentation](../../docs/ohos/webrtc_build.md) before compiling, and refer to the [Build FAQ](../../docs/ohos/webrtc_adapter_questions.md) for common compilation issues.

**Q: Video rendering shows a black screen?**

Verify that the XComponent surfaceId has been obtained correctly and `renderer.init(surfaceId)` has been called. Also ensure that the `ohos.permission.CAMERA` permission is properly configured.

**Q: No audio data captured?**

Ensure the `ohos.permission.MICROPHONE` permission is properly configured, and check that the AudioDeviceModule audio source type and sample rate settings match the device.

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

Feel free to open an [Issue](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/issues) if you encounter any problems, or submit a [PR](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pulls) to contribute.

## License

This project is licensed under the [BSD License](../../LICENSE).
