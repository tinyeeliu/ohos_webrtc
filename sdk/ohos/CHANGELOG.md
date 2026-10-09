# Changelog

## 1.0.4 (2026-10-09)

Current local changes for multi-device camera adaptation; no package release is assigned yet.

### Added

- Export `CameraPreviewSession` and its options/status types for camera selection, strict device IDs, front/rear preference fallback, foreground/background recovery, bounded retries, and track replacement notifications.
- Synchronize capture with the host window's display and observe display migration, camera availability, and fold events with runtime capability checks.
- Add optional `VideoSource.applyDisplayRotation()` and `getCaptureFrameCount()`, `NativeVideoRenderer.setSurfaceSize()`, and `FrameResolutionChangedEvent`. Native `cameraPreviewApiVersion` is now 2; version 1 supports preview without frame-count monitoring.
- Add opt-in capture-stall monitoring (`frameStallTimeoutMs`, disabled by default), using captured-frame counts and the session's bounded recovery policy.
- Add controller preview sizing and oriented frame-size notifications, plus the A–F multi-device camera lab: responsive layout, camera selection, aspect-ratio cropping, preview snapshots and local encode/decode loopback, crease-aware hover layout, and two-peer video calls.
- Add host-side session, layout, loopback, peer-call, and Native rotation/viewport regression tests, plus string-constraint cases in the device test suite.
- PR:[#145](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/145).

### Changed / Fixed

- Serialize camera rotation changes and preserve host-supplied orientation against default-display updates. Probe physical installation-angle support at runtime and handle HAL rotation failures with software fallback or session recovery when reset fails.
- Adapt GL and raster preview rendering to Surface dimensions, oriented frames, scaling, and mirroring while retaining legacy behavior for callers that do not opt into enhanced preview.
- Isolate stale capture/renderer callbacks and cancel recovery work when capture is deactivated or released.
- Correct string `ideal`/`exact` constraint parsing and reject non-string sequence elements and nested constraint dictionaries.
- Update English/Chinese SDK and demo documentation for integration, capability versions, A–F validation, and build requirements.

### Compatibility and Validation

- New sessions require a matching Native library with preview capability version 1 or later. Explicit frame-stall monitoring also requires `getCaptureFrameCount()`; the demo enables its 8-second sampling interval only with version 2 or later.
- Rebuild the complete Native library and replace the target ABI binary to use the current Native additions. Existing low-level API signatures remain available, and new optional declarations allow legacy implementations to remain valid.
- Automated tests and local loopback do not establish cross-device HAL, orientation, or recovery correctness. Preview snapshots capture the composited view; no raw-frame export or recognition engine is added. Two-peer demo calls use STUN without TURN, so restricted networks may prevent connection.

## 1.0.4 (2026-07-29)

- Support camera zoom via applyConstraints({ zoom }) and getZoomRange() [#121](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/121)
- Add camera flip/mirror interface and fix libohos_webrtc.so multiple loading resource contention [#128](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/128)
- Fix NapiMediaStreamTrack GC causing camera switch failure [#129](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/129)
- Fix local preview double mirror and GetVideoSource crash [#130](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/130)
- Migrate ohosTest module from HAR to entry module and fix import references [#131](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/131)
- Add video capture test coverage [#134](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/134)
- Fix AddSink failure when setVideoTrack is called before surface creation, and correctly re-attach sink on surface rebuild [#135](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/135)
- Fix screen capture instability [#136](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/136)
- Fix CFI detection issue during screen recording [#137](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/137)
- Remove video capture demo [#138](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/138)
- Remove version recommendation and upgrade notice from README [#139](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/139)

## 1.0.4-rc.4 (2026-07-24)

- Add applyConstraints XTS [#127](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/127)

## 1.0.4-rc.1 (2026-07-20)

- Support MediaStreamTrack.applyConstraints [#119](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/119)
- Fix crash and memory leak caused by connection [#125](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/125)
- Fix event-loop exception isolation, SDP/ICE parameter type validation, and audio device stability issues (incl. use-after-free on ADM teardown and double-free on audio terminate) [#102](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/102)
- Update the WebRTC build guide documentation (docs/ohos/webrtc_build.md) [#113](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/113)
- Add MediaDevices test coverage and switch loopback suites to host-only to fix test timeouts [#116](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/116)
- Add v1.0.2 upgrade notice and version recommendation table to sdk/ohos README [#117](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/117)
- Fix audio builder memory leaks and add null checks for NAPI Unwrap [#120](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/120)
- Add missing interface test coverage (VideoEncoderFactory, VideoRenderController), fix existing test races, and remove the non-existent getDefault declaration from webrtc.d.ts [#122](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/122)

## 1.0.2 (2026-06-11)
- Support real-time dynamic changes of the local resolution [#94](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/94)
- Implement support for the audio track pause and volume adjustment interface [#104](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/104)

## 1.0.2-rc3 (2026-05-31)
- Implement support for the audio track pause and volume adjustment interface [#104](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/104)

## 1.0.2-rc2 (2026-05-26)
- Support real-time dynamic changes of the local resolution [#94](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/94)
- Improve README documentation links and API documentation [#101](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/101)

## 1.0.2-rc1 (2026-05-11)
- Fix memory leak in hardware video encoder [#95](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/95)
- Fix multiple code quality issues in sdk/ohos webrtc and update OAT.xml [#96](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/96)
- Add resolution support documentation in libwebrtc API docs [#97](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/97)
- Fix camera capturer crash on API level 7.0.0 [#99](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/99)

## 1.0.1 (2026-04-21)
- Fix memory leak issues [#83](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/83)
- Fix crash issues [#85](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/85)
- Update documentation [#86](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/86)
- Add custom resolution support in demo [#91](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/91)
- Add abnormal resolution options in demo [#90](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/90)
- Ensure thread safety for WebRTC signaling thread API calls [#84](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/84)
- Add release() method declaration for MediaSource and AudioDeviceModule interfaces [#87](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/87)
- Fix DrawTexture incorrectly passing frame.width() as the frameHeight parameter [#88](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/88)
- Optimize sdk/ohos/README.md documentation [#89](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/89)

## 1.0.1-rc2 (2026-01-22)
- Fix incorrect rotation angle for rear camera [#80](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/80)
- Implement variable resolution support [#79](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/79)

## 1.0.1-rc1 (2026-01-15)
- Implement camera view rotation following application orientation [#76](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc/pull/76)

## 1.0.0 (2025-01-08)
- Fix DataChannel destructor issue

## 1.0.0-rc3 (2025-08-20)
- Support enabling/disabling audio track recording and playback

## 1.0.0-rc2 (2025-03-27)
- Support system audio sharing

## 1.0.0-rc1 (2025-02-22)
- Publish to ohpm

## 1.0.0-rc0 (2025-02-14)
### WebRTC adaptation for OpenHarmony, including:
- ArkTS API bindings
- OpenHarmony camera capture
- OpenHarmony screen capture
- OpenHarmony audio capture
