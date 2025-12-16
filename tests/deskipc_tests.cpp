#include "protocol/framing.h"
#include "rpc/rpc_client.h"
#include "rpc/rpc_server.h"
#include "transport/tcp/net.h"

#include <cassert>
#include <chrono>
#include <future>
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>

using namespace deskipc;

static void test_framing_basic() {
  FrameHeader h{};
  h.magic = kMagic;
  h.version = kVersion;
  h.header_len = kHeaderLen;
  h.msg_type = static_cast<uint8_t>(MsgType::kRequest);
  h.codec = static_cast<uint8_t>(Codec::kJson);
  h.flags = 0;
  h.request_id = 1;
  h.reserved = 0;
  h.header_crc32 = 0;

  const char* body = "{\"method\":\"ping\",\"params\":{}}";
  auto bytes = encode(h, reinterpret_cast<const uint8_t*>(body), std::strlen(body));

  // 半包
  FrameDecoder d;
  auto a = d.feed(bytes.data(), 10);
  assert(a.empty());
  auto b = d.feed(bytes.data() + 10, bytes.size() - 10);
  assert(b.size() == 1);
  assert(b[0].header.request_id == 1);

  // 粘包：两帧拼一起
  FrameHeader h2 = h; h2.request_id = 2;
  auto bytes2 = encode(h2, reinterpret_cast<const uint8_t*>(body), std::strlen(body));
  std::vector<uint8_t> both;
  both.insert(both.end(), bytes.begin(), bytes.end());
  both.insert(both.end(), bytes2.begin(), bytes2.end());

  FrameDecoder d2;
  auto fs = d2.feed(both.data(), both.size());
  assert(fs.size() == 2);
  assert(fs[0].header.request_id == 1);
  assert(fs[1].header.request_id == 2);

  // bad magic：应被拒绝（当前 feed() 会清 buffer 并返回空，属于“拒绝”的表现）
  FrameHeader bad = h;
  bad.magic = 0;
  auto bb = encode(bad, reinterpret_cast<const uint8_t*>(body), std::strlen(body));
  FrameDecoder d3;
  auto badfs = d3.feed(bb.data(), bb.size());
  assert(badfs.empty());
}

static uint16_t start_server_once(std::thread& th, std::atomic<int>& event_cnt) {
  socket_t ls = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  assert(ls != kInvalidSock);
  (void)set_reuseaddr(ls);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  in_addr ia{};
  bool ok = ip4_pton("127.0.0.1", &ia);
  assert(ok);
  addr.sin_addr = ia;
  addr.sin_port = htons(0); // 0 => ephemeral

  int rc = ::bind(ls, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  assert(rc == 0);

  rc = ::listen(ls, 1);
  assert(rc == 0);

  sockaddr_in bound{};
#ifdef _WIN32
  int blen = sizeof(bound);
#else
  socklen_t blen = sizeof(bound);
#endif
  rc = ::getsockname(ls, reinterpret_cast<sockaddr*>(&bound), &blen);
  assert(rc == 0);
  uint16_t port = ntohs(bound.sin_port);

  th = std::thread([ls, &event_cnt]() mutable {
    socket_t cs = ::accept(ls, nullptr, nullptr);
    sock_close(ls);
    assert(cs != kInvalidSock);

    RpcServer srv;
    srv.on("ping", [](const json&) { return Ok({{"pong", true}}); });
    srv.on("add", [](const json& p) {
      if (!p.contains("a") || !p.contains("b") || !p["a"].is_number_integer() || !p["b"].is_number_integer()) {
        return Err(RpcErrc::kInvalidRequest, "add expects {a:int,b:int}");
      }
      int a = p["a"].get<int>();
      int b = p["b"].get<int>();
      return Ok({{"sum", a + b}});
    });
    srv.on("sleep", [](const json& p) {
      int ms = 0;
      if (p.contains("ms") && p["ms"].is_number_integer()) ms = p["ms"].get<int>();
      std::this_thread::sleep_for(std::chrono::milliseconds(ms));
      return Ok({{"slept_ms", ms}});
    });
    srv.on("event_inc", [&event_cnt](const json&) {
      event_cnt.fetch_add(1);
      return Ok();
    });

    srv.serve(cs); // serve() 内部会 sock_close(cs)
  });

  return port;
}

static void test_rpc_v02() {
  std::atomic<int> event_cnt{0};

  std::thread th;
  uint16_t port = start_server_once(th, event_cnt);

  socket_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  assert(s != kInvalidSock);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  in_addr ia{};
  bool ok = ip4_pton("127.0.0.1", &ia);
  assert(ok);
  addr.sin_addr = ia;
  addr.sin_port = htons(port);

  int rc = ::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  assert(rc == 0);

  RpcClient cli(s);
  assert(cli.start());

  // notify/event：应该能让 server handler 被执行（不回包）
  assert(cli.notify("event_inc", json::object()));
  // 等待最多 500ms 看 event_cnt 是否递增
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
  while (std::chrono::steady_clock::now() < deadline) {
    if (event_cnt.load() >= 1) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  assert(event_cnt.load() >= 1);

  // 并发 100 个 add：验证 request_id 匹配没问题
  const int N = 100;
  std::vector<std::future<RpcResult>> futs;
  futs.reserve(N);
  for (int i = 0; i < N; ++i) {
    futs.emplace_back(std::async(std::launch::async, [&cli, i]() {
      return cli.call("add", json{{"a", i}, {"b", i + 1}}, 2000);
    }));
  }
  for (int i = 0; i < N; ++i) {
    RpcResult r = futs[i].get();
    assert(r.ok);
    assert(r.data["sum"].get<int>() == i + (i + 1));
  }

  // method_not_found
  {
    RpcResult r = cli.call("no_such_method", json::object(), 1000);
    assert(!r.ok);
    assert(r.error.code == static_cast<int>(RpcErrc::kMethodNotFound));
  }

  // timeout：sleep 200ms, timeout 50ms
  {
    RpcResult r = cli.call("sleep", json{{"ms", 200}}, 50);
    assert(!r.ok);
    assert(r.error.code == static_cast<int>(RpcErrc::kTimeout));
  }

  // timeout 后还能继续正常调用（验证晚到 response 不会污染后续）
  {
    RpcResult r = cli.call("ping", json::object(), 1000);
    assert(r.ok);
    assert(r.data["pong"].get<bool>() == true);
  }

  cli.stop();
  if (th.joinable()) th.join();
}

int main() {
  std::string err;
  assert(net_init(&err));
  assert(err.empty());

  test_framing_basic();
  test_rpc_v02();

  net_cleanup();
  return 0;
}
