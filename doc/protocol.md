# DeskIPC Protocol (v0.1)

本文档定义 DeskIPC 的消息格式（Message Framing）与基础语义（req/resp/event），用于在本机 IPC 传输（Named Pipe / UDS / TCP loopback）之上提供可靠的消息边界与 RPC 调用模型。

> 设计目标：**显式消息边界**、**可扩展**、**便于调试**、**实现简单**。

---

## 1. Terminology

- **Frame**：一条完整消息（Header + Body）
- **Header**：固定长度消息头，用于 framing
- **Body**：可变长度消息体（默认 JSON）
- **Request**：RPC 请求（带 request_id）
- **Response**：RPC 响应（带 request_id，对应请求）
- **Event**：通知消息（不需要 response，可选）

---

## 2. Encoding Rules

### 2.1 Byte Order
- 所有多字节整数均使用 **Little Endian**（与 Windows 生态一致）
- 若未来需要跨语言/跨端一致性，也可以切换为 Big Endian；本版本固定 LE

### 2.2 Alignment & Packing
- Header 为固定字节布局，必须按字节序列读写
- C/C++ 结构体实现时建议手动序列化，或使用 `#pragma pack(push, 1)` + 静态断言校验大小

### 2.3 Body Encoding
- v0.1 默认 `codec = JSON (UTF-8)`
- Header 中保留 `codec` 字段便于后续扩展（e.g. Protobuf）

---

## 3. Frame Format

每条消息由 **固定长度 Header（32 bytes）** + **Body（N bytes）** 构成。

```text
+------------------------------+--------------------+
| Header (32 bytes, fixed)     | Body (body_len)    |
+------------------------------+--------------------+
```

- Header：固定长度，用于 framing 与校验

- Body：可变长度消息体，长度由 body_len 指定

---

## 4. Header Layout (32 bytes)

| Offset | Size | Field        | Type    | Description |
|-------:|-----:|--------------|---------|-------------|
| 0      | 4    | magic        | u32     | Magic number: `0x43504944` ("DIPC" little-endian) |
| 4      | 2    | version      | u16     | Protocol version. v0.1 = `0x0001` |
| 6      | 2    | header_len   | u16     | Header length in bytes. v0.1 = `32` |
| 8      | 4    | body_len     | u32     | Body length in bytes |
| 12     | 1    | msg_type     | u8      | 1=req, 2=resp, 3=event |
| 13     | 1    | codec        | u8      | 1=json, 2=protobuf(reserved) |
| 14     | 2    | flags        | u16     | Bit flags (compression/encryption/reserved) |
| 16     | 8    | request_id   | u64     | Correlation id. req/resp 必须一致；event 可为 0 |
| 24     | 4    | reserved1    | u32     | Reserved for future use (0) |
| 28     | 4    | header_crc32 | u32     | Optional. v0.1 可置 0（未启用） |

### 4.1 `msg_type`
- `1` = REQUEST
- `2` = RESPONSE
- `3` = EVENT (one-way)

### 4.2 `codec`
- `1` = JSON (UTF-8)
- `2` = Protobuf (reserved)
- 其他值：保留

### 4.3 `flags` (bitset)
- bit0: `COMPRESSED`（v0.1 未实现）
- bit1: `ENCRYPTED`（v0.1 未实现）
- bit2: `HAS_CRC`（v0.1 未实现）
- others: reserved

> v0.1 建议：flags 全 0，`header_crc32` 全 0，先跑通闭环。

---

## 5. Body Schema (JSON, v0.1)

### 5.1 Request Body (msg_type = 1)
```json
{
  "method": "add",
  "params": { "a": 1, "b": 2 },
  "meta": {
    "deadline_ms": 2000,
    "trace_id": "optional"
  }
}
```
- method (string, required): 方法名
- params (object, optional): 参数
- meta (object, optional): 调试字段，可选

### 5.2 Response Body (msg_type = 2)

- 成功：
```json
{
  "ok": true,
  "data": { "sum": 3 }
}
```

- 失败：
```json
{
  "ok": false,
  "error": {
    "code": 1001,
    "message": "timeout"
  }
}
```

### 5.3 Event Body (msg_type = 3)
```json
{
  "event": "progress",
  "data": { "pct": 42 }
}
```

---

## 6. Size Limits & Validation

### 建议实现以下校验，避免内存攻击或读取错误：

- magic 必须匹配

- version 必须为支持的版本（v0.1 = 1）

- header_len 必须为 32（本版本）

- body_len 必须 <= MAX_BODY_LEN（建议 8MB 或 32MB，先保守）

- codec 必须为 1（json）

- 若校验失败：断开连接并记录日志

---

## 7. Framing Algorithm (Receiver)

### 接收端需支持半包/粘包：

- 维护一个 recv_buffer

- 若 recv_buffer.size < 32：继续读

- 读取前 32 字节解析 header

- 校验 header（magic/version/body_len 等）

- 若 recv_buffer.size < 32 + body_len：继续读

- 切出完整 frame：header + body

- 将 frame 投递到上层（RPC dispatcher）

- 从 buffer 移除已消费字节，继续循环解析

---

## 8. Request ID Rules

- Request：request_id 必须非 0（由 client 生成，单调递增或随机均可）

- Response：request_id 必须与 request 一致

- Event：request_id = 0（建议）

---

## 9. Error Codes (Suggested)

- 1000 = unknown_error

- 1001 = timeout

- 1002 = method_not_found

- 1003 = invalid_params

- 1004 = decode_error

- 1005 = internal_error

## 10. Compatibility Notes

- v0.1 固定 header=32 bytes，未来如扩展 header 可通过 header_len 向后兼容

- reserved1 / flags / header_crc32 为后续压缩、加密、校验预留

---
