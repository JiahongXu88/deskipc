#include "transport/tcp/net.h"
#include "rpc/rpc_server.h"

#include <iostream>
#include <thread>

using namespace deskipc;

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

  RpcServer server;

  server.on("ping", [](const json&) {
    return Ok("pong");
  });

  server.on("add", [](const json& params) {
    int a = params.value("a", 0);
    int b = params.value("b", 0);
    return Ok(json{{"sum", a + b}});
  });

  server.on("sleep", [](const json& params) {
    int ms = params.value("ms", 1000);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return Ok("done");
  });

  server.serve(cs);

  std::cout << "[worker] disconnected\n";

  sock_close(ls);
  net_cleanup();
  return 0;
}
