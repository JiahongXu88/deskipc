#pragma once
#include "rpc_types.h"

#include "protocol/framing.h"
#include "transport/tcp/net.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace deskipc {

class RpcServer {
public:
  using Handler = std::function<RpcResult(const json& params)>;

  RpcServer() = default;

  // Register handler for method
  void on(const std::string& method, Handler h);

  // Serve on a connected socket (blocking loop)
  void serve(socket_t s);

private:
  RpcResult dispatch(const std::string& method, const json& params);

  bool send_response(socket_t s, uint64_t req_id, const RpcResult& rr);

private:
  std::unordered_map<std::string, Handler> handlers_;
};

} // namespace deskipc
