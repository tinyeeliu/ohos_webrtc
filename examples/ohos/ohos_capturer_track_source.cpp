//
// Created on 2024/3/6.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "ohos_capturer_track_source.h"
#include "api/make_ref_counted.h"

namespace webrtc {
namespace ohos {

rtc::scoped_refptr<CapturerTrackSource> CapturerTrackSource::Create() {
  std::unique_ptr<OhosCameraCapture> capturer(OhosCameraCapture::Create());
  return rtc::make_ref_counted<CapturerTrackSource>(std::move(capturer));
}

CapturerTrackSource::~CapturerTrackSource() {}

rtc::VideoSourceInterface<webrtc::VideoFrame> *CapturerTrackSource::source() { 
  return capturer_.get();
}
} // namespace ohos
} // namespace webrtc
