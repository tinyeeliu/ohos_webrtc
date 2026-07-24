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

#include "hilog/log.h"
#include "napi/native_api.h"

#include "napi.h"

#include <atomic>
#include <mutex>

#include "rtc_base/ssl_adapter.h"
#include "rtc_base/logging.h"
#include "logging/hilog_sink.h"

#include "certificate.h"
#include "data_channel.h"
#include "ice_candidate.h"
#include "media_source.h"
#include "media_stream.h"
#include "media_stream_track.h"
#include "peer_connection.h"
#include "peer_connection_factory.h"
#include "rtp_receiver.h"
#include "rtp_sender.h"
#include "rtp_transceiver.h"
#include "sctp_transport.h"
#include "session_description.h"
#include "dtls_transport.h"
#include "dtmf_sender.h"
#include "ice_transport.h"
#include "media_devices.h"
#include "video_encoder_factory.h"
#include "video_decoder_factory.h"
#include "audio_processing_factory.h"
#include "audio_device/ohos_audio_device_module.h"
#include "render/native_video_renderer.h"
#include "logging/native_logging.h"

using namespace Napi;
using namespace webrtc;

// SSL 是进程级全局资源:多个 napi_env(RNOH 主线程引擎 + Worker 引擎)会各调一次
// 模块 Init。若每个 env 都 InitializeSSL/CleanupSSL,某个 env 销毁时的 CleanupSSL 会
// 破坏仍在使用 SSL 的另一个 env。用引用计数保证「首个 env 初始化、最后一个 env 才清理」。
static std::atomic<int> g_sslRefCount{0};

struct SSLInitializer {
    SSLInitializer()
    {
        if (g_sslRefCount.fetch_add(1) == 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "napi_module", "InitializeSSL");
            RTC_CHECK(rtc::InitializeSSL()) << "Failed to InitializeSSL()";
        }
    }

    ~SSLInitializer()
    {
        if (g_sslRefCount.fetch_sub(1) == 1) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "napi_module", "CleanupSSL");
            RTC_CHECK(rtc::CleanupSSL()) << "Failed to CleanupSSL()";
        }
    }
};

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "napi_module", "Init");

    static std::once_flag g_hilogSinkOnce;
    std::call_once(g_hilogSinkOnce, []() {
        rtc::LogMessage::AddLogToStream(new HilogSink(), rtc::LS_INFO);
    });

    Env e(env);
    Object exp(env, exports);

    NapiPeerConnectionFactory::Init(e, exp);
    NapiPeerConnection::Init(e, exp);
    NapiIceCandidate::Init(e, exp);
    NapiSessionDescription::Init(e, exp);
    NapiRtpSender::Init(e, exp);
    NapiRtpReceiver::Init(e, exp);
    NapiRtpTransceiver::Init(e, exp);
    NapiSctpTransport::Init(e, exp);
    NapiCertificate::Init(e, exp);
    NapiAudioSource::Init(e, exp);
    NapiVideoSource::Init(e, exp);
    NapiDataChannel::Init(e, exp);
    NapiMediaStream::Init(e, exp);
    NapiMediaStreamTrack::Init(e, exp);
    NapiNativeLogging::Init(e, exp);
    NapiAudioDeviceModule::Init(e, exp);
    NapiDtlsTransport::Init(e, exp);
    NapiDtmfSender::Init(e, exp);
    NapiIceTransport::Init(e, exp);
    NapiNativeVideoRenderer::Init(e, exp);
    NapiMediaDevices::Init(e, exp);
    NapiHardwareVideoEncoderFactory::Init(e, exp);
    NapiHardwareVideoDecoderFactory::Init(e, exp);
    NapiSoftwareVideoEncoderFactory::Init(e, exp);
    NapiSoftwareVideoDecoderFactory::Init(e, exp);
    NapiAudioProcessing::Init(e, exp);
    NapiAudioProcessingFactory::Init(e, exp);

    auto sslInitializer = new SSLInitializer;
    exp.AddFinalizer([](Napi::Env, SSLInitializer* ssl) { delete ssl; }, sslInitializer);

    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "ohos_webrtc",
    .nm_priv = ((void*)0),
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&demoModule);
}
