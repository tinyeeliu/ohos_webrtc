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

#include "media_track_constraints.h"

#include <map>
#include <sstream>
#include <vector>

#include "rtc_base/strings/string_builder.h"
#include "rtc_base/logging.h"

namespace webrtc {

using namespace Napi;

// A naked value is treated as an "ideal" value in the basic constraints,
// but as an exact value in "advanced" constraints.
// https://www.w3.org/TR/mediacapture-streams/#constrainable-interface
enum class NakedValueDisposition { kTreatAsIdeal, kTreatAsExact };

const size_t kMaxConstraintStringLength = 500;
const size_t kMaxConstraintStringSeqLength = 100;

const char kConstraintsMin[] = "min";
const char kConstraintsMax[] = "max";
const char kConstraintsExact[] = "exact";
const char kConstraintsIdeal[] = "ideal";
const char kConstraintsAdvanced[] = "advanced";

const std::map<std::string, bool> SUPPORTED_CONSTRAINTS_MAP = {
    {NapiMediaConstraints::kAttributeNameWidth, true},
    {NapiMediaConstraints::kAttributeNameHeight, true},
    {NapiMediaConstraints::kAttributeNameAspectRatio, true},
    {NapiMediaConstraints::kAttributeNameFrameRate, true},
    {NapiMediaConstraints::kAttributeNameFacingMode, true},
    {NapiMediaConstraints::kAttributeNameResizeMode, false},
    {NapiMediaConstraints::kAttributeNameSampleRate, false},
    {NapiMediaConstraints::kAttributeNameSampleSize, false},
    {NapiMediaConstraints::kAttributeNameEchoCancellation, true},
    {NapiMediaConstraints::kAttributeNameAutoGainControl, true},
    {NapiMediaConstraints::kAttributeNameNoiseSuppression, true},
    {NapiMediaConstraints::kAttributeNameLatency, false},
    {NapiMediaConstraints::kAttributeNameChannelCount, false},
    {NapiMediaConstraints::kAttributeNameDeviceId, true},
    {NapiMediaConstraints::kAttributeNameGroupId, true},
    {NapiMediaConstraints::kAttributeNameDisplaySurface, false},
    {NapiMediaConstraints::kAttributeNameBackgroundBlur, false},
    {NapiMediaConstraints::kAttributeNameGoogEchoCancellation, false},
    {NapiMediaConstraints::kAttributeNameGoogAutoGainControl, false},
    {NapiMediaConstraints::kAttributeNameGoogNoiseSuppression, false},
    {NapiMediaConstraints::kAttributeNameGoogHighpassFilter, false},
    {NapiMediaConstraints::kAttributeNameGoogAudioMirroring, false},
};

void CopyLongConstraint(const Napi::Value& jsValue, NakedValueDisposition naked_treatment, LongConstraint& nativeValue)
{
    if (jsValue.IsNumber()) {
        switch (naked_treatment) {
            case NakedValueDisposition::kTreatAsIdeal:
                nativeValue.SetIdeal(jsValue.As<Number>().Int32Value());
                break;
            case NakedValueDisposition::kTreatAsExact:
                nativeValue.SetExact(jsValue.As<Number>().Int32Value());
                break;
        }
    } else if (jsValue.IsObject()) {
        auto jsConstrainULongRange = jsValue.As<Object>();
        if (jsConstrainULongRange.Has(kConstraintsMin)) {
            nativeValue.SetMin(jsConstrainULongRange.Get(kConstraintsMin).As<Number>().Int32Value());
        }
        if (jsConstrainULongRange.Has(kConstraintsMax)) {
            nativeValue.SetMax(jsConstrainULongRange.Get(kConstraintsMax).As<Number>().Int32Value());
        }
        if (jsConstrainULongRange.Has(kConstraintsIdeal)) {
            nativeValue.SetIdeal(jsConstrainULongRange.Get(kConstraintsIdeal).As<Number>().Int32Value());
        }
        if (jsConstrainULongRange.Has(kConstraintsExact)) {
            nativeValue.SetExact(jsConstrainULongRange.Get(kConstraintsExact).As<Number>().Int32Value());
        }
    }
}

void CopyDoubleConstraint(
    const Napi::Value& jsValue, NakedValueDisposition naked_treatment, DoubleConstraint& nativeValue)
{
    // nativeValue.SetIsPresent(true);
    if (jsValue.IsNumber()) {
        switch (naked_treatment) {
            case NakedValueDisposition::kTreatAsIdeal:
                nativeValue.SetIdeal(jsValue.As<Number>().DoubleValue());
                break;
            case NakedValueDisposition::kTreatAsExact:
                nativeValue.SetExact(jsValue.As<Number>().DoubleValue());
                break;
        }
    } else if (jsValue.IsObject()) {
        auto jsConstrainDoubleRange = jsValue.As<Object>();
        if (jsConstrainDoubleRange.Has(kConstraintsMin)) {
            nativeValue.SetMin(jsConstrainDoubleRange.Get(kConstraintsMin).As<Number>().DoubleValue());
        }
        if (jsConstrainDoubleRange.Has(kConstraintsMax)) {
            nativeValue.SetMax(jsConstrainDoubleRange.Get(kConstraintsMax).As<Number>().DoubleValue());
        }
        if (jsConstrainDoubleRange.Has(kConstraintsIdeal)) {
            nativeValue.SetIdeal(jsConstrainDoubleRange.Get(kConstraintsIdeal).As<Number>().DoubleValue());
        }
        if (jsConstrainDoubleRange.Has(kConstraintsExact)) {
            nativeValue.SetExact(jsConstrainDoubleRange.Get(kConstraintsExact).As<Number>().DoubleValue());
        }
    }
}

bool ValidateString(const std::string& str, std::string& error_message)
{
    if (str.length() > kMaxConstraintStringLength) {
        error_message = "Constraint string too long.";
        return false;
    }
    return true;
}

bool ValidateStringSeq(const std::vector<std::string>& strs, std::string& error_message)
{
    if (strs.size() > kMaxConstraintStringSeqLength) {
        error_message = "Constraint string sequence too long.";
        return false;
    }

    for (const std::string& str : strs) {
        if (!ValidateString(str, error_message)) {
            return false;
        }
    }

    return true;
}

bool ValidateStringConstraint(const Value& jsValue, std::string& error_message)
{
    if (jsValue.IsString()) {
        return ValidateString(jsValue.As<String>().Utf8Value(), error_message);
    } else if (jsValue.IsArray()) {
        auto jsArray = jsValue.As<Array>();
        if (jsArray.Length() > kMaxConstraintStringSeqLength) {
            error_message = "Constraint string sequence too long.";
            return false;
        }

        for (uint32_t index = 0; index < jsArray.Length(); index++) {
            if (!ValidateString(jsValue.As<String>().Utf8Value(), error_message)) {
                return false;
            }
        }
        return true;
    } else if (jsValue.IsObject()) {
        auto jsObject = jsValue.As<Object>();
        if (jsObject.Has(kConstraintsIdeal) && !ValidateStringConstraint(jsObject.Get(kConstraintsIdeal).As<String>(), error_message)) {
            return false;
        }
        if (jsObject.Has(kConstraintsExact) && !ValidateStringConstraint(jsObject.Get(kConstraintsExact).As<String>(), error_message)) {
            return false;
        }
        return true;
    }

    return false;
}

bool ValidateAndCopyStringConstraint(
    const Value& jsValue, NakedValueDisposition naked_treatment, StringConstraint& nativeValue,
    std::string& error_message)
{
    if (!ValidateStringConstraint(jsValue, error_message)) {
        return false;
    }

    // web_form.SetIsPresent(true);
    if (jsValue.IsString()) {
        switch (naked_treatment) {
            case NakedValueDisposition::kTreatAsIdeal:
                nativeValue.SetIdeal(std::vector<std::string>(1, jsValue.As<String>().Utf8Value()));
                break;
            case NakedValueDisposition::kTreatAsExact:
                nativeValue.SetExact(std::vector<std::string>(1, jsValue.As<String>().Utf8Value()));
                break;
        }
    } else if (jsValue.IsArray()) {
        std::vector<std::string> nativeArray;
        auto jsArray = jsValue.As<Array>();
        for (uint32_t index = 0; index < jsArray.Length(); index++) {
            nativeArray.push_back(((Napi::Value)jsArray[index]).As<String>().Utf8Value());
        }

        switch (naked_treatment) {
            case NakedValueDisposition::kTreatAsIdeal:
                nativeValue.SetIdeal(nativeArray);
                break;
            case NakedValueDisposition::kTreatAsExact:
                nativeValue.SetExact(nativeArray);
                break;
        }
    } else if (jsValue.IsObject()) {
        auto jsObject = jsValue.As<Object>();
        if (jsObject.Has(kConstraintsIdeal)) {
            auto jsIdeal = jsObject.Get(kConstraintsIdeal);
            if (jsIdeal.IsString()) {
                nativeValue.SetIdeal(std::vector<std::string>(1, jsValue.As<String>().Utf8Value()));
            } else if (jsIdeal.IsArray()) {
                std::vector<std::string> nativeArray;
                auto jsArray = jsValue.As<Array>();
                for (uint32_t index = 0; index < jsArray.Length(); index++) {
                    nativeArray.push_back(((Napi::Value)jsArray[index]).As<String>().Utf8Value());
                }
                nativeValue.SetIdeal(nativeArray);
            }
        }
        if (jsObject.Has(kConstraintsExact)) {
            auto jsIdeal = jsObject.Get(kConstraintsExact);
            if (jsIdeal.IsString()) {
                nativeValue.SetExact(std::vector<std::string>(1, jsValue.As<String>().Utf8Value()));
            } else if (jsIdeal.IsArray()) {
                std::vector<std::string> nativeArray;
                auto jsArray = jsValue.As<Array>();
                for (uint32_t index = 0; index < jsArray.Length(); index++) {
                    nativeArray.push_back(((Napi::Value)jsArray[index]).As<String>().Utf8Value());
                }
                nativeValue.SetExact(nativeArray);
            }
        }
    }
    return true;
}

void CopyBooleanConstraint(
    const Napi::Value& jsValue, NakedValueDisposition naked_treatment, BooleanConstraint& nativeValue)
{
    if (jsValue.IsBoolean()) {
        switch (naked_treatment) {
            case NakedValueDisposition::kTreatAsIdeal:
                nativeValue.SetIdeal(jsValue.As<Boolean>());
                break;
            case NakedValueDisposition::kTreatAsExact:
                nativeValue.SetExact(jsValue.As<Boolean>());
                break;
        }
    } else if (jsValue.IsObject()) {
        auto jsObject = jsValue.As<Object>();
        if (jsObject.Has(kConstraintsIdeal)) {
            nativeValue.SetIdeal(jsObject.Get(kConstraintsIdeal).As<Boolean>());
        }
        if (jsObject.Has(kConstraintsExact)) {
            nativeValue.SetIdeal(jsObject.Get(kConstraintsExact).As<Boolean>());
        }
    }
}

bool ValidateAndCopyConstraintSet(
    const Object jsTrackConstraints, NakedValueDisposition naked_treatment, MediaTrackConstraintSet& trackConstraints,
    std::string& error_message)
{
    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameWidth))
    {
        CopyLongConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameWidth), naked_treatment, trackConstraints.width);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameHeight))
    {
        CopyLongConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameHeight), naked_treatment,
            trackConstraints.height);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameAspectRatio))
    {
        CopyDoubleConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameAspectRatio), naked_treatment,
            trackConstraints.aspectRatio);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameFrameRate))
    {
        CopyDoubleConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameFrameRate), naked_treatment,
            trackConstraints.frameRate);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameFacingMode))
    {
        if (!ValidateAndCopyStringConstraint(
                jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameFacingMode), naked_treatment,
                trackConstraints.facingMode, error_message))
        {
            return false;
        }
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameResizeMode))
    {
        if (!ValidateAndCopyStringConstraint(
                jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameResizeMode), naked_treatment,
                trackConstraints.resizeMode, error_message))
        {
            return false;
        }
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameSampleRate))
    {
        CopyLongConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameSampleRate), naked_treatment,
            trackConstraints.sampleRate);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameSampleSize))
    {
        CopyLongConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameSampleSize), naked_treatment,
            trackConstraints.sampleSize);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameEchoCancellation))
    {
        CopyBooleanConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameEchoCancellation), naked_treatment,
            trackConstraints.echoCancellation);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameAutoGainControl))
    {
        CopyBooleanConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameAutoGainControl), naked_treatment,
            trackConstraints.autoGainControl);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameNoiseSuppression))
    {
        CopyBooleanConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameNoiseSuppression), naked_treatment,
            trackConstraints.noiseSuppression);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameLatency))
    {
        CopyDoubleConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameLatency), naked_treatment,
            trackConstraints.latency);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameChannelCount))
    {
        CopyLongConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameChannelCount), naked_treatment,
            trackConstraints.channelCount);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameDeviceId))
    {
        if (!ValidateAndCopyStringConstraint(
                jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameDeviceId), naked_treatment,
                trackConstraints.deviceId, error_message))
        {
            return false;
        }
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameGroupId))
    {
        if (!ValidateAndCopyStringConstraint(
                jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameGroupId), naked_treatment,
                trackConstraints.groupId, error_message))
        {
            return false;
        }
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameBackgroundBlur))
    {
        CopyBooleanConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameBackgroundBlur), naked_treatment,
            trackConstraints.backgroundBlur);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameDisplaySurface))
    {
        if (!ValidateAndCopyStringConstraint(
                jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameDisplaySurface), naked_treatment,
                trackConstraints.displaySurface, error_message))
        {
            return false;
        }
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameGoogEchoCancellation)) {
        CopyBooleanConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameGoogEchoCancellation), naked_treatment,
            trackConstraints.googEchoCancellation);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameGoogAutoGainControl)) {
        CopyBooleanConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameGoogAutoGainControl), naked_treatment,
            trackConstraints.googAutoGainControl);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameGoogNoiseSuppression)) {
        CopyBooleanConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameGoogNoiseSuppression), naked_treatment,
            trackConstraints.googNoiseSuppression);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameGoogHighpassFilter)) {
        CopyBooleanConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameGoogHighpassFilter), naked_treatment,
            trackConstraints.googHighpassFilter);
    }

    if (jsTrackConstraints.Has(NapiMediaConstraints::kAttributeNameGoogAudioMirroring)) {
        CopyBooleanConstraint(
            jsTrackConstraints.Get(NapiMediaConstraints::kAttributeNameGoogAudioMirroring), naked_treatment,
            trackConstraints.googAudioMirroring);
    }

    return true;
}

MediaTrackConstraints ParseTrackConstraints(const Napi::Object& jsTrackConstraints, std::string& error_message)
{
    MediaTrackConstraintSet basic;
    if (!ValidateAndCopyConstraintSet(jsTrackConstraints, NakedValueDisposition::kTreatAsIdeal, basic, error_message)) {
        RTC_LOG(LS_ERROR) << "Failed to parse track constraints: " << error_message;
        return MediaTrackConstraints();
    }
    
    std::vector<MediaTrackConstraintSet> advanced;
    if (jsTrackConstraints.Has(kConstraintsAdvanced)) {
        MediaTrackConstraintSet advancedElement;
        auto jsArray = jsTrackConstraints.Get(kConstraintsAdvanced).As<Array>();
        for (uint32_t index = 0; index < jsArray.Length(); index++) {
            auto jsElement = (Napi::Value)jsArray[index];
            if (!ValidateAndCopyConstraintSet(
                    jsElement.As<Object>(), NakedValueDisposition::kTreatAsExact, advancedElement, error_message))
            {
                RTC_LOG(LS_ERROR) << "Failed to parse track constraints: " << error_message;
                return MediaTrackConstraints();
            }
            advanced.push_back(advancedElement);
        }
    }
    
    MediaTrackConstraints constraints;
    constraints.Initialize(basic, advanced);
    
    return constraints;
}

std::vector<std::string> NapiMediaConstraints::GetSupportedConstraints()
{
    std::vector<std::string> result;
    for (const auto& constraints : SUPPORTED_CONSTRAINTS_MAP) {
        if (constraints.second) {
            result.push_back(constraints.first);
        }
    }
    return result;
}

bool NapiMediaConstraints::IsConstraintSupported(const std::string& name)
{
    auto it = SUPPORTED_CONSTRAINTS_MAP.find(name);
    if (it != SUPPORTED_CONSTRAINTS_MAP.end()) {
        return it->second;
    }
    return true;
}

void NapiMediaConstraints::JsToNative(
    const Napi::Value& jsTrackConstraints, MediaTrackConstraints& nativeTrackConstraints)
{
    if (jsTrackConstraints.IsBoolean()) {
        if (jsTrackConstraints.As<Boolean>()) {
            MediaTrackConstraints constraints;
            constraints.Initialize();
            nativeTrackConstraints = constraints;
        } else {
            nativeTrackConstraints = MediaTrackConstraints();
        }
        return;
    } else if (jsTrackConstraints.IsObject()) {
        std::string error_message;
        auto constraints = ParseTrackConstraints(jsTrackConstraints.As<Object>(), error_message);
        if (constraints.IsNull()) {
            NAPI_THROW_VOID(TypeError::New(jsTrackConstraints.Env(), error_message));
        }
        nativeTrackConstraints = constraints;
        return;
    }

    nativeTrackConstraints = MediaTrackConstraints();
}

} // namespace webrtc
