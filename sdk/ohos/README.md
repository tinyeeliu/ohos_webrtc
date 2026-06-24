# <center>ohos_webrtc</center>

This project is based on [WebRTC](https://webrtc.googlesource.com/src).

## v1.0.2 Compatible Upgrade — Stability & Quality Improvements <sub>2026-06-11</sub>

We recommend upgrading to v1.0.2, which brings improvements in both stability and quality and resolves a number of historical issues.

> **Note:** Upgrading to v1.0.2 from v1.0.0 onwards is a compatible upgrade; no code changes are required.

This upgrade focuses on stability and quality improvements for the OpenHarmony platform.

### 🔧 Stability Improvements
- Fix memory leak in the hardware video encoder [#95](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/95), avoiding continuous memory growth during long call sessions
- Fix camera capturer crash on API level 7.0.0 [#99](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/99)

### ✨ Quality Improvements
- Fix multiple code quality issues in sdk/ohos webrtc and update OAT.xml [#96](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/96)

### 🔄 Compatibility Notes
- No Breaking Changes; API interfaces remain unchanged
- Only internal implementation optimizations; no changes required to existing code

> 📄 **For the full change history and issue fix details, see:** [CHANGELOG.md](./CHANGELOG.md)

## Introduction

This directory provides ArkTS interface bindings for WebRTC, and adapts the video capture, rendering, and codec modules at the C++ layer. Most interface designs follow the [WebRTC specification](https://www.w3.org/TR/webrtc/). Additional interfaces such as PeerConnectionFactory and AudioDeviceModule are also provided based on practical needs.

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
    "name": "ohos.permission.INTERNET",
    "reason": "$string:internet_reason",
    "usedScene": {
      "abilities": ["EntryAbility"],
      "when": "inuse"
    }
  },
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

For more examples, see the demo project in the `har_hap` directory. For instructions on compiling the dynamic library, see the [Build Documentation](../../docs/ohos/webrtc_build.md). For common issues during compilation, see the [Build FAQ](../../docs/ohos/webrtc_adapter_questions.md).

The implementation code is in the `src/ohos_webrtc` directory. NAPI interface bindings are built with the open-source project [node-addon-api](https://github.com/nodejs/node-addon-api).

## API Reference

Full type definitions are in `api/libohos_webrtc/webrtc.d.ts`. For detailed documentation, see the [API Documentation](./api/libwebrtc.md).

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

Feel free to open an [Issue](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/issues) if you encounter any problems, or submit a [PR](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pulls) to contribute.

## License

This project is licensed under the [BSD License](../../LICENSE).
