# DeskIPC

A small C++17 IPC/RPC library for desktop apps (UI ↔ Worker).  
**v0.2** focuses on: **single TCP connection**, **multi in-flight RPC**, **request_id correlation**, **timeout**, **minimal error model**.

> Transport: TCP loopback (v0.2).  
> Future: Named Pipes / UDS, multi-connection server, backpressure, cancel, logging (v0.3+).

---

## Features (v0.2)

- Framing protocol (fixed 32-byte header + JSON body)
  - Handles sticky/half packets (streaming decoder)
- RPC over a single connection
  - Multiple in-flight requests on the same socket
  - Request/Response correlation by `request_id`
  - Client-side timeout (`RpcErrc::kTimeout`)
- Fire-and-forget notification (Event)
  - `RpcClient::notify(method, params)` sends `MsgType::kEvent` (no response)
  - Server dispatches event handlers but does not reply

---

## Build

### Requirements
- CMake 3.15+
- C++17 compiler
- Windows: Winsock2
- Linux/macOS: POSIX sockets

### Build commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDESKIPC_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```
---

## Run Examples
> Example names may differ in your repo. Adjust executable names accordingly.

**Start worker/server**
#### Linux / macOS
```bash
./build/examples/worker_cli/worker_cli
```
#### Windows(MSVC)
```bash
build\examples\worker_cli\Debug\worker_cli.exe
```
**Run parent/client**
#### Linux / macOS
```bash
./build/examples/parent_cli/parent_cli
```
#### Windows(MSVC)
```bash
build\examples\parent_cli\Debug\parent_cli.exe
```
---

## Run Tests

To run the tests locally, use the following command:

#### Linux / macOS
```bash
ctest --test-dir build -C Debug --output-on-failure
```

#### Windows(MSVC)
```bash
ctest --test-dir build --output-on-failure
```
This command will run all the tests in the build directory and show output on failure.

---

## Wire format (v0.2)
**FrameHeader (32 bytes)**

### Defined in core/protocol/frame_header.h:

- magic: "DIPC" little-endian (0x43504944)
- version: 0x0001
- header_len: 32
- body_len: JSON byte length
- msg_type:
  - kRequest = 1
  - kResponse = 2
  - kEvent = 3
- codec: kJson = 1
- request_id:
  - Request/Response: non-zero
  - Event: 0
 
 ---

 ## Body schema (JSON)
 ### Request / Event body (same shape):
 
 ```text
{
  "method": "add",
  "params": { "a": 1, "b": 2 }
}
```

### Response body:

```text
{ "ok": true, "data": { "sum": 3 } }
```

### or error:

```text
{ "ok": false, "error": { "code": 1002, "message": "method_not_found" } }
```

---

## Error codes (v0.2)
### From core/rpc/rpc_types.h:

```text

| Code | Name            | Meaning                                            |
| ---- | --------------- | -------------------------------------------------- |
| 1000 | kParseError     | JSON parse failed                                  |
| 1001 | kInvalidRequest | Missing/invalid `method` or invalid request schema |
| 1002 | kMethodNotFound | No handler registered for `method`                 |
| 1003 | kTimeout        | Client timed out waiting for response              |
| 1004 | kConnectionLost | Socket closed / send failed / disconnected         |
| 1005 | kInternalError  | Handler exception or internal failure              |

```

---

## Notes

- Timeout semantics:
  - RpcClient::call() returns kTimeout when deadline is reached.
  - Late responses may arrive after timeout; client drops them (request removed from pending map).
- Event semantics:
  - RpcClient::notify() does not wait for any response.
  - Server executes handler if registered; parse errors/events are not replied.
 
---

## Project layout

```text

deskipc/
├── CMakeLists.txt
├── README.md
├── CHANGELOG.md            # v0.2 开始有，后续版本累积
│
├── core/                   # 核心库（deskipc_core）
│   ├── include/
│   │   └── deskipc/        # 对外 Public API（v0.3 开始逐步稳定）
│   │
│   ├── protocol/           # 线协议 & framing（纯协议层）
│   │
│   ├── transport/          # 传输层
│   │   └── tcp/            # v0.2 仅 TCP；v0.3+ 可加 pipe/uds
│   │
│   ├── rpc/                # RPC 语义层（client/server）
│   │
│   ├── runtime/            # 运行时设施（v0.3+ 才会逐步丰满）
│   │   # executor / timer / logger / metrics 等
│   │
│   └── common/             # 通用工具（小而克制）
│
├── examples/               # 示例（教学 & 验证）
│   ├── worker_cli/
│   ├── parent_cli/
│   └── parent_qt_gui/
│
├── tests/                  # 单元 / 集成测试
│   └── deskipc_tests.cpp
│
├── benchmarks/             # 性能基准（非必须，留好位置）
│
├── docs/                   # 设计文档
│   ├── rpc.md
│   └── design.md           # 可选：协议/架构说明
│
└── third_party/             # 第三方（头文件或子模块）
    └── nlohmann/

```
---

## Roadmap

### v0.3 (Planned)
- Multi-connection RPC server (multiple clients/sessions)
- Server-side concurrent execution (thread pool executor)
- Request cancellation (best-effort) and overload protection
- Pluggable logging infrastructure

### v0.4 (Exploratory)
- Additional transports (Named Pipes on Windows, UDS on Unix)
- Async client API (non-blocking calls)
- Basic metrics and observability hooks

### Non-goals
- Distributed RPC or cross-machine service discovery
- Heavyweight frameworks or code generation

---

## License
> MIT License
---
