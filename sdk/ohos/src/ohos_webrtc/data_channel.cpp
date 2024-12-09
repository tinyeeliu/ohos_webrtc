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

#include "data_channel.h"
#include "utils/marcos.h"

#include "rtc_base/logging.h"

namespace webrtc {

using namespace Napi;

const char kEnumBinaryTypeBlob[] = "blob";
const char kEnumBinaryTypeArrayBuffer[] = "arraybuffer";

const char kEnumDataChannelStateClosed[] = "closed";
const char kEnumDataChannelStateClosing[] = "closing";
const char kEnumDataChannelStateConnecting[] = "connecting";
const char kEnumDataChannelStateOpen[] = "open";

const char kClassName[] = "RTCDataChannel";

const char kAttributeNameLabel[] = "label";
const char kAttributeNameOrdered[] = "ordered";
const char kAttributeNameMaxPacketLifeTime[] = "maxPacketLifeTime";
const char kAttributeNameMaxRetransmits[] = "maxRetransmits";
const char kAttributeNameProtocol[] = "protocol";
const char kAttributeNameNegotiated[] = "negotiated";
const char kAttributeNameId[] = "id";
const char kAttributeNameReadyState[] = "readyState";
const char kAttributeNameBufferedAmount[] = "bufferedAmount";
const char kAttributeNameBufferedAmountLowThreshold[] = "bufferedAmountLowThreshold";
const char kAttributeNameBinaryType[] = "binaryType";
const char kAttributeNameOnBufferedAmountLow[] = "onbufferedamountlow";
const char kAttributeNameOnClose[] = "onclose";
const char kAttributeNameOnClosing[] = "onclosing";
const char kAttributeNameOnOpen[] = "onopen";
const char kAttributeNameOnMessage[] = "onmessage";
const char kAttributeNameOnError[] = "onerror";

const char kMethodNameClose[] = "close";
const char kMethodNameSend[] = "send";
const char kMethodNameToJson[] = "toJSON";

const char kEventNameBufferedAmountLow[] = "bufferedamountlow";
const char kEventNameClose[] = "close";
const char kEventNameClosing[] = "closing";
const char kEventNameOpen[] = "open";
const char kEventNameMessage[] = "message";
const char kEventNameError[] = "error";

std::unique_ptr<NapiDataChannel::Observer> NapiDataChannel::Observer::Create(DataChannelInterface* dataChannel)
{
    auto observer = std::make_unique<Observer>(dataChannel);
    return observer;
}

NapiDataChannel::Observer::Observer(DataChannelInterface* dataChannel)
    : dataChannel_(dataChannel), binaryType_(kEnumBinaryTypeBlob)
{
}

NapiDataChannel::Observer::~Observer()
{
    RTC_DLOG(LS_INFO) << __FUNCTION__;
    for (auto& handler : eventHandlers_) {
        handler.second.tsfn.Release();
    }
}

bool NapiDataChannel::Observer::GetEventHandler(const std::string& type, Napi::Function& fn)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto it = eventHandlers_.find(type);
    if (it == eventHandlers_.end()) {
        return false;
    }

    fn = it->second.ref.Value();

    return true;
}

bool NapiDataChannel::Observer::SetEventHandler(const std::string& type, Napi::Function fn, Napi::Value receiver)
{
    Reference<Napi::Value>* context = new Reference<Napi::Value>;
    *context = Persistent(receiver);

    EventHandler handler;
    handler.ref = Persistent(fn);
    handler.tsfn =
        ThreadSafeFunction::New(fn.Env(), fn, type, 0, 1, context, [](Napi::Env /*env*/, Reference<Napi::Value>* ctx) {
            ctx->Reset();
            delete ctx;
        });

    {
        UNUSED std::lock_guard<std::mutex> lock(mutex_);
        eventHandlers_[type] = std::move(handler);
    }

    if (type == kEventNameMessage) {
        auto messages = GetAndClearPendingMessages();
        for (auto& message : messages) {
            auto jsEvent = Object::New(fn.Env());
            if (message.binary) {
                auto externalData = new rtc::CopyOnWriteBuffer(message.data);
                auto arrayBuffer = ArrayBuffer::New(
                    fn.Env(), static_cast<void*>(externalData->MutableData()), externalData->size(),
                    [](Napi::Env /*env*/, void* /*data*/, rtc::CopyOnWriteBuffer* hint) {
                        RTC_DLOG(LS_VERBOSE) << "release rtc::CopyOnWriteBuffer";
                        delete hint;
                    },
                    externalData);
                jsEvent.Set("data", arrayBuffer);
            } else {
                jsEvent.Set("data", String::New(fn.Env(), std::string(message.data.data<char>(), message.size())));
            }
            RTC_DLOG(LS_VERBOSE) << "trigger event: " << type;
            fn.Call(receiver, {jsEvent});
        }
    }

    return true;
}

bool NapiDataChannel::Observer::RemoveEventHandler(const std::string& type)
{
    UNUSED std::lock_guard<std::mutex> lock(mutex_);
    auto it = eventHandlers_.find(type);
    if (it == eventHandlers_.end()) {
        return false;
    }

    it->second.tsfn.Release();
    eventHandlers_.erase(it);

    return true;
}

uint64_t NapiDataChannel::Observer::GetBufferedAmountLowThreshold() const
{
    return bufferedAmountLowThreshold_.load();
}

void NapiDataChannel::Observer::SetBufferedAmountLowThreshold(uint64_t bufferedAmountLowThreshold)
{
    bufferedAmountLowThreshold_.store(bufferedAmountLowThreshold);
}

std::string NapiDataChannel::Observer::GetBinaryType() const
{
    UNUSED std::lock_guard<std::mutex> lock(mutex_);
    return binaryType_;
}

void NapiDataChannel::Observer::SetBinaryType(std::string binaryType)
{
    UNUSED std::lock_guard<std::mutex> lock(mutex_);
    binaryType_ = std::move(binaryType);
}

std::vector<DataBuffer> NapiDataChannel::Observer::GetAndClearPendingMessages()
{
    UNUSED std::lock_guard<std::mutex> lock(mutex_);
    auto result = pendingMessages_;
    pendingMessages_.clear();
    return result;
}

void NapiDataChannel::Observer::OnStateChange()
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    static std::map<DataChannelInterface::DataState, std::string> STATE_EVENT_MAP = {
        {DataChannelInterface::kOpen, kEventNameOpen},
        {DataChannelInterface::kClosing, kEventNameClosing},
        {DataChannelInterface::kClosed, kEventNameClose},
    };

    auto state = dataChannel_->state();
    auto eventType = STATE_EVENT_MAP[state];

    UNUSED std::lock_guard<std::mutex> lock(mutex_);
    auto it = eventHandlers_.find(eventType);
    if (it == eventHandlers_.end()) {
        return;
    }

    auto& tsfn = it->second.tsfn;
    Reference<Napi::Value>* context = tsfn.GetContext();
    napi_status status = tsfn.NonBlockingCall([context](Napi::Env env, Napi::Function jsCallback) {
        auto jsEvent = Object::New(env);
        jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
    });
    if (status != napi_ok) {
        RTC_LOG(LS_ERROR) << " tsfn call error: " << status;
    }
}

void NapiDataChannel::Observer::OnMessage(const DataBuffer& buffer)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    auto state = dataChannel_->state();
    if (state != DataChannelInterface::kOpen) {
        RTC_LOG(LS_WARNING) << "Invalid state";
        return;
    }

    UNUSED std::lock_guard<std::mutex> lock(mutex_);
    auto it = eventHandlers_.find(kEventNameMessage);
    if (it == eventHandlers_.end()) {
        pendingMessages_.push_back(buffer);
        return;
    }

    auto& tsfn = it->second.tsfn;
    Reference<Napi::Value>* context = tsfn.GetContext();
    napi_status status = tsfn.NonBlockingCall([context, buffer](Napi::Env env, Napi::Function jsCallback) {
        auto jsEvent = Object::New(env);
        if (buffer.binary) {
            auto externalData = new rtc::CopyOnWriteBuffer(buffer.data);
            auto arrayBuffer = ArrayBuffer::New(
                env, static_cast<void*>(externalData->MutableData()), externalData->size(),
                [](Napi::Env /*env*/, void* /*data*/, rtc::CopyOnWriteBuffer* hint) {
                    RTC_DLOG(LS_VERBOSE) << "release rtc::CopyOnWriteBuffer";
                    delete hint;
                },
                externalData);
            jsEvent.Set("data", arrayBuffer);
        } else {
            jsEvent.Set("data", String::New(env, std::string(buffer.data.data<char>(), buffer.size())));
        }
        jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
    });
    if (status != napi_ok) {
        RTC_LOG(LS_ERROR) << " tsfn call error: " << status;
    }
}

void NapiDataChannel::Observer::OnBufferedAmountChange(uint64_t sent_data_size)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (sent_data_size > bufferedAmountLowThreshold_.load()) {
        return;
    }

    UNUSED std::lock_guard<std::mutex> lock(mutex_);

    auto it = eventHandlers_.find(kEventNameBufferedAmountLow);
    if (it == eventHandlers_.end()) {
        return;
    }

    auto& tsfn = it->second.tsfn;
    Reference<Napi::Value>* context = tsfn.GetContext();
    napi_status status = tsfn.NonBlockingCall([context](Napi::Env env, Napi::Function jsCallback) {
        auto jsEvent = Object::New(env);
        jsCallback.Call(context ? context->Value() : env.Undefined(), {jsEvent});
    });
    if (status != napi_ok) {
        RTC_LOG(LS_ERROR) << " tsfn call error: " << status;
    }
}

FunctionReference NapiDataChannel::constructor_;

void NapiDataChannel::Init(Napi::Env env, Napi::Object exports)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    Function func = DefineClass(
        env, kClassName,
        {
            InstanceAccessor<&NapiDataChannel::GetLabel>(kAttributeNameLabel),
            InstanceAccessor<&NapiDataChannel::GetOrdered>(kAttributeNameOrdered),
            InstanceAccessor<&NapiDataChannel::GetMaxPacketLifeTime>(kAttributeNameMaxPacketLifeTime),
            InstanceAccessor<&NapiDataChannel::GetMaxRetransmits>(kAttributeNameMaxRetransmits),
            InstanceAccessor<&NapiDataChannel::GetProtocol>(kAttributeNameProtocol),
            InstanceAccessor<&NapiDataChannel::GetNegotiated>(kAttributeNameNegotiated),
            InstanceAccessor<&NapiDataChannel::GetId>(kAttributeNameId),
            InstanceAccessor<&NapiDataChannel::GetReadyState>(kAttributeNameReadyState),
            InstanceAccessor<&NapiDataChannel::GetBufferedAmount>(kAttributeNameBufferedAmount),
            InstanceAccessor<
                &NapiDataChannel::GetBufferedAmountLowThreshold, &NapiDataChannel::SetBufferedAmountLowThreshold>(
                kAttributeNameBufferedAmountLowThreshold),
            InstanceAccessor<&NapiDataChannel::GetBinaryType, &NapiDataChannel::SetBinaryType>(
                kAttributeNameBinaryType),
            InstanceAccessor<&NapiDataChannel::GetEventHandler, &NapiDataChannel::SetEventHandler>(
                kAttributeNameOnBufferedAmountLow, napi_default, (void*)kEventNameBufferedAmountLow),
            InstanceAccessor<&NapiDataChannel::GetEventHandler, &NapiDataChannel::SetEventHandler>(
                kAttributeNameOnClose, napi_default, (void*)kEventNameClose),
            InstanceAccessor<&NapiDataChannel::GetEventHandler, &NapiDataChannel::SetEventHandler>(
                kAttributeNameOnClosing, napi_default, (void*)kEventNameClosing),
            InstanceAccessor<&NapiDataChannel::GetEventHandler, &NapiDataChannel::SetEventHandler>(
                kAttributeNameOnOpen, napi_default, (void*)kEventNameOpen),
            InstanceAccessor<&NapiDataChannel::GetEventHandler, &NapiDataChannel::SetEventHandler>(
                kAttributeNameOnMessage, napi_default, (void*)kEventNameMessage),
            InstanceAccessor<&NapiDataChannel::GetEventHandler, &NapiDataChannel::SetEventHandler>(
                kAttributeNameOnError, napi_default, (void*)kEventNameError),
            InstanceMethod<&NapiDataChannel::Close>(kMethodNameClose),
            InstanceMethod<&NapiDataChannel::Send>(kMethodNameSend),
            InstanceMethod<&NapiDataChannel::ToJson>(kMethodNameToJson),
        });
    exports.Set(kClassName, func);

    constructor_ = Persistent(func);
}

Napi::Object NapiDataChannel::NewInstance(Napi::Env env, rtc::scoped_refptr<DataChannelInterface> dataChannel)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    auto externalDataChannel = External<DataChannelInterface>::New(
        env, dataChannel.release(), [](Napi::Env /*env*/, DataChannelInterface* dc) {
            auto status = dc->Release();
            RTC_DLOG(LS_VERBOSE) << "DataChannelInterface release status=" << status;
        });

    return constructor_.New({externalDataChannel});
}

Napi::Object
NapiDataChannel::NewInstance(Napi::Env env, rtc::scoped_refptr<DataChannelInterface> dataChannel, Observer* observer)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    auto externalDataChannel = External<DataChannelInterface>::New(
        env, dataChannel.release(), [](Napi::Env /*env*/, DataChannelInterface* dc) {
            auto status = dc->Release();
            RTC_DLOG(LS_VERBOSE) << "DataChannelInterface release status=" << status;
        });

    auto externalObserver = External<Observer>::New(env, observer);

    return constructor_.New({externalDataChannel, externalObserver});
}

NapiDataChannel::NapiDataChannel(const CallbackInfo& info) : ObjectWrap<NapiDataChannel>(info)
{
    RTC_LOG(LS_VERBOSE) << __FUNCTION__;

    rtc::scoped_refptr<DataChannelInterface> dc;

    if (info.Length() > 0 && info[0].IsExternal()) {
        dc = info[0].As<External<DataChannelInterface>>().Data();
    } else {
        // nothing to do now
    }

    if (!dc) {
        NAPI_THROW_VOID(Error::New(info.Env(), "Invalid argument"));
    }

    std::unique_ptr<Observer> obs;
    if (info.Length() > 1 && info[1].IsExternal()) {
        obs = std::unique_ptr<Observer>(info[1].As<External<Observer>>().Data());
    } else {
        obs = Observer::Create(dc.get());
        dc->RegisterObserver(obs.get());
    }

    if (!obs) {
        NAPI_THROW_VOID(Error::New(info.Env(), "Invalid argument"));
    }

    dataChannel_ = dc;
    observer_ = std::move(obs);
}

NapiDataChannel::~NapiDataChannel()
{
    dataChannel_->UnregisterObserver();
}

rtc::scoped_refptr<DataChannelInterface> NapiDataChannel::Get() const
{
    return dataChannel_;
}

// readonly label: string;
Napi::Value NapiDataChannel::GetLabel(const CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    return String::New(info.Env(), dataChannel_->label());
}

// readonly ordered : boolean;
Napi::Value NapiDataChannel::GetOrdered(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    return Boolean::New(info.Env(), dataChannel_->ordered());
}

// readonly maxPacketLifeTime ?: number;
Napi::Value NapiDataChannel::GetMaxPacketLifeTime(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    if (dataChannel_->maxPacketLifeTime()) {
        return Number::New(info.Env(), dataChannel_->maxPacketLifeTime().value());
    }
    return info.Env().Undefined();
}

// readonly maxRetransmits ?: number;
Napi::Value NapiDataChannel::GetMaxRetransmits(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    if (dataChannel_->maxRetransmitsOpt()) {
        return Number::New(info.Env(), dataChannel_->maxRetransmitsOpt().value());
    }
    return info.Env().Undefined();
}

// readonly protocol : string;
Napi::Value NapiDataChannel::GetProtocol(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    return String::New(info.Env(), dataChannel_->protocol());
}

// readonly negotiated : boolean;
Napi::Value NapiDataChannel::GetNegotiated(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    return Boolean::New(info.Env(), dataChannel_->negotiated());
}

// readonly id ?: number;
Napi::Value NapiDataChannel::GetId(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    if (dataChannel_->id() != -1) {
        return Number::New(info.Env(), dataChannel_->id());
    }
    return info.Env().Undefined();
}

// readonly readyState : DataChannelState;
Napi::Value NapiDataChannel::GetReadyState(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    switch (dataChannel_->state()) {
        case DataChannelInterface::kConnecting:
            return String::New(info.Env(), kEnumDataChannelStateConnecting);
        case DataChannelInterface::kOpen:
            return String::New(info.Env(), kEnumDataChannelStateOpen);
        case DataChannelInterface::kClosing:
            return String::New(info.Env(), kEnumDataChannelStateClosing);
        case DataChannelInterface::kClosed:
            return String::New(info.Env(), kEnumDataChannelStateClosed);
        default:
            break;
    }

    NAPI_THROW(Error::New(info.Env(), "Invalid state"), info.Env().Undefined());
}

// readonly bufferedAmount : number;
Napi::Value NapiDataChannel::GetBufferedAmount(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    return Number::New(info.Env(), dataChannel_->buffered_amount());
}

// bufferedAmountLowThreshold : number;
Napi::Value NapiDataChannel::GetBufferedAmountLowThreshold(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    return Number::New(info.Env(), observer_->GetBufferedAmountLowThreshold());
}

void NapiDataChannel::SetBufferedAmountLowThreshold(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!value.IsNumber()) {
        NAPI_THROW_VOID(TypeError::New(info.Env(), "The argument is not number"));
    }

    auto bufferedAmountLowThreshold = value.As<Number>().Int64Value();
    if (bufferedAmountLowThreshold < 0) {
        NAPI_THROW_VOID(Error::New(info.Env(), "Invalid argument"));
    }

    observer_->SetBufferedAmountLowThreshold(bufferedAmountLowThreshold);
}

// binaryType : BinaryType;
Napi::Value NapiDataChannel::GetBinaryType(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    return String::New(info.Env(), observer_->GetBinaryType());
}

void NapiDataChannel::SetBinaryType(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (!value.IsString()) {
        NAPI_THROW_VOID(TypeError::New(info.Env(), "The argument is not string"));
    }

    auto binaryType = value.As<String>().Utf8Value();
    if (binaryType != kEnumBinaryTypeBlob && binaryType != kEnumBinaryTypeArrayBuffer) {
        NAPI_THROW_VOID(Error::New(info.Env(), "Invalid argument"));
    }

    observer_->SetBinaryType(binaryType);
}

Napi::Value NapiDataChannel::GetEventHandler(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    Function fn;
    if (observer_->GetEventHandler((const char*)info.Data(), fn)) {
        return fn;
    }

    return info.Env().Null();
}

void NapiDataChannel::SetEventHandler(const Napi::CallbackInfo& info, const Napi::Value& value)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    const auto type = (const char*)info.Data();

    observer_->RemoveEventHandler(type);

    if (value.IsFunction()) {
        Function cb = value.As<Function>();
        if (!observer_->SetEventHandler(type, std::move(cb), info.This())) {
            NAPI_THROW_VOID(Napi::Error::New(info.Env(), "Failed to set event handler"));
        }
    }
}

Napi::Value NapiDataChannel::Close(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;
    dataChannel_->Close();
    return info.Env().Undefined();
}

Napi::Value NapiDataChannel::Send(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    if (info.Length() == 0) {
        NAPI_THROW(Error::New(info.Env(), "Wrong number of arguments"), info.Env().Undefined());
    }

    if (dataChannel_->state() != DataChannelInterface::kOpen) {
        NAPI_THROW(Error::New(info.Env(), "Datachannel state is not open"), info.Env().Undefined());
    }

    if (info[0].IsString()) {
        RTC_DLOG(LS_VERBOSE) << "argument is string";
        auto jsStr = info[0].As<String>();
        dataChannel_->SendAsync(DataBuffer(jsStr.Utf8Value()), [&](RTCError err) {
            if (!err.ok()) {
                RTC_LOG(LS_ERROR) << "send array buffer error: " << err.type() << ", " << err.message();
            }
        });
    } else if (info[0].IsArrayBuffer()) {
        RTC_DLOG(LS_VERBOSE) << "argument is array buffer";
        auto jsArrayBuffer = info[0].As<ArrayBuffer>();

        auto data = jsArrayBuffer.Data();
        auto size = jsArrayBuffer.ByteLength();

        dataChannel_->SendAsync(DataBuffer(rtc::CopyOnWriteBuffer((uint8_t*)data, size), true), [&](RTCError err) {
            if (!err.ok()) {
                RTC_LOG(LS_ERROR) << "send array buffer error: " << err.type() << ", " << err.message();
            }
        });
    } else {
        RTC_LOG(LS_WARNING) << "unknown type of argument";
    }

    return info.Env().Undefined();
}

Napi::Value NapiDataChannel::ToJson(const Napi::CallbackInfo& info)
{
    RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

    auto json = Object::New(info.Env());
#ifndef NDEBUG
    json.Set("__native_class__", "NapiDataChannel");
#endif

    return json;
}

void JsToNativeDataChannelInit(const Napi::Object& jsDataChannelInit, DataChannelInit& init)
{
    if (jsDataChannelInit.Has(kAttributeNameOrdered)) {
        auto jsOrdered = jsDataChannelInit.Get(kAttributeNameOrdered);
        if (jsOrdered.IsBoolean()) {
            init.ordered = jsOrdered.As<Boolean>().Value();
        }
    }

    if (jsDataChannelInit.Has(kAttributeNameMaxPacketLifeTime)) {
        auto jsMaxPacketLifeTime = jsDataChannelInit.Get(kAttributeNameMaxPacketLifeTime);
        if (jsMaxPacketLifeTime.IsNumber()) {
            init.maxRetransmitTime = jsMaxPacketLifeTime.As<Number>().Uint32Value();
        }
    }

    if (jsDataChannelInit.Has(kAttributeNameMaxRetransmits)) {
        auto jsMaxRetransmits = jsDataChannelInit.Get(kAttributeNameMaxRetransmits);
        if (jsMaxRetransmits.IsNumber()) {
            init.maxRetransmits = jsMaxRetransmits.As<Number>().Uint32Value();
        }
    }

    if (jsDataChannelInit.Has(kAttributeNameProtocol)) {
        auto jsProtocol = jsDataChannelInit.Get(kAttributeNameProtocol);
        if (jsProtocol.IsString()) {
            init.protocol = jsProtocol.As<String>().Utf8Value();
        }
    }

    if (jsDataChannelInit.Has(kAttributeNameNegotiated)) {
        auto jsNegotiated = jsDataChannelInit.Get(kAttributeNameNegotiated);
        if (jsNegotiated.IsBoolean()) {
            init.negotiated = jsNegotiated.As<Boolean>().Value();
        }
    }

    if (jsDataChannelInit.Has(kAttributeNameId)) {
        auto jsId = jsDataChannelInit.Get(kAttributeNameId);
        if (jsId.IsNumber()) {
            init.id = jsId.As<Number>().Int32Value();
        }
    }
}

} // namespace webrtc
