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

#ifndef WEBRTC_ASYNC_WORKER_SET_SDP_H
#define WEBRTC_ASYNC_WORKER_SET_SDP_H

#include "napi.h"
#include "napi/native_api.h"

#include "api/peer_connection_interface.h"

namespace webrtc {

class AsyncWorkerSetSdp : public Napi::AsyncWorker {
public:
    AsyncWorkerSetSdp(Napi::Env env, const char* resourceName)
        : AsyncWorker(env, resourceName), deferred_(Napi::Promise::Deferred::New(env))
    {
    }

    Napi::Promise GetPromise()
    {
        return deferred_.Promise();
    }

    void SetError(const RTCError& error)
    {
        AsyncWorker::SetError(std::string(error.message()));
    }

protected:
    void Execute() override
    {
        // do nothing
    }

    virtual void OnOK() override
    {
        deferred_.Resolve(Env().Null());
    }

    virtual void OnError(const Napi::Error& e) override
    {
        deferred_.Reject(e.Value());
    }

private:
    Napi::Promise::Deferred deferred_;
};

class SetSdpObserver : public SetSessionDescriptionObserver {
public:
    static rtc::scoped_refptr<SetSdpObserver> Create(AsyncWorkerSetSdp* asyncWorker)
    {
        return rtc::make_ref_counted<SetSdpObserver>(asyncWorker);
    }

    ~SetSdpObserver() override
    {
        if (asyncWorker_) {
            delete asyncWorker_;
        }
    }

protected:
    SetSdpObserver(AsyncWorkerSetSdp* asyncWorker) : asyncWorker_(asyncWorker) {}

    void OnSuccess() override
    {
        RTC_LOG(LS_INFO) << __FUNCTION__;
        if (asyncWorker_) {
            asyncWorker_->Queue();
            asyncWorker_ = nullptr;
        }
    }

    void OnFailure(RTCError error) override
    {
        RTC_LOG(LS_INFO) << __FUNCTION__ << " " << webrtc::ToString(error.type()) << ": " << error.message();
        if (asyncWorker_) {
            asyncWorker_->SetError(std::move(error));
            asyncWorker_->Queue();
            asyncWorker_ = nullptr;
        }
    }

private:
    AsyncWorkerSetSdp* asyncWorker_;
};

} // namespace webrtc

#endif // WEBRTC_ASYNC_WORKER_SET_SDP_H
