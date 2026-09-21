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

#include "async_worker_get_stats.h"

namespace webrtc {

using namespace Napi;

const char kAttributeNameId[] = "id";
const char kAttributeNameType[] = "type";
const char kAttributeNameTimestamp[] = "timestamp";

class NapiMap : public Napi::Object {
public:
    NapiMap(napi_env env, napi_value value) : Object(env, value) {}

    static NapiMap Create(Napi::Env env)
    {
        return env.Global().Get("Map").As<Napi::Function>().New({}).As<NapiMap>();
    }

    void Set(Napi::Value key, Napi::Value value)
    {
        Get("set").As<Napi::Function>().Call(*this, {key, value});
    }
};

namespace {

// Scalar overloads instead of Napi::Value::From: std::vector<bool> yields a
// bit reference proxy that From cannot convert, so bool needs an exact match.
// Integer members go through the double overload (Numbers lose precision
// beyond 2^53).
Napi::Value ScalarToJs(Napi::Env env, bool value)
{
    return Napi::Boolean::New(env, value);
}

Napi::Value ScalarToJs(Napi::Env env, const std::string& value)
{
    return Napi::String::New(env, value);
}

Napi::Value ScalarToJs(Napi::Env env, double value)
{
    return Napi::Number::New(env, value);
}

Napi::Value ScalarToJs(Napi::Env env, int32_t value)
{
    return Napi::Number::New(env, value);
}

Napi::Value ScalarToJs(Napi::Env env, uint32_t value)
{
    return Napi::Number::New(env, value);
}

Napi::Value ScalarToJs(Napi::Env env, int64_t value)
{
    return Napi::Number::New(env, value);
}

Napi::Value ScalarToJs(Napi::Env env, uint64_t value)
{
    return Napi::Number::New(env, value);
}

// Converts a sequence member to a real Napi array.
template <typename T>
Napi::Value VectorToArray(Napi::Env env, const std::vector<T>& values)
{
    auto array = Napi::Array::New(env, values.size());
    for (size_t index = 0; index < values.size(); index++) {
        array[static_cast<uint32_t>(index)] = ScalarToJs(env, values[index]);
    }
    return array;
}

// Converts a map member to a real Napi object.
template <typename T>
Napi::Value MapToObject(Napi::Env env, const std::map<std::string, T>& values)
{
    auto object = Napi::Object::New(env);
    for (const auto& entry : values) {
        object.Set(entry.first, ScalarToJs(env, entry.second));
    }
    return object;
}

// Sequence/map members become real arrays/objects instead of JSON strings,
// consistent with the member conversion the sibling platform SDK performs
// in its jni stats collector.
Napi::Value StatsMemberToJs(Napi::Env env, const RTCStatsMemberInterface& member)
{
    switch (member.type()) {
        case RTCStatsMemberInterface::kSequenceBool:
            return VectorToArray(env, *member.cast_to<RTCStatsMember<std::vector<bool>>>());
        case RTCStatsMemberInterface::kSequenceInt32:
            return VectorToArray(env, *member.cast_to<RTCStatsMember<std::vector<int32_t>>>());
        case RTCStatsMemberInterface::kSequenceUint32:
            return VectorToArray(env, *member.cast_to<RTCStatsMember<std::vector<uint32_t>>>());
        case RTCStatsMemberInterface::kSequenceInt64:
            return VectorToArray(env, *member.cast_to<RTCStatsMember<std::vector<int64_t>>>());
        case RTCStatsMemberInterface::kSequenceUint64:
            return VectorToArray(env, *member.cast_to<RTCStatsMember<std::vector<uint64_t>>>());
        case RTCStatsMemberInterface::kSequenceDouble:
            return VectorToArray(env, *member.cast_to<RTCStatsMember<std::vector<double>>>());
        case RTCStatsMemberInterface::kSequenceString:
            return VectorToArray(env, *member.cast_to<RTCStatsMember<std::vector<std::string>>>());
        case RTCStatsMemberInterface::kMapStringUint64:
            return MapToObject(env, *member.cast_to<RTCStatsMember<std::map<std::string, uint64_t>>>());
        case RTCStatsMemberInterface::kMapStringDouble:
            return MapToObject(env, *member.cast_to<RTCStatsMember<std::map<std::string, double>>>());
        default:
            // Exotic member types still degrade to their JSON text.
            return Napi::String::New(env, member.ValueToJson());
    }
}

} // namespace

class GetStatsCallback : public RTCStatsCollectorCallback {
public:
    explicit GetStatsCallback(AsyncWorkerGetStats* asyncWorker) : asyncWorker_(asyncWorker) {}

protected:
    void OnStatsDelivered(const rtc::scoped_refptr<const RTCStatsReport>& report) override
    {
        if (!asyncWorker_) {
            return;
        }

        asyncWorker_->SetReport(report);
        asyncWorker_->Queue();
        asyncWorker_ = nullptr;
    }

private:
    AsyncWorkerGetStats* asyncWorker_;
};

AsyncWorkerGetStats* AsyncWorkerGetStats::Create(Napi::Env env, const char* resourceName)
{
    auto asyncWorker = new AsyncWorkerGetStats(env, resourceName);
    asyncWorker->callback_ = rtc::make_ref_counted<GetStatsCallback>(asyncWorker);
    return asyncWorker;
}

AsyncWorkerGetStats::AsyncWorkerGetStats(Napi::Env env, const char* resourceName)
    : AsyncWorker(env, resourceName), deferred_(Napi::Promise::Deferred::New(env)), callback_()
{
}

rtc::scoped_refptr<RTCStatsCollectorCallback> AsyncWorkerGetStats::GetCallback() const
{
    return callback_;
}

void AsyncWorkerGetStats::SetReport(const rtc::scoped_refptr<const RTCStatsReport>& report)
{
    report_ = report;
}

void AsyncWorkerGetStats::Execute()
{
    // do nothing
}

void AsyncWorkerGetStats::OnOK()
{
    auto jsStatsReportObj = Napi::Object::New(Env());
    auto jsStatsMap = NapiMap::Create(Env());

    if (report_) {
        for (auto it = report_->begin(); it != report_->end(); it++) {
            auto jsStats = Napi::Object::New(Env());
            jsStats.Set(kAttributeNameId, Napi::String::New(Env(), it->id()));
            jsStats.Set(kAttributeNameType, Napi::String::New(Env(), it->type()));
            jsStats.Set(kAttributeNameTimestamp, Napi::Number::New(Env(), it->timestamp().ms()));

            for (const auto& member : it->Members()) {
                if (!member || !member->is_defined()) {
                    continue;
                }

                switch (member->type()) {
                    case RTCStatsMemberInterface::kBool:
                        jsStats.Set(
                            member->name(), Napi::Boolean::New(Env(), *member->cast_to<RTCStatsMember<bool>>()));
                        break;
                    case RTCStatsMemberInterface::kInt32:
                        jsStats.Set(
                            member->name(), Napi::Number::New(Env(), *member->cast_to<RTCStatsMember<int32_t>>()));
                        break;
                    case RTCStatsMemberInterface::kUint32:
                        jsStats.Set(
                            member->name(), Napi::Number::New(Env(), *member->cast_to<RTCStatsMember<uint32_t>>()));
                        break;
                    case RTCStatsMemberInterface::kInt64:
                        jsStats.Set(
                            member->name(), Napi::Number::New(Env(), *member->cast_to<RTCStatsMember<int64_t>>()));
                        break;
                    case RTCStatsMemberInterface::kUint64:
                        jsStats.Set(
                            member->name(), Napi::Number::New(Env(), *member->cast_to<RTCStatsMember<uint64_t>>()));
                        break;
                    case RTCStatsMemberInterface::kDouble:
                        jsStats.Set(
                            member->name(), Napi::Number::New(Env(), *member->cast_to<RTCStatsMember<double>>()));
                        break;
                    case RTCStatsMemberInterface::kString:
                        jsStats.Set(member->name(), Napi::String::New(Env(), member->ValueToString()));
                        break;
                    default:
                        // Sequence/map members become real arrays/objects (#8).
                        jsStats.Set(member->name(), StatsMemberToJs(Env(), *member));
                        break;
                }
            }

            jsStatsMap.Set(Napi::String::New(Env(), it->id()), jsStats);
        }
    }

    jsStatsReportObj.Set("stats", jsStatsMap);
    deferred_.Resolve(jsStatsReportObj);
}

void AsyncWorkerGetStats::OnError(const Napi::Error& e)
{
    deferred_.Reject(e.Value());
}

} // namespace webrtc
