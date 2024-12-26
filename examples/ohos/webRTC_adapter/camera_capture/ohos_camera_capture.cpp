/*
# Copyright (c) 2024 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/
#include "modules/video_capture/video_capture_factory.h"
#include "api/video/i420_buffer.h"
#include "hilog/log.h"
#include "ohos_camera_capture.h"

namespace webrtc {
namespace ohos {

OhosCameraCapture::OhosCameraCapture() {}

bool OhosCameraCapture::Init(OhosCamera::CaptureType captureType)
{
    ohosCamera_ = std::make_shared<OhosCamera>(captureType);
    ImageReceiverOnManager::GetInstance().AddImageReceiverOn(ohosCamera_->GetImageReceiverID(), ohosCamera_);
    ohosCamera_->RegisterCaptureDataCallback(this);
    ohosCamera_->StartCamera();

    return true;
}

OhosCameraCapture *OhosCameraCapture::Create(OhosCamera::CaptureType captureType)
{
    std::unique_ptr<OhosCameraCapture> vcm_capturer = std::make_unique<OhosCameraCapture>();
    if (!vcm_capturer->Init(captureType)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, "OhosCameraCapture", "Init Failed");
        return nullptr;
    }
    return vcm_capturer.release();
}

void OhosCameraCapture::Destroy()
{
    ohosCamera_->StopCamera();
    ohosCamera_->UnregisterCaptureDataCallback();
    ImageReceiverOnManager::GetInstance().DelImageReceiverOn(ohosCamera_->GetImageReceiverID());
    ohosCamera_ = nullptr;
}

VideoFrame OhosCameraCapture::MaybePreprocess(const VideoFrame &frame)
{
    MutexLock lock(&lock_);
    if (preprocessor_ != nullptr) {
        return preprocessor_->Preprocess(frame);
    } else {
        return frame;
    }
}

void OhosCameraCapture::UpdateVideoAdapter()
{
    video_adapter_.OnSinkWants(broadcaster_.wants());
}

void OhosCameraCapture::AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame> *sink, const rtc::VideoSinkWants &wants)
{
    broadcaster_.AddOrUpdateSink(sink, wants);
    UpdateVideoAdapter();
}

void OhosCameraCapture::RemoveSink(rtc::VideoSinkInterface<VideoFrame> *sink)
{
    broadcaster_.RemoveSink(sink);
    UpdateVideoAdapter();
}

void OhosCameraCapture::OnFrame(const VideoFrame &original_frame)
{
    int cropped_width = 0;
    int cropped_height = 0;
    int out_width = 0;
    int out_height = 0;
    VideoFrame frame = MaybePreprocess(original_frame);

    bool enable_adaptation;
    {
        MutexLock lock(&lock_);
        enable_adaptation = enable_adaptation_;
    }
    if (!enable_adaptation) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OhosCameraCapture", "enable_adaptation");
        broadcaster_.OnFrame(frame);
        return;
    }
    if (!video_adapter_.AdaptFrameResolution(frame.width(), frame.height(), frame.timestamp_us() * 1000,
                                             &cropped_width, &cropped_height, &out_width, &out_height)) {
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
        broadcaster_.OnFrame(new_frame_builder.build());
    } else {
        // No adaptations needed, just return the frame as is.
        broadcaster_.OnFrame(frame);
    }
}

OhosCameraCapture::~OhosCameraCapture()
{
    Destroy();
}
} // namespace ohos
} // namespace webrtc