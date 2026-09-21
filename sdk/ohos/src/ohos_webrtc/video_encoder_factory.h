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

#ifndef WEBRTC_VIDEO_CODEC_VIDEO_ENCODER_FACTORY_H
#define WEBRTC_VIDEO_CODEC_VIDEO_ENCODER_FACTORY_H

#include "render/egl_context.h"
#include "utils/marcos.h"
#include "video_codec/codec_force_lists.h"

#include <napi.h>

#include <api/video_codecs/video_encoder_factory.h>

namespace webrtc {

class NapiHardwareVideoEncoderFactory : public Napi::ObjectWrap<NapiHardwareVideoEncoderFactory> {
public:
    NAPI_CLASS_NAME_DECLARE(HardwareVideoEncoderFactory);
    NAPI_ATTRIBUTE_NAME_DECLARE(SharedContext, sharedContext);
    NAPI_ATTRIBUTE_NAME_DECLARE(EnableH264HighProfile, enableH264HighProfile);
    NAPI_ATTRIBUTE_NAME_DECLARE(ForceSWCodecList, forceSWCodecList);
    NAPI_ATTRIBUTE_NAME_DECLARE(ForceHWCodecList, forceHWCodecList);
    NAPI_METHOD_NAME_DECLARE(ToJson, toJSON);
    NAPI_TYPE_TAG_DECLARE(0x0d9878cc5e534620, 0xb005829df9cc4eb5);

    static void Init(Napi::Env env, Napi::Object exports);

    ~NapiHardwareVideoEncoderFactory();

    std::shared_ptr<EglContext> GetSharedContext()
    {
        return sharedContext_;
    }
    bool GetEnableH264HighProfile() const
    {
        return enableH264HighProfile_;
    }
    const adapter::CodecForceLists& GetForceLists() const
    {
        return forceLists_;
    }
    bool HasForceLists() const
    {
        return forceListsSet_;
    }

protected:
    friend class ObjectWrap;

    explicit NapiHardwareVideoEncoderFactory(const Napi::CallbackInfo& info);

    // Parses the object-form options ({enableH264HighProfile?, sharedContext?,
    // forceSWCodecList?, forceHWCodecList?}); throws on malformed fields.
    void ApplyEncoderFactoryOptions(const Napi::Object& jsOptions);
    // Parses the two codec list options into forceLists_; throws on malformed
    // fields.
    void ApplyCodecForceListOptions(const Napi::Object& jsOptions);

    Napi::Value GetEnableH264HighProfile(const Napi::CallbackInfo& info);
    Napi::Value GetSharedContext(const Napi::CallbackInfo& info);
    Napi::Value ToJson(const Napi::CallbackInfo& info);

private:
    static thread_local Napi::FunctionReference constructor_;

    std::shared_ptr<EglContext> sharedContext_;
    bool enableH264HighProfile_{false};
    adapter::CodecForceLists forceLists_;
    bool forceListsSet_{false};
};

class NapiSoftwareVideoEncoderFactory : public Napi::ObjectWrap<NapiSoftwareVideoEncoderFactory> {
public:
    NAPI_CLASS_NAME_DECLARE(SoftwareVideoEncoderFactory);
    NAPI_METHOD_NAME_DECLARE(ToJson, toJSON);
    NAPI_TYPE_TAG_DECLARE(0x54d352bb27e3497a, 0xa7a147d57b7fce62);

    static void Init(Napi::Env env, Napi::Object exports);

    ~NapiSoftwareVideoEncoderFactory();

protected:
    friend class ObjectWrap;

    explicit NapiSoftwareVideoEncoderFactory(const Napi::CallbackInfo& info);
    Napi::Value ToJson(const Napi::CallbackInfo& info);

private:
    static thread_local Napi::FunctionReference constructor_;
};

std::unique_ptr<VideoEncoderFactory> CreateVideoEncoderFactory(const Napi::Object& jsVideoEncoderFactory);

std::unique_ptr<VideoEncoderFactory> CreateDefaultVideoEncoderFactory();

} // namespace webrtc

#endif // WEBRTC_VIDEO_CODEC_VIDEO_ENCODER_FACTORY_H
