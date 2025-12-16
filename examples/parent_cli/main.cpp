#include "transport/tcp/net.h"
#include "rpc/rpc_client.h"

#include <iostream>
#include <vector>
#include <chrono>

using namespace deskipc;

static void print_result(const char* name, const RpcResult& r) {
  if (r.ok) {
    std::cout << "[parent] " << name << " => ok=true data=" << r.data.dump() << "\n";
  } else {
    std::cout << "[parent] " << name << " => ok=false err_code=" << r.error.code
              << " err_msg=" << r.error.message << "\n";
  }
}

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

  // ---- v0.2: RpcClient (recv thread + pending + timeout) ----
  RpcClient client(s);
  if (!client.start()) {
    std::cerr << "[parent] client.start() failed\n";
    sock_close(s);
    net_cleanup();
    return 1;
  }

  // 1) Basic calls (sync) ----------------------------------------------------
  {
    auto r1 = client.call("ping", json::object(), 1000);
    print_result("ping", r1);

    auto r2 = client.call("add", json{{"a", 1}, {"b", 2}}, 1000);
    print_result("add", r2);

    // timeout demo: worker sleeps 2000ms, timeout 200ms => timeout expected
    auto r3 = client.call("sleep", json{{"ms", 2000}}, 200);
    print_result("sleep(timeout=200ms)", r3);

    // allow some time so the late response arrives; it must be DROPPED by client (no crash, no wrong completion)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  // 2) Simple concurrency demo (same connection, multiple in-flight) ---------
  // We'll launch a few calls in parallel using std::async; RpcClient is thread-safe for call()
  // (it uses a pending map + mutex, and a single recv loop).
  {
    const int N = 20;
    std::vector<std::future<RpcResult>> futs;
    futs.reserve(N);

    auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) {
      futs.emplace_back(std::async(std::launch::async, [&client, i] {
        if (i % 3 == 0) {
          return client.call("ping", json::object(), 1000);
        } else if (i % 3 == 1) {
          return client.call("add", json{{"a", i}, {"b", 2}}, 1000);
        } else {
          // short sleep so it usually succeeds
          return client.call("sleep", json{{"ms", 50}}, 500);
        }
      }));
    }

    int ok_cnt = 0, timeout_cnt = 0, err_cnt = 0;
    for (int i = 0; i < N; ++i) {
      RpcResult r = futs[i].get();
      if (r.ok) {
        ok_cnt++;
      } else if (r.error.code == static_cast<int>(RpcErrc::kTimeout)) {
        timeout_cnt++;
      } else {
        err_cnt++;
      }
    }

    auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    std::cout << "[parent] concurrent demo: N=" << N
              << " ok=" << ok_cnt
              << " timeout=" << timeout_cnt
              << " err=" << err_cnt
              << " elapsed_ms=" << ms
              << "\n";
  }

  client.stop();   // stops recv thread and closes socket
  net_cleanup();
  return 0;
}
