// Copyright (c) 2026. Licensed under the Apache License, Version 2.0.
// Standalone tests: compile with sei/h264_sei.cpp and a host C++17 compiler.
#include "sei/h264_sei.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <random>

namespace {
using webrtc::sei::Message;
using Bytes = std::vector<uint8_t>;
void Check(bool condition, const char* what)
{
    if (!condition) { std::cerr << "FAIL: " << what << '\n'; std::exit(1); }
}
const Bytes kSlice{0, 0, 1, 0x65, 0x88, 0x84};

Message MakeMessage(const Bytes& data)
{
    Message message;
    Check(webrtc::sei::ParseUuid("00112233-4455-6677-8899-aabbccddeeff", &message.uuid), "parse UUID");
    message.data = data;
    return message;
}

void RoundTrip(const Bytes& data)
{
    const auto message = MakeMessage(data);
    Bytes encoded;
    Check(webrtc::sei::Insert(kSlice.data(), kSlice.size(), message, &encoded), "insert");
    Check(std::equal(kSlice.begin(), kSlice.end(), encoded.end() - kSlice.size()), "original slice preserved");
    const auto parsed = webrtc::sei::Parse(encoded.data(), encoded.size());
    Check(parsed.size() == 1, "one message");
    Check(parsed[0].uuid == message.uuid && parsed[0].data == data, "byte-exact round trip");
}

void GoldenFixture()
{
    // Hand-authored Annex B fixture; no use of the production writer to build
    // the expected payload. payloadType=5, payloadSize=19 (16 UUID + 'abc').
    const Bytes golden{0, 0, 0, 1, 6, 5, 19,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
        'a', 'b', 'c', 0x80, 0, 0, 1, 0x65, 0x88, 0x84};
    const auto parsed = webrtc::sei::Parse(golden.data(), golden.size());
    Check(parsed.size() == 1 && parsed[0].data == Bytes({'a', 'b', 'c'}), "read independent golden fixture");
    Bytes encoded;
    Check(webrtc::sei::Insert(kSlice.data(), kSlice.size(), MakeMessage({'a', 'b', 'c'}), &encoded), "write fixture");
    Check(encoded == golden, "writer matches independent golden fixture");

    const Bytes prefix{0, 0, 1, 9, 0xf0, 0, 0, 0, 1, 0x67, 0x42, 0, 0x1f,
        0, 0, 1, 0x68, 0xce};
    Bytes accessUnit = prefix;
    accessUnit.insert(accessUnit.end(), kSlice.begin(), kSlice.end());
    Check(webrtc::sei::Insert(accessUnit.data(), accessUnit.size(), MakeMessage({'a', 'b', 'c'}), &encoded), "insert after AUD/SPS/PPS");
    Bytes expected = prefix;
    expected.insert(expected.end(), golden.begin(), golden.end());
    Check(encoded == expected, "NAL order and original bytes preserved");
}

void MalformedAndMultiple()
{
    for (const Bytes& nal : {Bytes{0, 0, 1, 6, 5, 255}, Bytes{0, 0, 1, 6, 5, 1, 0, 0x80},
        Bytes{0, 0, 1, 6, 0xff}, Bytes{0, 0, 1, 6, 5, 16, 0, 0, 3}}) {
        Check(webrtc::sei::Parse(nal.data(), nal.size()).empty(), "reject malformed SEI");
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
    Check(parsed.size() == 2 && parsed[0].data == Bytes{10} && parsed[1].data == Bytes{11}, "multiple SEI messages");
    // A malformed second message invalidates its whole SEI NAL (no partial callback).
    nal.pop_back(); nal.push_back(5); nal.push_back(200); nal.push_back(0x80);
    Check(webrtc::sei::Parse(nal.data(), nal.size()).empty(), "no partial messages from malformed NAL");

    Bytes once;
    Check(webrtc::sei::Insert(kSlice.data(), kSlice.size(), MakeMessage({1}), &once), "first existing SEI");
    Bytes twice;
    Check(webrtc::sei::Insert(once.data(), once.size(), MakeMessage({2}), &twice), "preserve existing SEI");
    Check(webrtc::sei::Parse(twice.data(), twice.size()).size() == 2, "two SEI NALs");

    Bytes many;
    for (size_t i = 0; i < 100; ++i) { many.insert(many.end(), once.begin(), once.end() - kSlice.size()); }
    Check(webrtc::sei::Parse(many.data(), many.size()).size() == webrtc::sei::kMaxReceivedSeiMessages, "bounded received messages");
}
} // namespace

int main()
{
    GoldenFixture();
    RoundTrip({0, 0, 0, 0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 3, 4});
    RoundTrip({0xe4, 0xb8, 0xad, 0xe6, 0x96, 0x87, 0xf0, 0x9f, 0x98, 0x80});
    for (const size_t length : {size_t(1), size_t(238), size_t(239), size_t(240), size_t(254),
        size_t(255), size_t(256), size_t(4096)}) {
        Bytes data(length);
        for (size_t i = 0; i < length; ++i) { data[i] = static_cast<uint8_t>(i); }
        RoundTrip(data);
    }
    Bytes output;
    Check(!webrtc::sei::Insert(kSlice.data(), kSlice.size(), MakeMessage({}), &output), "reject empty send");
    Check(!webrtc::sei::Insert(kSlice.data(), kSlice.size(), MakeMessage(Bytes(4097)), &output), "reject oversized send");
    Check(!webrtc::sei::Insert(nullptr, 0, MakeMessage({1}), &output), "reject empty frame");
    const Bytes avcc{0, 0, 0, 2, 0x65, 0x88};
    Check(!webrtc::sei::Insert(avcc.data(), avcc.size(), MakeMessage({1}), &output), "reject AVCC input");
    const Bytes headers{0, 0, 1, 0x67, 0x42};
    Check(!webrtc::sei::HasVcl(headers.data(), headers.size()), "codec-config-only data is not a video frame");
    webrtc::sei::Uuid uuid{};
    Check(!webrtc::sei::ParseUuid("bad", &uuid), "reject invalid UUID");
    Check(!webrtc::sei::ParseUuid("00112233-4455-6677-8899-aabbccddeefg", &uuid), "reject nonhex UUID");
    Check(webrtc::sei::ParseUuid("00112233-4455-6677-8899-AABBCCDDEEFF", &uuid), "accept uppercase UUID");
    Check(webrtc::sei::FormatUuid(uuid) == "00112233-4455-6677-8899-aabbccddeeff", "canonical UUID");
    MalformedAndMultiple();
    std::mt19937 random(42);
    for (size_t i = 0; i < 500; ++i) {
        Bytes data(1 + random() % 4096);
        for (auto& byte : data) { byte = static_cast<uint8_t>(random()); }
        RoundTrip(data);
    }
    // Deterministic malformed-input smoke fuzzing, also useful under ASan/UBSan.
    for (size_t i = 0; i < 10000; ++i) {
        Bytes data(random() % 512);
        for (auto& byte : data) { byte = static_cast<uint8_t>(random()); }
        if (data.size() >= 4) { data[0] = 0; data[1] = 0; data[2] = 1; data[3] = 6; }
        webrtc::sei::Parse(data.data(), data.size());
    }
    std::cout << "PASS: H.264 SEI golden fixtures, boundaries, binary/UTF-8, 500 round trips and 10000 malformed inputs\n";
}
