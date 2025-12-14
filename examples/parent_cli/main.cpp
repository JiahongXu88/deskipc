#include "protocol/framing.h"
#include "transport/tcp/net.h"

#include <iostream>
#include <string>

using namespace deskipc;

static uint64_t g_reqid = 1;

int main() {
  std::string err;
  if (!net_init(&err)) {
    std::cerr << "[parent] " << err << "\n";
    return 1;
  }

  const uint16_t port = 34567;

  socket_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == kInvalidSock) {
    std::cerr << "[parent] socket() failed\n";
    net_cleanup();
    return 1;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (!ip4_pton("127.0.0.1", &addr.sin_addr)) {
    std::cerr << "[parent] inet_pton failed\n";
    sock_close(s);
    net_cleanup();
    return 1;
  }

  if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    std::cerr << "[parent] connect() failed (start worker first)\n";
    sock_close(s);
    net_cleanup();
    return 1;
  }

  auto send_req = [&](const std::string& body_json) -> uint64_t {
    FrameHeader h{};
    h.magic = kMagic;
    h.version = kVersion;
    h.header_len = kHeaderLen;
    h.msg_type = static_cast<uint8_t>(MsgType::kRequest);
    h.codec = static_cast<uint8_t>(Codec::kJson);
    h.flags = 0;
    h.request_id = g_reqid++;
    h.reserved = 0;
    h.header_crc32 = 0;

    auto out = encode(h, reinterpret_cast<const uint8_t*>(body_json.data()), body_json.size());
    if (!send_all(s, out.data(), out.size())) {
      std::cerr << "[parent] send_all failed\n";
      return 0;
    }
    std::cout << "[parent] sent req_id=" << h.request_id << " body=" << body_json << "\n";
    return h.request_id;
  };

  // Send 3 requests
  send_req(R"({"method":"ping","params":{}})");
  send_req(R"({"method":"add","params":{"a":1,"b":2}})");
  send_req(R"({"method":"sleep","params":{"ms":2000}})");

  FrameDecoder decoder;
  uint8_t buf[4096];
  int received = 0;

  while (received < 3) {
    int n = recv_some(s, buf, sizeof(buf));
    if (n <= 0) {
      std::cerr << "[parent] disconnected\n";
      break;
    }

    auto frames = decoder.feed(buf, static_cast<size_t>(n));
    for (auto& f : frames) {
      std::string body(reinterpret_cast<const char*>(f.body.data()), f.body.size());
      std::cout << "[parent] got resp req_id=" << f.header.request_id << " body=" << body << "\n";
      received++;
    }
  }

  sock_close(s);
  net_cleanup();
  return 0;
}
