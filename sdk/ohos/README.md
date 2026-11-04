# <center>ohos_webrtc</center>

This project is based on [WebRTC](https://webrtc.googlesource.com/src).

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

OpenHarmony API version >= 15.

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

### Audio 3A and Track Events

- Audio 3A: the `echoCancellation`, `autoGainControl` and `noiseSuppression` constraints of `createAudioSource` (including explicit `false`) are passed to the WebRTC software audio processing (APM) configuration and are never silently ignored. Built-in system 3A switches are not currently bridged (`isBuiltInAcousticEchoCancelerSupported`/`isBuiltInNoiseSuppressorSupported` return false). Capture/playback use the voice-communication audio scene by default; the system may apply scene-fixed processing, which must not be mistaken for the effect of the software constraints above.
- Track events: `MediaStreamTrack` supports the `onended`/`onmute`/`onunmute` event properties and the readonly `muted` state. A user-initiated `stop()` does not fire `onended`; `enabled` toggles and silence frames do not fire mute/unmute. mute/unmute are driven by real system interruption signals: local audio tracks fire mute when the system forces an interruption (e.g. an incoming call) and unmute on resume. Local video and remote tracks have no reliable signal yet — `muted` stays false and no mute/unmute is dispatched (known difference).

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

## Multi-device Camera Preview

The current local changes add `CameraPreviewSession` to manage camera selection, foreground/background recovery, bounded retries, and host-window orientation for phones, foldable/trifold devices, and multiple displays. See the changelog for release status.

### Integration and Lifecycle

Grant camera permission first and attach the same `VideoRenderController` to your XComponent. The host supplies `context`, `factory`, and `hostWindow` below:

```typescript
import { CameraPreviewSession, VideoRenderController } from '@ohos/webrtc';

const controller = new VideoRenderController();
const session = new CameraPreviewSession(context, factory, controller, {
  front: true,
  allowFallback: true,
  autoRecover: true,
  maxRetries: 3
});
session.attachWindow(hostWindow);
session.setActive(true);

// Page hidden: session.setActive(false)
// Page visible again: session.setActive(true)
// Prefer the rear camera: session.setCameraPreference(false)
// Retry explicitly: session.retry()
// Page destroyed: session.release()
```

- Defaults are a 1280×720 capture request, rear camera, and AspectFit. Size is a capture request; use `controller.onFrameSizeChanged` for the actual oriented frame dimensions when laying out the preview.
- A nonempty `deviceId` is strict: a missing device reports an error. Without an ID, selection follows the front/rear preference; `allowFallback` permits falling back to the first currently available camera.
- The library observes window visibility, display changes, and camera/fold events. The page still owns permissions, layout, status messages, and its capture activation request through `setActive`.
- Recovery or camera switching can replace the track. For calls, update the sender binding through `onTrackChanged`, including the case where the track is `undefined`.
- `frameStallTimeoutMs` defaults to 0 (disabled). Set 1000–60000 milliseconds to detect stalled capture through frame counts and trigger bounded recovery. It does not detect static image content or downstream rendering stalls.

### Matching Native Library

| Capability version | Session behavior |
| --- | --- |
| Missing `NativeVideoRenderer.cameraPreviewApiVersion`, or below 1 | Reports `incompatible`; does not create a video source or retry automatically |
| 1 | Enhanced preview, host-window rotation, and Surface sizing; keep frame-stall monitoring disabled |
| 2 or later | Adds `VideoSource.getCaptureFrameCount()` for explicitly enabled frame-stall monitoring |

The current Native source declares version 2. To use frame-stall monitoring, rebuild the entire Native library from the current source and replace `libohos_webrtc.so` for the target ABI. Updating ArkTS files alone does not provide this capability. Explicitly requesting monitoring without the frame-count method reports `incompatible`.

The low-level `VideoSource` API remains available. `applyDisplayRotation(0/90/180/270)` enables host-supplied rotation; `-1` restores automatic default-display rotation. `CameraPreviewSession` synchronizes the angle. The controller handles Surface sizing and aspect-preserving rendering, while Native probes installation-angle support and handles HAL rotation fallback.

### Demo and Validation

Open “多设备相机实验室（A–F）” from the demo home page to inspect responsive layout, camera devices, preview ratios, preview orientation, hover layout, and local/remote video. See the [demo guide](./har_hap/README.md#多设备相机实验室af) for steps and test commands.

Preview snapshots check composited orientation and cropping; local loopback checks the encode/decode path, and section F supports two-peer video checks. Validate all four holding orientations, front/rear switching, folding/unfolding, foreground/background recovery, display migration, and capture-stall recovery on target devices. These changes do not include an image or barcode recognition engine.

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
│   │       ├── CameraPreviewSession.ets  # Multi-device preview and recovery
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
