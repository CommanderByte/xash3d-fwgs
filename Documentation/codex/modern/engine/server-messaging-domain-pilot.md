# Server Messaging Domain Pilot

Phase 138 physically groups the target-neutral server messaging helpers after
the Phase 136 layout plan and the Phase 137 resource-domain pilot.

## Consolidated Domain

The messaging domain now owns modern helpers for:

- common server message command/string envelopes;
- compact `svc_*` payload writers for text, service, sound, static, userinfo,
  voice, resource-list, customization, and spawn/signon messages;
- multicast, voice, and event recipient policy;
- frame datagram send gates;
- packet-entity delta cursor planning.

These files moved from the flat `src/engine/server/` layer into
`src/engine/server/messaging/`:

- `server_message_envelope.cpp`
- `server_text_messages.cpp`
- `server_service_messages.cpp`
- `server_sound_message.cpp`
- `server_static_messages.cpp`
- `server_userinfo_message.cpp`
- `server_voice_relay.cpp`
- `server_resource_message.cpp`
- `server_customization_message.cpp`
- `server_spawn_handshake.cpp`
- `server_multicast_policy.cpp`
- `server_event_playback_policy.cpp`
- `server_frame_datagram.cpp`
- `server_packet_entities_delta.cpp`

The matching C++ contracts moved into
`src/include/engine/server/messaging/`. A later cleanup pass removed the
temporary flat forwarding headers, so internal code should include the
canonical `engine/server/messaging/...` paths.

## Not Moved

Game DLL message-session helpers stay outside this domain for now:

- `game_dll_message_bridge`
- `game_dll_message_session`
- `game_dll_user_message_registry`
- `game_dll_payload_policy`
- `game_dll_output_policy`

They are message-related, but their primary compatibility constraint is the
game DLL callback ABI and `svgame` state, so Phase 139 owns that grouping.

## Legacy-Owned Boundaries

The following surfaces remain legacy-owned:

- `sizebuf_t`, `MSG_*`, and `MSG_BeginServerCmd`;
- `sv.multicast`, `sv.datagram`, `sv.reliable_datagram`, `sv.signon`, and
  per-client netchan buffers;
- signon and frame datagram mutation;
- netchan sends and fragmentation;
- BSP visibility masks and client iteration;
- rendered in-game console ownership;
- game DLL callback table publication and callback ordering.

The modern messaging helpers should keep consuming plain request structs,
recipient facts, and `NetworkBitBuffer` test buffers. They should not own live
server buffers, protocol routing side effects, or `server.h`.

## Aggregate Coverage

`tests/engine/server_message_envelope.cpp` now covers representative message
families across the grouped domain:

- print, service, sound, static decal, voice, signon, userinfo, resource-list,
  and customization payloads with command envelopes;
- shared recipient facts translated through multicast, event playback, and
  voice relay policy;
- frame datagram send/resend facts and packet-entity delta cursor facts.

No adapter merge was made in this phase. The existing adapter split is still
clearer than a shared helper because live destination buffers and side effects
remain different at each legacy call site.

## Validation

- Focused messaging-domain build:
  `.\waf.bat build --targets=test_engine_server_message_envelope,test_engine_server_text_messages,test_engine_server_service_messages,test_engine_server_sound_message,test_engine_server_static_messages,test_engine_server_userinfo_message,test_engine_server_voice_relay,test_engine_server_resource_message,test_engine_server_customization_message,test_engine_server_spawn_handshake,test_engine_server_multicast_policy,test_engine_server_event_playback_policy,test_engine_server_frame_datagram,test_engine_server_packet_entities_delta`
  passed 14/14.
- `.\waf.bat build --alltests` passed 129/129.
- Runtime smoke via `scripts/run-phase-validation.ps1 -SkipFocused -SkipFullTests`
  built `xash`, refreshed `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit`, reached first frame
  in 0.497 seconds, and stopped with reason `command` at May 11 2026
  13:58:36 local time.
