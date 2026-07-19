# Deep dive: client connections, resources, queries, filters, logging — `sv_client.c`, `sv_custom.c`, `sv_query.c`, `sv_filter.c`, `sv_log.c`

*Chunk 6 recon, 2026-07-04. Narrow-and-exact behavioural reference for the
xash3dpp server rewrite. Sources fully read: `engine/server/sv_client.c`
(3713 lines), `sv_custom.c` (581), `sv_query.c` (190), `sv_filter.c` (708),
`sv_log.c` (256), plus `server.h`, relevant parts of `sv_main.c`,
`sv_pmove.c`, `engine/common/masterlist.c`, `engine/common/protocol.h`.*

> **Refreshed 2026-07-06 (as-built cross-ref).** Shipped in
> `src/server/clients/`: `client_state.cpp` (handshake/slot machine),
> `net_io.cpp` + `messages.cpp` (packet read + user-message Begin/End),
> `snapshot.cpp` (the delta/PVS/PHS pipeline — 14 `assert_thread_role` sites),
> `info_string.cpp` (the NUL-terminated `Info_ValueForKey` codec),
> `query.cpp` (A2S/legacy), `filter.cpp` (bans), `log.cpp`. OQ-8 milestone
> trims (voice fan-out, HLTV datagram, testpacket) carry
> `// XASH3DPP-STUB(chunk6)` markers. See
> `docs/boundaries/server-boundary.md` §As-built.

## 1. Responsibility

- **sv_client.c** — everything about a client's lifetime: connectionless
  handshake (challenge/connect/bandwidth/info/rcon dispatch), slot
  allocation, state machine (new/spawn/begin), userinfo processing,
  per-client stringcmds (incl. enttools), clc_* message parsing
  (move/voice/cvar-query), ping/latency bookkeeping, drop/kick, rcon
  redirect plumbing.
- **sv_custom.c** — custom-resource (player decal/logo) machinery: server
  resource-list + consistency-list serialization to clients, client
  `mapname.res` upload negotiation, HPAK-backed customization creation and
  propagation to other players.
- **sv_query.c** — GoldSrc/Source-style out-of-band query responders:
  A2S_INFO ("TSource Engine Query"), A2S_PLAYER ('U'), A2S_RULES ('V').
- **sv_filter.c** — two ban lists: cheat-ID (uuid) filter and IP/CIDR
  filter, with console commands and persistence writers (`banned.cfg` /
  `listip.cfg` style files). Has engine self-tests.
- **sv_log.c** — HL-standard multiplayer log: timestamped lines to
  file/console and UDP `logaddress`, log file rotation, `log on|off` /
  `logaddress` commands.

## 2. Connection state machine

States (`server.h:84-98`): `cs_free → cs_connected → cs_spawning →
cs_spawned`, plus `cs_zombie` (post-drop) and upload sub-state
`us_inactive/us_processing/us_complete`.

### Connectionless handshake (`SV_ConnectionlessPacket`, sv_client.c:3179)

1. `SV_CheckIP` ban check — banned addresses are **silently dropped**
   (3184).
2. Skip 4-byte `-1` marker, tokenize first line.
3. If `!svs.initialized`: **only rcon** is processed — and it uses globals
   `net_from`/`net_message` instead of the `from`/`msg` params (3202,
   quirk).
4. Master-server addresses get exclusive handling: `M2S_CHALLENGE "s"` →
   `SV_AddToMaster`, `M2S_NAT_CONNECT "c"` → `SV_ConnectNatClient`; then
   return (3207-3219).
5. Dispatch: Source queries (matched against full `args` because "TSource
   Engine Query" contains spaces, 3221-3226), `netinfo`, `info`,
   `bandwidth`, `getchallenge`, `connect`, `ping`→`ack`, GoldSrc `i`→`j`,
   `rcon`, `ack`/`j` → console "ping" print; unknown → game
   `pfnConnectionlessPacket` with `MAX_SYSPATH` reply buffer sent as OOB
   (3263-3276).

### Challenge (stateless — no challenge table)

- `SV_GetChallenge` (sv_client.c:73): challenge = first 4 LE bytes of
  `MD5(ip_bytes ‖ svs.challenge_salt[16 dwords] ‖ time_window)`, where
  `time_window = realtime / CHALLENGE_WINDOW_SECONDS` (5 s, line 23).
  Loopback ⇒ challenge 0; unknown adr type ⇒ error.
- `SV_CheckChallenge` (214): accepts current **and previous** window (max
  lifetime ~10 s); failure ⇒ `SV_RejectConnection "no challenge for your
  address"`.
- `getchallenge` reply: `"challenge %i %i"` — second arg is 1 if a
  bandwidth testpacket is offered, 0 otherwise (112-123, 3239-3242).

### Bandwidth test (`SV_TestBandWidth`, 797)

`bandwidth <ver> <packetsize> <challenge>`: version must equal 49; empty
3rd arg ⇒ old client with swapped order, just send challenge; challenge
required; rejects unless `sv_allow_testpacket`, testpacket built, and
`FRAGMENT_MIN_SIZE(508) < packetsize ≤ 1400`; patches a pregenerated CRC32
(LittleLong from `svs.testpacket_crcs[ofs]`, `ofs = packetsize -
testpacket_filepos - 1`) into `svs.testpacket_crcpos` (unaligned memcpy)
and sends the raw pregenerated packet.

### connect (`SV_ConnectClient`, 295) — exact order

1. `Cmd_Argc() < 5` ⇒ reject "insufficient connection info". Format:
   `connect <ver> <challenge> <protinfo> <userinfo>`.
2. `version != PROTOCOL_VERSION (49)` ⇒ reject. **No legacy/GoldSrc-48
   connect path in this fork** — protocol.h defines
   `PROTOCOL_GOLDSRC_VERSION 48` but engine/server never accepts it;
   GoldSrc compat is only query/ping side.
3. `sv_lan` ⇒ `NET_IsReservedAdr` required (238-246).
4. Challenge check (local clients still validated — only loopback hashes
   to 0 consistently).
5. protinfo: `Q_strlen > sizeof || !Info_IsValid` ⇒ reject "invalid
   protinfo".
6. `SV_ProcessUserAgent` (sv_main.c:787): `uuid` must be **exactly 32
   chars of `[0-9a-f]` (lowercase only)** else "invalid authentication
   certificate"; `SV_CheckID(uuid)` ⇒ "You are banned!"; input devices key
   `d`: `sv_allow_noinputdevices/touch/mouse/joystick/vr` policy rejects
   with specific messages.
7. Extract `qport`, `ext` from protinfo.
8. userinfo validated same way.
9. Password: skipped for `NET_IsLocalAddress`; else if `SV_HavePassword()`
   (non-empty and != "none", server.h:545)
   `Q_stricmp(sv_password, userinfo["password"])` ⇒ reject "invalid
   password".
10. **Reconnect slot reuse** (372-385): scan non-free/non-zombie clients;
    match `NET_CompareBaseAdr && (qport match || port match)` ⇒ reuse
    slot, log "reconnect". Else `SV_FindEmptySlot` (first `cs_free`) or
    reject "server is full".
11. Slot init (401-464): `frames = Mem_Realloc(host.mempool, …,
    SV_UPDATE_BACKUP)` zeroed; `SV_ClearResourceLists`; **memset whole
    client but preserve `physinfo` and `pViewEntity`** (a1ba hack, 409-419
    — game logic can set them before connect); `edict =
    SV_EdictNum(slot+1)`; `userid = g_userid++`; `state = cs_connected`;
    `extensions = ext & NET_EXT_SPLITSIZE` only; `useragent = protinfo`;
    `listeners = -1` (HACKHACK: hear all by default, 429-430);
    `Netchan_Setup` with `NETCHAN_USE_LZSS` unless local client, frag-size
    callback `SV_GetFragmentSize`; `hashedcdkey` = first 32 chars of
    protinfo `uuid`; reply OOB `client_connect <protinfo>` where reply
    protinfo = `ext=<granted>` + `cheats=0/1`; `upstate = us_inactive`;
    `next_messageinterval = 0.05`; `delta_sequence = -1`; userinfo copied +
    `SV_UserinfoChanged`; `next_messagetime = realtime + sv.frametime +
    interval`; `next_checkpingtime = -1.0`.
12. `SV_MaybeNotifyPlayerCountChange` (269): if connected count becomes 1
    or maxclients ⇒ `NET_MasterClear()` (forces immediate heartbeat); logs
    `"<name><userid><slot><>" connected, address "..."`.

`SV_RejectConnection` (175) sends **three** OOB packets: `errormsg`,
`print`, then `disconnect\n`, and logs "connection refused".

### Fake clients (`SV_FakeConnect`, 474, GAME_EXPORT via pfnCreateFakeClient)

Empty slot, default userinfo (`name`=netname or "Bot", `model=gordon`,
`topcolor=1`, `bottomcolor=1`), frees `frames` (bots have none), full
memset (no physinfo preservation!), `state = cs_spawned` directly,
`FCL_FAKECLIENT`, edict flags `FL_CLIENT|FL_FAKECLIENT`,
`FCL_RESEND_USERINFO`, logs address "local". Note: `listeners` stays 0
(memset) — unlike real clients.

### "new" (`SV_New_f`, 1601) — cs_connected only

Builds a `MAX_INIT_MSG` buffer, in order:

1. `SV_SendServerdata` (1529): optional `svc_print` "BUILD %d SERVER (%i
   CRC)\nServer #%i" (developer or MP); `svc_serverdata`: long
   PROTOCOL_VERSION(49), long spawncount, long worldmapCRC, byte
   playerindex, byte maxclients, word GI->max_edicts, word MAX_MODELS,
   string mapname, string map message (world entity `message`), 1 bit
   background flag, string gamefolder, long host.features; then 4 hulls ×
   3 comps of player_mins/maxs as **chars**;
   `Delta_WriteDescriptionToClient`; full movevars delta vs null
   (`MSG_WriteDeltaMovevars`); user-message registrations (`SV_SendUserReg`
   for each `svgame.msg[1..]`); `svc_lightstyle` per used style (num,
   pattern, time).
2. Dead branch: `pfnClientDisconnect` if `cl->state == cs_spawned` —
   unreachable after the cs_connected guard (1621, keep for parity).
3. `pfnClientConnect` may reject ⇒ `SV_RejectConnection(reason)` +
   `SV_DropClient` (1629-1635).
4. `svc_stufftext "fullserverinfo \"%s\""` with `svs.serverinfo`.
5. `SV_FullClientUpdate` for every currently spawned client.
6. `memset cl->lastcmd` ("g-cont. why this is there?"), send whole thing
   via `Netchan_CreateFragments`+`FragSend`.

### Resources: client sends `sendres` (`SV_SendRes_f`, 2024) — cs_connected only

MP: second `sendres` no-ops via `FCL_SEND_RESOURCES`. Sends
`SV_SendResources` (sv_custom.c:557) as fragments — see §5.

### "spawn `<spawncount>`" (`SV_Spawn_f`, 2149) — cs_connected only

Stale spawncount ⇒ re-run `SV_New_f` (level changed during connect). Else
`SV_PutClientInServer`, `state = cs_spawning`; if paused, broadcast
`svc_setpause` + "Server is paused." print.

### `SV_PutClientInServer` (1344)

- **loadgame branch**: `fixangle==1` ⇒ `svc_setangle` + reset; if
  `pfnParmsChangeLevel` ⇒ `svc_restore` with
  `DEFAULT_SAVE_DIRECTORY "%s.HL2"` (slashes fixed), byte connectionCount,
  mapName strings; `svc_weaponanim 0 0`; clears `sv.loadgame`,
  `sv.paused`.
- **fresh branch**: userinfo `hltv` key ⇒ `FCL_HLTV_PROXY`; `SV_InitEdict`;
  HLTV ⇒ `FL_PROXY` else `v.flags = 0`; netname; `colormap = entindex`
  ("???"); `pfnClientPutInServer`; background map ⇒
  `FL_GODMODE|FL_NOTARGET`; `pViewEntity = NULL`.
- `cdAudioTrack` ⇒ `svc_stufftext "cd loop %3d"`.
- `HACKS_RELATED_HLMODS`: gamefolder "invasion" + godmode/notarget ⇒
  executes client command "test\n" (1424-1428).
- Sets `FCL_RESEND_USERINFO|FCL_RESEND_MOVEVARS`; resets
  `connecttime/ignorecmdtime/cmdtime`.
- Non-fakeclient: appends **entire `sv.signon` buffer bit-exact**
  (baselines/static ents/ambients built at level activation), then
  `svc_setview` (word viewent = pViewEntity or own edict), `svc_signonnum
  1`. Overflow ⇒ Host_Error (SP) or drop (MP). Sent as fragments. Static
  buffer `MAX_INIT_MSG + 0x200`.

### "begin" (`SV_Begin_f`, 2180) — cs_spawning only ⇒ `cs_spawned`, `connecttime = realtime`

### Drop / zombie / timeout

- `SV_DropClient` (577): zombie guard; if not crash: `svc_disconnect` into
  reliable stream (non-fake), `pfnClientDisconnect` if spawned, final
  `Netchan_TransmitBits(0)`; clears FAKECLIENT/HLTV flags; `state =
  cs_zombie`; `name[0]=0`; frees `frames`; ends rcon redirect if the
  redirect address matches (612); `Netchan_Clear`; wipes
  userinfo+physinfo; `COM_ClearCustomizationList`; `edict = NULL`;
  broadcasts `SV_FullClientUpdate` (empty-name form) into
  `sv.reliable_datagram`; if server now empty ⇒ `NET_MasterClear()`.
- `SV_KickPlayer` (530): local player unkickable; logs `Kick: "..."`,
  broadcasts, prints to victim, OOB `errormsg` only if useragent
  non-empty.
- `SV_CheckTimeouts` (sv_main.c:489, per frame): `cs_zombie` → `cs_free`
  immediately (FIXME comment — zombie lives exactly one frame);
  `cs_connected/cs_spawning` non-local: `connection_started < realtime -
  sv_connect_timeout(60)` ⇒ drop; drop during connect can **auto-ban** via
  `Cbuf "addip <sv_connect_timeout_ban_time=2> <ip>"` when
  `sv_connect_timeout_ban=1` (464-474); `cs_spawned` non-local:
  `netchan.last_received < realtime - sv_timeout(65)` ⇒ drop. Timed-out
  clients skip zombie (`state = cs_free`). Unpauses server when no players
  remain.
- Fake clients never time out.

## 3. User command processing

### Packet entry (`SV_ReadPackets`, sv_main.c:375)

Connectionless if first dword == -1. Else read 2 sequence longs + qport
short; match client by base adr + qport; **remote port is rewritten if it
changed** (NAT routers, 413-414); `Netchan_Process` then
`SV_ExecuteClientMessage` (skipped for zombie/no-frames); fragment
reassembly path repeats the same; completed file fragments ⇒
`SV_ProcessFile` (customization upload). `FCL_SEND_NET_MESSAGE` set for
reply-at-frame-end when SP-unlimited or state != spawned.

### `SV_ExecuteClientMessage` (sv_client.c:3634)

- Frame ping: `ping_time = realtime - frame->senttime -
  next_messageinterval`; zero if `senttime==0` or within first 2 s of
  connection (3643-3653).
- `latency = SV_CalcClientTime()`; `delta_sequence = -1` (no delta unless
  clc_delta arrives).
- Loop until `< 8 bits` left; overflow ⇒ drop. Opcodes:
  - `clc_nop`
  - `clc_delta` — byte sequence number.
  - `clc_move` — **second clc_move in one packet ⇒ silently return**
    ("someone is trying to cheat", 3682).
  - `clc_stringcmd` — `SV_ExecuteClientCommand`; return if it made us
    zombie.
  - `clc_resourcelist`, `clc_fileconsistency`, `clc_voicedata`,
    `clc_requestcvarvalue`, `clc_requestcvarvalue2`.
  - default ⇒ `clc_bad` message + **drop client**.

### Move parsing (`SV_ParseClientMove`, 3305)

Wire: byte checksum, byte packet_loss, byte numbackup, byte numcmds, then
`numcmds+numbackup` delta usercmds, decoded **in reverse index order**,
each delta'd from the previous decoded cmd starting from a null cmd
(`MSG_ReadDeltaUsercmd`). `net_drop -= (numcmds-1)`. Validation:

- `totalcmds < 0 || >= CMD_MASK (63)` ⇒ drop client ("sending too many
  commands").
- After decode, bail (no run) if not `cs_spawned`.
- Checksum: `CRC32_BlockSequence(data after checksum byte, size,
  incoming_sequence)` must equal byte checksum — **skipped for local
  client**; mismatch ⇒ ignore packet (no drop).
- `GameState->loadGame` ⇒ return (freeze).
- Paused / not-in-game / `SV_PlayerIsFrozen` (sv_background_freeze +
  background, or FL_FROZEN unless ENGINE_QUAKE_COMPATIBLE, 3279): zero
  msec/moves/buttons per cmd; impulse zeroed **only if frozen**; `v_angle`
  still updated from each cmd's viewangles; `net_drop = 0`. Else `v_angle
  = cmds[0].viewangles` unless `fixangle`.
- `SV_EstablishTimeBase` (1143): for `dropped < 24`, replay `lastcmd.msec`
  for drops beyond numbackup, then backup cmd msecs; `timebase = sv.time +
  sv.frametime - Σ msec`.
- Dropped-packet replay: `net_drop < 24`: run `lastcmd` for drops beyond
  numbackup, then the backup cmds; then run the `numcmds` new cmds
  newest-last (`SV_RunCmd(cl, &cmds[i], incoming_sequence - i)` — the
  random_seed).
- Post: if kicked mid-run (state ≤ zombie) stop; `lastcmd = cmds[0]`;
  `frame->ping_time -= lastcmd.msec*0.5/1000` clamped ≥ 0; studio-model
  players get `animtime` clamped to `svgame.globals->time + sv.frametime`
  (comment: intentionally globals->time, not sv.time, 3416-3423).

### Anti-speedhack clock window

- `SV_CheckCmdTimes` (sv_main.c:249, 1 Hz, MP only): `diff = connecttime +
  cmdtime - realtime`; `diff > net_clockwindow` ⇒ `ignorecmdtime = realtime +
  window` and resync cmdtime; `diff < -window` ⇒ resync only.
- `SV_RunCmd` (sv_pmove.c:904): while `ignorecmdtime > realtime`: warn once
  per batch ("time is faster than server time (speed hack?)"), increment
  `ignorecmdtime_warns`, kick when warns > `sv_speedhack_kick` (default
  10, 0 disables); cmd is consumed (cmdtime += msec) but not simulated.
  Also: `cmd.msec > 50` ⇒ split into two halves recursively, `impulse`
  only in first half (sv_pmove.c:925-934).

### stringcmds (`SV_ExecuteClientCommand`, 3103)

- Fixed table `ucmds[]` (3070, kept sorted): `_sv_build_info, begin,
  disconnect, dlfile, god, info, kill, new, noclip, notarget, pause,
  sendres, setinfo, spawn, status`. Handler returning false prints "'%s'
  is not valid from the console".
- Then, if `sv.state == ss_active`:
  - enttools (`ent_create/ent_fire/ent_getvars/ent_info/ent_list`) when
    `cs_spawned && sv_enttools_enable > 0 && !sv.background`; every use is
    **security-logged** via `Log_Printf "...performed: %s"` (3133).
  - `fullupdate` rate-limited by `sv_fullupdate_penalty_time` (default
    1 s); passes everything (incl. fullupdate) to game `pfnClientCommand`;
    fullupdate additionally re-sends ambient sounds, decals, static ents,
    and viewentity (3141-3165).
- Cheat cmds (god/noclip/notarget) require
  `Cvar_VariableInteger("sv_cheats")` and not background. `kill` requires
  spawned + health > 0. `pause` requires `sv_pausable`, non-HLTV.
- `setinfo <k> <v>` writes into userinfo, sets `FCL_RESEND_USERINFO` if ≥
  connected.

### Userinfo pipeline (`SV_UserinfoChanged`, 1805)

- Spam penalty (`SV_ShouldUpdateUserinfo`, 1716): enabled by
  `sv_userinfo_enable_penalty`; skip for bots/SP; base penalty
  `sv_userinfo_penalty_time` (0.3), multiplier 2, after 4 attempts within
  a window penalty doubles; while penalized, updates are **ignored**
  entirely.
- `Info_IsValid` gate. Name: trim spaces (`COM_TrimSpace`); "console"
  (case-insens.) ⇒ "unnamed"; empty ⇒ "unnamed"; dedupe against spawned
  clients ⇒ `"%s (%u)"` suffix loop (1847-1872).
- `rate` → netchan.rate, default 9999, bound [1000,100000]; `cl_nopred` →
  !FCL_PREDICT_MOVEMENT; `cl_lc` → FCL_LAG_COMPENSATION; `cl_lw` →
  FCL_LOCAL_WEAPONS; `cl_updaterate` (≤0 ⇒ 20) → `next_messageinterval`
  clamped by `sv_maxupdaterate/sv_minupdaterate` (`SV_CheckUpdateRate`,
  451, 1763).
- **`SV_CheckRate` (1780) is a no-op bug**: both clamp branches
  `return rate` unchanged — rate cvars never actually clamp there
  (bug-compat).
- Calls game `pfnClientUserInfoChanged`, then re-reads `name` (game may
  override) and sets `ent->v.netname`.

### Voice (`SV_ParseVoiceData`, 3566)

Wire in: byte loopback, byte frames, short size, `size` bytes. `size >
4096` ⇒ **drop client**. Ignored unless `sv_voiceenable` and spawned.
Optional `physFuncs.pfnVoiceData` hook may consume. SP requires
`sv_voice_singleplayer`. Fan-out: receivers must be ≥ connected; gated by
**sender's** `cl->listeners & BIT(receiver_index)`; needs `size+6` bytes
free in receiver datagram else skipped; echo to self has `length = 0`
unless loopback flag. Out: `svc_voicedata` [byte sender][byte frames][short
length][bytes].

### Cvar query

`clc_requestcvarvalue` → `pfnCvarValue(edict, value)`;
`clc_requestcvarvalue2` → long requestID, string name, string value →
`pfnCvarValue2`. Both Con_Reportf logged.

## 4. Connectionless / query surface

- **Xash `info` query** (`SV_Info`, 864): ignored in SP/uninitialized;
  wrong protocol ⇒ `"<hostname>: wrong version"`; else infostring keys in
  order: `p` (=49), `map`, `dm`, `team`, `coop`, `numcl` (excl. bots),
  `maxcl`, `gamedir`, `password` ("0"/"1"), then `host` truncated to
  remaining space (898-908). Reply `A2A_INFO"\n%s"`.
- **`netinfo <ver> <context> <type>`** (`SV_BuildNetAnswer`, 937):
  protocol mismatch ⇒ `neterror=protocol`. Types: `NETAPI_REQUEST_PING`
  (empty), `RULES` (all FCVAR_SERVER cvars; FCVAR_PROTECTED ⇒ "1"/"0" by
  non-empty-and-not-"none"; plus `rules=count`), `PLAYERS` (forbidden ⇒
  `neterror=forbidden` if `!sv_expose_player_list || password set`; else
  `p%iname/p%ifrags/p%itime` + `players=count`), `DETAILS`
  (`hostname/gamedir/current/max/map`), default ⇒ `neterror=undefined`.
  Reply `netinfo %i %i %s`.
- **Source/GoldSrc queries** (sv_query.c): header dword 0xFFFFFFFF,
  replies:
  - `TSource Engine Query` ⇒ `'I'`: byte protocol **49**, hostname, map,
    gamefolder, `pfnGetGameDescription()`, short 0 (appid), byte players
    (**bots included**), byte maxclients, byte bots, byte 'd'/'l', byte
    'w'/'m'/'l' (OS), byte password flag, byte `GI->secure`, string
    XASH_VERSION. **No A2S challenge mechanism at all** (reply
    unconditional).
  - `'V'` rules ⇒ `'E'`: short count (back-patched via `MSG_SeekToBit`),
    name/value string pairs, PROTECTED ⇒ "1"/"0". **No reply at all** if
    zero cvars.
  - `'U'` players ⇒ `'D'`: gated by `sv_expose_player_list && !password`
    (silently no reply); byte count back-patched; per player: byte index
    (**writes running `count`, not slot index**), string name, long
    frags, float connect-time (−1.0 for bots). No reply if empty.
- **rcon** (`SV_RemoteCommand`, 1058): requires `rcon_enable` and
  non-empty `rcon_password`; **always** prints & Log_Printf's the raw
  command+address before validating (1068-1069); `Rcon_Validate` = plain
  `Q_strcmp(Cmd_Argv(1), rcon_password)` — no challenge, no rate limit, no
  constant-time compare; on success re-quotes argv[2..] and executes
  through `Cmd_ExecuteString` under `SV_BeginRedirect(RD_PACKET, 2048−16
  buffer)`; replies as OOB `print\n%s` packets. Bad password ⇒ console
  error only (no reply).
  - Redirect internals (650-723): `Rcon_Print` flushes on newline,
    decrements `rd->lines` (init −1 = unlimited); `RD_CLIENT` target only
    works for **fake clients** (`FCL_FAKECLIENT` check in
    `SV_FlushRedirect`, 670).
- **Master server**: heartbeat lives in `engine/common/masterlist.c:241`
  (`NET_MasterHeartbeat`), called every server frame from
  `Host_ServerFrame` (sv_main.c:718); gated `public_server || sv_nat`,
  `maxclients > 1`; per-master interval `HEARTBEAT_SECONDS`; sends
  `S2M_HEARTBEAT "q\xff"` + random `heartbeat_challenge` dword. Master
  replies `M2S_CHALLENGE "s"` ⇒ `SV_AddToMaster` (sv_main.c:730):
  validates sender is a known master, `challenge2 == heartbeat_challenge`,
  and `last_heartbeat + sv_master_response_timeout ≥ realtime`; replies
  `S2M_INFO "0\n"` infostring:
  protocol/challenge/players/max/bots/gamedir/map/type d|l/`password`
  (**inverted: "0" when password IS set**, sv_main.c:768)/os "w"
  hardcoded/secure "0"/lan "0"/version/region "255"/product/nat.
  `NET_MasterClear` sets `last_heartbeat = MAX_HEARTBEAT` to force an
  immediate heartbeat (called on first/last player and server empty).
- **NAT punch** (`SV_ConnectNatClient`, 914): only if `sv_nat` and packet
  from master; target parsed from Argv(1), must NOT be a reserved address;
  sends the `info` reply straight to the prospective client.

## 5. Resource / download system

### Server → client resource list (`SV_SendResources`, sv_custom.c:557)

Order inside the `sendres` fragment: `svc_resourcerequest` [long
spawncount][long 0]; if `sv_downloadurl` non-empty and `< 256` chars ⇒
`svc_resourcelocation` [string] (**HTTP download redirect**);
`svc_resourcelist`: count in 13 bits, each resource via `SV_SendResource`
(536): type 4 bits, name string, index 12 bits (`MAX_MODEL_BITS`), download
size **signed 24 bits**, flags 3 bits (only
`RES_FATALIFMISSING|RES_WASMISSING` mask), if `RES_CUSTOM` 16-byte MD5,
then 1 bit + 32-byte `rguc_reserved` iff non-zero. Then consistency list.

### Consistency

- `SV_TransferConsistencyInfo` (196): for each precached resource found in
  `sv.consistency_list` (filled from game's forced-consistency calls): set
  `RES_CHECKFILE`, MD5-hash the file (`sound/` prefix prepended for
  t_sound), for models: `force_model_samebounds` ⇒ studio bounds via
  `Mod_GetStudioBounds` (**Host_Error on failure**) packed at
  `rguc_reserved[0x01]`/`[0x0D]` with check_type at `[0]`;
  `force_model_specifybounds` ⇒ bounds from list. Count →
  `sv.num_consistency`.
- `SV_SendConsistencyList` (249): writes 0 bit and clears
  `FCL_FORCE_UNMODIFIED` if SP, `!sv_consistency`, none, or HLTV proxy.
  Else 1 bit; per RES_CHECKFILE resource: 1 bit, then either (delta ≤ 31:
  1 bit + 5-bit delta) or (0 bit + 12-bit absolute index); 0-bit
  terminator.
- Client response `clc_fileconsistency` (`SV_ParseConsistencyResponse`,
  89): bit-prefixed entries: 12-bit index must be in range and
  RES_CHECKFILE (else break); if stored `rguc_reserved` is all-zero: read
  32-bit value, `LittleLongSW`, compare against **first 4 bytes** of MD5;
  else read raw 12-byte cmins+cmaxs and compare per `FORCE_TYPE`
  (samebounds exact `VectorCompare`; specifybounds containment). Processed
  count must equal `sv.num_consistency` else `"%s sent bad file data"` +
  drop. Bad index ⇒ `pfnInconsistentFile(edict, filename, dropmessage)`;
  nonzero return ⇒ optional print + drop. Success ⇒ clear
  `FCL_FORCE_UNMODIFIED`.

### Client → server custom resources (decals) — `clc_resourcelist` (`SV_ParseResourceList`, sv_client.c:3433)

Wire: short total, per resource: string name, byte type, short index, long
size, byte flags (+16-byte MD5 if RES_CUSTOM). `RES_WASMISSING`
force-cleared. **Validation: `type > t_world || size > 1 GiB` ⇒ wipe both
lists and bail.** Rate limit: `resourcelist_next_changetime`
(`sv_upload_penalty_time`, 60 s default). Size accounting printed per
type; `SV_EstimateNeededResources` (sv_custom.c:385): only `t_decal`
counted, HPAK-present ones skipped, `RES_WASMISSING` set on sized entries;
total > `sv_uploadmax` MiB ⇒ wipe lists. Then `upstate = us_processing` +
`SV_BatchUploadRequest` (503): non-missing → onhand; custom decals →
`SV_CheckFile` (291): if `!MD5<32hex>` (len 36) present in HPAK ⇒ have it;
`!sv_allow_upload` ⇒ pretend have it; else `svc_stufftext "upload
\"!MD5...\""` (note: if the name wasn't !MD5-form, the stuffed hash is
all-zero — p is zero-initialized).

- Upload arrives as netchan **file fragments** → `SV_ProcessFile`
  (sv_main.c:304): name must start '!'; hash parsed from name+4; must
  match a `resourcesneeded` entry ("Unrequested decal" else);
  `nDownloadSize` must equal `netchan.tempbuffersize`; `HPAK_AddLump` into
  `hpk_custom_file` (cvar, default custom.hpk); move to onhand;
  `COM_CreateCustomization` with
  `FCUST_FROMHPAK|FCUST_WIPEDATA|FCUST_IGNOREINIT`, dup-MD5 ignored.
- Completion sweep each frame (`SV_RequestMissingResources`,
  sv_custom.c:488, from Host_ServerFrame): spawned + `us_processing` ⇒
  `SV_UploadComplete` (473): when needed-list empty: `SV_RegisterResources`
  (462: **calls `SV_CreateCustomizationList` once per onhand resource** —
  rebuild quirk — and `SV_Customization(..., skipSelf=true)`),
  `SV_PropagateCustomizations` (438: all other spawned players' in-use
  customizations sent to this client), `upstate = us_complete`.
- `SV_SendCustomization` (340): `svc_customization` [byte playernum][byte
  type][string name][short index][long size][byte ucFlags full byte][16-byte
  MD5 iff RES_CUSTOM].
- `SV_CreateCustomizationList` (19): dedups by MD5; `pCust->nUserData2 =
  nLumps`; game `pfnPlayerCustomization` per new customization.

### Direct downloads — `dlfile` (`SV_DownloadFile_f`, sv_client.c:2052)

- `COM_IsSafeFileToDownload` + `sv_allow_download` gate ⇒ else
  `svc_filetxferfailed` (string name).
- Normal files: `sv_send_resources` gate; **must match a precached
  `sv.resources` entry** (`Q_strncmp` 64 chars; `sound/` prefix cut from
  the requested name for t_sound comparison, 2078-2093); `.mdl` also
  pushes `Mod_StudioTexName` T-file if present; served via
  `Netchan_CreateFileFragments` + `FragSend`.
- `!MD5<hex>` (len 36) logos: `sv_send_logos` gate; HPAK lookup ⇒
  `Netchan_CreateFileFragmentsFromBuffer`.
- HTTP precedence is client-driven: server only advertises
  `svc_resourcelocation` (sv_downloadurl); netchan fragments are the
  fallback the client falls to (client-side logic).
- Fragment sizing (`SV_GetFragmentSize`, 125): local netchan ⇒ 64000.
  `FRAGSIZE_UNRELIABLE`: userinfo `cl_urmax`, 0 ⇒ NET_MAX_MESSAGE, else
  `bound(FRAGMENT_MAX_SIZE, cl_urmax, NET_MAX_MESSAGE)` (**min bound is
  64000 — quirk of arg order**). Download/split: `cl_dlmax` bound [508,
  64000]; `FRAGSIZE_SPLIT` returns it only with `NET_EXT_SPLITSIZE`
  extension else 0 ("original engine behaviour"). `FRAGSIZE_FRAG` in-game
  (`cs_spawned`): userinfo `cl_frmax` if within [508,64000] else
  `cl_dlmax/2` ("window for unreliable"), minus `HEADER_BYTES` (= 8 +
  MAX_STREAMS*13).

## 6. Filtering & logging

### sv_filter.c

- **ID filter** (uuid bans): singly-linked `cidfilter` list `{endTime, id
  string}`. `SV_CheckID` (62): compares with `Q_strncmp` over
  `min(len(id), len(filter))` — **mutual prefix match**; prunes expired
  entries mid-iteration (re-entrant `SV_RemoveID` while walking — fragile
  pattern, returns false if list ends during prune). Commands (all
  `Cmd_AddRestrictedCommand`): `banid <minutes> <id> [kick]` — minutes 0 =
  permanent, `#userid` form **disabled** (`#if 0`, prints "not supported",
  113-128); strips `STEAM_`/`VALVE_` (6) and `XASH_` (5) prefixes; matches
  spawned non-fake players by uuid prefix; stores the player's full
  useragent uuid; trailing arg literally `"kick"` ⇒ `Cbuf "kick #%d
  \"Kicked and banned\""`. `listid` (skips expired), `removeid
  <#slot|id>`, `writeid` → file named by `bannedcfgfile` cvar, header
  comment block + `banid 0 <id>` lines, **permanent bans only**. Shutdown
  does NOT auto-write (commented out, 254-255); banned.cfg is not
  auto-executed by engine (comment).
- **IP filter**: `ipfilter` list `{endTime, netadr, prefixlen}` (CIDR,
  IPv4+IPv6, rehlds-inspired). `addip <minutes> <addr[/CIDR]>`: minutes <
  0.1 ⇒ permanent; **immediately drops all connected clients matching the
  mask** with "The server operator has added you to banned list"
  (436-445). `SV_CheckIP` (352): linear `NET_CompareAdrByMask`, expired
  entries skipped but not removed. `removeip <addr[/CIDR]> [removeAll]`
  removes filters *included by* the argument; `SV_RemoveIPFilter` has a
  latent use-after-free in the removeAll path (`back = &f->next` after
  free, 339-342) — do not replicate, but note behaviour. `listip
  [filter]`. `writeip` → `listipcfgfile` cvar file,
  `addip 0 <base>/<prefix>\n` lines, permanent only. `SV_InitFilter`/
  `SV_ShutdownFilter` register/free everything. Engine tests
  `Test_RunIPFilter` validate `NET_StringToFilterAdr` partial-IPv4 forms
  ("192.168" ⇒ /16 etc.) and inclusion logic (578-707).

### sv_log.c

- State: `svs.log { active, net_log, net_address, file }`
  (server.h:107-113).
- `Log_Open` (19): needs `svs.log.active`; `sv_log_onefile` keeps current
  file; `mp_logfile 0` ⇒ console-only; path
  `<logsdir|logs>/L<MM><DD><nnn>.log`, nnn 000-999 first free; `logsdir`
  rejected if contains ':' or ".."; failure ⇒ logging disabled. Header:
  `Log file started (file "X") (game "<*gamedir serverinfo key>") (version
  "49/<XASH_VERSION>/<buildnum>")`.
- `Log_Printf` (101): every line prefixed `MM/DD/YYYY - HH:MM:SS: `; **UDP
  `log %s` OOB to logaddress whenever `net_log`, even if `active` is
  false** (125-126); file/console echo requires `active && (maxclients > 1
  || sv_log_singleplayer)`; `mp_logecho` ⇒ console, `mp_logfile` ⇒ file.
- `Log_PrintServerVars`: `Server cvars start` / `Server cvar "n" = "v"` /
  `Server cvars end` for FCVAR_SERVER.
- `logaddress <ip> <port>` / `logaddress off` (167); `log on|off` (224).
- Line triggers across the engine: connect (sv_client.c:284), kick
  (550/558), rcon (1069), enttools use (3133), map start/load
  (sv_init.c:657, 953), server shutdown (sv_main.c:1148), fatal errors
  (sv_game.c:97), game pfnAlertMessage at_logged (sv_game.c:2879), `say`
  from console (sv_cmds.c:747), FCVAR_SERVER cvar changes incl. PROTECTED
  masking (common/cvar.c:170-175), HPAK oversize deletion
  (common/hpak.c:525). Log format matches HL1 standard
  (`"name<userid><uuid><>"` fields).

## 7. External surface (verified by grep outside engine/server/)

- `Log_Printf` — common/cvar.c:170,175; common/hpak.c:525 (decl
  common/common.h:768).
- `Rcon_Print` — common/system.c:523 (console print pump into active
  redirect; decl common/common.h:830).
- `SV_GetPlayerCount` — common/host.c:344 (status line).
- `SV_ShutdownFilter` — common/host.c:1358 (host shutdown).
- `Test_RunIPFilter` — common/tests.h:44,56.
- `SV_FakeConnect` — GAME_EXPORT, wired to game DLL as
  `pfnCreateFakeClient` (via sv_game.c).
- Game DLL callbacks invoked from here:
  `pfnClientConnect/Disconnect/PutInServer/Kill/ClientCommand/
  ClientUserInfoChanged/KeyValue/Touch/Use/Spawn/Think/
  PlayerCustomization/InconsistentFile/ConnectionlessPacket/CvarValue/
  CvarValue2/pfnVoiceData(physics iface)/ParmsChangeLevel/
  GetGameDescription`.
- Everything else (SV_DropClient, SV_KickPlayer, SV_ConnectionlessPacket,
  SV_ExecuteClientMessage, SV_SendServerdata, SV_FullClientUpdate,
  SV_CheckIP/ID, SV_ClientById/ByName, SV_CalcPing, SV_GetPlayerStats,
  SV_TogglePause, SV_BuildReconnect, resource-list helpers, Log_*,
  SV_SetLogAddress_f/SV_ServerLog_f) is consumed inside engine/server/
  (sv_main, sv_cmds, sv_game, sv_frame, sv_init, sv_save).

## 8. Owned state

- **sv_client.c**: `static int g_userid = 1` (monotonic, never reset per
  map); `SV_GetClientIDString` static `result[MAX_QPATH]`;
  `SV_GetPlayerStats` static `last_ping[MAX_CLIENTS]`,
  `last_loss[MAX_CLIENTS]`; `SV_RemoteCommand` static `outputbuf[2048]`;
  `SV_PutClientInServer` static `msg_buf[MAX_INIT_MSG+0x200]`; const
  tables `ucmds[]`, `enttoolscmds[]`.
- **sv_custom.c**: `SV_SendResource` static `nullrguc[32]` (zero compare
  buffer). Per-client resource lists live in `sv_client_t` (circular
  doubly-linked sentinels `resourcesneeded/resourcesonhand`, `customdata`
  list).
- **sv_query.c**: none.
- **sv_filter.c**: `static cidfilter_t *cidfilter`, `static ipfilter_t
  *ipfilter` (heap, host.mempool).
- **sv_log.c**: `Log_Printf` static `string[1024]`; the actual log state
  is `svs.log`.
- Shared: `svs.challenge_salt[16]`, `svs.testpacket*` (challenge +
  bandwidth test state, owned by sv_init/sv_main), `host.rd` (redirect),
  `sv.current_client`.

## 9. Dependencies (by subsystem)

- **Netchan_**: Setup, OutOfBandPrint/OutOfBand, TransmitBits, Clear,
  CreateFragments, FragSend, CreateFileFragments(FromBuffer), IsLocal,
  Process, IncomingReady, CopyNormalFragments, CopyFileFragments
  (frag-size callback contract `fragsize_t`).
- **NET_**: NetadrType, NetadrToIP6Bytes, AdrToString, BaseAdrToString,
  CompareAdr, CompareBaseAdr, CompareAdrByMask, IsReservedAdr,
  IsLocalAddress, IsMasterAdr, StringToAdr, StringToFilterAdr, SendPacket,
  GetLocalAddress, MasterClear, MasterHeartbeat (common/masterlist),
  GetMaster.
- **MSG_/sizebuf**: Init, Begin(Server/Client)Cmd,
  Write{Byte,Char,Short,Word,Long,Dword,Float,String,Stringf,OneBit,
  UBitLong,SBitLong,Bytes,Bits,Vec3Angles,DeltaMovevars},
  Read{Byte,Short,Long,Dword,String,StringLine,OneBit,UBitLong,Bytes,
  ClientCmd,DeltaUsercmd}, Clear, SeekToBit, GetNumBitsWritten/
  BytesWritten/BitsLeft/BytesLeft, CheckOverflow, GetData,
  GetRealBytesRead, GetMaxBytes.
- **Delta_**: WriteDescriptionToClient (MSG_ReadDeltaUsercmd /
  MSG_WriteDeltaMovevars live in net_encode).
- **Info_**: ValueForKey, SetValueForKey(f), IsValid, RemovePrefixedKeys,
  Print.
- **COM_/crt**: StringEmpty(OrNULL), HexConvert, TrimSpace, FileExtension,
  IsSafeFileToDownload, SizeofResourceList, CreateCustomization,
  ClearCustomizationList, FixSlashes, Q_str*/Q_snprintf/Q_atoi/Q_atof,
  MD5Init/Update/Final, MD5_HashFile, MD5_Print, CRC32_BlockSequence,
  LittleLong(SW).
- **FS_**: Open, Close, Printf, Write, FileExists (filter persistence,
  logs, downloads).
- **Cvar_**: GetList, VariableString, VariableInteger, LookupVars,
  RegisterVariable + ~40 sv_* cvars (timeouts, penalties, allow-flags,
  rates).
- **Cmd_**: TokenizeString, Argc/Argv, ExecuteString,
  AddRestrictedCommand, RemoveCommand, Cbuf_AddTextf.
- **HPAK_**: GetDataPointer, ResourceForHash, AddLump (hpk_custom_file
  cvar).
- **host/global**: host.realtime, host.rd, host.features, host.mempool,
  host.player_mins/maxs, GameState->loadGame, UI_CreditsActive,
  CL_IsInGame, Host_IsLocalClient/IsDedicated/IsSinglePlayerGame,
  Host_Error, Mem_*, Log via svs.log.
- **svgame**: dllFuncs/dllFuncs2/physFuncs callbacks, svgame.movevars,
  svgame.msg[], globals (maxClients, deathmatch/teamplay/coop,
  cdAudioTrack, time, frametime).
- **Other server units**: SV_RunCmd/SV_SetupMoveInterpolant (sv_pmove),
  SV_ClientPrintf/BroadcastPrintf (sv_cmds),
  SV_RestartAmbientSounds/Decals/StaticEnts, SV_ModelHandle,
  SV_EdictNum/SV_InitEdict/SV_CreateNamedEntity/SV_Move/SV_LinkEdict,
  Mod_GetStudioBounds/Mod_StudioTexName.

## 10. Quirks & invariants (bug-compat inventory, file:line)

1. Stateless challenge: MD5(ip‖salt‖window), 5 s windows,
   current+previous accepted (~10 s lifetime); loopback ⇒ 0
   (sv_client.c:23,73-110,214-229).
2. Reject sends 3 OOB packets: `errormsg`, `print`, `disconnect\n`
   (185-187).
3. `SV_GetFragmentSize` `cl_urmax` bound has swapped min/max:
   `bound(FRAGMENT_MAX_SIZE, x, NET_MAX_MESSAGE)` (139); split-size only
   honored with `NET_EXT_SPLITSIZE` else return 0 = "original engine
   behaviour" (147-150); in-game frag = `cl_dlmax/2` unless valid
   `cl_frmax`, minus `HEADER_BYTES` (154-165).
4. Connect requires exactly protocol 49; **no legacy/GoldSrc 48 netchan
   client support server-side** (316-318, protocol.h:19;
   PROTOCOL_GOLDSRC_VERSION only used client-side).
5. Reconnect match: base addr AND (qport OR exact port) — port match alone
   suffices (379).
6. Slot wipe preserves `physinfo` and `pViewEntity` across reconnect
   (409-419, "a1ba" comment).
7. `listeners = -1` HACKHACK for real clients (429-430); fake clients get
   0 — bot voice fan-out is empty by default (SV_FakeConnect never sets
   it).
8. `hashedcdkey` = first 32 chars of protinfo `uuid`, not re-hashed
   (438-439); `SV_FullClientUpdate` sends `MD5(hashedcdkey[34])` digest
   (1261-1265); `_`-prefixed userinfo keys stripped before broadcast
   (1258).
9. uuid must be 32 chars, `[0-9a-f]` **lowercase only** — uppercase hex
   rejected (sv_main.c:800-809).
10. Master infostring `password` key inverted: "0" when password set
    (sv_main.c:768); `os` always "w" (769).
11. `NET_MasterClear` on 1st client, full server, and server-empty
    transitions (sv_client.c:281-282, 633-640).
12. Kick: local player immune; OOB errormsg only when useragent non-empty
    (536-538, 553-554, 561-562).
13. Drop: zombie for one frame only (SV_CheckTimeouts converts next frame,
    sv_main.c:511-514); timed-out clients skip zombie entirely (468);
    connect-phase timeout can auto-`addip` ban for
    `sv_connect_timeout_ban_time` min (470-473).
14. `RD_CLIENT` redirect only reaches **fake clients** (sv_client.c:670).
15. rcon logs full command line before password validation (1068-1069);
    plain strcmp validation, no challenge/rate-limit (1040-1047); reply
    buffer 2048−16.
16. `SV_CalcPing` frame index `incoming_acknowledged + ~i` (1121); back =
    UPDATE_BACKUP/2 if ≤31 else 16 (1110-1115).
17. `SV_CalcClientTime` returns 0 unless min/max ping spread over last ≤4
    frames ≤ 0.2 s (1222-1225); unlag samples clamp 1..16.
18. `SV_CheckRate` is a total no-op — sv_minrate/sv_maxrate never clamp
    netchan.rate here (1780-1795). `SV_CheckUpdateRate` works
    (1763-1778).
19. Userinfo spam penalty: 0.3 s base, ×2 multiplier after 4 attempts;
    updates *silently ignored* while penalized (1716-1761).
20. Name fixups: trim, "console"→"unnamed", empty→"unnamed", dup ⇒
    `"name (N)"` counting from 1, compared only against **spawned**
    clients (1823-1872).
21. `cl_updaterate ≤ 0` ⇒ 20 fps default; connect default interval 0.05
    (1895-1900, 451).
22. Spawn with stale spawncount re-runs `SV_New_f` (2155-2159).
23. `SV_New_f` contains unreachable `cs_spawned` disconnect branch
    (1621-1622) and clears `lastcmd` with a "why is this there?" comment
    (1649-1650).
24. signon overflow: SP ⇒ Host_Error "spawn player: overflowed", MP ⇒ drop
    (1455-1460).
25. loadgame spawn writes `svc_restore` with save named `save/<map>.HL2`
    and resets weaponanim (1372-1387); background maps set
    `FL_GODMODE|FL_NOTARGET` (1411-1412); Invasion-mod "test" command hack
    under `HACKS_RELATED_HLMODS` (1424-1428).
26. Pause: `svc_setpause` 1 bit; auto-unpause when last player leaves
    (sv_main.c:535-539); UI credits block pause (1693).
27. Move parse: `totalcmds >= CMD_MASK(63)` ⇒ drop (3323-3328);
    reverse-order chained delta decode from null cmd (3330-3335); CRC
    skipped for local client (3340-3351); checksum fail ⇒ ignore, not
    drop; frozen zeroes moves but still applies viewangles, impulse
    cleared only when frozen (3360-3376); `net_drop` replay capped at 24
    (3385); `random_seed = incoming_sequence - i` (3396,3403); ping
    adjusted by −msec/2 (3413-3415); studio `animtime` clamp uses
    `svgame.globals->time` deliberately (3420-3422).
28. Double `clc_move` in one packet ⇒ silent return, "someone is trying to
    cheat" (3681-3684); unknown clc ⇒ drop (3706-3709).
29. Voice: size > 4096 ⇒ **drop client** (3576-3581); sender's `listeners`
    mask gates receivers (3608); receiver datagram needs len+6 free;
    self-echo truncated to 0 bytes unless loopback (3617).
30. Resourcelist upload: `type > t_world || size > 1 GiB` ⇒ wipe
    (3460-3465); 60 s rate limit (3469-3477); `sv_uploadmax` MiB cap
    (3512-3517).
31. `SV_CheckFile` with non-!MD5 name stuffs `upload "!MD5<zero-hash>"`
    (sv_custom.c:293-311); `!sv_allow_upload` pretends file exists
    (305-306).
32. Consistency response count must exactly equal `sv.num_consistency`
    (171-176); MD5 compared only on first 4 bytes (127); force-type from
    `rguc_reserved[0]`, bounds at offsets 0x01/0x0D (140-158, 232-239);
    `Mod_GetStudioBounds` failure at level-load is a **Host_Error**
    (230-231).
33. Consistency list delta encoding: 5-bit delta if ≤31 else absolute
    12-bit (270-282); skipped entirely for SP/HLTV/disabled (254-259).
34. `SV_RegisterResources` calls `SV_CreateCustomizationList` once per
    resource on hand (466-470 — O(n²) rebuild, preserve behaviour);
    customization playernum mismatch ⇒ Host_Error (418-421).
35. Download: precache whitelist compare is `Q_strncmp(…, 64)` with
    `sound/` prefix cut for sounds (2078-2093); `.mdl` auto-sends texture
    file (2096-2100); logos need len==36 `!MD5` + `sv_send_logos` (2113).
36. `svc_resourcelocation` only if `strlen(sv_downloadurl) < 256` (565).
37. A2S queries: no challenge; INFO reports protocol **49** byte and bots
    included in player count (sv_query.c:31,37); players/rules replies
    suppressed entirely when empty (106,158); player entry "index" byte is
    the running count (148); players gated by `sv_expose_player_list` and
    password (130-131).
38. `SV_Info` clamps hostname to fit infostring, error branch replies
    `"<hostname>: wrong version"` (874-908).
39. `netinfo` PROTECTED cvars ⇒ "1"/"0" by non-empty-and-not-"none"
    (975-979) — same rule as `SV_HavePassword`.
40. `SV_CheckID` is mutual-prefix match (min-length strncmp)
    (sv_filter.c:69-86); expiry pruned lazily during checks; `banid
    #userid` disabled; STEAM_/VALVE_/XASH_ prefixes stripped (134-137);
    persistence writes only permanent entries; files not
    auto-executed/auto-written on shutdown (254-255, 554-555).
41. `addip` minutes < 0.1 ⇒ permanent (414-415); immediately kicks
    matching clients (436-445); `SV_RemoveIPFilter` removeAll path walks
    freed memory (339-347) — reproduce semantics (remove all included
    filters), not the UAF.
42. Log: net_log UDP output works even when `svs.log.active` false
    (sv_log.c:110,125-126); file logging requires MP or
    `sv_log_singleplayer`; `logsdir` sanitized against ':' and ".." (48);
    1000-file rotation cap disables logging (72-77).
43. `SV_ConnectionlessPacket` pre-init rcon uses `net_from`/`net_message`
    globals, not its parameters (3201-3202).
44. `fullupdate` penalty (default 1 s) + resends ambients/decals/static
    ents/view (3141-3165); enttools require spawned + `sv_enttools_enable` +
    !background and are audit-logged (3126-3138).
45. Timeout constants: sv_timeout 65 s (spawned), sv_connect_timeout 60 s
    (connecting), zombie 1 frame; net_clockwindow cmd-time window with
    `sv_speedhack_kick` = 10 warns (sv_main.c:44-47, 249-294;
    sv_pmove.c:904-922).
46. `cmd.msec > 50` split into halves, impulse only first half
    (sv_pmove.c:925-934).
47. `SV_EntCreate_f` fallbacks: `physFuncs.SV_CreateEntity` →
    `pfnCreateEntitiesInRestoreList` with magic `flags = 1337`
    (2925-2942); auto-targetname `<lowercased-name>_<userid>_e<entindex>`
    (2984-3009).

## 11. ABI / wire touchpoints

- **svc_ emitted here** (protocol.h values): `svc_disconnect` 2,
  `svc_setview` 5, `svc_print` 8, `svc_stufftext` 9, `svc_setangle` 10,
  `svc_serverdata` 11, `svc_lightstyle` 12, `svc_updateuserinfo` 13,
  `svc_setpause` 24, `svc_signonnum` 25, `svc_restore` 33,
  `svc_weaponanim` 35, `svc_resourcelist` 43, `svc_deltamovevars` 44,
  `svc_resourcerequest` 45, `svc_customization` 46, `svc_filetxferfailed`
  49, `svc_voicedata` 53, `svc_resourcelocation` 56.
- **clc_ consumed**: `clc_nop` 1, `clc_move` 2, `clc_stringcmd` 3,
  `clc_delta` 4, `clc_resourcelist` 5, `clc_fileconsistency` 7,
  `clc_voicedata` 8, `clc_requestcvarvalue` 9, `clc_requestcvarvalue2` 10
  (protocol.h:85-96).
- **OOB strings**: `getchallenge`/`challenge`, `connect`/`client_connect`,
  `bandwidth`/`testpacket`, `errormsg`, `print`, `disconnect`, `info`,
  `netinfo`, `ping`/`ack`, GoldSrc `i`/`j`, `rcon`, `log`, master `s`,
  `c`, `q\xff`, `0\n`, `TSource Engine Query`/`I`, `U`/`D`, `V`/`E`
  (protocol.h:321-366).
- **Wire structs**: `resource_t` (client-upload wire form: name/byte
  type/short index/long size/byte flags/16B MD5; server list form: 4-bit
  type/12-bit index/24-bit signed size/3-bit flags/optional 32B
  rguc_reserved), `customization_t` (via COM_CreateCustomization),
  `usercmd_t` (delta-encoded, CMD_BACKUP=64), `movevars_t` (delta vs
  null), `clientdata_t/weapon_data_t` (in client_frame_t). Bit widths:
  MAX_CLIENT_BITS 5, MAX_MODEL_BITS 12, MAX_RESOURCE_BITS 13.
- **userinfo keys with engine meaning**: `name`, `password`, `rate`,
  `cl_updaterate`, `cl_lw`, `cl_lc`, `cl_nopred`, `hltv`, `cl_dlmax`,
  `cl_urmax`, `cl_frmax`, `model/topcolor/bottomcolor` (bot defaults),
  `_*` (stripped from broadcast).
- **protinfo/useragent keys**: `qport`, `ext` (NET_EXT_SPLITSIZE=1),
  `uuid` (32 hex), `d` (input-device bitmask
  INPUT_DEVICE_MOUSE/TOUCH/JOYSTICK/VR); server reply protinfo: `ext`,
  `cheats`.
- **Constants**: PROTOCOL_VERSION 49; CHALLENGE_WINDOW_SECONDS 5;
  FRAGMENT_MIN/DEFAULT/MAX 508/1200/64000; HEADER_BYTES 8+13·MAX_STREAMS;
  rates 1000–100000 default 9999; CMD_BACKUP 64/CMD_MASK 63; voice max
  4096; upload cap sv_uploadmax MiB, resource size cap 1 GiB; testpacket
  max 1400.
