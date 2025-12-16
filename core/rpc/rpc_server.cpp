#include "rpc_server.h"

#include <iostream>

namespace deskipc {

static inline std::string bytes_to_string(const std::vector<uint8_t>& v) {
  return std::string(reinterpret_cast<const char*>(v.data()), v.size());
}

void RpcServer::on(const std::string& method, Handler h) {
  handlers_[method] = std::move(h);
}

RpcResult RpcServer::dispatch(const std::string& method, const json& params) {
  auto it = handlers_.find(method);
  if (it == handlers_.end()) {
    return Err(RpcErrc::kMethodNotFound, "method_not_found");
  }
  try {
    return it->second(params);
  } catch (...) {
    return Err(RpcErrc::kInternalError, "internal_error");
  }
}

bool RpcServer::send_response(socket_t s, uint64_t req_id, const RpcResult& rr) {
  json body = MakeResponseBody(rr);
  const std::string payload = body.dump();

  FrameHeader h{};
  h.magic = kMagic;
  h.version = kVersion;
  h.header_len = kHeaderLen;
  h.msg_type = static_cast<uint8_t>(MsgType::kResponse);
  h.codec = static_cast<uint8_t>(Codec::kJson);
  h.flags = 0;
  h.request_id = req_id;
  h.reserved = 0;
  h.header_crc32 = 0;

  auto out = encode(h,
                    reinterpret_cast<const uint8_t*>(payload.data()),
                    payload.size());
  return send_all(s, out.data(), out.size());
}

void RpcServer::serve(socket_t s) {
  FrameDecoder decoder;
  uint8_t buf[4096];

  while (true) {
    int n = recv_some(s, buf, sizeof(buf));
    if (n <= 0) break;

    auto frames = decoder.feed(buf, static_cast<size_t>(n));
    // v0.2: framing 在 “半包/协议错误” 时都可能返回空，这里不做强 close，后续再升级 framing 才能区分

    for (auto& f : frames) {
      const auto mt = static_cast<MsgType>(f.header.msg_type);
      if (mt != MsgType::kRequest && mt != MsgType::kEvent) continue;

      // request 必须有 request_id；event 按约定 request_id=0（不强制，但 event 永不回包）
      if (mt == MsgType::kRequest && f.header.request_id == 0) continue;

      json req;
      try {
        req = json::parse(bytes_to_string(f.body));
      } catch (...) {
        if (mt == MsgType::kRequest) {
          (void)send_response(s, f.header.request_id, Err(RpcErrc::kParseError, "parse_error"));
        }
        continue; // event 不回包
      }

      if (!req.contains("method") || !req["method"].is_string()) {
        if (mt == MsgType::kRequest) {
          (void)send_response(s, f.header.request_id, Err(RpcErrc::kInvalidRequest, "invalid_request"));
        }
        continue;
      }

      const std::string method = req["method"].get<std::string>();
      const json params = req.contains("params") ? req["params"] : json::object();

      if (mt == MsgType::kEvent) {
        (void)dispatch(method, params); // notify/event：只执行，不回包
        continue;
      }

      RpcResult rr = dispatch(method, params);
      if (!send_response(s, f.header.request_id, rr)) {
        break;
      }
    }
  }

  sock_close(s);
}

} // namespace deskipc
