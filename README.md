# DeskIPC

> A lightweight cross-platform IPC & RPC framework for desktop applications (C++17)

DeskIPC 是一个 **面向桌面应用的跨进程通信（IPC）与轻量级 RPC 框架**，用于构建 **多进程桌面程序中的可靠通信机制**。

该项目以 **C++17** 实现，核心库不依赖 Qt，支持在 **Windows / Linux / macOS** 上使用，适用于 Qt / Flutter / Electron / 原生 C++ 等桌面应用架构。

---

## Background & Motivation

在真实的桌面客户端项目中，常见的架构形态是：

- **主进程（UI / 控制逻辑）**
- **子进程（耗时计算 / 高风险模块 / 第三方组件）**

例如：
- Qt 主程序 + CEF 子进程
- UI 进程 + 大文件解析 / 索引构建进程
- 主进程 + 插件 / 扩展进程

在这些场景下，开发者通常会遇到以下问题：

- 本机进程通信需要 **低延迟、高可靠性**
- 直接使用 socket/pipe 容易引入 **粘包、乱序、阻塞**
- 缺乏统一的 **请求-响应（RPC）模型**
- 子进程异常退出或卡死，容易影响主进程稳定性
- HTTP / gRPC 等方案对桌面 IPC 来说 **过重且不合适**

DeskIPC 的目标是：

> **为桌面应用提供一套工程可控、边界清晰、易于调试的 IPC + RPC 解决方案。**

---

## Design Goals

- **Desktop-first**  
  面向本机进程通信，而非分布式系统

- **Lightweight**  
  无需引入大型依赖或运行时环境

- **Explicit Boundaries**  
  显式的消息边界、超时、错误处理

- **Process Isolation Friendly**  
  适合主进程 / Worker 进程模型

- **Cross-platform**  
  支持主流桌面操作系统

---

## Architecture Overview

```mermaid
flowchart TB
  subgraph GUI["GUI Process (Qt / CLI / Flutter)"]
    UI["UI / Controller"]
    C1["RPC Client"]
    UI --> C1
  end

  subgraph CORE["DeskIPC Core"]
    FR["Message Framing"]
    RPC["RPC Layer"]
    EM["Timeout / Error Model"]
    TL["Transport Layer"]
    FR --> RPC --> EM --> TL
  end

  subgraph WORKER["Worker Process"]
    S1["RPC Server"]
    T1["Heavy / Unsafe Tasks"]
    S1 --> T1
  end

  C1 -- "RPC (req/resp)" --> FR
  TL -- "IPC" --> S1
```

---

## Core Features

### Transport Layer
- Windows: **Named Pipe**
- Linux / macOS: **Unix Domain Socket (UDS)**
- TCP (loopback) for development & debugging (optional)

### Protocol & Framing
- 固定长度 Header + Body（显式消息边界）
- 处理半包 / 粘包
- 请求-响应通过 `request_id` 关联
- 可扩展的版本与 flags（为压缩 / 加密 / 编码预留）

### RPC Layer
- Request / Response 调用模型
- 超时控制（timeout）与错误模型（error code + message）
- 并发 in-flight 请求（同一连接上多路复用）
- 支持通知类消息（event / one-way，可选）

### Process-oriented
- 面向多进程桌面架构：UI 进程稳定优先
- Worker 崩溃 / 卡死不拖垮主进程（可配合 process manager 自动重启）

---

## Quick Start

> 说明：v0.1 阶段优先提供 CLI 示例，先把 framing + RPC 闭环跑通；  
> Qt GUI Demo 会在后续里程碑加入。

### Build

**Requirements**
- C++17 compiler
- CMake >= 3.15

```bash
git clone https://github.com/<yourname>/deskipc.git
cd deskipc
cmake -S . -B build
cmake --build build -j
```
### Run Examples (CLI)

**启动 Worker（RPC Server）：**
```bash
./build/examples/worker_cli/worker_cli
```
**启动 Parent（RPC Client）并发起调用:**
```bash
./build/examples/parent_cli/parent_cli
```
---

## Project Structure

```text
deskipc/
├── core/
│   ├── include/deskipc/          # Public APIs
│   ├── transport/                # namedpipe / uds / tcp
│   ├── protocol/                 # framing / codec
│   ├── rpc/                      # client / server, timeout
│   ├── runtime/                  # io loop / timers / thread pool
│   └── common/                   # logging, errors, utils
├── examples/
│   ├── worker_cli/               # Worker process (RPC server)
│   ├── parent_cli/               # Parent process (RPC client)
│   └── parent_qt_gui/            # Qt GUI demo (optional)
├── tests/                        # Unit tests
├── benchmarks/                   # Benchmarks
└── docs/                         # protocol.md / architecture.md
```

## Roadmap

- v0.1: Framing + RPC (TCP / loopback) + CLI demo (ping / add / sleep)
- v0.2: Timeout / cancellation + concurrent calls + basic unit tests
- v0.3: Windows Named Pipe transport
- v0.4: Unix Domain Socket transport (Linux / macOS)
- v0.5: Qt GUI demo (parent) + worker process manager (restart / handshake)
- v0.6: Benchmarks + CI (Windows / Linux) + docs polishing

---

## Design Notes

### Why not HTTP / gRPC? 
- DeskIPC 面向 本机 IPC 场景，更关注低延迟、显式消息边界、进程隔离与可控的错误 / 超时模型。
- HTTP / gRPC 对桌面 IPC 往往过重，并引入额外依赖与调试成本。

### Why framing first?
- Framing 是 IPC 的“消息边界协议”。一旦稳定，上层 RPC 与下层 transport 的替换成本会非常低。

### Qt is optional
- 核心库不依赖 Qt。
- Qt 仅用于提供一个更贴近真实桌面客户端的 GUI 示例。

---

## License
> MIT License
---
