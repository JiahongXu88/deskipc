#include "rpc_client.h"

#include <iostream>

namespace deskipc {

static inline std::string bytes_to_string(const std::vector<uint8_t>& v) {
  return std::string(reinterpret_cast<const char*>(v.data()), v.size());
}

RpcClient::RpcClient(socket_t s) : sock_(s) {}

RpcClient::~RpcClient() {
  stop();
}

bool RpcClient::start() {
  if (sock_ == kInvalidSock) return false;
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) return true;

  stopping_.store(false);
  recv_thread_ = std::thread([this] { recv_loop(); });
  return true;
}

void RpcClient::stop() {
  if (!running_.load()) return;

  stopping_.store(true);
  close_socket();

  if (recv_thread_.joinable()) {
    recv_thread_.join();
  }

  // ensure pending cleared
  fail_all(Err(RpcErrc::kConnectionLost, "connection_lost"));
  running_.store(false);
}

uint64_t RpcClient::next_request_id() {
  return rid_.fetch_add(1);
}

RpcResult RpcClient::call(const std::string& method,
                          const json& params,
                          uint32_t timeout_ms) {
  if (sock_ == kInvalidSock || !running_.load()) {
    return Err(RpcErrc::kConnectionLost, "not_connected");
  }

  const uint64_t req_id = next_request_id();
  auto entry = std::make_shared<PendingEntry>();
  auto fut = entry->prom.get_future();

  {
    std::lock_guard<std::mutex> lk(mu_);
    pending_[req_id] = entry;
  }

  if (!send_request(req_id, method, params)) {
    // send failed => complete with connection_lost
    complete(req_id, Err(RpcErrc::kConnectionLost, "send_failed"));
    return fut.get();
  }

  // wait for response or timeout
  if (timeout_ms == 0) timeout_ms = 1;

  auto st = fut.wait_for(std::chrono::milliseconds(timeout_ms));
  if (st == std::future_status::ready) {
    return fut.get();
  }

  // timeout: try to complete; if receiver already completed, complete() will fail and fut will be ready
  complete(req_id, Err(RpcErrc::kTimeout, "timeout"));
  return fut.get();
}

bool RpcClient::notify(const std::string& method, const json& params) {
  if (sock_ == kInvalidSock || !running_.load()) return false;
  return send_event(method, params);
}

bool RpcClient::send_request(uint64_t req_id, const std::string& method, const json& params) {
  json body = MakeRequestBody(method, params);
  const std::string payload = body.dump();

  FrameHeader h{};
  h.magic = kMagic;
  h.version = kVersion;
  h.header_len = kHeaderLen;
  h.msg_type = static_cast<uint8_t>(MsgType::kRequest);
  h.codec = static_cast<uint8_t>(Codec::kJson);
  h.flags = 0;
  h.request_id = req_id;
  h.reserved = 0;
  h.header_crc32 = 0;

  auto out = encode(h,
                    reinterpret_cast<const uint8_t*>(payload.data()),
                    payload.size());
  return send_all(sock_, out.data(), out.size());
}

bool RpcClient::send_event(const std::string& method, const json& params) {
  json body = MakeRequestBody(method, params);
  const std::string payload = body.dump();

  FrameHeader h{};
  h.magic = kMagic;
  h.version = kVersion;
  h.header_len = kHeaderLen;
  h.msg_type = static_cast<uint8_t>(MsgType::kEvent);
  h.codec = static_cast<uint8_t>(Codec::kJson);
  h.flags = 0;
  h.request_id = 0; // event must be 0 per validate()
  h.reserved = 0;
  h.header_crc32 = 0;

  auto out = encode(h,
                    reinterpret_cast<const uint8_t*>(payload.data()),
                    payload.size());
  return send_all(sock_, out.data(), out.size());
}

void RpcClient::recv_loop() {
  uint8_t buf[4096];

  while (!stopping_.load()) {
    int n = recv_some(sock_, buf, sizeof(buf));
    if (n <= 0) {
      break;
    }

    auto frames = decoder_.feed(buf, static_cast<size_t>(n));
    // decoder returns {} on protocol error (buffer cleared). treat as fatal.
    if (frames.empty() && n > 0) {
      // Could be incomplete or protocol error; we cannot distinguish reliably here.
      // If protocol error happened, subsequent frames won't parse; in v0.2, treat as fatal:
      // (You can refine this later by adding an error flag to decoder)
    }

    for (const auto& f : frames) {
      on_frame(f);
    }
  }

  // socket closed / error: fail all pending
  fail_all(Err(RpcErrc::kConnectionLost, "connection_lost"));
  running_.store(false);
}

void RpcClient::on_frame(const Frame& f) {
  // only handle responses
  if (f.header.msg_type != static_cast<uint8_t>(MsgType::kResponse)) return;
  if (f.header.request_id == 0) return;

  RpcResult rr;
  try {
    const std::string body = bytes_to_string(f.body);
    json j = json::parse(body);

    if (!j.contains("ok") || !j["ok"].is_boolean()) {
      rr = Err(RpcErrc::kInvalidRequest, "invalid_response");
    } else if (j["ok"].get<bool>()) {
      rr.ok = true;
      rr.data = j.contains("data") ? j["data"] : json::object();
    } else {
      rr.ok = false;
      if (j.contains("error") && j["error"].is_object()) {
        rr.error.code = j["error"].value("code", static_cast<int>(RpcErrc::kInternalError));
        rr.error.message = j["error"].value("message", "error");
      } else {
        rr.error.code = static_cast<int>(RpcErrc::kInternalError);
        rr.error.message = "error";
      }
    }
  } catch (...) {
    rr = Err(RpcErrc::kParseError, "response_parse_error");
  }

  // If timed out already, complete() returns false and we drop it.
  complete(f.header.request_id, std::move(rr));
}

bool RpcClient::complete(uint64_t req_id, RpcResult rr) {
  std::shared_ptr<PendingEntry> entry;

  {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = pending_.find(req_id);
    if (it == pending_.end()) return false;
    entry = it->second;

    // state machine guard
    if (entry->done.exchange(true)) {
      // already completed by another path
      return false;
    }

    // erase pending now so late responses are dropped
    pending_.erase(it);
  }

  try {
    entry->prom.set_value(std::move(rr));
  } catch (...) {
    // ignore double-set or broken promise
  }
  return true;
}

void RpcClient::fail_all(RpcResult rr) {
  std::unordered_map<uint64_t, std::shared_ptr<PendingEntry>> tmp;
  {
    std::lock_guard<std::mutex> lk(mu_);
    tmp.swap(pending_);
  }

  for (auto& kv : tmp) {
    auto& entry = kv.second;
    if (!entry) continue;
    if (entry->done.exchange(true)) continue;
    try { entry->prom.set_value(rr); } catch (...) {}
  }
}

void RpcClient::close_socket() {
  if (sock_ == kInvalidSock) return;

#ifdef _WIN32
  shutdown(sock_, SD_BOTH);
#else
  shutdown(sock_, SHUT_RDWR);
#endif
  sock_close(sock_);
  sock_ = kInvalidSock;
}

} // namespace deskipc
