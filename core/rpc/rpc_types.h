#pragma once
#include <cstdint>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace deskipc {

using json = nlohmann::json;

// ---- Error codes (v0.2 minimal set) ----
enum class RpcErrc : int {
  kParseError      = 1000,
  kInvalidRequest  = 1001,
  kMethodNotFound  = 1002,
  kTimeout         = 1003,
  kConnectionLost  = 1004,
  kInternalError   = 1005,
};

struct RpcError {
  int code = 0;
  std::string message;
};

struct RpcResult {
  bool ok = false;
  json data = json::object();
  RpcError error{};
};

// helpers
inline RpcResult Ok(json data = json::object()) {
  RpcResult r;
  r.ok = true;
  r.data = std::move(data);
  return r;
}

inline RpcResult Err(RpcErrc code, std::string message) {
  RpcResult r;
  r.ok = false;
  r.error.code = static_cast<int>(code);
  r.error.message = std::move(message);
  return r;
}

// JSON encode/decode for body
inline json MakeRequestBody(const std::string& method, const json& params) {
  json j;
  j["method"] = method;
  j["params"] = params.is_null() ? json::object() : params;
  return j;
}

inline json MakeResponseBody(const RpcResult& rr) {
  json j;
  j["ok"] = rr.ok;
  if (rr.ok) {
    j["data"] = rr.data;
  } else {
    j["error"] = { {"code", rr.error.code}, {"message", rr.error.message} };
  }
  return j;
}

} // namespace deskipc
