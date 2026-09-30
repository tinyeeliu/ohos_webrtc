/**
 * Copyright (c) 2024 Archermind Technology (Nanjing) Co. Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "video_track_source.h"
#include "utils/marcos.h"
#include "video_frame_receiver_gl.h"
#if defined(__x86_64__)
#include "video_frame_receiver_native.h"
#endif

#include "api/video/i420_buffer.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

const int kRequiredResolutionAlignment = 2;

} // namespace

rtc::scoped_refptr<OhosVideoTrackSource> OhosVideoTrackSource::Create(
    std::unique_ptr<VideoCapturer> capturer, rtc::Thread* signalingThread, std::shared_ptr<EglContext> sharedContext)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!capturer) {
        RTC_LOG(LS_ERROR) << "The capturer is nullptr";
        return nullptr;
    }

    return rtc::make_ref_counted<OhosVideoTrackSource>(std::move(capturer), signalingThread, sharedContext);
}

OhosVideoTrackSource::OhosVideoTrackSource(
    std::unique_ptr<VideoCapturer> capturer, rtc::Thread* signalingThread, std::shared_ptr<EglContext> sharedContext)
    : thread_(rtc::Thread::Create()),
      signalingThread_(signalingThread),
      capturer_(std::move(capturer)),
      sharedContext_(sharedContext),
      videoAdapter_(kRequiredResolutionAlignment),
      state_(kInitializing)
{
    RTC_LOG(LS_INFO) << "OhosVideoTrackSource ctor: " << this;

    thread_->SetName("v-track-source", capturer_.get());
    thread_->Start();
    thread_->PostTask(
        [this, capturer = capturer_, sharedContext = sharedContext_] {
#if defined(__x86_64__)
            // x86_64 模拟器的 dgles 不支持外部纹理(EGLImage)零拷贝，
            // 只能走 CPU(ImageReceiver -> I420) 采集路径。
            (void)sharedContext;
            capturer->Init(VideoFrameReceiverNative::Create("v-frame-receiver"), this);
#else
            capturer->Init(
                VideoFrameReceiverGl::Create("v-frame-receiver", sharedContext), this);
#endif
        });
}

OhosVideoTrackSource::~OhosVideoTrackSource()
{
    RTC_LOG(LS_INFO) << __FUNCTION__;
    is_destroying_.store(true, std::memory_order_release);
    thread_->PostTask([this] {
        if (capturer_) {
            capturer_->Stop();
            capturer_->Release();
        }
        if (pendingSwitchCapturer_) {
            pendingSwitchCapturer_->Stop();
            pendingSwitchCapturer_->Release();
        }
    });
    thread_->Stop();
}

void OhosVideoTrackSource::Start()
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    thread_->PostTask([capturer = capturer_] { capturer->Start(); });
}

void OhosVideoTrackSource::Stop()
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    if (is_destroying_.load(std::memory_order_acquire)) {
        RTC_LOG(LS_WARNING) << "Stop called during destruction, ignoring";
        return;
    }

    // Align with OhosLocalAudioSource::Stop(): skip duplicate FireOnChanged and capturer stop.
    if (state_.exchange(kEnded) == kEnded) {
        return;
    }

    // FireOnChanged notifies VideoTrack/RtpSender on signaling thread (same as audio source).
    if (rtc::Thread::Current() == signalingThread_) {
        FireOnChanged();
    } else {
        signalingThread_->PostTask(
            [self = rtc::scoped_refptr<OhosVideoTrackSource>(this)] {
                if (!self->is_destroying_.load(std::memory_order_acquire)) {
                    self->FireOnChanged();
                }
            });
    }

    // Capturer runs on dedicated v-track-source thread; stop hardware capture there.
    thread_->PostTask([capturer = capturer_] { capturer->Stop(); });
}

void OhosVideoTrackSource::SetCapturerObserver(VideoCapturer::Observer* observer)
{
    UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
    capturerObserver_ = observer;
}

void OhosVideoTrackSource::SetState(SourceState state)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__ << " state: " << state;
    if (is_destroying_.load(std::memory_order_acquire)) {
        RTC_LOG(LS_WARNING) << "SetState called during destruction, ignoring";
        return;
    }

    if (state_.exchange(state) != state) {
        // Observers require signaling thread; PostTask if called from capturer/worker thread.
        if (rtc::Thread::Current() == signalingThread_) {
            FireOnChanged();
        } else {
            signalingThread_->PostTask(
                [self = rtc::scoped_refptr<OhosVideoTrackSource>(this)] {
                    if (!self->is_destroying_.load(std::memory_order_acquire)) {
                        self->FireOnChanged();
                    }
                });
        }
    }
}

void OhosVideoTrackSource::SetState(bool isLive)
{
    SetState(isLive ? kLive : kEnded);
}

bool OhosVideoTrackSource::ApplyRotation()
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    return broadcaster_.wants().rotation_applied;
}

bool OhosVideoTrackSource::AdaptFrame(
    int width, int height, int64_t time_us, int* out_width, int* out_height, int* crop_width, int* crop_height,
    int* crop_x, int* crop_y)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    {
        UNUSED MutexLock lock(&statsMutex_);
        stats_ = Stats{width, height};
    }

    if (!broadcaster_.frame_wanted()) {
        return false;
    }

    if (!videoAdapter_.AdaptFrameResolution(
        width, height, time_us * rtc::kNumNanosecsPerMicrosec, crop_width, crop_height, out_width, out_height)) {
        // VideoAdapter dropped the frame.
        return false;
    }

    constexpr int kCenterDivisor = 2;
    *crop_x = (width - *crop_width) / kCenterDivisor;
    *crop_y = (height - *crop_height) / kCenterDivisor;

    return true;
}

void OhosVideoTrackSource::AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink, const rtc::VideoSinkWants& wants)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    broadcaster_.AddOrUpdateSink(sink, wants);
    videoAdapter_.OnSinkWants(broadcaster_.wants());

    if (broadcaster_.frame_wanted()) {
        thread_->PostTask([capturer = capturer_] { capturer->Start(); });
    }
}

void OhosVideoTrackSource::RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    broadcaster_.RemoveSink(sink);
    videoAdapter_.OnSinkWants(broadcaster_.wants());

    if (!broadcaster_.frame_wanted()) {
        thread_->PostTask([capturer = capturer_] { capturer->Stop(); });
    }
}

MediaSourceInterface::SourceState OhosVideoTrackSource::state() const
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    return state_.load();
}

bool OhosVideoTrackSource::IsEnded() const
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    return state_.load() == MediaSourceInterface::kEnded;
}

bool OhosVideoTrackSource::remote() const
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    return false;
}

bool OhosVideoTrackSource::is_screencast() const
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    return capturer_ ? capturer_->IsScreencast() : false;
}

absl::optional<bool> OhosVideoTrackSource::needs_denoising() const
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    return false;
}

bool OhosVideoTrackSource::GetStats(Stats* stats)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    UNUSED MutexLock lock(&statsMutex_);

    if (!stats_) {
        return false;
    }

    *stats = *stats_;
    return true;
}

void OhosVideoTrackSource::ProcessConstraints(const VideoTrackSourceConstraints& constraints)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    broadcaster_.ProcessConstraints(constraints);
}

void OhosVideoTrackSource::OnOutputFormatRequest(int width, int height, int maxFps)
{
    if (width <= 0 || height <= 0) {
        RTC_LOG(LS_WARNING) << __FUNCTION__ << " invalid resolution: " << width << "x" << height;
        return;
    }

    RTC_LOG(LS_INFO) << __FUNCTION__ << " width=" << width << ", height=" << height << ", maxFps=" << maxFps;

    videoAdapter_.OnOutputFormatRequest(std::make_pair(width, height), width * height,
        maxFps > 0 ? absl::optional<int>(maxFps) : absl::optional<int>());
}

void OhosVideoTrackSource::ApplyConstraints(const VideoTrackSourceConstraints& constraints, int width, int height,
    int maxFps)
{
    thread_->PostTask([this, constraints, width, height, maxFps] {
        OnOutputFormatRequest(width, height, maxFps);
        ProcessConstraints(constraints);
    });
}

void OhosVideoTrackSource::SetZoomRatio(double ratio)
{
    RTC_LOG(LS_INFO) << __FUNCTION__ << ": ratio=" << ratio;
    thread_->PostTask([this, ratio] {
        if (is_destroying_.load(std::memory_order_acquire)) {
            RTC_LOG(LS_WARNING) << "SetZoomRatio: source destroying, skip";
            return;
        }
        if (pendingSwitchCapturer_) {
            // Camera switch in progress; defer zoom to OnCapturerStarted
            pendingZoomRatio_ = ratio;
            RTC_LOG(LS_INFO) << "SetZoomRatio: switch in progress, deferred ratio=" << ratio;
            return;
        }
        if (capturer_) {
            capturer_->SetZoomRatio(ratio);
        } else {
            RTC_LOG(LS_WARNING) << "SetZoomRatio: capturer_ is null, skip";
        }
    });
}

bool OhosVideoTrackSource::GetZoomRatioRange(float* minZoom, float* maxZoom)
{
    if (is_destroying_.load(std::memory_order_acquire)) {
        return false;
    }
    if (!capturer_) {
        return false;
    }
    return capturer_->GetZoomRatioRange(minZoom, maxZoom);
}

void OhosVideoTrackSource::SwitchCapturer(std::shared_ptr<VideoCapturer> capturer)
{
    RTC_LOG(LS_INFO) << __FUNCTION__;

    if (is_destroying_.load(std::memory_order_acquire)) {
        RTC_LOG(LS_WARNING) << "SwitchCapturer called during destruction, ignoring";
        return;
    }

    if (!capturer) {
        RTC_LOG(LS_ERROR) << "The capturer is nullptr";
        return;
    }

    thread_->PostTask([this, capturer] {
        RTC_LOG(LS_INFO) << "Switching capturer on thread";

        if (pendingSwitchCapturer_) {
            RTC_LOG(LS_WARNING) << "SwitchCapturer: cancelling previous pending switch";
            pendingSwitchCapturer_->Stop();
            pendingSwitchCapturer_->Release();
            pendingSwitchCapturer_.reset();
            pendingZoomRatio_ = 0.0;
        }

        // Stop and release old capturer first to avoid camera resource conflict.
        if (capturer_) {
            RTC_LOG(LS_INFO) << "Stopping old capturer before switching";
            capturer_->Stop();
            capturer_->Release();
        }

        // Store new capturer as pending; do NOT replace old capturer yet.
        // Replacement happens in OnCapturerStarted callback after confirming success.
        pendingSwitchCapturer_ = capturer;

        // Initialize and start new capturer.
        // OnCapturerStarted callback will handle success/failure.
#if defined(__x86_64__)
        // x86_64 模拟器的 dgles 不支持外部纹理(EGLImage)零拷贝，
        // 只能走 CPU(ImageReceiver -> I420) 采集路径。
        capturer->Init(VideoFrameReceiverNative::Create("v-frame-receiver"), this);
#else
        capturer->Init(
            VideoFrameReceiverGl::Create("v-frame-receiver", sharedContext_), this);
#endif
        capturer->Start();
    });
}

void OhosVideoTrackSource::OnCapturerStarted(bool success)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    // Handle SwitchCapturer pending capturer replacement.
    if (pendingSwitchCapturer_) {
        HandleSwitchCapturerResult(success);
    } else {
        // Normal startup (not from SwitchCapturer).
        SetState(success);
    }

    VideoCapturer::Observer* observer = nullptr;
    {
        UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
        observer = capturerObserver_;
    }

    if (observer) {
        observer->OnCapturerStarted(success);
    }
}

void OhosVideoTrackSource::HandleSwitchCapturerResult(bool success)
{
    if (!success) {
        RTC_LOG(LS_ERROR) << "New capturer failed to start, discarding and keeping old capturer";
        // Stop and release the failed new capturer.
        pendingSwitchCapturer_->Stop();
        pendingSwitchCapturer_->Release();
        pendingSwitchCapturer_.reset();
        pendingZoomRatio_ = 0.0;
        return;
    }

    RTC_LOG(LS_INFO) << "New capturer started successfully, replacing old capturer";
    // Stop and release old capturer.
    if (capturer_) {
        capturer_->Stop();
        capturer_->Release();
    }
    // Replace with new capturer.
    capturer_ = std::move(pendingSwitchCapturer_);
    // Apply deferred zoom if any (from SetZoomRatio during switch).
    if (pendingZoomRatio_ > 0.0) {
        capturer_->SetZoomRatio(pendingZoomRatio_);
        RTC_LOG(LS_INFO) << "Applied deferred zoom ratio=" << pendingZoomRatio_;
        pendingZoomRatio_ = 0.0;
    }
    // Only set state to live if we successfully replaced the capturer.
    SetState(true);
}

void OhosVideoTrackSource::OnCapturerStopped()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    VideoCapturer::Observer* observer = nullptr;
    {
        UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
        observer = capturerObserver_;
    }

    if (observer) {
        observer->OnCapturerStopped();
    }
}

void OhosVideoTrackSource::SwitchCamera()
{
    if (is_destroying_.load(std::memory_order_acquire)) {
        RTC_LOG(LS_WARNING) << "SwitchCamera: source destroying, skip";
        return;
    }
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;
    thread_->PostTask([capturer = capturer_] { capturer->SwitchCamera(); });
}

void OhosVideoTrackSource::SwitchCamera(const std::string& cameraId)
{
    if (is_destroying_.load(std::memory_order_acquire)) {
        RTC_LOG(LS_WARNING) << "SwitchCamera: source destroying, skip";
        return;
    }
    RTC_LOG(LS_VERBOSE) << __FUNCTION__ << ": cameraId=" << cameraId;
    thread_->PostTask([capturer = capturer_, cameraId] { capturer->SwitchCamera(cameraId); });
}

void OhosVideoTrackSource::ApplyDisplayRotation(int displayDegrees)
{
    if (is_destroying_.load(std::memory_order_acquire)) {
        RTC_LOG(LS_WARNING) << "ApplyDisplayRotation: source destroying, skip";
        return;
    }
    RTC_LOG(LS_VERBOSE) << __FUNCTION__ << ": displayDegrees=" << displayDegrees;
    thread_->PostTask([capturer = capturer_, displayDegrees] { capturer->ApplyDisplayRotation(displayDegrees); });
}

void OhosVideoTrackSource::OnCameraSwitchDone(const std::string& newCameraId)
{
    if (is_destroying_.load(std::memory_order_acquire)) {
        RTC_LOG(LS_WARNING) << "OnCameraSwitchDone: source destroying, skip";
        return;
    }
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << ": newCameraId=" << newCameraId;
    VideoCapturer::Observer* observer = nullptr;
    {
        UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
        observer = capturerObserver_;
    }

    if (observer) {
        observer->OnCameraSwitchDone(newCameraId);
    }
}

void OhosVideoTrackSource::OnCameraSwitchError(const std::string& errorMessage)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << ": errorMessage=" << errorMessage;

    VideoCapturer::Observer* observer = nullptr;
    {
        UNUSED std::lock_guard<std::mutex> lock(obsMutex_);
        observer = capturerObserver_;
    }

    if (observer) {
        observer->OnCameraSwitchError(errorMessage);
    }
}

void OhosVideoTrackSource::OnFrameCaptured(
    rtc::scoped_refptr<VideoFrameBuffer> buffer, int64_t timestampUs, VideoRotation rotation)
{
    if (buffer && buffer->width() > 0 && buffer->height() > 0) {
        captureFrameCount_.fetch_add(1, std::memory_order_relaxed);
    }
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << " timestampUs=" << timestampUs << ", rotation=" << rotation;
    auto alignedTimestampUs = timestampAligner_.TranslateTimestamp(timestampUs, rtc::TimeMicros());
    RTC_DLOG(LS_VERBOSE) << "alignedTimestampUs=" << alignedTimestampUs;

    int adapted_width = 0;
    int adapted_height = 0;
    int crop_width = 0;
    int crop_height = 0;
    int crop_x = 0;
    int crop_y = 0;
    bool drop;

    if (rotation % kVideoRotation_180 == 0) {
        drop = !AdaptFrame(
            buffer->width(), buffer->height(), timestampUs, &adapted_width, &adapted_height, &crop_width, &crop_height,
            &crop_x, &crop_y);
    } else {
        // Swap all width/height and x/y.
        drop = !AdaptFrame(
            buffer->height(), buffer->width(), timestampUs, &adapted_height, &adapted_width, &crop_height, &crop_width,
            &crop_y, &crop_x);
    }

    RTC_DLOG(LS_VERBOSE) << "adapted_width=" << adapted_width << ", adapted_height=" << adapted_height
                         << ", crop_width=" << crop_width << ", crop_height=" << crop_height << ", crop_x=" << crop_x
                         << ", crop_y=" << crop_y;

    if (drop) {
        RTC_DLOG(LS_VERBOSE) << "dropped";
        broadcaster_.OnDiscardedFrame();
        return;
    }

    if (adapted_height != buffer->height() || adapted_width != buffer->width()) {
        buffer = buffer->CropAndScale(crop_x, crop_y, crop_width, crop_height, adapted_width, adapted_height);
        if (!buffer) {
            RTC_LOG(LS_ERROR) << "CropAndScale returned nullptr. drop frame.";
            broadcaster_.OnDiscardedFrame();
            return;
        }
        RTC_LOG(LS_VERBOSE) << "After CropAndScale: buffer size=" << buffer->width() << "x" << buffer->height()
                            << ", type=" << static_cast<int>(buffer->type());
    } else {
        // No adaptations needed, just return the frame as is.
    }

    auto frame = VideoFrame::Builder()
                     .set_id(1)
                     .set_video_frame_buffer(buffer)
                     .set_rotation(rotation)
                     .set_timestamp_us(alignedTimestampUs)
                     .build();
    broadcaster_.OnFrame(frame);
}

} // namespace webrtc
