// Copyright (c) 2026. Licensed under the Apache License, Version 2.0.
#ifndef OHOS_WEBRTC_SEI_TRANSFORMER_H
#define OHOS_WEBRTC_SEI_TRANSFORMER_H

#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "api/frame_transformer_interface.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "sei/h264_sei.h"

namespace webrtc {
namespace sei {

struct ReceivedMessage {
    Message message;
    uint32_t ssrc;
    uint32_t rtpTimestamp;
};

class Observer {
public:
    virtual ~Observer() = default;
    virtual void Notify(const ReceivedMessage& message) = 0;
    virtual void Close() = 0;
};

class SeiTransformer : public FrameTransformerInterface {
public:
    explicit SeiTransformer(bool sending) : sending_(sending) {}
    void Transform(std::unique_ptr<TransformableFrameInterface> frame) override;
    void RegisterTransformedFrameSinkCallback(rtc::scoped_refptr<TransformedFrameCallback> callback,
        uint32_t ssrc) override;
    void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override;
    // Used by adapters/tests implementing the non-SSRC callback form.
    void RegisterTransformedFrameCallback(rtc::scoped_refptr<TransformedFrameCallback> callback) override;
    void UnregisterTransformedFrameCallback() override;

    bool Enqueue(Message message, std::string* error);
    bool AddObserver(std::shared_ptr<Observer> observer);
    bool Ready() const;
    size_t Pending() const;
    uint64_t Injected() const;
    void Clear();
    void Close();

protected:
    ~SeiTransformer() override = default;

private:
    const bool sending_;
    mutable std::mutex mutex_;
    bool closed_ = false;
    bool ready_ = false;
    uint64_t injected_ = 0;
    std::deque<Message> pending_;
    // Replicate a consumed message to simulcast layers with the same RTP timestamp.
    // This is bounded and never keeps an encoded frame alive.
    struct CachedMessage { uint32_t timestamp; Message message; };
    std::deque<CachedMessage> recent_;
    std::map<uint32_t, rtc::scoped_refptr<TransformedFrameCallback>> callbacks_;
    rtc::scoped_refptr<TransformedFrameCallback> defaultCallback_;
    std::vector<std::weak_ptr<Observer>> observers_;
};

// Owned by the N-API PeerConnection and shared by every sender/receiver wrapper.
// Its maps are accessed only on that environment's ArkTS thread. Transformers
// contain no PeerConnection/factory references, avoiding native reference cycles.
class PeerSeiContext {
public:
    ~PeerSeiContext() { Close(); }
    rtc::scoped_refptr<SeiTransformer> Sender(rtc::scoped_refptr<RtpSenderInterface> sender);
    rtc::scoped_refptr<SeiTransformer> Receiver(rtc::scoped_refptr<RtpReceiverInterface> receiver);
    bool Closed() const { return closed_; }
    void Close();

private:
    bool closed_ = false;
    std::map<std::string, rtc::scoped_refptr<SeiTransformer>> senders_;
    std::map<std::string, rtc::scoped_refptr<SeiTransformer>> receivers_;
};

} // namespace sei
} // namespace webrtc
#endif
