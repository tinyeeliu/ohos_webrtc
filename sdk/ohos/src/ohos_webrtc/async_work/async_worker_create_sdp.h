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

#ifndef WEBRTC_ASYNC_WORKER_CREATE_SDP_H
#define WEBRTC_ASYNC_WORKER_CREATE_SDP_H

#include "api/peer_connection_interface.h"
#include "napi.h"

namespace webrtc {

class AsyncWorkerCreateSdp : public Napi::AsyncWorker {
public:
    AsyncWorkerCreateSdp(Napi::Env env, const char* resourceName)
        : AsyncWorker(env, resourceName), deferred_(Napi::Promise::Deferred::New(env)), error_(RTCError::OK())
    {
    }

    Napi::Promise GetPromise()
    {
        return deferred_.Promise();
    }

    void SetError(RTCError error)
    {
        error_ = std::move(error);
        AsyncWorker::SetError(std::string(error_.message()));
    }

    void SetDesc(SessionDescriptionInterface* desc)
    {
        desc_ = std::unique_ptr<SessionDescriptionInterface>(desc);
    }

protected:
    void Execute() override
    {
        // do nothing
    }

    virtual void OnOK() override
    {
        std::string sdp;
        desc_->ToString(&sdp);
        RTC_DLOG(LS_VERBOSE) << "sdp: " << sdp;

        auto result = Napi::Object::New(Env());
        result.Set("sdp", Napi::String::New(Env(), sdp));
        result.Set("type", Napi::String::New(Env(), webrtc::SdpTypeToString(desc_->GetType())));

        deferred_.Resolve(result);
    }

    virtual void OnError(const Napi::Error& e) override
    {
        deferred_.Reject(e.Value());
    }

private:
    Napi::Promise::Deferred deferred_;
    RTCError error_;
    std::unique_ptr<SessionDescriptionInterface> desc_;
};

class SdpObserver : public CreateSessionDescriptionObserver {
public:
    SdpObserver(AsyncWorkerCreateSdp* asyncWorker) : asyncWorker_(asyncWorker) {}

    ~SdpObserver() override
    {
        if (asyncWorker_) {
            delete asyncWorker_;
        }
    }

protected:
    void OnSuccess(SessionDescriptionInterface* desc) override
    {
        RTC_LOG(LS_INFO) << "CreateSessionDescription success: " << desc;

        if (asyncWorker_) {
            asyncWorker_->SetDesc(desc);
            asyncWorker_->Queue();
            asyncWorker_ = nullptr;
        }
    }

    void OnFailure(RTCError error) override
    {
        RTC_LOG(LS_ERROR) << "CreateSessionDescription failed";
        if (asyncWorker_) {
            asyncWorker_->SetError(std::move(error));
            asyncWorker_->Queue();
            asyncWorker_ = nullptr;
        }
    }

private:
    AsyncWorkerCreateSdp* asyncWorker_;
};

} // namespace webrtc

#endif // WEBRTC_ASYNC_WORKER_CREATE_SDP_H
