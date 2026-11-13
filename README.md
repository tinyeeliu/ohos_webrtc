# HarmonyOS WebRTC fork

This repository is a fork of
[CPF-ApplicationTPC/ohos_webrtc](https://gitcode.com/CPF-ApplicationTPC/ohos_webrtc),
imported from GitCode with its upstream commit history and tags preserved.
Our GitHub copy is [tinyeeliu/ohos_webrtc](https://github.com/tinyeeliu/ohos_webrtc).
The original project documentation follows; upstream licenses and attribution
remain in place.

---

**WebRTC is a free, open software project** that provides browsers and mobile
applications with Real-Time Communications (RTC) capabilities via simple APIs.
The WebRTC components have been optimized to best serve this purpose.

**Our mission:** To enable rich, high-quality RTC applications to be
developed for the browser, mobile platforms, and IoT devices, and allow them
all to communicate via a common set of protocols.

The WebRTC initiative is a project supported by Google, Mozilla and Opera,
amongst others.

### Development

See [here][native-dev] for instructions on how to get started
developing with the native code.

[Authoritative list](native-api.md) of directories that contain the
native API header files.

### Build With OHOS SDK

1. How to build WebRTC with OHOS SDK followed [this article](./docs/ohos/webrtc_adapter_ohos.md).
2. OHOS Audio adapter code: [ohos audio](./modules/audio_device/ohos).
3. How to use OHOS Video for webrtc followed [this project](https://gitee.com/han_jin_fei/oh_web-rtc).
4. ArkTS SDK integration and multi-device camera preview: [English](./sdk/ohos/README.md), [中文](./sdk/ohos/README_zh.md). See the [demo guide](./sdk/ohos/har_hap/README.md) and [changelog](./sdk/ohos/CHANGELOG.md) for the current local changes.

### More info

 * Official web site: http://www.webrtc.org.
 * Master source code repo: https://webrtc.googlesource.com/src.
 * Samples and reference apps: https://github.com/webrtc.
 * Mailing list: http://groups.google.com/group/discuss-webrtc.
 * Continuous build: https://ci.chromium.org/p/webrtc/g/ci/console.
 * [Coding style guide](g3doc/style-guide.md).
 * [Code of conduct](CODE_OF_CONDUCT.md).
 * [Reporting bugs](docs/bug-reporting.md).
 * [Documentation](g3doc/sitemap.md).

[native-dev]: https://webrtc.googlesource.com/src/+/main/docs/native-code/
