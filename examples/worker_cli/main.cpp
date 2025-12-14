#include "protocol/framing.h"
#include "transport/tcp/net.h"

#include <iostream>
#include <string>

using namespace deskipc;

static std::string bytes_to_string(const std::vector<uint8_t>& v) {
  return std::string(reinterpret_cast<const char*>(v.data()), v.size());
}

int main() {
  std::string err;
  if (!net_init(&err)) {
    std::cerr << "[worker] " << err << "\n";
    return 1;
  }

  const uint16_t port = 34567;

  socket_t ls = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (ls == kInvalidSock) {
    std::cerr << "[worker] socket() failed\n";
    net_cleanup();
    return 1;
  }

  set_reuseaddr(ls);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);

  if (bind(ls, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    std::cerr << "[worker] bind() failed\n";
    sock_close(ls);
    net_cleanup();
    return 1;
  }

  if (listen(ls, 1) != 0) {
    std::cerr << "[worker] listen() failed\n";
    sock_close(ls);
    net_cleanup();
    return 1;
  }

  std::cout << "[worker] listening on 127.0.0.1:" << port << "\n";

  socket_t cs = accept(ls, nullptr, nullptr);
  if (cs == kInvalidSock) {
    std::cerr << "[worker] accept() failed\n";
    sock_close(ls);
    net_cleanup();
    return 1;
  }

  std::cout << "[worker] client connected\n";

  FrameDecoder decoder;
  uint8_t buf[4096];

  while (true) {
    int n = recv_some(cs, buf, sizeof(buf));
    if (n <= 0) {
      std::cout << "[worker] disconnected\n";
      break;
    }

    auto frames = decoder.feed(buf, static_cast<size_t>(n));
    if (frames.empty() && n > 0) {
      // could be: incomplete frame; or protocol error cleared buffer
      // We don't know which here—v0.1: keep going unless socket breaks.
    }

    for (auto& f : frames) {
      const std::string body = bytes_to_string(f.body);
      std::cout << "[worker] req_id=" << f.header.request_id << " body=" << body << "\n";

      // v0.1: simplest dispatch (no JSON parser yet)
      std::string resp_body;
      if (body.find("\"method\":\"ping\"") != std::string::npos) {
        resp_body = R"({"ok":true,"data":"pong"})";
      } else if (body.find("\"method\":\"add\"") != std::string::npos) {
        resp_body = R"({"ok":true,"data":{"sum":3}})";
      } else if (body.find("\"method\":\"sleep\"") != std::string::npos) {
        resp_body = R"({"ok":true,"data":"done"})";
      } else {
        resp_body = R"({"ok":false,"error":{"code":1002,"message":"method_not_found"}})";
      }

      FrameHeader rh{};
      rh.magic = kMagic;
      rh.version = kVersion;
      rh.header_len = kHeaderLen;
      rh.msg_type = static_cast<uint8_t>(MsgType::kResponse);
      rh.codec = static_cast<uint8_t>(Codec::kJson);
      rh.flags = 0;
      rh.request_id = f.header.request_id;
      rh.reserved = 0;
      rh.header_crc32 = 0;

      auto out = encode(rh, reinterpret_cast<const uint8_t*>(resp_body.data()), resp_body.size());
      if (!send_all(cs, out.data(), out.size())) {
        std::cerr << "[worker] send_all failed\n";
        break;
      }
    }
  }

  sock_close(cs);
  sock_close(ls);
  net_cleanup();
  return 0;
}
