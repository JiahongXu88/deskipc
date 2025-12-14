#pragma once
#include <cstdint>
#include <string>

#ifdef _WIN32
  #define NOMINMAX
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
  using socket_t = SOCKET;
  static constexpr socket_t kInvalidSock = INVALID_SOCKET;
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
  #include <errno.h>
  using socket_t = int;
  static constexpr socket_t kInvalidSock = -1;
#endif

namespace deskipc {

inline bool net_init(std::string* err = nullptr) {
#ifdef _WIN32
  WSADATA wsa{};
  int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
  if (rc != 0) {
    if (err) *err = "WSAStartup failed";
    return false;
  }
#endif
  return true;
}

inline void net_cleanup() {
#ifdef _WIN32
  WSACleanup();
#endif
}

inline void sock_close(socket_t s) {
#ifdef _WIN32
  closesocket(s);
#else
  close(s);
#endif
}

// Send all bytes (avoid partial send).
inline bool send_all(socket_t s, const uint8_t* data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
#ifdef _WIN32
    int n = send(s, reinterpret_cast<const char*>(data + sent), static_cast<int>(len - sent), 0);
#else
    int n = send(s, data + sent, len - sent, 0);
#endif
    if (n <= 0) return false;
    sent += static_cast<size_t>(n);
  }
  return true;
}

// Receive some bytes
inline int recv_some(socket_t s, uint8_t* buf, size_t cap) {
#ifdef _WIN32
  return recv(s, reinterpret_cast<char*>(buf), static_cast<int>(cap), 0);
#else
  return recv(s, buf, cap, 0);
#endif
}

inline bool set_reuseaddr(socket_t s) {
  int on = 1;
#ifdef _WIN32
  return setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&on), sizeof(on)) == 0;
#else
  return setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == 0;
#endif
}

inline bool ip4_pton(const char* ip, in_addr* out) {
#ifdef _WIN32
  return InetPtonA(AF_INET, ip, out) == 1;
#else
  return inet_pton(AF_INET, ip, out) == 1;
#endif
}

} // namespace deskipc
