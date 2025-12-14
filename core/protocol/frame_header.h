#pragma once
#include <cstdint>
#include <string>

namespace deskipc {

// "DIPC" as little-endian u32 (bytes on wire: 44 49 50 43)
static constexpr uint32_t kMagic     = 0x43504944;
static constexpr uint16_t kVersion   = 0x0001;
static constexpr uint16_t kHeaderLen = 32;

enum class MsgType : uint8_t {
  kRequest  = 1,
  kResponse = 2,
  kEvent    = 3,
};

enum class Codec : uint8_t {
  kJson     = 1,
  kProtobuf = 2, // reserved
};

// v0.1: fixed header = 32 bytes
#pragma pack(push, 1)
struct FrameHeader {
  uint32_t magic;        // 0
  uint16_t version;      // 4
  uint16_t header_len;   // 6
  uint32_t body_len;     // 8
  uint8_t  msg_type;     // 12
  uint8_t  codec;        // 13
  uint16_t flags;        // 14
  uint64_t request_id;   // 16
  uint32_t reserved;     // 24
  uint32_t header_crc32; // 28 (v0.1 unused, set 0)
};
#pragma pack(pop)

static_assert(sizeof(FrameHeader) == 32, "FrameHeader must be 32 bytes");

struct ValidateResult {
  bool ok = false;
  std::string reason;
};

} // namespace deskipc
