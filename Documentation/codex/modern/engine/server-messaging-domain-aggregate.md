# Server Messaging Domain Aggregate Audit

Phase 150 checks the grouped messaging helpers as a domain after the flat
forwarding headers were removed. The goal is to prove the modern helpers compose
cleanly before any adapter-shrink pass touches legacy message call sites.

## Current Modern Helpers

- `server_message_envelope` owns generic command/string writers, simple envelope
  metadata, and shared recipient facts that can be projected into multicast,
  event, and voice recipient requests.
- `server_multicast_policy` owns multicast destination classification and
  recipient routing decisions.
- `server_event_playback_policy` owns event flag normalization, recipient
  filtering, event queue slot selection, and queue emit clamping.
- `server_frame_datagram` owns frame-side buffer transfer plans, reliable resend
  plans, and client frame send gates.
- `server_packet_entities_delta` owns packet-entity header and cursor planning.
- `server_text_messages`, `server_service_messages`, `server_sound_message`,
  `server_static_messages`, `server_resource_message`,
  `server_customization_message`, `server_userinfo_message`,
  `server_spawn_handshake`, and `server_voice_relay` own payload shape writers
  and plain policy decisions, not live network-buffer ownership.

## Aggregate Coverage Added

`tests/engine/server_messaging_domain.cpp` adds domain-level coverage for:

- reliable and spectator envelope choices composed with multicast recipient
  routing;
- shared recipient facts projected into multicast, event, and voice decisions
  while preserving policy differences between visibility and listener masks;
- multiple payload writers sharing a byte/bit scratch buffer shape without
  claiming `sizebuf_t` ownership;
- signon-envelope planning for `MSG_INIT` while keeping signon buffer mutation
  external;
- event update-slot selection, reliable resend planning, packet-entity delta
  planning, and frame datagram transfer decisions in one frame-adjacent
  scenario.

The existing focused tests remain the authority for individual edge cases and
binary payload details.

## Legacy-Owned Boundaries

Phase 150 deliberately keeps these legacy-owned:

- `sizebuf_t` allocation, mutation, overflow clearing, and ownership;
- `sv.datagram`, `sv.reliable_datagram`, `sv.signon`, per-client reliable
  buffers, and spectator datagrams;
- `client_frame_t`, `packet_entities_t`, live `edict_t`, and client mutation;
- final `Netchan_Transmit*`, `MSG_Write*`, and multicast send side effects.

Modern helpers should continue to return plans, routes, decisions, and payload
writer shapes. Runtime buffer ownership can move later only after fixture-backed
tests cover the exact legacy side effects.

## Validation

Phase 150 validation:

- focused messaging target set passed 14/14, including
  `test_engine_server_messaging_domain`;
- `.\waf.bat build --targets=test_engine_server_message_envelope` completed
  successfully;
- full `.\waf.bat build --alltests` passed 136/136.
