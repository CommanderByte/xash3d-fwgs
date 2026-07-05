# Clients and Messaging

> **Defined in**: `private/server/clients.hpp`, `snapshot.hpp`,
> `info_string.hpp` / `src/server/clients/*.cpp`\
> **Namespace**: `xash::server`

## Overview

This layer manages client connections end-to-end and everything sent to them:
the challenge/connect handshake, the connection state machine, usercmd
execution, the delta-compressed snapshot stream, multicast + user messages, the
netchan demux, and the operator satellites (A2S/`info` queries, ban filters,
server log). Following Q-2, the legacy `svs.clients` / `svgame.msg[]` /
`sv.multicast` / ban-filter / log file-scope state all folds into **one**
`ClientMachinery` aggregate owned by `ServerRuntime`, and the S9 snapshot state
into `SnapshotState` — a clean merge boundary that let S9 land as a single
runtime member concurrently with the S8 physics slice.

The messaging functions are mostly **free functions over the `EngineBridge`**
(the pfn slots reach `g_bridge` for `clients` / `snapshot` / `arena`), while the
connection functions are free functions over `ServerRuntime` (host/packet
driven).

______________________________________________________________________

## `ClientMachinery` and `ServerClient`

**Header**: `clients.hpp` · **Source**: `clients/client_state.cpp`

`ClientMachinery` holds the `clients[32]` slot array, the slot-parallel
`Netchan netchans[32]` (one shared `ServerFragmentSizer` backs them all — the
aggregate is address-stable, so the borrowed provider pointer stays valid), the
serverinfo/localinfo buffers, the `UserMessageRegistry` + in-flight
`MessageState` + the multicast scratch buffer, the broadcast staging buffers
(`reliable_datagram` / `datagram` / `spec_datagram`), the `BanFilters`, the
`ServerLog`, and `g_userid` (monotonic, **never** reset per map). `clients_init`
binds the self-referential `MessageBuf`s once (safe because `ServerRuntime` is
address-stable for its lifetime).

`ServerClient` is one `sv_client_t` slot: state, address/qport, userinfo/
physinfo, the connect/timebase clocks, the speed-hack clock fields, the
per-client reliable/datagram staging, the snapshot delta state (`frames` ring,
`delta_sequence`, chokecount), and the event queue + ping/latency cache. Note
the last is held **per-client** (`last_ping`/`last_loss`) for Q-2 rather than the
legacy function-static array.

`ClientState` (`Free`/`Connected`/`Spawning`/`Spawned`/`Zombie`) integer order is
not wire, but the transition rules are behavioural contract.

______________________________________________________________________

## Connection state machine

**Source**: `clients/client_state.cpp`, `clients/net_io.cpp`

The connectionless (OOB) path enters through `read_packets` (`SV_ReadPackets`,
`net_io.cpp`): drain `rt.net`'s server socket for one frame and dispatch each
`-1`-prefixed datagram to `handle_connectionless`, replying through a
`NetworkContext`-backed `IOobSink` (`Netchan_OutOfBandPrint`). No-op when
`rt.net` is null (offline server / unit fixtures). The host owns the single
`NetworkContext` and its sockets; the server holds a **non-owning** handle and
never opens a socket ("host-routes" model).

- `compute_challenge` / `check_challenge` — stateless challenges: `MD5(ip ‖
  salt ‖ 5-second window)`, first four bytes little-endian; accept the current
  **or** previous window.
- `handle_connectionless` — tokenize and route `getchallenge` / `connect` /
  `ping`.
- `connect_client` — validate protocol (**49 exactly**) + challenge, find/reuse
  a slot (reconnect matches base-addr + qport/port; a slot wipe preserves
  `physinfo`/`pViewEntity`), init it, run `pfnClientConnect`, reply
  `client_connect`. Returns the slot index or −1 on rejection (which sends three
  OOB reject packets).
- `execute_client_command` — dispatch a client stringcmd: handle
  `new`/`spawn`/`begin`/`disconnect`/`setinfo` directly (driving the
  `Connected → Spawning → Spawned` walk, calling `pfnClientConnect` at `new` and
  `pfnClientPutInServer` at `spawn`) and forward everything else to
  `pfnClientCommand`.
- `execute_client_message` — parse one demuxed (post-netchan) client message:
  frame-ping bookkeeping, then the `clc_*` opcode loop (`nop`/`delta`/`move`/
  `stringcmd`; unsupported opcodes drop the client). Drives `SV_ParseClientMove`
  (usercmd delta decode → `lastcmd`/`packet_loss`/ping) and feeds the per-command
  pmove run (`sv_run_cmd`).
- `userinfo_changed` — name fixups (trim/console/empty/dedupe) + rate/updaterate,
  then `pfnClientUserInfoChanged`.
- `drop_client` — `pfnClientDisconnect` (if spawned) → zombie.
- `check_timeouts` — zombie → free, connect/spawn timeouts.
- `fake_connect` — default-userinfo bot straight to `Spawned`.

**Anti-abuse parity** (part of the contract): the cmd-time speedhack window +
warn/kick counters, the usercmd count cap (≥63 = drop), a second `clc_move` in
one packet aborting the packet's entire remainder (immediate return), voice
payload > 4096 drop, userinfo spam penalties. Known-broken bits stay broken
(`SV_CheckRate` no-op, master-info inverted `password` key, rcon plain-strcmp).

______________________________________________________________________

## Snapshot / delta pipeline

**Header**: `snapshot.hpp` · **Source**: `clients/snapshot.cpp`

`SnapshotState` owns `svs.baselines` (pool-allocated at `load_progs`, sized
`max_edicts`), `sv.instanced[]` (custom baselines, wire-capped at 63 usable), the
**shared circular** `packet_entities` ring every client frame indexes into (with
the monotonic `next_client_entities` write cursor), and the per-client
`ClientFrame` rings (`SV_UPDATE_BACKUP` = SP 16 / MP 64). The byte-exact
`entity_state_t` delta codec itself lives in `networking`
(`DeltaTables::write_delta_entity`); this module only **orchestrates** baselines,
the visible-entity gather, the ring, and the delta emission.

Allocation lifecycle: `snapshot_alloc_baselines` (at load), `snapshot_alloc_ring`
(at `setup_clients`, keyed on maxclients — frees + reallocates on change),
`snapshot_alloc_signon` (once, persists across spawns), `snapshot_reset` (per
spawn), `snapshot_shutdown` (before `game_pool` destruction — the pool asserts on
leaks).

The gather + emit chain (S9 completion): `create_baselines` (per valid edict,
`pfnCreateBaseline` fills the state) → `write_entities_to_client` (gather visible
entities through `pfnSetupVisibility` + `pfnAddToFullPack`, qsort by number, copy
into the ring, emit `svc_(delta)packetentities`) → `write_clientdata_to_message`
(`svc_choke`/fixangle, `pfnUpdateClientData` fill, clientdata + weapondata
deltas) → `emit_events` / `emit_pings` → `send_client_datagram` /
`send_client_messages` (the per-frame send driver + rate gate) and
`update_to_reliable_messages` (fan broadcast buffers to every client).

Note the engine has **no** `SV_FillEntityState`: `pfnAddToFullPack` does *both*
the visibility test and the entire `entity_state_t` fill. Baseline selection is
instanced → best-baseline search → static baseline.

______________________________________________________________________

## Multicast and user messages

**Source**: `clients/messages.cpp`

- `reg_user_msg` (`SV_RegUserMsg`) — dedupe by name, bound size, assign wire
  number `svc_lastmsg + slot`; broadcast the registration when the server is
  active.
- `message_begin` / `message_end` — strict Begin/End pairing (a `Host_Error` on
  nesting or an unregistered message); a fixed-size mismatch drops the message
  with a console `S_ERROR` (not silent, not a `Host_Error`); variable-size
  patches a reserved word; `MSG_INIT` appends to signon during load, else becomes
  reliable-ALL.
- `message_write_*` — append into the multicast buffer, tracking `realsize`.
- `sv_multicast` (`SV_Multicast`) — route the scratch to the recipient set and
  clear it; returns the client count. The PVS/PHS mask path is honoured when a
  world + origin are available; with no mask the legacy "NULL mask → visible"
  rule sends to every eligible slot.
- `playback_event_full` (`SV_PlaybackEventFull`) — the `pfnPlaybackEvent`
  producer: either bypass the queue with a reliable `svc_event_reliable`
  (`FEV_RELIABLE`) or fill the per-client event ring `emit_events` drains.

______________________________________________________________________

## Satellites

**Source**: `clients/query.cpp`, `filter.cpp`, `log.cpp`, `info_string.cpp`

- **Query** (`query_info`, `SV_Info`) — the Xash `info` query answer built from
  live server state; a protocol mismatch yields `"<hostname>: wrong version"`.
  (The A2S `TSource Engine Query` responders proper are OQ-8-trimmed for the
  milestone.)
- **Filters** (`sv_filter.c`) — `filter_check_id` (mutual-prefix match + lazy
  expiry prune) / `filter_add_id` / `filter_remove_id` and the IP variants
  (`filter_check_ip` linear `NET_CompareAdrByMask`, CIDR-aware). `minutes 0`
  (id) / `< 0.1` (ip) = permanent.
- **Log** (`log_printf`, `sv_log.c`) — prefix each line with
  `"MM/DD/YYYY - HH:MM:SS: "`, emit to the UDP logaddress when configured (even
  if `!active`) and to the console/file when active.
- **Info strings** (`info_string.cpp`) — `info_value_for_key` /
  `info_set_value_for_key` / `info_remove_key` / `info_remove_prefixed_keys` /
  `info_is_valid` over caller-owned buffers. Server-scoped until the utilities
  `Info_` consolidation; the legacy rotating-static-buffer contract is replaced
  by a caller buffer, so there is **no** shared mutable state here.

______________________________________________________________________

## Threading model

Main-thread only (OQ-9). The mutating entry points — `execute_client_message`,
`drop_client`, `check_timeouts`, `read_packets`, `send_client_messages` — all
self-assert `ThreadRole::Main`. The whole `ClientMachinery` / `SnapshotState`
aggregate (the client array, the shared packet-entity ring, the multicast +
broadcast scratch, the ban lists) is single-owner state reached only through
these entry points. The `info_string` helpers are pure functions over caller
buffers (no statics). The game callbacks fired here (`pfnAddToFullPack`,
`pfnClientConnect`, …) inherit the Main context transitively. See
[docs/threading-analysis/server-threading.md](../../threading-analysis/server-threading.md).

## Error handling

No exceptions. Rejections/timeouts drop the client (zombie → free). Allocation
failures in the snapshot allocators are logged and surfaced as `false`. Message
protocol violations `Host_Error` (nesting/unregistered) or drop-with-`S_ERROR`
(size mismatch), matching legacy. Unsupported `clc_*` opcodes drop the client.

## Edge cases and invariants

- `g_userid` is monotonic and never reset per map.
- The `ClientMachinery` aggregate must stay address-stable — the netchans borrow
  its `fragment_sizer` and the `MessageBuf`s hold self-references.
- Snapshot pool frees must precede `game_pool` destruction.
- `SV_UPDATE_BACKUP` (16 SP / 64 MP) sizes both the frames ring and the delta
  mask.
- **Deferred (documented, not broken):** the ~90-item S9 completion bucket — the
  multicast/sound message pipeline, per-client messages
  (`pfnClientPrintf`/`SetView`/…), the `net_encode` delta-table field ops, voice
  matrices, player stats/auth-ids, the dropped-packet replay + command-checksum +
  `SV_CalcClientTime` unlag, and the recipient PVS/PHS visibility cull — is
  landed-but-deferred behind `XASH3DPP-STUB` markers. Not milestone-blocking
  against the fake DLL; completes the real-client surface. The in-session netchan
  demux ("Slice-C" seam) and the `S8-seam` `realtime`/`incoming_acknowledged`
  mirrors are the frame-loop splice points.

## See also

- [physics-and-pmove.md](./physics-and-pmove.md) — `execute_client_message` feeds
  `sv_run_cmd`; the frame loop drives `send_client_messages`
- [abi-bridge.md](./abi-bridge.md) — the messaging/snapshot pfn slots reach the
  bridge for `clients` / `snapshot`
- [world-interaction.md](./world-interaction.md) — the PVS/PHS mask the multicast
  recipient cull uses
- `docs/legacy-survey/deep-dive-server-clients.md`,
  `deep-dive-server-world-frame.md`, `deep-dive-delta-encoder.md`
