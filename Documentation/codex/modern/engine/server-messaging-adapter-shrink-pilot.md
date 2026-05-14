# Server Messaging Adapter Shrink Pilot

Phase 151 reduces repeated mechanical glue in messaging adapters after the
Phase 150 aggregate test proved the grouped message helpers compose cleanly.

## Repeated Adapter Pattern

Several adapters repeated the same sequence:

1. create a `NetworkBitBuffer` over legacy-owned memory;
2. call one modern payload writer;
3. convert the final cursor and overflow flag into a legacy C result struct.

This appeared in text, service, voice, sound, static, spawn-handshake, userinfo,
resource, and customization message adapters. The pattern was mechanical and did
not own `sizebuf_t`, choose a destination buffer, clear overflows, or send data.

## Shared Glue Added

`engine/server/server_message_adapter_shared.hpp` now provides
`WriteWithNetworkBitBuffer<WriteResult>()`. It centralizes only the repeated
buffer-wrapper/result conversion pattern:

- negative bit sizes and cursors continue to be clamped by
  `MakeNetworkBitBuffer()`;
- payload writers still receive a normal `NetworkBitBuffer &`;
- legacy adapters still expose the same C result structs;
- no live `sizebuf_t` or datagram lifetime is hidden behind the helper.

`tests/engine/server_message_adapter_shared.cpp` covers cursor propagation,
overflow propagation, and negative cursor clamping for the shared helper.

## Adapter Updates

The following adapters now route payload writes through the shared helper:

- `server_text_messages_adapter.cpp`;
- `server_service_messages_adapter.cpp`;
- `server_sound_message_adapter.cpp`;
- `server_static_messages_adapter.cpp`;
- `server_spawn_handshake_adapter.cpp`;
- `server_userinfo_message_adapter.cpp`;
- `server_voice_relay_adapter.cpp`;
- `server_resource_message_adapter.cpp`;
- `server_customization_message_adapter.cpp`.

Adapters that only translate decisions or touch unrelated resource glue were
left alone.

## Legacy-Owned Boundaries

This phase intentionally keeps these in legacy call sites:

- `sizebuf_t` allocation, storage, overflow clearing, and ownership;
- `sv.datagram`, `sv.reliable_datagram`, `sv.signon`, per-client reliable
  buffers, and spectator datagrams;
- live send calls and `MSG_Write*` call-site ordering;
- client, frame, edict, and packet lifetime.

The helper is adapter glue, not a messaging runtime facade.

## Validation

- `test_engine_server_message_adapter_shared` passed.
- Focused messaging/adapter executables passed 16/16.
- Full `.\waf.bat build --alltests` passed 137/137.
- `.\scripts\run-phase-validation.ps1 -FocusedTarget
  test_engine_server_message_adapter_shared -SkipFullTests
  -AllowSmokeNonZeroExit -StopRunningXash` passed.
- Smoke reached first frame in 0.509 seconds and stopped with reason `command`.
