#include "framing.h"

#include <cstring>

namespace deskipc {

// ---- Little-endian helpers ----
static inline void write_le16(uint8_t* dst, uint16_t v) {
  dst[0] = static_cast<uint8_t>(v & 0xFF);
  dst[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}
static inline void write_le32(uint8_t* dst, uint32_t v) {
  dst[0] = static_cast<uint8_t>(v & 0xFF);
  dst[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
  dst[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
  dst[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}
static inline void write_le64(uint8_t* dst, uint64_t v) {
  for (int i = 0; i < 8; ++i) dst[i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFF);
}

static inline uint16_t read_le16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
static inline uint32_t read_le32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0])      ) |
         (static_cast<uint32_t>(p[1]) <<  8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}
static inline uint64_t read_le64(const uint8_t* p) {
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v |= (static_cast<uint64_t>(p[i]) << (8 * i));
  return v;
}

// ---- validate ----
ValidateResult validate(const FrameHeader& h) {
  ValidateResult r;

  if (h.magic != kMagic) { r.reason = "bad magic"; return r; }
  if (h.version != kVersion) { r.reason = "unsupported version"; return r; }
  if (h.header_len != kHeaderLen) { r.reason = "bad header_len"; return r; }
  if (h.body_len > kMaxBodyLen) { r.reason = "body_len too large"; return r; }

  if (!(h.msg_type == (uint8_t)MsgType::kRequest ||
        h.msg_type == (uint8_t)MsgType::kResponse ||
        h.msg_type == (uint8_t)MsgType::kEvent)) {
    r.reason = "bad msg_type"; return r;
  }

  if (h.codec != (uint8_t)Codec::kJson) { r.reason = "unsupported codec"; return r; }
  if (h.flags != 0) { r.reason = "flags must be 0 in v0.1"; return r; }
  if (h.reserved != 0) { r.reason = "reserved must be 0 in v0.1"; return r; }
  if (h.header_crc32 != 0) { r.reason = "crc must be 0 in v0.1"; return r; }

  if (h.msg_type == (uint8_t)MsgType::kEvent) {
    if (h.request_id != 0) { r.reason = "event request_id must be 0"; return r; }
  } else {
    if (h.request_id == 0) { r.reason = "request_id must be non-zero"; return r; }
  }

  r.ok = true;
  return r;
}

// ---- encode / decode ----
std::vector<uint8_t> encode(const FrameHeader& h, const uint8_t* body, size_t body_len) {
  std::vector<uint8_t> out;
  out.resize(kHeaderLen + body_len);

  uint8_t* p = out.data();
  write_le32(p + 0,  h.magic);
  write_le16(p + 4,  h.version);
  write_le16(p + 6,  h.header_len);
  write_le32(p + 8,  static_cast<uint32_t>(body_len));
  p[12] = h.msg_type;
  p[13] = h.codec;
  write_le16(p + 14, h.flags);
  write_le64(p + 16, h.request_id);
  write_le32(p + 24, h.reserved);
  write_le32(p + 28, h.header_crc32);

  if (body_len > 0 && body) {
    std::memcpy(p + kHeaderLen, body, body_len);
  }
  return out;
}

std::optional<FrameHeader> decode_header(const uint8_t* p, size_t n) {
  if (!p || n < kHeaderLen) return std::nullopt;

  FrameHeader h{};
  h.magic        = read_le32(p + 0);
  h.version      = read_le16(p + 4);
  h.header_len   = read_le16(p + 6);
  h.body_len     = read_le32(p + 8);
  h.msg_type     = p[12];
  h.codec        = p[13];
  h.flags        = read_le16(p + 14);
  h.request_id   = read_le64(p + 16);
  h.reserved     = read_le32(p + 24);
  h.header_crc32 = read_le32(p + 28);
  return h;
}

// ---- FrameDecoder ----
std::vector<Frame> FrameDecoder::feed(const uint8_t* data, size_t len) {
  std::vector<Frame> frames;
  if (!data || len == 0) return frames;

  buf_.insert(buf_.end(), data, data + len);

  while (true) {
    if (buf_.size() < kHeaderLen) break;

    auto oh = decode_header(buf_.data(), buf_.size());
    if (!oh) break;

    const FrameHeader& h = *oh;
    auto vr = validate(h);
    if (!vr.ok) {
      // v0.1: simplest strategy: clear buffer; caller should close socket.
      buf_.clear();
      return {};
    }

    const size_t total = kHeaderLen + static_cast<size_t>(h.body_len);
    if (buf_.size() < total) break;

    Frame f;
    f.header = h;
    f.body.resize(h.body_len);
    if (h.body_len > 0) {
      std::memcpy(f.body.data(), buf_.data() + kHeaderLen, h.body_len);
    }

    frames.push_back(std::move(f));
    buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(total));
  }

  return frames;
}

} // namespace deskipc
