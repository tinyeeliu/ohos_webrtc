// Copyright (c) 2026. Licensed under the Apache License, Version 2.0.
#ifndef OHOS_WEBRTC_SEI_H264_SEI_H
#define OHOS_WEBRTC_SEI_H264_SEI_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace webrtc {
namespace sei {

constexpr size_t kMaxSeiDataBytes = 4096;
constexpr size_t kMaxPendingSeiMessages = 64;
constexpr size_t kMaxReceivedSeiMessages = 64;
constexpr char kDefaultSeiUuid[] = "5f7d3c20-9b61-4e8a-a2f0-6c8d174b9301";
using Uuid = std::array<uint8_t, 16>;

struct Message {
    Uuid uuid{};
    std::vector<uint8_t> data;
};

bool ParseUuid(const std::string& text, Uuid* uuid);
std::string FormatUuid(const Uuid& uuid);
// Input/output are H.264 Annex B access units (three/four byte start codes).
// Inserts before the first VCL NAL, preserving all original bytes. No VCL => false.
bool Insert(const uint8_t* frame, size_t size, const Message& message, std::vector<uint8_t>* output);
// Returns only user_data_unregistered (payloadType 5). Invalid/oversized SEI
// NALs are ignored; the caller must always continue forwarding the video frame.
std::vector<Message> Parse(const uint8_t* frame, size_t size);
bool HasVcl(const uint8_t* frame, size_t size);

} // namespace sei
} // namespace webrtc
#endif
