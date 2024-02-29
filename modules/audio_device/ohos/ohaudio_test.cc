/*
# Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include <algorithm>
#include <array>
#include <memory>
#include <utility>
#include <cstdio>
#include <cerrno>

#include "modules/audio_device/include/test_audio_device.h"
#include "modules/audio_device/ohos/audio_device_module.h"
#include "modules/audio_device/include/audio_device_defines.h"

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "common_audio/wav_file.h"
#include "common_audio/wav_header.h"
#include "modules/audio_device/include/audio_device_defines.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/time_controller/simulated_time_controller.h"
#include "common_audio/resampler/include/resampler.h"

using namespace webrtc;

#define SAMPLE_RATE         48000
#define USLEEP_1000MS       1000
#define SAMPLES_PER_10MS    (SAMPLE_RATE / 100)

#define TEST_PLAY           0
#define TEST_RECORD         1
#define TEST_PLAY_RECORD    2

static int g_testMode = TEST_PLAY;
static int g_playing = 0;

namespace {

class ChunkBuffer {
private:
    int chunkCount = 0;
    int chunkSize = 0;
    int used = 0;
    int first = 0;
    char *buf = nullptr;

public:
    ChunkBuffer(int chunkCount, int chunkSize)
    {
        this->chunkCount = chunkCount;
        this->chunkSize = chunkSize;
        used = 0;
        first = 0;
    }

    ~ChunkBuffer()
    {
        if (buf != nullptr) {
            free(buf);
        }
    }

    int CreateBuff()
    {
        if (chunkCount <= 0 && chunkSize <= 0) {
            return -1;
        }
        buf = reinterpret_cast<char *>(malloc(chunkCount * chunkSize));
        return buf != nullptr ? 0 : -1;
    }

    void *Push()
    {
        if (used >= chunkCount) {
            return nullptr;
        }
        int available = (first + used) % chunkCount;
        used++;
        return buf + available * chunkSize;
    }

    void *Pop()
    {
        if (used <= 0) {
            return nullptr;
        }
        void *ret = buf + first * chunkSize;
        used--;
        first = (++first) % chunkCount;
        return ret;
    }
};

class OHOSTestAudioTransport : public AudioTransport {
public:
    enum class Mode {
        kPlaying,
        kRecording
    };
    explicit OHOSTestAudioTransport(FILE *recFile, FILE *plyFile) : recFile_(recFile), plyFile_(plyFile)
    {
        buffer = new ChunkBuffer(buffersize, SAMPLES_PER_10MS * sizeof(int16_t));
        buffer->CreateBuff();
    }

    ~OHOSTestAudioTransport()
    {
        delete buffer;
    }

    int32_t RecordedDataIsAvailable(
        const void* audioSamples,
        size_t samplesPerChannel,
        size_t bytesPerSample,
        size_t numberOfChannels,
        uint32_t samplesPerSecond,
        uint32_t totalDelay,
        int32_t clockDrift,
        uint32_t currentMicLevel,
        bool keyPressed,
        uint32_t& newMicLevel,
        absl::optional<int64_t> estimatedCaptureTime) override
    {
        newMicLevel = 1;
        RTC_LOG(LS_INFO) << "" << __FUNCTION__;
        MutexLock lock(&mutex_);
        RTC_LOG(LS_INFO) << "samplesPerChannel=" << samplesPerChannel;
        RTC_LOG(LS_INFO) << "numberOfChannels=" << numberOfChannels;
        RTC_LOG(LS_INFO) << "bytesPerSample=" << bytesPerSample;
        RTC_LOG(LS_INFO) << "samplesPerSecond=" << samplesPerSecond;
        RTC_LOG(LS_INFO) << "totalDelay_=" << totalDelay;
        RTC_LOG(LS_INFO) << "clockDrift_=" << clockDrift;

        if (g_testMode == TEST_RECORD) {
            FILE *file = const_cast<FILE *>(recFile_);
            if (file == nullptr) {
                RTC_LOG(LS_ERROR)  << "file handle is invalid!";
                return -1;
            }
            fwrite(audioSamples, sizeof(int16_t), samplesPerChannel * numberOfChannels, file);
        } else if (g_testMode == TEST_PLAY_RECORD) {
            RTC_LOG(LS_INFO) << "" << __FUNCTION__;
            void *samples = buffer->Push();
            if (samples == nullptr) {
                buffer->Pop();
                samples = buffer->Push();
                return 0;
            }
            memcpy(samples, audioSamples, samplesPerChannel * numberOfChannels * sizeof(int16_t));
            RTC_LOG(LS_INFO) << "dsize=" << (int)(samplesPerChannel * numberOfChannels * sizeof(int16_t));
            RTC_LOG(LS_INFO) << "" << __FUNCTION__;
        }
        return 0;
    }

    int32_t NeedMorePlayData(size_t samplesPerChannel,
                           size_t bytesPerSample,
                           size_t numberOfChannels,
                           uint32_t samplesPerSecond,
                           void* audioSamples,
                           size_t& samplesOut,
                           int64_t* elapsedTime,
                           int64_t* ntpTime) override
    {
        const size_t numBytes = samplesPerChannel * numberOfChannels;
        // std::memset(audioSamples, 1, numBytes);
        MutexLock lock(&mutex_);
        samplesOut = numBytes;
        *elapsedTime = 0;
        *ntpTime = 0;
        RTC_LOG(LS_INFO) << "samplesPerChannel=" << samplesPerChannel;
        RTC_LOG(LS_INFO) << "numberOfChannels=" << numberOfChannels;
        RTC_LOG(LS_INFO) << "bytesPerSample=" << bytesPerSample;
        RTC_LOG(LS_INFO) << "samplesPerSecond=" << samplesPerSecond;
        RTC_LOG(LS_INFO) << "dsize=" << numBytes;

        if (g_testMode == TEST_PLAY_RECORD) {
            RTC_LOG(LS_INFO) << "" << __FUNCTION__;
            void *samples = buffer->Pop();
            if (samples == nullptr) {
                RTC_LOG(LS_INFO) <<"no data for playout";
                return 0;
            }
            memcpy(audioSamples, samples, numBytes * sizeof(int16_t));
        } else if (g_testMode == TEST_PLAY) {
            FILE *file = const_cast<FILE *>(plyFile_);
            if (file == nullptr) {
                RTC_LOG(LS_ERROR)  << "file handle is invalid!";
                g_playing = 0;
                return -1;
            }
            if (fread(audioSamples, sizeof(int16_t), samplesPerChannel * numberOfChannels, file) <= 0) {
                g_playing = 0;
                return -1;
            }
        }

        RTC_LOG(LS_INFO) << "" << __FUNCTION__;
        return 0;
    }

    int32_t RecordedDataIsAvailable(const void* audioSamples,
                                  size_t samplesPerChannel,
                                  size_t bytesPerSample,
                                  size_t numberOfChannels,
                                  uint32_t samplesPerSecond,
                                  uint32_t totalDelay,
                                  int32_t clockDrift,
                                  uint32_t currentMicLevel,
                                  bool keyPressed,
                                  uint32_t& newMicLevel) override
    {
        RTC_CHECK(false) << "This methods should be never executed";
    }

    void PullRenderData(int bits_per_sample,
                      int sample_rate,
                      size_t numberOfChannels,
                      size_t number_of_frames,
                      void* audio_data,
                      int64_t* elapsedTime,
                      int64_t* ntpTime) override
    {
        RTC_CHECK(false) << "This methods should be never executed";
    }

private:
    const FILE *recFile_ = nullptr;
    const FILE *plyFile_ = nullptr;
    mutable Mutex mutex_;
    ChunkBuffer *buffer = nullptr;

    static const int buffersize = 10000;
};
}

int main(int argc, char **argv)
{
    static const int channles = 2;
    static const int amplitudes = 1000;
    static const int delayTims = 1000;
    if (argc > 1) {
        g_testMode = atoi(argv[1]);
    }

    RTC_LOG(LS_INFO) << "current test mode is " << g_testMode;

    FILE *fileRead = nullptr;
    FILE *fileWrite = nullptr;

    std::unique_ptr<TestAudioDeviceModule::Renderer> renderer = nullptr;
    std::unique_ptr<TestAudioDeviceModule::PulsedNoiseCapturer> capturer = nullptr;
    GlobalSimulatedTimeController time_controller(Timestamp::Millis(delayTims));

    if (g_testMode == TEST_RECORD) {
        fileWrite = fopen("/data/recorder.raw", "w+");
    } else if (g_testMode == TEST_PLAY) {
        fileRead = fopen("/data/resources/near48_stereo.pcm", "rb");
    }
    OHOSTestAudioTransport audioTransport(fileWrite, fileRead);
    
    if (g_testMode != TEST_RECORD) {
        renderer = TestAudioDeviceModule::CreateDiscardRenderer(SAMPLE_RATE, channles);
    }
    if (g_testMode != TEST_PLAY) {
        capturer = TestAudioDeviceModule::CreatePulsedNoiseCapturer(amplitudes, SAMPLE_RATE, channles);
    }
    RTC_LOG(LS_INFO) << "CreatedADMCanRecord";

    rtc::scoped_refptr<AudioDeviceModule> adm = TestAudioDeviceModule::Create(
        time_controller.GetTaskQueueFactory(), std::move(capturer), std::move(renderer));

    if (adm->RegisterAudioCallback(&audioTransport) != 0) {
        RTC_LOG(LS_INFO) << "RegisterAudioCallback failed!";
        return -1;
    }
    RTC_LOG(LS_INFO) << "Init";
    if (adm->Init() != 0) {
        RTC_LOG(LS_INFO) << "Init failed!";
        return -1;
    }

    if (g_testMode != TEST_PLAY) {
        RTC_LOG(LS_INFO) << "InitRecording";
        if (adm->InitRecording() != 0) {
            RTC_LOG(LS_INFO) << "InitRecording failed!";
            return -1;
        }
        RTC_LOG(LS_INFO) << "RecordingIsInitialized";
        if (!adm->RecordingIsInitialized()) {
            RTC_LOG(LS_INFO) << "RecordingIsInitialized failed!";
            return -1;
        }
        RTC_LOG(LS_INFO) << "StartRecording";
        if (adm->StartRecording() != 0) {
            RTC_LOG(LS_INFO) << "StartRecording failed!";
            return -1;
        }
    }

    if (g_testMode != TEST_RECORD) {
        RTC_LOG(LS_INFO) << "InitPlayout";
        if (adm->InitPlayout() != 0) {
            RTC_LOG(LS_INFO) << "InitPlayout failed!";
            return -1;
        }
        RTC_LOG(LS_INFO) << "PlayoutIsInitialized";
        if (!adm->PlayoutIsInitialized()) {
            RTC_LOG(LS_INFO) << "PlayoutIsInitialized failed!";
            return -1;
        }

        RTC_LOG(LS_INFO) << "StartPlayout";
        if (adm->StartPlayout() != 0) {
            RTC_LOG(LS_INFO) << "StartPlayout failed!";
            return -1;
        }
    }

    int count = 20;
    if (g_testMode == TEST_PLAY) {
        g_playing = 1;
        count = 1;
    }
    while (count > 0) {
        time_controller.AdvanceTime(TimeDelta::Millis(delayTims));
        if (!adm->Recording()) {
            RTC_LOG(LS_INFO) << "not Recording!";
        } else {
            RTC_LOG(LS_INFO) << "is Recording!";
        }
        if (!adm->Playing()) {
            RTC_LOG(LS_INFO) << "not Playing!";
        } else {
            RTC_LOG(LS_INFO) << "is Playing!";
        }
        sleep(1);
        if (g_playing == 0) {
            count--;
        }
    }
    time_controller.AdvanceTime(TimeDelta::Millis(delayTims));

    if (g_testMode != TEST_PLAY) {
        RTC_LOG(LS_INFO) << "StopRecording";
        if (adm->StopRecording() != 0) {
            RTC_LOG(LS_INFO) << "StopRecording failed!";
            return -1;
        }
    }

    if (g_testMode != TEST_RECORD) {
        RTC_LOG(LS_INFO) << "StopPlayout";
        if (adm->StopPlayout() != 0) {
            RTC_LOG(LS_INFO) << "StopPlayout failed!";
            return -1;
        }
    }

    RTC_LOG(LS_INFO) << "fclose";
    if (fileRead != nullptr) {
        fclose(fileRead);
        fileRead = nullptr;
    }
    if (fileWrite != nullptr) {
        fclose(fileWrite);
        fileWrite = nullptr;
    }

    return 0;
}
