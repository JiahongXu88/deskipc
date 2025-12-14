#pragma once
#include "frame_header.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace deskipc {

static constexpr uint32_t kMaxBodyLen = 8 * 1024 * 1024; // 8MB (v0.1)

struct Frame {
  FrameHeader header{};
  std::vector<uint8_t> body; // raw bytes (JSON UTF-8 in v0.1)
};

ValidateResult validate(const FrameHeader& h);

// Encode a frame to bytes (wire format: little-endian header + body).
std::vector<uint8_t> encode(const FrameHeader& h, const uint8_t* body, size_t body_len);

// Decode ONLY header from at least 32 bytes.
std::optional<FrameHeader> decode_header(const uint8_t* p, size_t n);

// Streaming decoder for half-packet / sticky-packet
class FrameDecoder {
public:
  // Feed raw bytes, returns 0..N complete frames.
  // On protocol error: clears internal buffer and returns empty.
  std::vector<Frame> feed(const uint8_t* data, size_t len);

  void clear() { buf_.clear(); }

private:
  std::vector<uint8_t> buf_;
};

} // namespace deskipc
