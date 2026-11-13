// Copyright 2026 The WebRTC project authors. All Rights Reserved.
// Use of this source code is governed by a BSD-style license in LICENSE.
#include "playout_observation.h"
#include <array>
#include <cassert>
#include <thread>

int main()
{
    webrtc::PlayoutObserver observer;
    assert(!observer.Read().available);
    std::array<int16_t, 960> stereo{};
    stereo[959] = -30000;
    observer.Observe(stereo.data(), stereo.size(), 2, 48000, 960, 480, 100, true, false);
    auto voice = observer.Read();
    assert(voice.available && voice.submittedFrames == 1440 && voice.playedFrames == 480);
    assert(voice.lastNonSilentFrame == 1440 && voice.lastAudibleFrame == 1440);
    stereo.fill(0);
    observer.Observe(stereo.data(), stereo.size(), 2, 48000, 1440, 960, 110, true, false);
    auto buffered = observer.Read();
    assert(buffered.lastNonSilentFrame > buffered.playedFrames); // Not heard yet.
    observer.Observe(stereo.data(), stereo.size(), 2, 48000, 1920, 1440, 120, true, false);
    assert(observer.Read().lastNonSilentFrame <= observer.Read().playedFrames);
    stereo[0] = 200; // The silence floor is symmetric and excludes small noise.
    observer.Observe(stereo.data(), stereo.size(), 2, 48000, 2400, 1920, 130, true, false);
    assert(observer.Read().lastNonSilentFrame == 1440);
    stereo[0] = 1000;
    observer.Observe(stereo.data(), stereo.size(), 2, 48000, 2880, 2400, 140, true, true);
    assert(observer.Read().lastNonSilentFrame == 2881);
    assert(observer.Read().lastAudibleFrame == 1440 && observer.Read().muted);
    observer.Observe(stereo.data(), stereo.size(), 2, 48000, 2880, 2400, 150, false, false);
    assert(!observer.Read().available); // Missing OS clock cannot pass completion.
    observer.Observe(stereo.data(), stereo.size(), 2, 48000, 0, 0, 160, true, false);
    assert(observer.Read().lastNonSilentFrame == 1); // New renderer clock.
    observer.Invalidate();
    assert(!observer.Read().available);
    std::thread reader([&] { for (int i = 0; i < 10000; ++i) (void)observer.Read(); });
    for (int i = 0; i < 10000; ++i) observer.Observe(stereo.data(), 960, 2, 48000, i * 480, i * 480, i, true, false);
    reader.join();
}
