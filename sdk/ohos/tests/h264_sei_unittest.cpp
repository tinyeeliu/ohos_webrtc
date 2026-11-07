// Copyright (c) 2026. Licensed under the Apache License, Version 2.0.
#include "sei/h264_sei.h"

#include <algorithm>
#include <random>

#include "test/gtest.h"

namespace {
using webrtc::sei::Message;
using Bytes = std::vector<uint8_t>;
const Bytes kSlice{0, 0, 1, 0x65, 0x88, 0x84};

Message MakeMessage(const Bytes& data)
{
    Message message;
    EXPECT_TRUE(webrtc::sei::ParseUuid("00112233-4455-6677-8899-aabbccddeeff", &message.uuid));
    message.data = data;
    return message;
}

void RoundTrip(const Bytes& data)
{
    const auto message = MakeMessage(data);
    Bytes encoded;
    ASSERT_TRUE(webrtc::sei::Insert(kSlice.data(), kSlice.size(), message, &encoded));
    ASSERT_GE(encoded.size(), kSlice.size());
    EXPECT_TRUE(std::equal(kSlice.begin(), kSlice.end(), encoded.end() - kSlice.size()));
    const auto parsed = webrtc::sei::Parse(encoded.data(), encoded.size());
    ASSERT_EQ(parsed.size(), 1u);
    EXPECT_EQ(parsed[0].uuid, message.uuid);
    EXPECT_EQ(parsed[0].data, data);
}

TEST(OhosH264Sei, MatchesGoldenFixtureAndPreservesNalOrder)
{
    // Hand-authored Annex B fixture; no use of the production writer to build
    // the expected payload. payloadType=5, payloadSize=19 (16 UUID + 'abc').
    const Bytes golden{0, 0, 0, 1, 6, 5, 19,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
        'a', 'b', 'c', 0x80, 0, 0, 1, 0x65, 0x88, 0x84};
    const auto parsed = webrtc::sei::Parse(golden.data(), golden.size());
    ASSERT_EQ(parsed.size(), 1u);
    EXPECT_EQ(parsed[0].data, Bytes({'a', 'b', 'c'}));
    EXPECT_EQ(parsed[0].uuid, MakeMessage({'a', 'b', 'c'}).uuid);
    Bytes encoded;
    ASSERT_TRUE(webrtc::sei::Insert(kSlice.data(), kSlice.size(), MakeMessage({'a', 'b', 'c'}), &encoded));
    EXPECT_EQ(encoded, golden);

    const Bytes prefix{0, 0, 1, 9, 0xf0, 0, 0, 0, 1, 0x67, 0x42, 0, 0x1f,
        0, 0, 1, 0x68, 0xce};
    Bytes accessUnit = prefix;
    accessUnit.insert(accessUnit.end(), kSlice.begin(), kSlice.end());
    ASSERT_TRUE(webrtc::sei::Insert(accessUnit.data(), accessUnit.size(), MakeMessage({'a', 'b', 'c'}), &encoded));
    Bytes expected = prefix;
    expected.insert(expected.end(), golden.begin(), golden.end());
    EXPECT_EQ(encoded, expected);
}

TEST(OhosH264Sei, RejectsMalformedAndParsesMultipleMessages)
{
    for (const Bytes& nal : {Bytes{0, 0, 1, 6, 5, 255}, Bytes{0, 0, 1, 6, 5, 1, 0, 0x80},
        Bytes{0, 0, 1, 6, 0xff}, Bytes{0, 0, 1, 6, 5, 16, 0, 0, 3}}) {
        EXPECT_TRUE(webrtc::sei::Parse(nal.data(), nal.size()).empty());
    }
    // Other message types are skipped; two payloadType=5 messages in one NAL.
    Bytes nal{0, 0, 1, 6, 6, 1, 0x80};
    for (int n = 0; n < 2; ++n) {
        nal.push_back(5); nal.push_back(17);
        for (int i = 1; i <= 16; ++i) { nal.push_back(static_cast<uint8_t>(i)); }
        nal.push_back(static_cast<uint8_t>(n + 10));
    }
    nal.push_back(0x80);
    const auto parsed = webrtc::sei::Parse(nal.data(), nal.size());
    ASSERT_EQ(parsed.size(), 2u);
    EXPECT_EQ(parsed[0].data, Bytes{10});
    EXPECT_EQ(parsed[1].data, Bytes{11});
    // A malformed second message invalidates its whole SEI NAL (no partial callback).
    nal.pop_back(); nal.push_back(5); nal.push_back(200); nal.push_back(0x80);
    EXPECT_TRUE(webrtc::sei::Parse(nal.data(), nal.size()).empty());

    Bytes once;
    ASSERT_TRUE(webrtc::sei::Insert(kSlice.data(), kSlice.size(), MakeMessage({1}), &once));
    Bytes twice;
    ASSERT_TRUE(webrtc::sei::Insert(once.data(), once.size(), MakeMessage({2}), &twice));
    EXPECT_EQ(webrtc::sei::Parse(twice.data(), twice.size()).size(), 2u);

    Bytes many;
    ASSERT_GE(once.size(), kSlice.size());
    for (size_t i = 0; i < 100; ++i) { many.insert(many.end(), once.begin(), once.end() - kSlice.size()); }
    EXPECT_EQ(webrtc::sei::Parse(many.data(), many.size()).size(), webrtc::sei::kMaxReceivedSeiMessages);
}

TEST(OhosH264Sei, RoundTripsBinaryAndUtf8)
{
    RoundTrip({0, 0, 0, 0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 3, 4});
    RoundTrip({0xe4, 0xb8, 0xad, 0xe6, 0x96, 0x87, 0xf0, 0x9f, 0x98, 0x80});
}

TEST(OhosH264Sei, RoundTripsBoundaryLengths)
{
    for (const size_t length : {size_t(1), size_t(238), size_t(239), size_t(240), size_t(254),
        size_t(255), size_t(256), size_t(4096)}) {
        SCOPED_TRACE(length);
        Bytes data(length);
        for (size_t i = 0; i < length; ++i) { data[i] = static_cast<uint8_t>(i); }
        RoundTrip(data);
    }
}

TEST(OhosH264Sei, RejectsInvalidInput)
{
    Bytes output;
    EXPECT_FALSE(webrtc::sei::Insert(kSlice.data(), kSlice.size(), MakeMessage({}), &output));
    EXPECT_FALSE(webrtc::sei::Insert(kSlice.data(), kSlice.size(), MakeMessage(Bytes(4097)), &output));
    EXPECT_FALSE(webrtc::sei::Insert(nullptr, 0, MakeMessage({1}), &output));
    const Bytes avcc{0, 0, 0, 2, 0x65, 0x88};
    EXPECT_FALSE(webrtc::sei::Insert(avcc.data(), avcc.size(), MakeMessage({1}), &output));
    const Bytes headers{0, 0, 1, 0x67, 0x42};
    EXPECT_FALSE(webrtc::sei::HasVcl(headers.data(), headers.size()));
}

TEST(OhosH264Sei, ValidatesUuid)
{
    webrtc::sei::Uuid uuid{};
    EXPECT_FALSE(webrtc::sei::ParseUuid("bad", &uuid));
    EXPECT_FALSE(webrtc::sei::ParseUuid("00112233-4455-6677-8899-aabbccddeefg", &uuid));
    ASSERT_TRUE(webrtc::sei::ParseUuid("00112233-4455-6677-8899-AABBCCDDEEFF", &uuid));
    EXPECT_EQ(webrtc::sei::FormatUuid(uuid), "00112233-4455-6677-8899-aabbccddeeff");
}

TEST(OhosH264Sei, RandomizedRoundTrips)
{
    std::mt19937 random(42);
    for (size_t i = 0; i < 500; ++i) {
        SCOPED_TRACE(i);
        Bytes data(1 + random() % 4096);
        for (auto& byte : data) { byte = static_cast<uint8_t>(random()); }
        RoundTrip(data);
    }
}

TEST(OhosH264Sei, MalformedInputSmoke)
{
    // Deterministic malformed-input smoke fuzzing, also useful under ASan/UBSan.
    std::mt19937 random(42);
    for (size_t i = 0; i < 10000; ++i) {
        Bytes data(random() % 512);
        for (auto& byte : data) { byte = static_cast<uint8_t>(random()); }
        if (data.size() >= 4) { data[0] = 0; data[1] = 0; data[2] = 1; data[3] = 6; }
        webrtc::sei::Parse(data.data(), data.size());
    }
}
} // namespace
