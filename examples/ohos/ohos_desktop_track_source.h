//
// Created on 2024/3/6.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef OH_WEB_RTC_DESKTOP_TRACK_SOURCE_H
#define OH_WEB_RTC_DESKTOP_TRACK_SOURCE_H

#include "ohos_desktop_capture.h"
#include <absl/memory/memory.h>
#include "pc/video_track_source.h"
#include <api/make_ref_counted.h>

namespace webrtc {
namespace ohos {
class DesktopTrackSource : public webrtc::VideoTrackSource {
public:
    static rtc::scoped_refptr<DesktopTrackSource> Create();
    virtual ~DesktopTrackSource();

protected:
    explicit DesktopTrackSource(std::unique_ptr<webrtc::ohos::OhosDesktopCapture> capturer)
      : webrtc::VideoTrackSource(false), capturer_(std::move(capturer)) {}


private:
    rtc::VideoSourceInterface<webrtc::VideoFrame> *source() override;
    std::unique_ptr<OhosDesktopCapture> capturer_;
};
} // namespace ohos
} // namespace webrtc

#endif // OH_WEB_RTC_DESKTOP_TRACK_SOURCE_H
