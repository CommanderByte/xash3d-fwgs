# Client Session Boundary Audit

Phase 124 audits `engine/server/sv_client.c` before the next extraction. The
file is too mixed to move as a unit, so the useful boundary is a set of
client/session concepts with explicit compatibility fences.

## Current Ownership Map

| Area | Legacy functions | Current state |
| --- | --- | --- |
| Admission and challenge | `SV_SendChallenge()`, `SV_CheckChallenge()`, `SV_CheckIPRestrictions()`, `SV_ConnectClient()` | Challenge time-window and response text are helper-owned, but address hashing, LAN checks, password checks, `Cmd_Argv()` parsing, netchan setup, and rejection side effects remain legacy-owned. |
| Session slots and population | `SV_GetPlayerCount()`, `SV_FindEmptySlot()`, `SV_MaybeNotifyPlayerCountChange()`, parts of `SV_ConnectClient()`, `SV_FakeConnect()`, `SV_DropClient()` | Mostly plain slot-state scans. This is the lowest-risk next seam because it can use snapshots instead of `sv_client_t`. |
| Spawn handshake | `SV_New_f()`, `SV_PutClientInServer()`, `SV_Spawn_f()`, `SV_Begin_f()`, `SV_SendServerdata()` | Handshake decisions and payload writers exist, but live edict setup, game DLL connect/spawn callbacks, fragments, and serverdata emission remain legacy-owned. |
| Userinfo and rates | `SV_ShouldUpdateUserinfo()`, `SV_CheckUpdateRate()`, `SV_CheckRate()`, `SV_UserinfoChanged()`, `SV_SetInfo_f()` | Rate, flag, and penalty decisions are helper-owned. Name normalization, duplicate-name mutation, info-string writes, and `pfnClientUserInfoChanged()` remain legacy-owned. |
| Client commands and debug entity commands | `SV_ExecuteClientCommand()`, command handlers, `SV_Ent*` helpers | Command classification is helper-owned. Handler side effects, cheats, edict mutation, and enttools should stay in legacy/server-world ownership for now. |
| Transfers and resources | `SV_DownloadFile_f()`, `SV_ParseResourceList()` | Download policy and upload-list gates are helper-owned. Filesystem probes, HPAK/resource list mutation, batch uploads, and resource lifetime remain legacy-owned. |
| Voice | `SV_ParseVoiceData()` | Voice relay gates, recipient decisions, and payload writes are helper-owned. Message reads, physics callbacks, recipient iteration, and datagram mutation remain legacy-owned. |
| Cvar query responses | `SV_ParseCvarValue()`, `SV_ParseCvarValue2()` | Not yet grouped. These are game DLL callback dispatchers and should wait for a game DLL/client-query bridge. |
| Remote admin and connectionless | `SV_RemoteCommand()`, redirect helpers, `SV_ConnectionlessPacket()`, `SV_Info()`, `SV_BuildNetAnswer()` | Classifier, NetAPI/source-query payloads, and response formatting are helper-owned. Redirect buffers, rcon password checks, command execution, and packet sends remain legacy-owned. |
| Movement packet parsing | `SV_ParseClientMove()`, `SV_ExecuteClientMessage()` | Too coupled to protocol bitstreams, `usercmd_t`, `SV_RunCmd()`, PMove, frozen-state checks, and packet sequencing for this phase. |

## Existing Helpers

Reusable client/session concepts:

- `client_policy`: userinfo penalties, rates, userinfo flags, and private
  client flag predicates;
- `client_command_dispatch`: built-in, enttools, fullupdate, ignore, and game
  DLL command routing;
- `connectionless_classifier`: connectionless command routing;
- `connection_response`: challenge and rejection response text;
- `server_challenge_policy`: challenge time-window math;
- `server_download_policy`: download admission decisions;
- `server_upload_queue`: client resource-list timing, descriptor, and size
  policy;
- `server_voice_relay`: voice input gates, recipient selection, and payload
  encoding;
- `server_spawn_handshake`: new/spawn/begin admission and signon/serverdata
  payload helpers;
- `server_userinfo_message`: full client update message payload;
- `server_service_messages`: small service payload writers for reconnect,
  set-view, pause, and failed transfer messages;
- `source_query` and `netapi_info`: connectionless info/query payloads.

Remaining adapter-like surfaces:

- `server_service_messages` is mostly a wire payload writer, not a session
  owner;
- `server_userinfo_message` writes one payload shape but does not own userinfo
  lifetime;
- `server_spawn_handshake` owns useful policy, but its adapter is still tied to
  live signon side effects;
- `client_policy` is the strongest existing client/session concept, but it
  intentionally avoids duplicate-name mutation and game DLL callbacks.

## Recommended Phase 125 Seam

Start with a `ClientSessionSlots` or `ClientPopulation` helper under
`src/engine/server` using plain snapshots:

- slot state;
- private client flags;
- whether a slot is reusable/free;
- optional base-address/qport match data only if reconnect matching can be
  expressed without `netadr_t`.

The first pass should cover:

- player/bot counts currently handled by `SV_GetPlayerCount()`;
- first free slot selection currently handled by `SV_FindEmptySlot()`;
- heartbeat-relevant population decisions currently duplicated in
  `SV_MaybeNotifyPlayerCountChange()` and `SV_DropClient()`.

This is small, testable, and genuinely session-shaped. It also gives later
admission work a typed view of the client array without moving `sv_client_t`,
netchan, edicts, or game DLL callbacks.

## Deferred Seams

Keep these out of Phase 125:

- full `SV_ConnectClient()` admission, because it combines command parsing,
  protocol validation, challenge checks, password checks, reconnect matching,
  memory allocation, edict setup, netchan setup, and response packets;
- `SV_New_f()`, `SV_PutClientInServer()`, `SV_Spawn_f()`, and `SV_Begin_f()`,
  because they are game DLL callback and edict lifecycle boundaries;
- `SV_UserinfoChanged()` duplicate-name mutation, because it mutates info
  strings, updates edict netnames, and calls game DLL code;
- `SV_ParseClientMove()` and the movement parts of `SV_ExecuteClientMessage()`,
  because they need protocol and PMove fixtures;
- `SV_ParseResourceList()`, because resource list ownership belongs with the
  resource-transfer domain;
- `SV_ParseCvarValue*()`, because the useful abstraction is a game DLL
  cvar-query bridge, not a client-session helper;
- rcon/redirect handling, because it should converge with the console/router
  work instead of client session ownership;
- enttools/debug entity commands, because they belong to a future
  server-world/admin-command split.
