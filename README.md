# DeskIPC

> A lightweight, cross-platform IPC & RPC framework for desktop applications(c++ 17).

DeskIPC 是一个 **面向桌面应用的跨进程通信（IPC）与轻量级 RPC 框架**，用于解决多进程桌面程序中 **通信可靠性、进程隔离与工程可维护性** 问题。

该项目以 **C++17** 实现，核心不依赖 Qt，支持在 **Windows / Linux / macOS** 上使用，可用于 Qt / Flutter / Electron / 原生 C++ 桌面程序。

---

## Why DeskIPC

在真实的桌面客户端项目中，常常会遇到以下问题：

- UI 主进程需要调用 **子进程** 执行耗时或高风险任务
- 本机进程通信需要 **高性能、低延迟**，但又不希望引入 HTTP / gRPC 等重型方案
- 需要处理 **粘包、乱序、超时、重连、进程崩溃** 等工程问题
- 希望通信方式 **像函数调用一样简单（RPC）**

DeskIPC 的目标不是“再造一个通用 RPC 框架”，而是：

> **为桌面应用提供一套工程可控、可调试、可扩展的 IPC + RPC 解决方案。**

---

## Typical Use Cases

- Qt / Flutter / Electron 主进程 + Worker 子进程
- CEF / Chromium 子进程通信
- 大文件解析、索引构建、音视频处理等 **后台任务进程**
- 插件进程 / 不可信代码隔离
- 崩溃敏感或高负载模块的进程拆分

---

## Architecture Overview

