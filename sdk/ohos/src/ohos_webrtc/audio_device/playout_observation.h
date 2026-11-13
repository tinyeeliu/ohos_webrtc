// Copyright 2026 The WebRTC project authors. All Rights Reserved.
// Use of this source code is governed by a BSD-style license in LICENSE.
#ifndef SDK_OHOS_PLAYOUT_OBSERVATION_H_
#define SDK_OHOS_PLAYOUT_OBSERVATION_H_

#include <cstddef>
#include <cstdint>
#include <mutex>

namespace webrtc {

// No PCM is retained or forwarded to the application. Frame positions refer to
// the same OS renderer clock, not RTP reception or provider generation time.
struct PlayoutObservation {
    bool available = false;
    bool muted = false;
    uint64_t sequence = 0;
    int sampleRate = 0;
    int64_t submittedFrames = 0;
    int64_t playedFrames = 0;
    int64_t lastNonSilentFrame = 0;
    int64_t lastAudibleFrame = 0;
    int64_t observedAtNs = 0;
};

class PlayoutObserver {
public:
    void Observe(const int16_t* samples, size_t count, int channels, int rate,
                 int64_t written, int64_t played, int64_t atNs, bool clockValid, bool muted)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++snapshot_.sequence;
        snapshot_.muted = muted;
        snapshot_.sampleRate = rate;
        snapshot_.observedAtNs = atNs;
        if (!samples || channels < 1 || rate < 1 || count % channels != 0 ||
            !clockValid || written < 0 || played < 0 || played > written) {
            snapshot_.available = false;
            return;
        }
        // A renderer restart or clock reset must not inherit a prior tail.
        if (written < lastWritten_) {
            snapshot_.lastNonSilentFrame = 0;
            snapshot_.lastAudibleFrame = 0;
        }
        lastWritten_ = written;
        snapshot_.available = true;
        snapshot_.submittedFrames = written + static_cast<int64_t>(count / channels);
        snapshot_.playedFrames = played;
        for (size_t index = count; index > 0; --index) {
            const int32_t sample = samples[index - 1];
            if (sample > 200 || sample < -200) {
                const int64_t frame = written + static_cast<int64_t>((index - 1) / channels) + 1;
                snapshot_.lastNonSilentFrame = frame;
                if (!muted) snapshot_.lastAudibleFrame = frame;
                break;
            }
        }
    }

    void Invalidate()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.available = false;
        ++snapshot_.sequence;
    }

    PlayoutObservation Read() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshot_;
    }

private:
    mutable std::mutex mutex_;
    PlayoutObservation snapshot_;
    int64_t lastWritten_ = 0;
};
} // namespace webrtc
#endif
