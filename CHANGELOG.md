- # Changelog

  ## v0.2.0 — 2025-12-17

  ### Added
  - Streaming framing decoder (half/sticky packets)
  - Single-connection TCP RPC with multiple in-flight requests
  - Client-side timeout handling and request_id correlation
  - Fire-and-forget event notifications (`MsgType::kEvent`)

  ### Fixed
  - Server dispatch for event frames (notifications are now handled)

  ### Notes
  - Late responses after client timeout are dropped on client side