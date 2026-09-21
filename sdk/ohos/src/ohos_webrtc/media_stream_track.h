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

#include <set>

#include "napi.h"

#include <optional>

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

protected:
    friend class ObjectWrap;

    explicit NapiMediaStreamTrack(const Napi::CallbackInfo& info);

    Napi::Value GetId(const Napi::CallbackInfo& info);
    Napi::Value GetKind(const Napi::CallbackInfo& info);
    Napi::Value GetEnabled(const Napi::CallbackInfo& info);
    void SetEnabled(const Napi::CallbackInfo& info, const Napi::Value& value);
    Napi::Value GetReadyState(const Napi::CallbackInfo& info);
    Napi::Value Stop(const Napi::CallbackInfo& info);
    Napi::Value GetSource(const Napi::CallbackInfo& info);
    Napi::Value ApplyConstraints(const Napi::CallbackInfo& info);
    Napi::Value GetZoomRange(const Napi::CallbackInfo& info);
    Napi::Value CaptureFrame(const Napi::CallbackInfo& info);
    Napi::Value ToJson(const Napi::CallbackInfo& info);

    Napi::Value GetAudioSamplesHandler(const Napi::CallbackInfo& info);
    void SetAudioSamplesHandler(const Napi::CallbackInfo& info, const Napi::Value& value);

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

    // ---- captureFrame (#6): one-shot sink + tsfn delivery ----
    class CaptureSink;
    friend class CaptureSink;

    // Called on the video thread by CaptureSink; converts and forwards.
    void HandleCapturedFrame(const VideoFrame& frame);
    // JS-thread only: resolves/rejects the pending promise and detaches the sink.
    void FinishCapture(Napi::Value result, bool ok);
    // Video-thread helper rejecting a capture that could not be converted.
    void RejectCapture(const std::string& message);

    std::unique_ptr<CaptureSink> captureSink_;
    std::optional<Napi::Promise::Deferred> captureDeferred_;
    Napi::ThreadSafeFunction captureTsfn_;
    std::mutex captureMutex_;
    bool capturePending_{false};

    // ---- onaudiosamplesready (#5): playout-side per-sample events ----
    class AudioSampleSink;
    friend class AudioSampleSink;

    // Audio thread: called by AudioSampleSink with interleaved PCM.
    void HandleAudioSampleData(
        const void* audioData, int bitsPerSample, int sampleRate, size_t channels, size_t frames);
    // JS thread: attach/detach the native sink and the tsfn.
    void StartAudioSamples(Napi::Env env);
    void StopAudioSamples();

    std::unique_ptr<AudioSampleSink> audioSampleSink_;
    Napi::ThreadSafeFunction audioSamplesTsfn_;
    Napi::FunctionReference audioSamplesHandlerRef_;
    bool audioSamplesHandlerSet_{false};
};

} // namespace webrtc

#endif // WEBRTC_MEDIA_STREAM_TRACK_H
