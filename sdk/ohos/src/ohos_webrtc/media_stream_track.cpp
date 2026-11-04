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

#include "media_stream_track.h"
#include "media_source.h"
#include "media_track_constraints.h"
#include "peer_connection_factory.h"
#include "audio_device/ohos_local_audio_source.h"
#include "video/video_track_source.h"
#include "user_media/media_constraints_util.h"
#include "camera/camera_capturer.h"
#include "camera/camera_enumerator.h"

#include "rtc_base/logging.h"
#include "utils/marcos.h"

namespace webrtc {

using namespace Napi;

const char kClassName[] = "MediaStreamTrack";

const char kAttributeNameId[] = "id";
const char kAttributeNameKind[] = "kind";
const char kAttributeNameLabel[] = "label";
const char kAttributeNameEnabled[] = "enabled";
const char kAttributeNameMuted[] = "muted";
const char kAttributeNameReadyState[] = "readyState";
const char kAttributeNameOnMute[] = "onmute";
const char kAttributeNameOnUnmute[] = "onunmute";
const char kAttributeNameOnEnded[] = "onended";

const char kMethodNameStop[] = "stop";
const char kMethodNameGetSource[] = "getSource";
const char kMethodNameToJson[] = "toJSON";
const char kMethodNameApplyConstraints[] = "applyConstraints";
const char kMethodNameGetZoomRange[] = "getZoomRange";

const char kEnumMediaStreamTrackStateLive[] = "live";
const char kEnumMediaStreamTrackStateEnded[] = "ended";

const char kEventNameMute[] = "mute";
const char kEventNameUnmute[] = "unmute";
const char kEventNameEnded[] = "ended";

void NapiMediaStreamTrack::Init(Napi::Env env, Object exports)
{
    Function func = DefineClass(
        env, kClassName,
        {
            InstanceAccessor<&NapiMediaStreamTrack::GetKind>(kAttributeNameKind),
            InstanceAccessor<&NapiMediaStreamTrack::GetId>(kAttributeNameId),
            InstanceAccessor<&NapiMediaStreamTrack::GetEnabled, &NapiMediaStreamTrack::SetEnabled>(
                kAttributeNameEnabled),
            InstanceAccessor<&NapiMediaStreamTrack::GetReadyState>(kAttributeNameReadyState),
            InstanceAccessor<&NapiMediaStreamTrack::GetMuted>(kAttributeNameMuted),
            InstanceAccessor<&NapiMediaStreamTrack::GetOnEnded, &NapiMediaStreamTrack::SetOnEnded>(
                kAttributeNameOnEnded),
            InstanceAccessor<&NapiMediaStreamTrack::GetOnMute, &NapiMediaStreamTrack::SetOnMute>(
                kAttributeNameOnMute),
            InstanceAccessor<&NapiMediaStreamTrack::GetOnUnmute, &NapiMediaStreamTrack::SetOnUnmute>(
                kAttributeNameOnUnmute),
            InstanceMethod<&NapiMediaStreamTrack::Stop>(kMethodNameStop),
            InstanceMethod<&NapiMediaStreamTrack::GetSource>(kMethodNameGetSource),
            InstanceMethod<&NapiMediaStreamTrack::ApplyConstraints>(kMethodNameApplyConstraints),
            InstanceMethod<&NapiMediaStreamTrack::GetZoomRange>(kMethodNameGetZoomRange),
            InstanceMethod<&NapiMediaStreamTrack::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    Constructor() = Persistent(func);
    Constructor().SuppressDestruct();
}

Napi::Object NapiMediaStreamTrack::NewInstance(
    std::shared_ptr<PeerConnectionFactoryWrapper> factory, rtc::scoped_refptr<MediaStreamTrackInterface> track)
{
    auto env = Constructor().Env();
    if (!factory || !track) {
        NAPI_THROW(Error::New(env, "Invalid argument"), Object::New(env));
    }

    return Constructor().New(
        {External<std::shared_ptr<PeerConnectionFactoryWrapper>>::New(env, &factory),
         Napi::External<rtc::scoped_refptr<MediaStreamTrackInterface>>::New(env, &track)});
}

Napi::FunctionReference& NapiMediaStreamTrack::Constructor()
{
    static thread_local Napi::FunctionReference constructor;
    return constructor;
}

NapiMediaStreamTrack::NapiMediaStreamTrack(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<NapiMediaStreamTrack>(info)
{
    RTC_DLOG(LS_INFO) << __FUNCTION__;

    auto env = info.Env();
    // Create from native with 2 paramters, and SHOULD NOT create from ArkTS
    if (info.Length() != 2 || !info[0].IsExternal() || !info[1].IsExternal()) {
        NAPI_THROW_VOID(Napi::Error::New(info.Env(), "Invalid Operation"));
    }

    factory_ = *info[0].As<External<std::shared_ptr<PeerConnectionFactoryWrapper>>>().Data();
    track_ = *info[1].As<External<rtc::scoped_refptr<MediaStreamTrackInterface>>>().Data();

    track_->RegisterObserver(this);
    muted_ = QuerySourceMuted();

    // Event dispatch to the JS thread. The callback only runs while the JS object is
    // alive (weak self reference); destruction aborts the tsfn first.
    selfRef_ = Napi::Persistent(this->Value());
    selfRef_.SetWeak();
    tsfn_ = Napi::ThreadSafeFunction::New(
        env, Napi::Function::New(env, [this](const Napi::CallbackInfo& cbInfo) { DrainEvents(cbInfo.Env()); }),
        "MediaStreamTrackEvents", 0, 1);
}

NapiMediaStreamTrack::~NapiMediaStreamTrack()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    // Block any in-flight dispatch before tearing down members.
    destroyed_.store(true);
    if (tsfn_) {
        tsfn_.Abort();
    }
    {
        std::lock_guard<std::mutex> lock(eventsMutex_);
        onendedRef_.Reset();
        onmuteRef_.Reset();
        onunmuteRef_.Reset();
        selfRef_.Reset();
        while (!pendingEvents_.empty()) {
            pendingEvents_.pop();
        }
    }

    track_->UnregisterObserver(this);

    RemoveAllVideoSinks();

    if (IsAudioTrack()) {
        factory_->RemoveAudioSource(track_);
    } else if (IsVideoTrack()) {
        factory_->RemoveVideoSource(track_);
    }
}

void NapiMediaStreamTrack::UpdateHandlerRef(Napi::FunctionReference& ref, const Napi::Value& value)
{
    if (value.IsFunction()) {
        ref = Napi::Persistent(value.As<Napi::Function>());
    } else {
        // null/undefined (or any non-function) unbinds the handler.
        ref.Reset();
    }
}

bool NapiMediaStreamTrack::QuerySourceMuted() const
{
    if (!factory_ || !track_) {
        return false;
    }
    if (IsAudioTrack()) {
        auto source = factory_->GetAudioSource(track_);
        return source && source->state() == MediaSourceInterface::kMuted;
    }
    if (IsVideoTrack()) {
        auto source = factory_->GetVideoSource(track_);
        return source && source->state() == MediaSourceInterface::kMuted;
    }
    return false;
}

void NapiMediaStreamTrack::EnqueueEvent(TrackEventType type)
{
    {
        std::lock_guard<std::mutex> lock(eventsMutex_);
        pendingEvents_.push(type);
    }

    napi_status status = tsfn_.NonBlockingCall([](Napi::Env /*env*/, Napi::Function /*jsCallback*/) {});
    if (status != napi_ok) {
        RTC_LOG(LS_ERROR) << "Track event tsfn call failed: " << status;
    }
}

void NapiMediaStreamTrack::DrainEvents(Napi::Env env)
{
    if (destroyed_.load()) {
        return;
    }
    if (selfRef_.IsEmpty()) {
        // JS object already collected; drop queued events.
        std::lock_guard<std::mutex> lock(eventsMutex_);
        while (!pendingEvents_.empty()) {
            pendingEvents_.pop();
        }
        return;
    }

    std::queue<TrackEventType> events;
    {
        std::lock_guard<std::mutex> lock(eventsMutex_);
        events.swap(pendingEvents_);
    }

    Napi::HandleScope scope(env);
    while (!events.empty()) {
        auto type = events.front();
        events.pop();
        if (destroyed_.load()) {
            break;
        }
        DispatchEvent(env, type);
    }
}

void NapiMediaStreamTrack::DispatchEvent(Napi::Env env, TrackEventType type)
{
    Napi::FunctionReference* handlerRef = nullptr;
    const char* eventType = nullptr;
    switch (type) {
        case TrackEventType::Ended:
            handlerRef = &onendedRef_;
            eventType = kEventNameEnded;
            break;
        case TrackEventType::Mute:
            handlerRef = &onmuteRef_;
            eventType = kEventNameMute;
            break;
        case TrackEventType::Unmute:
            handlerRef = &onunmuteRef_;
            eventType = kEventNameUnmute;
            break;
        default:
            return;
    }

    Napi::Function handler;
    {
        std::lock_guard<std::mutex> lock(eventsMutex_);
        if (handlerRef->IsEmpty() || selfRef_.IsEmpty()) {
            return;
        }
        handler = handlerRef->Value();
    }

    RTC_DLOG(LS_INFO) << __FUNCTION__ << "(" << track_->kind() << "): " << eventType;

    auto event = Napi::Object::New(env);
    event.Set("type", Napi::String::New(env, eventType));

    handler.Call(selfRef_.Value(), {event});

    // Isolate user callback exceptions: log and continue cleanup, never propagate.
    bool isPending = false;
    napi_is_exception_pending(env, &isPending);
    if (isPending) {
        napi_value exception;
        napi_get_and_clear_last_exception(env, &exception);
        RTC_LOG(LS_ERROR) << "Track event handler (" << eventType << ") threw an exception";
    }
}

bool NapiMediaStreamTrack::IsAudioTrack() const
{
    return (track_->kind() == MediaStreamTrackInterface::kAudioKind);
}

bool NapiMediaStreamTrack::IsVideoTrack() const
{
    return (track_->kind() == MediaStreamTrackInterface::kVideoKind);
}

AudioTrackInterface* NapiMediaStreamTrack::GetAudioTrack() const
{
    if (!IsAudioTrack()) {
        RTC_LOG(LS_WARNING) << "Not a audio track";
        return nullptr;
    }
    return static_cast<AudioTrackInterface*>(track_.get());
}

VideoTrackInterface* NapiMediaStreamTrack::GetVideoTrack() const
{
    if (!IsVideoTrack()) {
        RTC_LOG(LS_WARNING) << "Not a video track";
        return nullptr;
    }
    return static_cast<VideoTrackInterface*>(track_.get());
}

void NapiMediaStreamTrack::AddSink(rtc::VideoSinkInterface<VideoFrame>* sink)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    AddVideoSink(sink);
}

void NapiMediaStreamTrack::RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    RemoveVideoSink(sink);
}

void NapiMediaStreamTrack::AddVideoSink(rtc::VideoSinkInterface<VideoFrame>* sink)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!IsVideoTrack()) {
        RTC_LOG(LS_WARNING) << "Not a video track";
        return;
    }

    {
        UNUSED std::lock_guard<std::mutex> lock(sinksMutex_);
        auto ret = videoSinks_.insert(sink);
        if (!ret.second) {
            RTC_LOG(LS_WARNING) << "Failed to insert video sink";
            return;
        }
    }

    auto videoTrack = static_cast<VideoTrackInterface*>(track_.get());
    videoTrack->AddOrUpdateSink(sink, rtc::VideoSinkWants());
}

void NapiMediaStreamTrack::RemoveVideoSink(rtc::VideoSinkInterface<VideoFrame>* sink)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!IsVideoTrack()) {
        RTC_LOG(LS_WARNING) << "Not a video track";
        return;
    }

    {
        UNUSED std::lock_guard<std::mutex> lock(sinksMutex_);
        if (videoSinks_.erase(sink) == 0) {
            RTC_LOG(LS_WARNING) << "Failed to erase video sink";
            return;
        }
    }

    auto videoTrack = static_cast<VideoTrackInterface*>(track_.get());
    videoTrack->RemoveSink(sink);
}

void NapiMediaStreamTrack::RemoveAllVideoSinks()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!IsVideoTrack()) {
        return;
    }

    std::set<rtc::VideoSinkInterface<VideoFrame>*> sinks;
    {
        UNUSED std::lock_guard<std::mutex> lock(sinksMutex_);
        videoSinks_.swap(sinks);
    }

    auto videoTrack = static_cast<VideoTrackInterface*>(track_.get());
    for (auto& sink : sinks) {
        if (sink) {
            videoTrack->RemoveSink(sink);
        }
    }
}

void NapiMediaStreamTrack::OnChanged()
{
    RTC_DLOG(LS_INFO) << __FUNCTION__ << "(" << track_->kind() << ") state=" << track_->state()
                      << ", enabled=" << track_->enabled();

    if (destroyed_.load()) {
        return;
    }

    auto state = track_->state();
    bool sourceMuted = QuerySourceMuted();

    // ended: kLive -> kEnded for reasons other than this track's own stop().
    if (state == MediaStreamTrackInterface::kEnded && !userStopped_ && !endedDispatched_) {
        endedDispatched_ = true;
        EnqueueEvent(TrackEventType::Ended);
    }

    // mute/unmute: driven solely by the source muted state; enabled changes are
    // deliberately excluded (black frames / silence are not "media unavailable").
    if (!endedDispatched_ && sourceMuted != muted_) {
        muted_ = sourceMuted;
        EnqueueEvent(sourceMuted ? TrackEventType::Mute : TrackEventType::Unmute);
    }
}

Napi::Value NapiMediaStreamTrack::GetId(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!track_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    return String::New(info.Env(), track_->id());
}

Napi::Value NapiMediaStreamTrack::GetKind(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!track_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    return String::New(info.Env(), track_->kind());
}

Napi::Value NapiMediaStreamTrack::GetEnabled(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!track_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    return Boolean::New(info.Env(), track_->enabled());
}

void NapiMediaStreamTrack::SetEnabled(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!value.IsBoolean()) {
        NAPI_THROW_VOID(Error::New(info.Env(), "Invalid argument"));
    }

    if (!track_) {
        NAPI_THROW_VOID(Error::New(info.Env(), "Illegal state"));
    }

    track_->set_enabled(value.As<Boolean>());
}

Napi::Value NapiMediaStreamTrack::GetReadyState(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!track_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    auto state = track_->state();
    switch (state) {
        case MediaStreamTrackInterface::kLive:
            return String::New(info.Env(), kEnumMediaStreamTrackStateLive);
        case MediaStreamTrackInterface::kEnded:
            return String::New(info.Env(), kEnumMediaStreamTrackStateEnded);
        default:
            break;
    }

    NAPI_THROW(Error::New(info.Env(), "Invalid state"), info.Env().Undefined());
}

Napi::Value NapiMediaStreamTrack::GetMuted(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!track_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    std::lock_guard<std::mutex> lock(eventsMutex_);
    return Napi::Boolean::New(info.Env(), muted_.load());
}

Napi::Value NapiMediaStreamTrack::GetOnEnded(const Napi::CallbackInfo& info)
{
    std::lock_guard<std::mutex> lock(eventsMutex_);
    return onendedRef_.IsEmpty() ? info.Env().Null() : Napi::Value(info.Env(), onendedRef_.Value());
}

void NapiMediaStreamTrack::SetOnEnded(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    std::lock_guard<std::mutex> lock(eventsMutex_);
    UpdateHandlerRef(onendedRef_, value);
}

Napi::Value NapiMediaStreamTrack::GetOnMute(const Napi::CallbackInfo& info)
{
    std::lock_guard<std::mutex> lock(eventsMutex_);
    return onmuteRef_.IsEmpty() ? info.Env().Null() : Napi::Value(info.Env(), onmuteRef_.Value());
}

void NapiMediaStreamTrack::SetOnMute(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    std::lock_guard<std::mutex> lock(eventsMutex_);
    UpdateHandlerRef(onmuteRef_, value);
}

Napi::Value NapiMediaStreamTrack::GetOnUnmute(const Napi::CallbackInfo& info)
{
    std::lock_guard<std::mutex> lock(eventsMutex_);
    return onunmuteRef_.IsEmpty() ? info.Env().Null() : Napi::Value(info.Env(), onunmuteRef_.Value());
}

void NapiMediaStreamTrack::SetOnUnmute(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    std::lock_guard<std::mutex> lock(eventsMutex_);
    UpdateHandlerRef(onunmuteRef_, value);
}

Napi::Value NapiMediaStreamTrack::Stop(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!track_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    // W3C: a track ended by its own stop() does not fire ended.
    userStopped_ = true;

    auto state = track_->state();
    if (state == MediaStreamTrackInterface::kEnded) {
        RTC_LOG(LS_VERBOSE) << "The track is already ended";
        return info.Env().Undefined();
    }

    // MediaStreamTrack.stop(): end underlying source; observers then set track ended and stop RTP.
    // NAPI calls arrive off signaling thread: PostTask source->Stop() there (no BlockingCall).
    // Video sinks are removed synchronously to avoid dangling sink pointers in PostTask.
    auto signalingThread = factory_->GetSignalingThread();
    if (IsAudioTrack()) {
        auto source = factory_->GetAudioSource(track_);
        if (source) {
            if (signalingThread->IsCurrent()) {
                source->Stop();
            } else {
                signalingThread->BlockingCall([source] { source->Stop(); });
            }
        }
    } else if (IsVideoTrack()) {
        RemoveAllVideoSinks();
        auto source = factory_->GetVideoSource(track_);
        if (source) {
            if (signalingThread->IsCurrent()) {
                source->Stop();
            } else {
                signalingThread->BlockingCall([source] { source->Stop(); });
            }
        }
    }

    return info.Env().Undefined();
}

Napi::Value NapiMediaStreamTrack::GetSource(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!track_) {
        NAPI_THROW(Error::New(info.Env(), "Illegal state"), info.Env().Undefined());
    }

    if (IsAudioTrack()) {
        auto source = factory_->GetAudioSource(track_);
        if (source) {
            return NapiAudioSource::NewInstance(info.Env(), source);
        }
    } else if (IsVideoTrack()) {
        auto source = factory_->GetVideoSource(track_);
        if (source) {
            return NapiVideoSource::NewInstance(info.Env(), source);
        }
    }

    return info.Env().Null();
}

bool NapiMediaStreamTrack::ReadLongConstraint(const LongConstraint& constraint, const ConstraintReadContext& ctx,
    int32_t& outValue, bool& outHasValue)
{
    if (!constraint.IsConstrained()) {
        return true;
    }
    if (constraint.HasExact()) {
        outValue = constraint.Exact();
        outHasValue = true;
    } else if (constraint.HasIdeal()) {
        outValue = constraint.Ideal();
        outHasValue = true;
    }
    if (outHasValue && outValue <= 0) {
        auto error = Napi::Error::New(ctx.env, std::string(ctx.name) + " must be a positive number");
        ctx.deferred.Reject(error.Value());
        return false;
    }
    return true;
}

bool NapiMediaStreamTrack::ReadDoubleConstraint(const DoubleConstraint& constraint, const ConstraintReadContext& ctx,
    double& outValue, bool& outHasValue)
{
    if (!constraint.IsConstrained()) {
        return true;
    }
    if (constraint.HasExact()) {
        outValue = constraint.Exact();
        outHasValue = true;
    } else if (constraint.HasIdeal()) {
        outValue = constraint.Ideal();
        outHasValue = true;
    }
    if (outHasValue && outValue <= 0) {
        auto error = Napi::Error::New(ctx.env, std::string(ctx.name) + " must be a positive number");
        ctx.deferred.Reject(error.Value());
        return false;
    }
    return true;
}

bool NapiMediaStreamTrack::ReadVideoConstraints(const Napi::Object& constraints, Napi::Env env,
    Napi::Promise::Deferred& deferred, VideoConstraints& out)
{
    MediaTrackConstraints trackConstraints;
    NapiMediaConstraints::JsToNative(constraints, trackConstraints);

    const auto& basic = trackConstraints.Basic();

    if (!ReadLongConstraint(basic.width, {env, deferred, "width"}, out.width, out.hasWidth)) {
        return false;
    }
    if (!ReadLongConstraint(basic.height, {env, deferred, "height"}, out.height, out.hasHeight)) {
        return false;
    }
    if (!ReadDoubleConstraint(basic.frameRate, {env, deferred, "frameRate"}, out.frameRate, out.hasFrameRate)) {
        return false;
    }

    if (!ReadDoubleConstraint(basic.zoom, {env, deferred, "zoom"}, out.zoom, out.hasZoom)) {
        return false;
    }
    RTC_LOG(LS_INFO) << __FUNCTION__ << ": hasZoom=" << out.hasZoom << ", zoom=" << out.zoom;

    if (basic.facingMode.IsConstrained()) {
        if (basic.facingMode.HasExact() && !basic.facingMode.Exact().empty()) {
            out.facingMode = basic.facingMode.Exact()[0];
            out.hasFacingMode = true;
            RTC_LOG(LS_INFO) << __FUNCTION__ << ": facingMode exact=" << out.facingMode;
        } else if (basic.facingMode.HasIdeal() && !basic.facingMode.Ideal().empty()) {
            out.facingMode = basic.facingMode.Ideal()[0];
            out.hasFacingMode = true;
            RTC_LOG(LS_INFO) << __FUNCTION__ << ": facingMode ideal=" << out.facingMode;
        } else {
            RTC_LOG(LS_WARNING) << __FUNCTION__ << ": facingMode is constrained but has no exact/ideal value";
        }
    } else {
        RTC_LOG(LS_INFO) << __FUNCTION__ << ": facingMode is not constrained";
    }

    RTC_LOG(LS_INFO) << __FUNCTION__ << ": VideoConstraints parsed: hasWidth=" << out.hasWidth
                       << ", hasHeight=" << out.hasHeight << ", hasFrameRate=" << out.hasFrameRate
                       << ", hasFacingMode=" << out.hasFacingMode << ", facingMode=" << out.facingMode;

    return true;
}

NapiMediaStreamTrack::Resolution NapiMediaStreamTrack::ExtractResolution(const VideoConstraints& vc) const
{
    Resolution res;
    if (vc.hasWidth || vc.hasHeight) {
        if (vc.width > 0 && vc.height > 0) {
            res.width = vc.width;
            res.height = vc.height;
            RTC_LOG(LS_INFO) << "ApplyConstraints video resolution: " << res.width << "x" << res.height;
        } else if (vc.width > 0) {
            RTC_LOG(LS_WARNING) << "ApplyConstraints video width only: " << vc.width;
        } else if (vc.height > 0) {
            RTC_LOG(LS_WARNING) << "ApplyConstraints video height only: " << vc.height;
        }
    }
    return res;
}

int NapiMediaStreamTrack::ExtractFrameRate(const VideoConstraints& vc) const
{
    if (vc.hasFrameRate && vc.frameRate > 0) {
        RTC_LOG(LS_INFO) << "ApplyConstraints video frameRate: " << vc.frameRate;
        return static_cast<int>(vc.frameRate);
    }
    return 0;
}

void NapiMediaStreamTrack::ApplyResolutionConstraints(const rtc::scoped_refptr<OhosVideoTrackSource>& videoSource,
    const VideoConstraints& vc)
{
    auto res = ExtractResolution(vc);
    int maxFps = ExtractFrameRate(vc);

    VideoTrackSourceConstraints constraints;
    if (maxFps > 0) {
        constraints.max_fps = vc.frameRate;
    }

    if (res.width > 0 && res.height > 0) {
        videoSource->ApplyConstraints(constraints, res.width, res.height, maxFps);
    } else if (constraints.max_fps.has_value()) {
        videoSource->ProcessConstraints(constraints);
    }
}

bool NapiMediaStreamTrack::DoApplyVideoConstraints(const rtc::scoped_refptr<OhosVideoTrackSource>& videoSource,
    const VideoConstraints& vc)
{
    // Step 1: Validate all constraints before applying any of them.
    if (vc.hasFacingMode) {
        int width = vc.hasWidth ? vc.width : 0;
        int height = vc.hasHeight ? vc.height : 0;
        int frameRate = vc.hasFrameRate ? static_cast<int>(vc.frameRate) : 0;
        if (!CanApplyFacingModeConstraint(vc.facingMode, width, height, frameRate)) {
            return false;
        }
    }

    // Step 2: Apply all constraints atomically (all must succeed since validation passed).
    if (vc.hasFacingMode) {
        int width = vc.hasWidth ? vc.width : 0;
        int height = vc.hasHeight ? vc.height : 0;
        int frameRate = vc.hasFrameRate ? static_cast<int>(vc.frameRate) : 0;
        if (!ApplyFacingModeConstraint(videoSource, vc.facingMode, width, height, frameRate)) {
            return false;
        }
    }

    // Step 3: Apply resolution and framerate constraints to the video adapter.
    ApplyResolutionConstraints(videoSource, vc);

    // Step 4: Apply zoom to the camera capturer (hardware zoom).
    if (vc.hasZoom && vc.zoom > 0.0) {
        RTC_LOG(LS_INFO) << __FUNCTION__ << ": applying zoom=" << vc.zoom;
        videoSource->SetZoomRatio(vc.zoom);
    }

    return true;
}

bool NapiMediaStreamTrack::CanApplyFacingModeConstraint(
    const std::string& facingMode, int width, int height, int frameRate)
{
    RTC_LOG(LS_INFO) << "Validate facingMode constraint: " << facingMode;

    MediaTrackConstraints facingModeConstraint;
    facingModeConstraint.Initialize();
    facingModeConstraint.MutableBasic().facingMode.SetExact(facingMode);

    int selectWidth = width > 0 ? width : kDefaultWidth;
    int selectHeight = height > 0 ? height : kDefaultHeight;
    int selectFrameRate = frameRate > 0 ? frameRate : kDefaultFrameRate;

    CameraCaptureSettings selectedSetting;
    std::string failedConstraintName;
    if (!SelectSettingsForVideo(
        CameraEnumerator::GetDevices(), facingModeConstraint, selectWidth, selectHeight, selectFrameRate,
        selectedSetting, failedConstraintName)) {
        RTC_LOG(LS_ERROR) << "Failed to select settings for facingMode: " << facingMode
                          << ", unsatisfied constraint: " << failedConstraintName;
        return false;
    }

    auto newCapturer = CameraCapturer::Create(selectedSetting.deviceId, selectedSetting.profile);
    if (!newCapturer) {
        RTC_LOG(LS_ERROR) << "Failed to create camera capturer for facingMode: " << facingMode;
        return false;
    }

    // Validation passed, but we don't apply the constraint here.
    // The capturer is created but will be destroyed since it's not used.
    return true;
}

bool NapiMediaStreamTrack::ApplyFacingModeConstraint(const rtc::scoped_refptr<OhosVideoTrackSource>& videoSource,
    const std::string& facingMode, int width, int height, int frameRate)
{
    RTC_LOG(LS_INFO) << "ApplyConstraints video facingMode: " << facingMode;

    MediaTrackConstraints facingModeConstraint;
    facingModeConstraint.Initialize();
    facingModeConstraint.MutableBasic().facingMode.SetExact(facingMode);

    int selectWidth = width > 0 ? width : kDefaultWidth;
    int selectHeight = height > 0 ? height : kDefaultHeight;
    int selectFrameRate = frameRate > 0 ? frameRate : kDefaultFrameRate;

    CameraCaptureSettings selectedSetting;
    std::string failedConstraintName;
    if (!SelectSettingsForVideo(
        CameraEnumerator::GetDevices(), facingModeConstraint, selectWidth, selectHeight, selectFrameRate,
        selectedSetting, failedConstraintName)) {
        RTC_LOG(LS_ERROR) << "Failed to select settings for facingMode: " << facingMode
                          << ", unsatisfied constraint: " << failedConstraintName;
        return false;
    }

    auto newCapturer = CameraCapturer::Create(selectedSetting.deviceId, selectedSetting.profile);
    if (!newCapturer) {
        RTC_LOG(LS_ERROR) << "Failed to create camera capturer for facingMode: " << facingMode;
        return false;
    }

    videoSource->SwitchCapturer(std::shared_ptr<VideoCapturer>(newCapturer.release()));
    return true;
}

Napi::Value NapiMediaStreamTrack::ValidateApplyConstraintsInput(
    const Napi::CallbackInfo& info, Napi::Promise::Deferred& deferred)
{
    auto env = info.Env();
    if (!track_) {
        deferred.Reject(Error::New(env, "Illegal state").Value());
        return deferred.Promise();
    }
    if (track_->state() == MediaStreamTrackInterface::kEnded) {
        deferred.Reject(Error::New(env, "Cannot apply constraints to an ended track").Value());
        return deferred.Promise();
    }
    if (info.Length() == 0 || !info[0].IsObject()) {
        deferred.Reject(TypeError::New(env, "Invalid argument").Value());
        return deferred.Promise();
    }
    return env.Undefined();
}

Napi::Value NapiMediaStreamTrack::ApplyConstraints(const Napi::CallbackInfo& info)
{
    RTC_LOG(LS_INFO) << __FUNCTION__ << " called on " << (IsVideoTrack() ? "video" : "audio") << " track";

    auto env = info.Env();
    auto deferred = Napi::Promise::Deferred::New(env);

    auto validationResult = ValidateApplyConstraintsInput(info, deferred);
    if (!validationResult.IsUndefined()) {
        return validationResult;
    }

    auto constraints = info[0].As<Object>();

    if (IsVideoTrack()) {
        auto videoSource = factory_->GetVideoSource(track_);
        if (!videoSource) {
            RTC_LOG(LS_ERROR) << __FUNCTION__ << ": videoSource is null";
            deferred.Reject(Error::New(env, "Video source is not available").Value());
            return deferred.Promise();
        }
        if (videoSource->IsEnded()) {
            deferred.Reject(Error::New(env, "Cannot apply constraints to an ended track").Value());
            return deferred.Promise();
        }
        VideoConstraints videoConstraints;
        if (!ReadVideoConstraints(constraints, env, deferred, videoConstraints)) {
            RTC_LOG(LS_ERROR) << __FUNCTION__ << ": ReadVideoConstraints failed";
            return deferred.Promise();
        }

        if (!DoApplyVideoConstraints(videoSource, videoConstraints)) {
            RTC_LOG(LS_ERROR) << __FUNCTION__ << ": DoApplyVideoConstraints failed";
            deferred.Reject(Error::New(env, "Constraints could not be satisfied").Value());
            return deferred.Promise();
        }
        deferred.Resolve(env.Undefined());
        return deferred.Promise();
    }

    if (IsAudioTrack()) {
        auto error = TypeError::New(env, "Applying echoCancellation/autoGainControl/noiseSuppression "
            "constraints to an audio track at runtime is not supported");
        deferred.Reject(error.Value());
        return deferred.Promise();
    }

    deferred.Resolve(env.Undefined());
    return deferred.Promise();
}

Napi::Value NapiMediaStreamTrack::GetZoomRange(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto obj = Object::New(env);

    if (!IsVideoTrack()) {
        RTC_LOG(LS_WARNING) << __FUNCTION__ << ": not a video track";
        obj.Set("min", 1.0);
        obj.Set("max", 1.0);
        obj.Set("supported", false);
        return obj;
    }

    auto videoSource = factory_->GetVideoSource(track_);
    if (!videoSource) {
        RTC_LOG(LS_WARNING) << __FUNCTION__ << ": videoSource is null";
        obj.Set("min", 1.0);
        obj.Set("max", 1.0);
        obj.Set("supported", false);
        return obj;
    }

    float minZoom = 1.0f;
    float maxZoom = 1.0f;
    bool supported = videoSource->GetZoomRatioRange(&minZoom, &maxZoom);

    obj.Set("min", static_cast<double>(minZoom));
    obj.Set("max", static_cast<double>(maxZoom));
    obj.Set("supported", supported);

    RTC_LOG(LS_INFO) << __FUNCTION__ << ": supported=" << supported << ", min=" << minZoom << ", max=" << maxZoom;

    return obj;
}

Napi::Value NapiMediaStreamTrack::ToJson(const Napi::CallbackInfo& info)
{
    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", "NapiMediaStreamTrack");
#endif
    if (track_) {
        json.Set(kAttributeNameId, GetId(info));
        json.Set(kAttributeNameKind, GetKind(info));
        json.Set(kAttributeNameEnabled, GetEnabled(info));
        json.Set(kAttributeNameReadyState, GetReadyState(info));
    }

    return json;
}

} // namespace webrtc
