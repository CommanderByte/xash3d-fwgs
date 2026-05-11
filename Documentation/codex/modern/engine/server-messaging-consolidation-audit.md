# Server Messaging Consolidation Audit

Phase 118 audits the server messaging helpers after the resource-transfer
grouping work. The goal is to avoid turning many useful small helpers into a
single vague "messaging" bucket while still identifying the real concepts that
should consolidate.

## Current Answer

Server messaging should consolidate, but not as one mega-module.

The current helpers split into three real concepts:

1. **Payload and envelope writers**
   Stable byte/bit layouts for specific `svc_*` commands.
2. **Destination and recipient policies**
   Decisions about who receives a message and which legacy buffer receives it.
3. **Session and bridge state**
   Game DLL `pfnMessageBegin()` / `pfnMessageEnd()`, user-message registration,
   rewrite behavior, and live `sv.multicast` ownership.

Phase 119 should start with aggregate tests around the first two concepts:
message command/envelope writers plus destination/recipient policy. It should
not move game DLL message sessions yet.

## Legacy Ownership Map

### `sv_cmds.c`

`sv_cmds.c` owns operator-facing text helpers:

- `SV_ClientPrintf()` writes `svc_print` to one client's reliable netchan
  message;
- `SV_BroadcastPrintf()` writes `svc_print` to spawned real clients and echoes
  to the server console;
- `SV_BroadcastCommand()` writes `svc_stufftext` to `sv.reliable_datagram`.

Legacy-owned state and side effects:

- formatting limits and command validation;
- client iteration and fake-client/spawned-client filtering;
- destination buffer choice;
- console echo behavior.

### `sv_client.c`

`sv_client.c` owns client setup, compact service messages, userinfo updates,
resource-list sends, downloads, pause handling, and voice relay:

- `SV_FailDownload()` writes `svc_filetxferfailed`;
- `SV_BuildReconnect()` writes `svc_stufftext` with `reconnect\n`;
- `SV_UpdateClientView()` writes `svc_setview`;
- `SV_TogglePause()` writes `svc_setpause` and performs pause state mutation;
- `SV_SendServerdata()`, `SV_New_f()`, `SV_Spawn_f()`, and `SV_Begin_f()` own
  setup and signon sequencing;
- `SV_ParseVoiceData()` owns incoming `clc_voicedata` parsing and outgoing
  `svc_voicedata` fanout.

Legacy-owned state and side effects:

- command argument reads and client state gates;
- game DLL connect/spawn callbacks;
- `sv.signon`, `sv.reliable_datagram`, `cl->datagram`, and
  `cl->netchan.message`;
- netchan fragmentation and send calls;
- cvar reads, pause mutation, and drop/print side effects.

### `sv_game.c`

`sv_game.c` is the widest messaging owner:

- `SV_Multicast()` decodes `MSG_*` destinations and flushes `sv.multicast`;
- `SV_StartSound()` / `SV_BuildSoundMsg()` write sound payloads and route them
  through multicast;
- static decal and static entity writers target signon/reliable buffers;
- game DLL message callbacks build user messages in `sv.multicast`;
- game DLL callbacks also expose client command, centerprint, setview,
  crosshair angle, sound fade, cvar query, and event playback surfaces.

Legacy-owned state and side effects:

- `sv.multicast` lifetime and rewrite positions;
- `MSG_*` destination constants, `MSG_ONE` entity lookup, and visibility mask
  construction;
- game DLL ABI callback table order;
- user-message size validation and compatibility rewrites;
- direct writes into client reliable buffers and signon data.

### `sv_frame.c`

`sv_frame.c` owns per-frame message assembly and final fanout:

- `SV_EmitEvents()` serializes queued unreliable events;
- `SV_SendClientDatagram()` builds each client's frame datagram;
- `SV_UpdateToReliableMessages()` resends userinfo/movevars and fans out
  server reliable/unreliable/spectator datagrams;
- `SV_SendClientMessages()` schedules and transmits netchan packets.

Legacy-owned state and side effects:

- entity snapshot generation and delta encoding;
- pings, choke, clientdata, packet entities, and frame timing;
- netchan transmit/fragment APIs;
- reliable overflow drops and console diagnostics.

## Existing Modern Helper Groups

### Payload And Envelope Writers

These helpers mostly own command numbers and byte/bit layout:

- `server_text_messages`: `svc_print`, `svc_stufftext`;
- `server_service_messages`: file-transfer failure, reconnect, setview,
  setpause, voiceinit;
- `server_sound_message`: `svc_sound`, `svc_restoresound`;
- `server_static_messages`: `svc_bspdecal` and static-entity admission;
- `server_voice_relay`: `svc_voicedata` payload plus voice gates;
- `server_userinfo_message`: `svc_updateuserinfo` payload;
- `server_resource_message`: `svc_resourcelist` rows;
- `server_customization_message`: `svc_customization` payload;
- `server_spawn_handshake`: `svc_serverdata` and `svc_signonnum`.

These are good candidates for aggregate tests because they all serialize to a
`NetworkBitBuffer`, but only the command-byte/envelope part is broadly shared.
The payload layouts themselves should stay separate.

### Destination And Recipient Policies

These helpers own delivery decisions, not final writes:

- `server_multicast_policy`: `MSG_*` destination and recipient route plans;
- `server_voice_relay`: input gates and per-recipient voice relay decisions;
- `server_event_playback_policy`: playback flags, recipient admission, queue
  slot selection, and emit-count clamp;
- `server_frame_datagram`: end-of-frame buffer transfer and send-loop gates.

These helpers are related but not identical. They share vocabulary such as
spawned client, fake client, visibility pass, reliable/unreliable delivery, and
datagram capacity. A common recipient model may eventually be useful, but it
should be proven by aggregate tests before it becomes code.

### Session And Bridge State

These helpers sit near game DLL bridge ownership:

- `game_dll_message_session`;
- `game_dll_user_message_registry`;
- `game_dll_payload_policy`;
- `game_dll_output_policy`.

They should not be merged into the server messaging module yet. They have to
preserve game DLL ABI callback order, `svgame` state, user-message rewrite
compatibility, and exported callback behavior.

## Separation Rules

### Payload Writers

Target-neutral C++ may own:

- command-byte constants;
- payload field order;
- bit widths and optional-field flags;
- small admission decisions tightly coupled to payload size or command shape.

Legacy C should still own:

- live destination buffer selection;
- formatted text generation and command validation;
- entity, client, edict, and cvar access;
- `Host_Error()`, `Con_Printf()`, `SV_DropClient()`, and callback side effects.

### Recipient Routing

Target-neutral C++ may own:

- destination classification from plain values;
- recipient skip reasons from adapter-provided booleans;
- capacity/overflow action plans.

Legacy C should still own:

- BSP visibility mask generation;
- portal camera checks;
- `svs.clients` iteration;
- writes into `sv.signon`, `sv.datagram`, `sv.spec_datagram`,
  `sv.reliable_datagram`, `cl->datagram`, and `cl->netchan.message`;
- netchan transmit and fragmentation.

### Rendered Console

The rendered in-game console is not a server messaging sink for Phase 118.
`engine/client/console.c` stays legacy-owned until a client/rendering console
phase exists. Server print payloads can be tested as `svc_print`, but rendered
console scrollback, notify lines, keyboard handling, and drawing remain out of
scope.

## Common Concepts Worth Testing

1. **Message envelope**
   A command byte plus payload writer. Many helpers already expose
   `Write*Message()` functions, but there is no aggregate test proving command
   identities across text, service, sound, static, voice, and spawn setup.

2. **Destination class**
   Reliable, unreliable, signon/init, spectator, and single-client destinations
   appear in multicast, spawn setup, frame fanout, and service-message call
   sites.

3. **Recipient facts**
   Spawned state, fake-client filtering, edict presence, group filtering,
   visibility, listener masks, and datagram capacity recur across multicast,
   voice relay, and event playback.

4. **Adapter write result**
   Several adapters translate `NetworkBitBuffer` state into `current_bit` and
   `overflow` results. This is a Phase 120 shrink candidate, not a Phase 119
   domain helper.

## Recommended Phase 119 Scope

Start with aggregate tests for a small **server message envelope and delivery
vocabulary**:

- write representative complete messages from `server_text_messages`,
  `server_service_messages`, `server_sound_message`, `server_static_messages`,
  `server_voice_relay`, and `server_spawn_handshake`;
- assert command bytes, basic payload boundaries, and overflow behavior through
  `NetworkBitBuffer`;
- add a small destination/recipient fixture comparing multicast, event, and
  voice decisions from the same plain client facts;
- add a helper only if the aggregate tests reveal repeated command-envelope or
  recipient-fixture glue.

Do not move `pfnMessageBegin()` / `pfnMessageEnd()` or user-message rewrite
logic in Phase 119. Those belong with the game DLL bridge regrouping.

## Decision

Proceed with one small messaging aggregate pass before game DLL bridge
regrouping. That pass should clarify command/envelope and recipient vocabulary
for the later bridge phases, because the game DLL message session eventually
depends on `sv.multicast`, user-message registration, and multicast routing.

Defer broad adapter regrouping until after Phase 119 tests exist. Defer game
DLL message session consolidation to Phase 121 or later. Defer rendered-console
sink work until a client/rendering console phase exists.
