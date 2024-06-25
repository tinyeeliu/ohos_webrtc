//
// Created on 2024/3/6.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "ohos_desktop_track_source.h"
#include "api/make_ref_counted.h"
#include "ohos_desktop_capture.h"

namespace webrtc {
namespace ohos {

rtc::scoped_refptr<DesktopTrackSource> DesktopTrackSource::Create() {
    std::unique_ptr<OhosDesktopCapture> capturer(OhosDesktopCapture::Create());
    return rtc::make_ref_counted<DesktopTrackSource>(std::move(capturer));
}

DesktopTrackSource::~DesktopTrackSource() {
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "DesktopTrackSource", "~DesktopTrackSource");
}
  
rtc::VideoSourceInterface<webrtc::VideoFrame> *DesktopTrackSource::source() { 
    return capturer_.get();
}
} // namespace ohos
} // namespace webrtc
