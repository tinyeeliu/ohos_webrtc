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

#include "default_video_encoder_factory.h"
#include "hardware_video_encoder_factory.h"
#include "software_video_encoder_factory.h"

#include "api/video_codecs/video_encoder_software_fallback_wrapper.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace adapter {

DefaultVideoEncoderFactory::DefaultVideoEncoderFactory(
    std::shared_ptr<EglContext> sharedEglContext, bool enableH264HighProfile)
    : DefaultVideoEncoderFactory(std::make_unique<HardwareVideoEncoderFactory>(sharedEglContext, enableH264HighProfile))
{
}

DefaultVideoEncoderFactory::DefaultVideoEncoderFactory(
    std::unique_ptr<VideoEncoderFactory> hardwareVideoEncoderFactory, CodecForceLists forceLists)
    : hardwareVideoEncoderFactory_(std::move(hardwareVideoEncoderFactory)),
      softwareVideoEncoderFactory_(std::make_unique<SoftwareVideoEncoderFactory>()), forceLists_(std::move(forceLists))
{
}

std::vector<SdpVideoFormat> DefaultVideoEncoderFactory::GetSupportedFormats() const
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto hardwareSupportedFormats = hardwareVideoEncoderFactory_->GetSupportedFormats();
    auto softwareSupportedFormats = softwareVideoEncoderFactory_->GetSupportedFormats();

    std::vector<SdpVideoFormat> supportedFormats;
    supportedFormats.insert(supportedFormats.end(), hardwareSupportedFormats.begin(), hardwareSupportedFormats.end());
    supportedFormats.insert(supportedFormats.end(), softwareSupportedFormats.begin(), softwareSupportedFormats.end());

    RTC_DLOG(LS_VERBOSE) << "Supported formats: ";
    for (auto& format : supportedFormats) {
        RTC_DLOG(LS_VERBOSE) << "\t format: " << format.ToString();
    }

    return supportedFormats;
}

std::unique_ptr<VideoEncoder> DefaultVideoEncoderFactory::CreateVideoEncoder(const SdpVideoFormat& format)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    // #9: force-HW codecs must not silently fall back to software; if the
    // hardware path cannot create them there is no encoder.
    if (forceLists_.IsForceHardware(format)) {
        return hardwareVideoEncoderFactory_->CreateVideoEncoder(format);
    }

    auto hardwareEncoder = hardwareVideoEncoderFactory_->CreateVideoEncoder(format);
    auto softwareEncoder = softwareVideoEncoderFactory_->CreateVideoEncoder(format);
    if (hardwareEncoder && softwareEncoder) {
        // Both hardware and software supported, wrap it in a software fallback
        return CreateVideoEncoderSoftwareFallbackWrapper(std::move(softwareEncoder), std::move(hardwareEncoder));
    }

    return hardwareEncoder ? std::move(hardwareEncoder) : std::move(softwareEncoder);
}

} // namespace adapter
} // namespace webrtc
