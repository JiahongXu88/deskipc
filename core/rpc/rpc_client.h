#pragma once
#include "rpc_types.h"

#include "protocol/framing.h"
#include "transport/tcp/net.h"

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace deskipc {

class RpcClient {
public:
  explicit RpcClient(socket_t s);
  ~RpcClient();

  RpcClient(const RpcClient&) = delete;
  RpcClient& operator=(const RpcClient&) = delete;

  // Start background recv loop.
  bool start();
  // Stop recv loop; fail all pending with connection_lost.
  void stop();

  // Synchronous call (internally uses pending + future)
  RpcResult call(const std::string& method,
                 const json& params,
                 uint32_t timeout_ms);

  // Optional: fire-and-forget notification (msg_type=event, no request_id)
  bool notify(const std::string& method, const json& params);

  bool is_running() const { return running_.load(); }

private:
  struct PendingEntry {
    std::promise<RpcResult> prom;
    std::atomic<bool> done{false}; // state machine: pending -> completed/timedout/connlost
  };

  uint64_t next_request_id();

  bool send_request(uint64_t req_id, const std::string& method, const json& params);
  bool send_event(const std::string& method, const json& params);

  void recv_loop();
  void on_frame(const Frame& f);

  bool complete(uint64_t req_id, RpcResult rr);
  void fail_all(RpcResult rr);

  void close_socket();

private:
  socket_t sock_ = kInvalidSock;

  std::atomic<bool> running_{false};
  std::atomic<bool> stopping_{false};
  std::thread recv_thread_;

  FrameDecoder decoder_;
  std::atomic<uint64_t> rid_{1};

  std::mutex mu_;
  std::unordered_map<uint64_t, std::shared_ptr<PendingEntry>> pending_;
};

} // namespace deskipc
