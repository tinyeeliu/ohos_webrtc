//
// Created on 2024/3/6.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef OH_WEB_RTC_CAPTURER_TRACK_SOURCE_H
#define OH_WEB_RTC_CAPTURER_TRACK_SOURCE_H

#include "ohos_camera_capture.h"
#include <absl/memory/memory.h>
#include "pc/video_track_source.h"
#include "rtc_base/ref_count.h"
#include "ohos_camera.h"
#include "napi/native_api.h"
#include <api/make_ref_counted.h>

namespace webrtc {
namespace ohos {
class CapturerTrackSource : public webrtc::VideoTrackSource {
public:
  static rtc::scoped_refptr<CapturerTrackSource> Create();
  virtual ~CapturerTrackSource();

protected:
  explicit CapturerTrackSource(std::unique_ptr<webrtc::ohos::OhosCameraCapture> capturer)
      : webrtc::VideoTrackSource(false), capturer_(std::move(capturer)) {}


private:
  rtc::VideoSourceInterface<webrtc::VideoFrame> *source() override;
  std::unique_ptr<OhosCameraCapture> capturer_;
};
} // namespace ohos
} // namespace webrtc

#endif // OH_WEB_RTC_CAPTURER_TRACK_SOURCE_H
