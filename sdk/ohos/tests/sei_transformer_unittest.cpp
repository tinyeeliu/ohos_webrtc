// Copyright (c) 2026. Licensed under the Apache License, Version 2.0.
#include "sei/sei_transformer.h"
#include "api/make_ref_counted.h"
#include "test/gtest.h"

namespace webrtc {
namespace sei {
namespace {
class Frame : public TransformableVideoFrameInterface {
public:
    explicit Frame(uint32_t timestamp = 1, uint32_t ssrc = 7, std::string mime = "video/H264")
        : timestamp_(timestamp), ssrc_(ssrc), mime_(std::move(mime)) {}
    rtc::ArrayView<const uint8_t> GetData() const override { return data_; }
    void SetData(rtc::ArrayView<const uint8_t> data) override { data_.assign(data.begin(), data.end()); }
    uint8_t GetPayloadType() const override { return 96; }
    uint32_t GetSsrc() const override { return ssrc_; }
    uint32_t GetTimestamp() const override { return timestamp_; }
    void SetRTPTimestamp(uint32_t timestamp) override { timestamp_ = timestamp; }
    std::string GetMimeType() const override { return mime_; }
    bool IsKeyFrame() const override { return true; }
    VideoFrameMetadata Metadata() const override {
        VideoFrameMetadata metadata;
        RTPVideoHeaderH264 header{};
        header.packetization_mode = mode;
        metadata.SetRTPVideoHeaderCodecSpecifics(header);
        return metadata;
    }
    void SetMetadata(const VideoFrameMetadata&) override {}
    H264PacketizationMode mode = H264PacketizationMode::NonInterleaved;
    std::vector<uint8_t> data_{0, 0, 1, 0x65, 0x88, 0x84};
private:
    uint32_t timestamp_;
    uint32_t ssrc_;
    std::string mime_;
};

class Output : public TransformedFrameCallback {
public:
    void OnTransformedFrame(std::unique_ptr<TransformableFrameInterface> frame) override
    {
        auto data = frame->GetData();
        frames.emplace_back(data.begin(), data.end());
    }
    std::vector<std::vector<uint8_t>> frames;
};

class Listener : public Observer {
public:
    void Notify(const ReceivedMessage& message) override { if (!closed) { messages.push_back(message); } }
    void Close() override { closed = true; }
    bool closed = false;
    std::vector<ReceivedMessage> messages;
};

Message Payload(uint8_t byte)
{
    Message message;
    ParseUuid(kDefaultSeiUuid, &message.uuid);
    message.data = {byte, 0, 0, 1, 0, 0, 3};
    return message;
}

TEST(OhosSeiTransformer, QueueLimitsReadyCodecChangeAndClose)
{
    auto sender = rtc::make_ref_counted<SeiTransformer>(true);
    auto output = rtc::make_ref_counted<Output>();
    sender->RegisterTransformedFrameSinkCallback(output, 7);
    std::string error;
    EXPECT_FALSE(sender->Enqueue(Payload(1), &error));
    sender->Transform(std::make_unique<Frame>());
    EXPECT_TRUE(sender->Ready());
    for (size_t i = 0; i < kMaxPendingSeiMessages; ++i) { EXPECT_TRUE(sender->Enqueue(Payload(1), &error)); }
    EXPECT_FALSE(sender->Enqueue(Payload(1), &error));
    EXPECT_EQ(sender->Pending(), kMaxPendingSeiMessages);
    sender->Clear();
    EXPECT_EQ(sender->Pending(), 0u);
    EXPECT_TRUE(sender->Enqueue(Payload(2), &error));
    sender->Transform(std::make_unique<Frame>(2, 7, "video/VP8"));
    EXPECT_FALSE(sender->Ready());
    EXPECT_EQ(sender->Pending(), 0u);
    EXPECT_FALSE(sender->Enqueue(Payload(3), &error));
    auto mode0 = std::make_unique<Frame>(3);
    mode0->mode = H264PacketizationMode::SingleNalUnit;
    sender->Transform(std::move(mode0));
    EXPECT_FALSE(sender->Ready());
    EXPECT_FALSE(sender->Enqueue(Payload(3), &error));
    sender->Close();
    EXPECT_FALSE(sender->Ready());
    EXPECT_FALSE(sender->Enqueue(Payload(4), &error));
}

TEST(OhosSeiTransformer, FifoAndSimulcastTimestampReplication)
{
    auto sender = rtc::make_ref_counted<SeiTransformer>(true);
    auto output = rtc::make_ref_counted<Output>();
    sender->RegisterTransformedFrameSinkCallback(output, 7);
    sender->RegisterTransformedFrameSinkCallback(output, 8);
    sender->Transform(std::make_unique<Frame>());
    std::string error;
    ASSERT_TRUE(sender->Enqueue(Payload(10), &error));
    ASSERT_TRUE(sender->Enqueue(Payload(11), &error));
    sender->Transform(std::make_unique<Frame>(2, 7));
    sender->Transform(std::make_unique<Frame>(2, 8));
    EXPECT_EQ(sender->Pending(), 1u);
    sender->Transform(std::make_unique<Frame>(3, 7));
    ASSERT_EQ(output->frames.size(), 4u);
    for (size_t i = 1; i <= 3; ++i) {
        auto messages = Parse(output->frames[i].data(), output->frames[i].size());
        ASSERT_EQ(messages.size(), 1u);
        EXPECT_EQ(messages[0].data, Payload(i == 3 ? 11 : 10).data);
    }
    EXPECT_EQ(sender->Injected(), 3u);
    sender->Close();
}

TEST(OhosSeiTransformer, ReceiverDoesNotModifyVideoAndNotifiesEachListener)
{
    auto receiver = rtc::make_ref_counted<SeiTransformer>(false);
    auto output = rtc::make_ref_counted<Output>();
    receiver->RegisterTransformedFrameSinkCallback(output, 7);
    auto first = std::make_shared<Listener>();
    auto second = std::make_shared<Listener>();
    ASSERT_TRUE(receiver->AddObserver(first));
    ASSERT_TRUE(receiver->AddObserver(second));
    auto frame = std::make_unique<Frame>(123, 7);
    std::vector<uint8_t> encoded;
    ASSERT_TRUE(Insert(frame->data_.data(), frame->data_.size(), Payload(42), &encoded));
    frame->data_ = encoded;
    receiver->Transform(std::move(frame));
    ASSERT_EQ(first->messages.size(), 1u);
    ASSERT_EQ(second->messages.size(), 1u);
    EXPECT_EQ(first->messages[0].message.data, Payload(42).data);
    EXPECT_EQ(first->messages[0].ssrc, 7u);
    EXPECT_EQ(first->messages[0].rtpTimestamp, 123u);
    ASSERT_EQ(output->frames.size(), 1u);
    EXPECT_EQ(output->frames[0], encoded);
    first->Close();
    receiver->Transform(std::make_unique<Frame>(124, 7));
    EXPECT_EQ(output->frames.size(), 2u);
    receiver->Close();
    EXPECT_TRUE(second->closed);
    EXPECT_FALSE(receiver->AddObserver(std::make_shared<Listener>()));
}

TEST(OhosSeiTransformer, MalformedSeiStillForwardsVideo)
{
    auto receiver = rtc::make_ref_counted<SeiTransformer>(false);
    auto output = rtc::make_ref_counted<Output>();
    receiver->RegisterTransformedFrameCallback(output);
    auto listener = std::make_shared<Listener>();
    receiver->AddObserver(listener);
    auto frame = std::make_unique<Frame>();
    frame->data_ = {0, 0, 1, 6, 5, 255, 0, 0, 1, 0x65, 0x88};
    receiver->Transform(std::move(frame));
    EXPECT_EQ(output->frames.size(), 1u);
    EXPECT_TRUE(listener->messages.empty());
    receiver->Close();
}
} // namespace
} // namespace sei
} // namespace webrtc
