// Copyright (c) 2026. Licensed under the Apache License, Version 2.0.
#include "sei/sei_transformer.h"

#include <algorithm>
#include <cctype>
#include <utility>
#include "api/make_ref_counted.h"

namespace webrtc {
namespace sei {
namespace {
bool IsH264(std::string mime)
{
    std::transform(mime.begin(), mime.end(), mime.begin(), [](unsigned char c) { return std::tolower(c); });
    return mime == "video/h264";
}
constexpr size_t kRecentFrames = 16;
constexpr size_t kMaxObservers = 16;

bool SupportsFragmentation(const TransformableFrameInterface& frame)
{
    // Installed only on video senders. Mode 0 cannot fragment a 4096-byte SEI
    // NAL and could otherwise cause the whole access unit to be discarded.
    const auto& video = static_cast<const TransformableVideoFrameInterface&>(frame);
    const auto metadata = video.Metadata();
    const auto* h264 = absl::get_if<RTPVideoHeaderH264>(&metadata.GetRTPVideoHeaderCodecSpecifics());
    return h264 && h264->packetization_mode == H264PacketizationMode::NonInterleaved;
}
} // namespace

void SeiTransformer::Transform(std::unique_ptr<TransformableFrameInterface> frame)
{
    if (!frame) { return; }
    rtc::scoped_refptr<TransformedFrameCallback> callback;
    std::vector<std::shared_ptr<Observer>> observers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) { return; }
        auto it = callbacks_.find(frame->GetSsrc());
        callback = it == callbacks_.end() ? defaultCallback_ : it->second;
        if (!callback) { return; }
        const auto bytes = frame->GetData();
        if (sending_) {
            ready_ = IsH264(frame->GetMimeType()) && HasVcl(bytes.data(), bytes.size()) && SupportsFragmentation(*frame);
            if (ready_) {
                const auto timestamp = frame->GetTimestamp();
                auto cached = std::find_if(recent_.begin(), recent_.end(), [timestamp](const CachedMessage& item) {
                    return item.timestamp == timestamp;
                });
                Message message;
                if (cached != recent_.end()) {
                    message = cached->message;
                } else {
                    if (!pending_.empty()) { message = std::move(pending_.front()); pending_.pop_front(); }
                    recent_.push_back({timestamp, message});
                    if (recent_.size() > kRecentFrames) { recent_.pop_front(); }
                }
                std::vector<uint8_t> output;
                if (!message.data.empty() && Insert(bytes.data(), bytes.size(), message, &output)) {
                    frame->SetData(output);
                    ++injected_;
                }
            } else {
                // Never send stale queued data after a codec/format change.
                pending_.clear();
                recent_.clear();
            }
        } else if (IsH264(frame->GetMimeType())) {
            for (auto it = observers_.begin(); it != observers_.end();) {
                if (auto observer = it->lock()) { observers.push_back(std::move(observer)); ++it; }
                else { it = observers_.erase(it); }
            }
        }
    }
    if (!observers.empty()) {
        const auto bytes = frame->GetData();
        for (auto& message : Parse(bytes.data(), bytes.size())) {
            ReceivedMessage received{std::move(message), frame->GetSsrc(), frame->GetTimestamp()};
            for (auto& observer : observers) { observer->Notify(received); }
        }
    }
    // Forward outside the lock: delegates may unregister or destroy themselves.
    callback->OnTransformedFrame(std::move(frame));
}

void SeiTransformer::RegisterTransformedFrameSinkCallback(rtc::scoped_refptr<TransformedFrameCallback> callback,
    uint32_t ssrc)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!closed_) { callbacks_[ssrc] = std::move(callback); }
}

void SeiTransformer::UnregisterTransformedFrameSinkCallback(uint32_t ssrc)
{
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.erase(ssrc);
    if (callbacks_.empty()) { ready_ = false; pending_.clear(); recent_.clear(); }
}

void SeiTransformer::RegisterTransformedFrameCallback(rtc::scoped_refptr<TransformedFrameCallback> callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!closed_) { defaultCallback_ = std::move(callback); }
}

void SeiTransformer::UnregisterTransformedFrameCallback()
{
    std::lock_guard<std::mutex> lock(mutex_);
    defaultCallback_ = nullptr;
    ready_ = false;
    pending_.clear();
    recent_.clear();
}

bool SeiTransformer::Enqueue(Message message, std::string* error)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const char* reason = nullptr;
    if (closed_) { reason = "PeerConnection is closed"; }
    else if (!sending_ || !ready_) { reason = "H.264 Annex B packetization-mode=1 encoder is not ready; check EncoderWrapper.ready"; }
    else if (message.data.empty() || message.data.size() > kMaxSeiDataBytes) { reason = "SEI data must be 1..4096 bytes"; }
    else if (pending_.size() >= kMaxPendingSeiMessages) { reason = "SEI send queue is full"; }
    if (reason) { if (error) { *error = reason; } return false; }
    pending_.push_back(std::move(message));
    return true;
}

bool SeiTransformer::AddObserver(std::shared_ptr<Observer> observer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.erase(std::remove_if(observers_.begin(), observers_.end(),
        [](const std::weak_ptr<Observer>& value) { return value.expired(); }), observers_.end());
    if (closed_ || observers_.size() >= kMaxObservers) { return false; }
    observers_.push_back(observer);
    return true;
}

bool SeiTransformer::Ready() const { std::lock_guard<std::mutex> lock(mutex_); return !closed_ && ready_; }
size_t SeiTransformer::Pending() const { std::lock_guard<std::mutex> lock(mutex_); return pending_.size(); }
uint64_t SeiTransformer::Injected() const { std::lock_guard<std::mutex> lock(mutex_); return injected_; }
void SeiTransformer::Clear() { std::lock_guard<std::mutex> lock(mutex_); pending_.clear(); recent_.clear(); }

void SeiTransformer::Close()
{
    std::vector<std::shared_ptr<Observer>> observers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) { return; }
        closed_ = true;
        ready_ = false;
        pending_.clear();
        recent_.clear();
        for (auto& weak : observers_) { if (auto observer = weak.lock()) { observers.push_back(observer); } }
        observers_.clear();
        callbacks_.clear();
        defaultCallback_ = nullptr;
    }
    for (auto& observer : observers) { observer->Close(); }
}

rtc::scoped_refptr<SeiTransformer> PeerSeiContext::Sender(rtc::scoped_refptr<RtpSenderInterface> sender)
{
    if (closed_) { return nullptr; }
    auto& transformer = senders_[sender->id()];
    if (!transformer) {
        transformer = rtc::make_ref_counted<SeiTransformer>(true);
        sender->SetEncoderToPacketizerFrameTransformer(transformer);
    }
    return transformer;
}

rtc::scoped_refptr<SeiTransformer> PeerSeiContext::Receiver(rtc::scoped_refptr<RtpReceiverInterface> receiver)
{
    if (closed_) { return nullptr; }
    auto& transformer = receivers_[receiver->id()];
    if (!transformer) {
        transformer = rtc::make_ref_counted<SeiTransformer>(false);
        receiver->SetDepacketizerToDecoderFrameTransformer(transformer);
    }
    return transformer;
}

void PeerSeiContext::Close()
{
    if (closed_) { return; }
    closed_ = true;
    for (auto& item : senders_) { item.second->Close(); }
    for (auto& item : receivers_) { item.second->Close(); }
    senders_.clear();
    receivers_.clear();
}
} // namespace sei
} // namespace webrtc
