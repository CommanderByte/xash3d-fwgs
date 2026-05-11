# Server Messaging Adapter Shrink Review

Phase 120 reviewed the message-related C++ adapters after the Phase 119
`server_message_envelope` pilot.

## Reviewed Adapters

The review covered the legacy-facing message adapters that translate C call
sites into modern payload helpers:

- `server_text_messages_adapter`;
- `server_service_messages_adapter`;
- `server_sound_message_adapter`;
- `server_static_messages_adapter`;
- `server_spawn_handshake_adapter`;
- `server_voice_relay_adapter`;
- `server_resource_message_adapter`;
- `server_customization_message_adapter`;
- `server_userinfo_message_adapter`.

The game DLL message session and user-message registry adapters were reviewed
as nearby messaging code, but were intentionally left alone. They publish the
game DLL callback boundary and carry message-size/session policy that should be
handled in the later game DLL bridge phases.

## Shrink Applied

The duplicated adapter-only conversion from:

- raw `unsigned char *` plus bit counts into `NetworkBitBuffer`; and
- `NetworkBitBuffer` state into `{ current_bit, overflow }` write results

now lives in `engine/server/server_message_adapter_shared.hpp`.

The exported C functions and legacy result structs remain in their existing
headers. This keeps call sites in `sv_game.c` and related legacy files stable
while reducing repeated glue inside the adapter implementations.

## Explicitly Not Merged

The pass did not merge:

- packet buffers owned by `sv.multicast`, client netchannels, signon buffers, or
  datagrams;
- netchan writes and client packet sends;
- `pfnMessageBegin()` / `pfnMessageEnd()` game DLL callback publication;
- user-message registration and rewrite policy;
- rendered-console or in-game console routing.

Those areas still need their own ownership pass because they carry ABI,
ordering, or live engine-state behavior beyond simple payload serialization.

## Follow-Up

Phase 121 should treat game DLL bridge grouping as a separate boundary decision.
If future message adapters need more shared code, prefer helper functions that
preserve the C adapter shape and keep live engine state outside the helper.
