// Copyright (c) 2026. Licensed under the Apache License, Version 2.0.
#include "sei/h264_sei.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace webrtc {
namespace sei {
namespace {
constexpr size_t kMaxSeiNalBytes = 65536;
struct Nal { size_t start; size_t payload; size_t end; };

std::vector<Nal> FindNals(const uint8_t* data, size_t size)
{
    std::vector<Nal> nals;
    if (!data) { return nals; }
    for (size_t i = 0; i + 2 < size;) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
            const size_t start = i > 0 && data[i - 1] == 0 ? i - 1 : i;
            if (!nals.empty()) { nals.back().end = start; }
            nals.push_back({start, i + 3, size});
            i += 3;
        } else { ++i; }
    }
    return nals;
}

size_t VclOffset(const uint8_t* data, size_t size)
{
    for (const auto& nal : FindNals(data, size)) {
        if (nal.payload < nal.end) {
            const auto type = data[nal.payload] & 0x1f;
            if (type >= 1 && type <= 5) { return nal.start; }
        }
    }
    return size;
}

void WriteLength(size_t value, std::vector<uint8_t>* data)
{
    while (value >= 255) { data->push_back(255); value -= 255; }
    data->push_back(static_cast<uint8_t>(value));
}

bool ReadLength(const std::vector<uint8_t>& data, size_t* position, size_t* value)
{
    *value = 0;
    while (*position < data.size()) {
        const auto byte = data[(*position)++];
        if (*value > std::numeric_limits<size_t>::max() - byte) { return false; }
        *value += byte;
        if (byte != 255) { return true; }
    }
    return false;
}

bool ParseNal(const uint8_t* data, size_t size, std::vector<Message>* messages)
{
    if (size > kMaxSeiNalBytes) { return false; }
    std::vector<uint8_t> rbsp;
    rbsp.reserve(size);
    size_t zeros = 0;
    for (size_t i = 0; i < size; ++i) {
        if (zeros >= 2 && data[i] == 3) {
            if (i + 1 == size || data[i + 1] > 3) { return false; }
            zeros = 0;
            continue;
        }
        rbsp.push_back(data[i]);
        zeros = data[i] == 0 ? zeros + 1 : 0;
    }
    size_t position = 0;
    while (position < rbsp.size()) {
        if (rbsp[position] == 0x80 &&
            std::all_of(rbsp.begin() + position + 1, rbsp.end(), [](uint8_t v) { return v == 0; })) {
            return true; // rbsp_trailing_bits and optional trailing_zero_8bits.
        }
        size_t type = 0;
        size_t length = 0;
        if (!ReadLength(rbsp, &position, &type) || !ReadLength(rbsp, &position, &length) ||
            length > rbsp.size() - position) { return false; }
        if (type == 5) {
            if (length < 16) { return false; }
            if (length - 16 <= kMaxSeiDataBytes && messages->size() < kMaxReceivedSeiMessages) {
                Message message;
                std::copy_n(rbsp.begin() + position, 16, message.uuid.begin());
                message.data.assign(rbsp.begin() + position + 16, rbsp.begin() + position + length);
                messages->push_back(std::move(message));
            }
        }
        position += length;
    }
    return false; // Missing rbsp_trailing_bits.
}
} // namespace

bool ParseUuid(const std::string& text, Uuid* uuid)
{
    if (!uuid || text.size() != 36) { return false; }
    Uuid result{};
    size_t digit = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (text[i] != '-') { return false; }
            continue;
        }
        const char c = text[i];
        const int value = c >= '0' && c <= '9' ? c - '0' :
            c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (value < 0) { return false; }
        result[digit / 2] = static_cast<uint8_t>((result[digit / 2] << 4) | value);
        ++digit;
    }
    *uuid = result;
    return true;
}

std::string FormatUuid(const Uuid& uuid)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string text;
    for (size_t i = 0; i < uuid.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) { text += '-'; }
        text += hex[uuid[i] >> 4];
        text += hex[uuid[i] & 15];
    }
    return text;
}

bool HasVcl(const uint8_t* frame, size_t size) { return VclOffset(frame, size) != size; }

bool Insert(const uint8_t* frame, size_t size, const Message& message, std::vector<uint8_t>* output)
{
    if (!output || message.data.empty() || message.data.size() > kMaxSeiDataBytes) { return false; }
    const auto offset = VclOffset(frame, size);
    if (offset == size) { return false; }
    std::vector<uint8_t> rbsp;
    rbsp.push_back(5);
    WriteLength(16 + message.data.size(), &rbsp);
    rbsp.insert(rbsp.end(), message.uuid.begin(), message.uuid.end());
    rbsp.insert(rbsp.end(), message.data.begin(), message.data.end());
    rbsp.push_back(0x80);
    std::vector<uint8_t> nal{0, 0, 0, 1, 6};
    size_t zeros = 0;
    for (auto byte : rbsp) {
        if (zeros >= 2 && byte <= 3) { nal.push_back(3); zeros = 0; }
        nal.push_back(byte);
        zeros = byte == 0 ? zeros + 1 : 0;
    }
    if (size > std::numeric_limits<size_t>::max() - nal.size()) { return false; }
    std::vector<uint8_t> result;
    result.reserve(size + nal.size());
    result.insert(result.end(), frame, frame + offset);
    result.insert(result.end(), nal.begin(), nal.end());
    result.insert(result.end(), frame + offset, frame + size);
    *output = std::move(result);
    return true;
}

std::vector<Message> Parse(const uint8_t* frame, size_t size)
{
    std::vector<Message> messages;
    for (const auto& nal : FindNals(frame, size)) {
        if (nal.payload < nal.end && (frame[nal.payload] & 0x9f) == 6) {
            std::vector<Message> parsed;
            if (ParseNal(frame + nal.payload + 1, nal.end - nal.payload - 1, &parsed)) {
                for (auto& message : parsed) {
                    if (messages.size() == kMaxReceivedSeiMessages) { return messages; }
                    messages.push_back(std::move(message));
                }
            }
        }
    }
    return messages;
}
} // namespace sei
} // namespace webrtc
