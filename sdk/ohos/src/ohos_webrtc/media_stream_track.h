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

#ifndef WEBRTC_MEDIA_STREAM_TRACK_H
#define WEBRTC_MEDIA_STREAM_TRACK_H

#include <atomic>
#include <mutex>
#include <queue>
#include <set>

#include "napi.h"

#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"

namespace webrtc {

class PeerConnectionFactoryWrapper;
class OhosVideoTrackSource;
class OhosLocalAudioSource;
class LongConstraint;
class DoubleConstraint;

class NapiMediaStreamTrack : public Napi::ObjectWrap<NapiMediaStreamTrack>, public ObserverInterface {
public:
    static void Init(Napi::Env env, Napi::Object exports);

    static Napi::Object NewInstance(
        std::shared_ptr<PeerConnectionFactoryWrapper> factory, rtc::scoped_refptr<MediaStreamTrackInterface> track);

    ~NapiMediaStreamTrack() override;

    rtc::scoped_refptr<MediaStreamTrackInterface> Get() const
    {
        return track_;
    }

    bool IsAudioTrack() const;
    bool IsVideoTrack() const;

    AudioTrackInterface* GetAudioTrack() const;
    VideoTrackInterface* GetVideoTrack() const;

    void AddSink(rtc::VideoSinkInterface<VideoFrame>* sink);
    void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink);

protected:
    static Napi::FunctionReference& Constructor();

    void AddVideoSink(rtc::VideoSinkInterface<VideoFrame>* sink);
    void RemoveVideoSink(rtc::VideoSinkInterface<VideoFrame>* sink);
    void RemoveAllVideoSinks();

    // ObserverInterface implementation
    void OnChanged() override;

    // Event dispatch infrastructure (JS thread). Events are enqueued from the
    // signaling thread and drained on the JS thread via ThreadSafeFunction.
    enum class TrackEventType { Ended, Mute, Unmute };
    void EnqueueEvent(TrackEventType type);
    void DrainEvents(Napi::Env env);
    void DispatchEvent(Napi::Env env, TrackEventType type);
    // Reads the underlying source muted state; false when no reliable signal exists.
    bool QuerySourceMuted() const;

    // Stores or clears an event handler reference (null/undefined clears).
    static void UpdateHandlerRef(Napi::FunctionReference& ref, const Napi::Value& value);

protected:
    friend class ObjectWrap;

    explicit NapiMediaStreamTrack(const Napi::CallbackInfo& info);

    Napi::Value GetId(const Napi::CallbackInfo& info);
    Napi::Value GetKind(const Napi::CallbackInfo& info);
    Napi::Value GetEnabled(const Napi::CallbackInfo& info);
    void SetEnabled(const Napi::CallbackInfo& info, const Napi::Value& value);
    Napi::Value GetReadyState(const Napi::CallbackInfo& info);
    Napi::Value GetMuted(const Napi::CallbackInfo& info);
    Napi::Value GetOnEnded(const Napi::CallbackInfo& info);
    void SetOnEnded(const Napi::CallbackInfo& info, const Napi::Value& value);
    Napi::Value GetOnMute(const Napi::CallbackInfo& info);
    void SetOnMute(const Napi::CallbackInfo& info, const Napi::Value& value);
    Napi::Value GetOnUnmute(const Napi::CallbackInfo& info);
    void SetOnUnmute(const Napi::CallbackInfo& info, const Napi::Value& value);
    Napi::Value Stop(const Napi::CallbackInfo& info);
    Napi::Value GetSource(const Napi::CallbackInfo& info);
    Napi::Value ApplyConstraints(const Napi::CallbackInfo& info);
    Napi::Value GetZoomRange(const Napi::CallbackInfo& info);
    Napi::Value ToJson(const Napi::CallbackInfo& info);

private:
    // Validate track state and input arguments for ApplyConstraints.
    // Returns a valid Promise on error, or Undefined() on success (caller should continue).
    Napi::Value ValidateApplyConstraintsInput(const Napi::CallbackInfo& info, Napi::Promise::Deferred& deferred);

    // Read and validate video constraints from the input object.
    struct VideoConstraints {
        int width = 0;
        int height = 0;
        double frameRate = 0.0;
        std::string facingMode;
        double zoom = 0.0;
        bool hasWidth = false;
        bool hasHeight = false;
        bool hasFrameRate = false;
        bool hasFacingMode = false;
        bool hasZoom = false;
    };
    bool ReadVideoConstraints(const Napi::Object& constraints, Napi::Env env, Napi::Promise::Deferred& deferred,
        VideoConstraints& out);
    // Apply validated constraints to video source via source thread.
    // Returns true on success, false on failure (caller should reject Promise).
    bool DoApplyVideoConstraints(const rtc::scoped_refptr<OhosVideoTrackSource>& videoSource,
        const VideoConstraints& constraints);

private:
    // Extract resolution from VideoConstraints.
    struct Resolution {
        int width = 0;
        int height = 0;
    };
    Resolution ExtractResolution(const VideoConstraints& vc) const;
    // Extract frameRate from VideoConstraints.
    int ExtractFrameRate(const VideoConstraints& vc) const;
    // Apply resolution and framerate constraints to the video adapter.
    void ApplyResolutionConstraints(const rtc::scoped_refptr<OhosVideoTrackSource>& videoSource,
        const VideoConstraints& vc);

public:
    // Validate facingMode constraint without applying it.
    // Returns true if facingMode can be satisfied, false otherwise.
    bool CanApplyFacingModeConstraint(const std::string& facingMode, int width, int height, int frameRate);
    // Apply facingMode constraint (must be called after CanApplyFacingModeConstraint returns true).
    // Returns true on success, false on failure.
    bool ApplyFacingModeConstraint(const rtc::scoped_refptr<OhosVideoTrackSource>& videoSource,
        const std::string& facingMode, int width, int height, int frameRate);

    // Context for reading numeric constraints.
    struct ConstraintReadContext {
        Napi::Env env;
        Napi::Promise::Deferred& deferred;
        const char* name;
    };
    // Helper to read a numeric constraint (width/height) and validate it's positive.
    bool ReadLongConstraint(const LongConstraint& constraint, const ConstraintReadContext& ctx,
        int32_t& outValue, bool& outHasValue);
    // Helper to read a numeric constraint (frameRate) and validate it's positive.
    bool ReadDoubleConstraint(const DoubleConstraint& constraint, const ConstraintReadContext& ctx,
        double& outValue, bool& outHasValue);

    std::shared_ptr<PeerConnectionFactoryWrapper> factory_;
    rtc::scoped_refptr<MediaStreamTrackInterface> track_;

    std::mutex sinksMutex_;
    std::set<rtc::VideoSinkInterface<VideoFrame>*> videoSinks_;

    // Event dispatch state. All napi objects are only touched on the JS thread;
    // flags below are atomic because OnChanged arrives on the signaling thread.
    Napi::ThreadSafeFunction tsfn_;
    Napi::ObjectReference selfRef_; // weak: events stop firing once the JS object is gone
    mutable std::mutex eventsMutex_;
    std::queue<TrackEventType> pendingEvents_;
    Napi::FunctionReference onendedRef_;
    Napi::FunctionReference onmuteRef_;
    Napi::FunctionReference onunmuteRef_;
    std::atomic<bool> destroyed_{false};
    std::atomic<bool> userStopped_{false};
    std::atomic<bool> muted_{false};
    bool endedDispatched_{false};
};

} // namespace webrtc

#endif // WEBRTC_MEDIA_STREAM_TRACK_H
