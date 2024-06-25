//
// Created on 2024/3/6.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef OH_WEB_RTC_OHOS_DESKTOP_CAPTURE_H
#define OH_WEB_RTC_OHOS_DESKTOP_CAPTURE_H

#include <memory>
#include <algorithm>
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "media/base/video_adapter.h"
#include "media/base/video_broadcaster.h"
#include "ohos_desktop.h"
#include "rtc_base/synchronization/mutex.h"
#include "api/video/video_sink_interface.h"
#include "api/scoped_refptr.h"
#include "modules/video_capture/video_capture.h"
#include "ohos_camera.h"

namespace webrtc {
namespace ohos {

class OhosDesktopCapture : public rtc::VideoSourceInterface<VideoFrame>,
                           public rtc::VideoSinkInterface<VideoFrame> {
public:
    OhosDesktopCapture();
    ~OhosDesktopCapture();
    class FramePreprocessor {
    public:
        virtual ~FramePreprocessor() = default;
        virtual VideoFrame Preprocess(const VideoFrame &frame) = 0;
    };
    static OhosDesktopCapture *Create();
    void AddOrUpdateSink(VideoSinkInterface<VideoFrame>* sink,const rtc::VideoSinkWants& wants) override;
    void RemoveSink(rtc::VideoSinkInterface<VideoFrame> *sink) override;
    void OnFrame(const VideoFrame &frame) override;
    void SetFramePreprocessor(std::unique_ptr<FramePreprocessor> preprocessor){};
private:
    bool Init();
    void Destroy();
    void UpdateVideoAdapter();
    VideoFrame MaybePreprocess(const VideoFrame &frame);

    Mutex lock_;
    OhosDesktop *desktop_ {nullptr};
    rtc::VideoBroadcaster broadcaster_;
    cricket::VideoAdapter video_adapter_;
    bool enable_adaptation_ RTC_GUARDED_BY(lock_) = false;
    std::unique_ptr<FramePreprocessor> preprocessor_ RTC_GUARDED_BY(lock_);
};
}
}


#endif //OH_WEB_RTC_OHOS_DESKTOP_CAPTURE_H
