# Server Messaging Aggregate Pilot

Phase 119 adds the first target-neutral aggregate helper for server messaging.
It intentionally stays smaller than a `server/messaging` submodule.

## Helper

`server_message_envelope` owns two narrow concepts:

- common command-byte and NUL-terminated string envelope writers used by
  multiple `svc_*` payload helpers;
- a plain `ServerMessageRecipientFacts` fixture that can be translated into
  multicast, event-playback, and voice-recipient request structs.

The helper does not own live packet buffers, netchan sends, `sv.multicast`,
game DLL message sessions, user-message rewrites, or rendered-console routing.

## Routed Writers

The following modern payload helpers now use the shared envelope writer for
their command bytes, NUL strings, or byte fields:

- `server_text_messages`;
- `server_service_messages`;
- `server_sound_message`;
- `server_static_messages`;
- `server_voice_relay`;
- `server_spawn_handshake`.

The payload-specific field order remains in each focused helper. For example,
sound flags and coordinate encoding stay in `server_sound_message`, while
serverdata hull serialization stays in `server_spawn_handshake`.

## Aggregate Test

`tests/engine/server_message_envelope.cpp` covers:

- representative complete messages for print, setview, sound, BSP decal, voice
  relay, and signon-number payloads;
- the shared command/string envelope helpers;
- overflow behavior through `NetworkBitBuffer`;
- the same plain recipient facts translated through multicast, event playback,
  and voice relay policy requests;
- intentional policy differences, such as voice relay ignoring visibility while
  multicast and event playback honor it.

## Decision

The aggregate helper is useful as test and payload glue, but it is not enough
to justify merging message adapters yet. Phase 120 should review adapter shape
after this helper and keep game DLL callback publication and netchan writes
explicit at the legacy boundary.
