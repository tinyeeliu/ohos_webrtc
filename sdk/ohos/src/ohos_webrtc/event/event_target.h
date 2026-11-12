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

#ifndef WEBRTC_EVENT_EVENT_TARGET_H
#define WEBRTC_EVENT_EVENT_TARGET_H

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <queue>
#include <mutex>
#include <string>
#include <thread>

#include "napi.h"
#include "napi/native_api.h"

#include "rtc_base/logging.h"

#include "event.h"
#include "event_queue.h"
#include "../utils/marcos.h"

namespace webrtc {

template <typename T>
class NapiEventTarget : public Napi::ObjectWrap<T>, public EventQueue<T> {
public:
    explicit NapiEventTarget(const Napi::CallbackInfo& info) : Napi::ObjectWrap<T>(info)
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        tsfn_ = Napi::ThreadSafeFunction::New(
            info.Env(), Napi::Function::New(info.Env(), [this](const Napi::CallbackInfo& info) { this->Run(info); }),
            "NapiEventTarget", 0, 1);

        // Hold the reference until stopped
        this->Ref();

        // Env-level safety net: if the consumer never calls Stop() the Ref()
        // above keeps the object alive, so this finalizer can only run when
        // the env is torn down. It shuts the target down if the destructor
        // has not run yet; the destructor disarms it via finalizerGuard_ so
        // it never touches the object after destruction.
        finalizerGuard_ = std::make_shared<std::atomic<bool>>(true);
        auto guard = finalizerGuard_;
        info.This().As<Napi::Object>().AddFinalizer(
            [guard](Napi::Env, NapiEventTarget* self) {
                if (guard->exchange(false)) {
                    self->ShutdownEventTarget();
                }
            },
            this);
    }

    ~NapiEventTarget() override
    {
        // Disarm the finalizer first: it must never touch this object after
        // the destructor has run.
        if (finalizerGuard_) {
            finalizerGuard_->store(false);
        }
        ShutdownEventTarget();
    }

    /**
     * Stops event delivery and balances the constructor's Ref(). Idempotent
     * and thread-compatible with Run(): the DidStop()/Unref() pair runs
     * exactly once, here or at the end of Run(), whichever comes first.
     * Subclass destructors must call this before releasing native resources
     * whose observer callbacks dispatch events, so late events are dropped
     * instead of being delivered to a dying object.
     */
    void ShutdownEventTarget()
    {
        shouldStop_ = true;
        NotifyStopped();
        if (!tsfnReleased_) {
            tsfnReleased_ = true;
            tsfn_.Release();
        }
    }

    void Dispatch(std::unique_ptr<Event<T>> event)
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        if (shouldStop_) {
            // Dropped: the target is stopped or being destroyed.
            return;
        }

        this->Enqueue(std::move(event));

        napi_status status = tsfn_.BlockingCall([](Napi::Env env, Napi::Function func) { func.Call({}); });
        if (status != napi_ok) {
            RTC_LOG(LS_ERROR) << " tsfn call error: " << status;
        }
    }

    virtual void Stop()
    {
        shouldStop_ = true;
        // Wake Run() so it observes the stop, runs DidStop() and balances the
        // constructor Ref(). Events are no longer accepted once stopped.
        napi_status status = tsfn_.BlockingCall([](Napi::Env env, Napi::Function func) { func.Call({}); });
        if (status != napi_ok && status != napi_closing) {
            RTC_LOG(LS_ERROR) << " tsfn call error: " << status;
        }
    }

    bool ShouldStop() const
    {
        return shouldStop_;
    }

    bool GetEventHandler(const std::string& type, Napi::Function& fn) const
    {
        UNUSED std::lock_guard<std::mutex> lock(mutex_);

        auto it = eventHandlers_.find(type);
        if (it == eventHandlers_.end()) {
            return false;
        }

        fn = it->second.Value();
        return true;
    }

    void SetEventHandler(const std::string& type, const Napi::Function& fn)
    {
        UNUSED std::lock_guard<std::mutex> lock(mutex_);

        auto it = eventHandlers_.find(type);
        if (it != eventHandlers_.end()) {
            it->second.Unref();
            eventHandlers_.erase(it);
        }

        eventHandlers_[type] = Persistent(fn);
    }

    void RemoveEventHandler(const std::string& type)
    {
        UNUSED std::lock_guard<std::mutex> lock(mutex_);

        auto it = eventHandlers_.find(type);
        if (it != eventHandlers_.end()) {
            it->second.Unref();
            eventHandlers_.erase(it);
        }
    }

protected:
    virtual void Run(const Napi::CallbackInfo& info)
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

        while (!shouldStop_) {
            auto event = this->Dequeue();
            if (!event) {
                break;
            }

            Napi::HandleScope scope(info.Env());
            T* target = static_cast<T*>(this);
            event->Process(*target);

            bool isPending = false;
            napi_is_exception_pending(info.Env(), &isPending);
            if (isPending) {
                napi_value exception;
                napi_get_and_clear_last_exception(info.Env(), &exception);

                std::string errorMsg = "unknown";
                napi_value strVal;
                napi_status s = napi_coerce_to_string(info.Env(), exception, &strVal);
                if (s != napi_ok) {
                    napi_value tmp2;
                    napi_get_and_clear_last_exception(info.Env(), &tmp2);
                    RTC_LOG(LS_ERROR) << "Event processing failed, skipping event: " << errorMsg;
                    continue;
                }

                size_t len = 0;
                s = napi_get_value_string_utf8(info.Env(), strVal, nullptr, 0, &len);
                if (s == napi_ok && len > 0) {
                    constexpr size_t kMaxErrorMessageLength = 512;
                    len = std::min(len, kMaxErrorMessageLength);
                    errorMsg.resize(len + 1);
                    size_t actualLen = 0;
                    napi_get_value_string_utf8(info.Env(), strVal, &errorMsg[0], len + 1, &actualLen);
                    errorMsg.resize(actualLen);
                }
                RTC_LOG(LS_ERROR) << "Event processing failed, skipping event: " << errorMsg;
            }
        }

        if (shouldStop_) {
            NotifyStopped();
        }
    }

    virtual void DidStop()
    {
        // Do nothing.
    }

protected:
    void MakeCallback(const char* name, const std::initializer_list<napi_value>& args)
    {
        RTC_DLOG(LS_VERBOSE) << __FUNCTION__ << ": " << name;

        Napi::Function func;
        GetEventHandler(name, func);
        if (func.IsEmpty()) {
            RTC_DLOG(LS_WARNING) << "No event handler: " << name;
            return;
        }

        auto self = this->Value();
        func.MakeCallback(self, args);
    }

private:
    // Runs DidStop() and balances the constructor Ref() exactly once.
    void NotifyStopped()
    {
        bool expected = false;
        if (!stopNotified_.compare_exchange_strong(expected, true)) {
            return;
        }
        this->DidStop();
        auto refCount = this->Unref();
        if (refCount > 0) {
            RTC_LOG(LS_WARNING) << "Reference count of napi object is greater than 0: " << refCount;
        }
    }

    std::atomic<bool> shouldStop_{false};
    std::atomic<bool> stopNotified_{false};
    bool tsfnReleased_{false};
    std::shared_ptr<std::atomic<bool>> finalizerGuard_;
    mutable std::mutex mutex_;
    std::map<std::string, Napi::FunctionReference> eventHandlers_;
    Napi::ThreadSafeFunction tsfn_;
};

} // namespace webrtc

#endif // WEBRTC_EVENT_EVENT_TARGET_H
