//
// Created on 2024/3/6.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".
#include "ohos_desktop_capture.h"
#include "ohos_desktop.h"

#include "modules/video_capture/video_capture_factory.h"
#include "api/video/i420_buffer.h"
#include "hilog/log.h"

namespace webrtc {
namespace ohos {

OhosDesktopCapture::OhosDesktopCapture() {}

bool OhosDesktopCapture::Init() {
    desktop_ = new OhosDesktop();
    desktop_->Create();
    desktop_->RegisterCaptureDataCallback(this);
    desktop_->Config();
    desktop_->Start();
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktopCapture", "init Finish!!!!");
    return true;
}

OhosDesktopCapture* OhosDesktopCapture::Create() {
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktopCapture", "had been try create123");
    std::unique_ptr<OhosDesktopCapture> vcm_capturer(new OhosDesktopCapture());
    if (!vcm_capturer->Init()) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktopCapture", "Init Failed");
        return nullptr;
    }
    return vcm_capturer.release();
}

void OhosDesktopCapture::Destroy() {
    desktop_->Stop();
    desktop_->UnregisterCaptureDataCallback();
}

VideoFrame OhosDesktopCapture::MaybePreprocess(const VideoFrame &frame) {
    MutexLock lock(&lock_);
    if (preprocessor_ != nullptr) {
        return preprocessor_->Preprocess(frame);
    } else {
        return frame;
    }
}

void OhosDesktopCapture::UpdateVideoAdapter() { 
    video_adapter_.OnSinkWants(broadcaster_.wants()); 
}

void OhosDesktopCapture::AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame> *sink,
                                         const rtc::VideoSinkWants &wants) {
    broadcaster_.AddOrUpdateSink(sink, wants);
    UpdateVideoAdapter();
}

void OhosDesktopCapture::RemoveSink(rtc::VideoSinkInterface<VideoFrame> *sink) {
    broadcaster_.RemoveSink(sink);
    UpdateVideoAdapter();
}

void OhosDesktopCapture::OnFrame(const VideoFrame &original_frame) {
    int32_t cropped_width = 0;
    int32_t cropped_height = 0;
    int32_t out_width = 0;
    int32_t out_height = 0;
    VideoFrame frame = MaybePreprocess(original_frame);

    bool enable_adaptation;
    {
        MutexLock lock(&lock_);
        enable_adaptation = enable_adaptation_;
    }
    if (!enable_adaptation) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktopCapture", "OhosDesktopCapture::OnFrame 1");
        broadcaster_.OnFrame(frame);
        return;
    }
    if (!video_adapter_.AdaptFrameResolution(frame.width(), frame.height(), frame.timestamp_us() * 1000, &cropped_width,
                                             &cropped_height, &out_width, &out_height)) {
        return;
    }
    if (out_height != frame.height() || out_width != frame.width()) {
        rtc::scoped_refptr<I420Buffer> scaled_buffer = I420Buffer::Create(out_width, out_height);
        scaled_buffer->ScaleFrom(*frame.video_frame_buffer()->ToI420());
        VideoFrame::Builder new_frame_builder = VideoFrame::Builder()
                                                    .set_video_frame_buffer(scaled_buffer)
                                                    .set_rotation(kVideoRotation_0)
                                                    .set_timestamp_us(frame.timestamp_us())
                                                    .set_id(frame.id());
        if (frame.has_update_rect()) {
            VideoFrame::UpdateRect new_rect = frame.update_rect().ScaleWithFrame(
                frame.width(), frame.height(), 0, 0, frame.width(), frame.height(), out_width, out_height);
            new_frame_builder.set_update_rect(new_rect);
        }
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktopCapture", "OhosDesktopCapture::OnFrame 2");
        broadcaster_.OnFrame(new_frame_builder.build());

    } else {
        // No adaptations needed, just return the frame as is.
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktopCapture", "OhosDesktopCapture::OnFrame 3");
        broadcaster_.OnFrame(frame);
    }
}

OhosDesktopCapture::~OhosDesktopCapture() {
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosDesktopCapture", "OhosDesktopCapture::~OhosDesktopCapture");
    Destroy();
}
} // namespace ohos
} // namespace webrtc