# xash3dpp — Implementation Status and Work Plan

*Generated: 2026-05-15 — Updated: 2026-07-04*

> Status-table labels: **Complete / Partial / Skeleton** (structural, as
> reported by `xash3dpp/tools/status_table.py --check`); chunk headings use
> **✅ DONE / IN PROGRESS / TODO** (workflow status incl. gates). A
> subsystem can be structurally Complete while its chunk still has open
> gates.

______________________________________________________________________

## Status Table

| Subsystem | src/ files | include/ | tests/ | Status |
|------------|-----------|----------|--------|----------|
| utilities | 9 | ✓ | ✓ | **Complete** |
| memory | 1 | ✓ | ✓ | **Complete** |
| filesystem | 10 | ✓ | ✓ | **Complete** |
| platform | 14 | ✓ | ✓ | **Complete** (incl. os_socket + IPlatformSockets) |
| core | 4 | ✓ | ✓ | **Complete** |
| cmd_cvar | 11 | ✓ | ✓ | **Complete** |
| host | 2 | ✓ | ✓ | **Complete** (Chunk 3 ✅; EngineContext owns networking) |
| abi | 1 | ✓ | ✗ | **Partial** (`Host_Error` shim + accessor only) |
| map_loader | 10 | ✓ | ✓ | **Complete** (BSP v29/30/BSP2/30ext → immutable WorldData; PVS + trace kernel, Q-18 golden-gated; FSM loads worlds; PHS module per Q-19) |
| launcher | 1 | ✗ | ✗ | **Partial** (thin argv bootstrap, no tests) |
| networking | 24 | ✓ | ✓ | **Complete** (Layers 0–4 incl. delta encoder + satellites, wired into EngineContext; DNS/bz2 deferred) |
| server | 1 | ✓ | ✓ | **In progress** (Chunk 6 — scaffold landed 2026-07-04, stubs only; session ladder under Chunk 6) |
| client | 0 | ✓ | ✗ | **Skeleton** (include stub exists) |
| content | 0 | ✗ | ✗ | **Skeleton** |
| demo | 0 | ✗ | ✗ | **Skeleton** |
| input | 0 | ✗ | ✗ | **Skeleton** |
| physics | 0 | ✗ | ✗ | **Skeleton** |
| renderer | 0 | ✗ | ✗ | **Skeleton** |
| save | 0 | ✗ | ✗ | **Skeleton** |
| sound | 0 | ✗ | ✗ | **Skeleton** |
| ui | 0 | ✗ | ✗ | **Skeleton** |
| world | 0 | ✗ | ✗ | **Skeleton** |

______________________________________________________________________

## Dependency Graph Summary

The legacy DAG flows: **launcher → host → (cmd_cvar + networking + filesystem) → server ↔ client → plugins (renderer, filesystem)**. The entire foundation tier is complete: `utilities`, `memory`, `filesystem`, `platform`, `core`, `cmd_cvar`, `host`/`engine_context` — **Chunks 1 and 3 are done**. Networking (**Chunk 2, done**; Chunk 4 was a renumbering duplicate — see its tombstone) shipped as a standalone library (address/MessageBuf, transport, wire codecs, netchan, GoldSrc protocol driver, master list, **delta encoder**) and was wired into `EngineContext` on 2026-07-03. `map_loader` (**Chunk 5, done 2026-07-04**) ships BSP v29/30/BSP2/30ext loading, the full PVS query surface, and the Q-18-gated trace kernel with the edict-free trace API Chunk 6 needs. Next is the server path (Chunk 6 — recon complete: `boundaries/server-boundary.md` + six `legacy-survey/deep-dive-server-*.md`), which is significantly more isolated than the client path because it only needs the Game DLL ABI; client adds sound, input, content, rendering, and UI on top.

______________________________________________________________________

## Subsystem Scores

| Subsystem | Isolation | Foundational | Complexity | ABI risk |
|------------|-----------|--------------|---------------------------------------------------|-----------------------------|
| cmd_cvar | High | **High** | Medium (~5 legacy files, global linked list) | Low |
| networking | High | **High** | Medium (~7 legacy files, UDP + channel layer) | Low |
| world | Medium | **High** | High (~8 files, trace + PM callbacks) | Medium (`pm_shared`) |
| server | Low | **High** | Very High (~15 files, 100+ `eiface` callbacks) | **High** (Game DLL FROZEN) |
| content | Medium | Medium | Medium (~5 format loaders) | Low |
| sound | High | Low | Medium (~5 files, DSP/mix) | Low |
| input | High | Low | Small (~5 files) | Low |
| physics | Medium | High | Medium (`pm_shared`, determinism) | **High** (`pm_shared` FROZEN) |
| save | Medium | Low | Small (1 file) | Medium (binary format) |
| demo | High | Low | Small (1 file) | Medium (delta encoding) |
| client | Low | **High** | Very High (30+ files, Client DLL ABI) | **High** (Client DLL FROZEN)|
| renderer | Low | Low | Very High (multiple backends) | Low (internal — free to redesign) |
| ui | Medium | Low | Small (`cl_gameui.c`) | Low |

______________________________________________________________________

## Recommended Work Plan

### Chunk 1 — cmd/cvar ✅ DONE

**Subsystems**: `cmd_cvar`\
**Depends on**: utilities, memory, platform *(all done)*\
**Recon/Boundary**: boundary spec `docs/boundaries/cmd_cvar-boundary.md`\
**Status**: 11 `.cpp` files, 4 public headers, 5 test files — `xash3dpp_cmd_cvar` builds and tests pass.

______________________________________________________________________

### Chunk 2 — networking ✅ DONE

*Delivered: the full networking library — address/MessageBuf, pluggable
transport + loopback, wire codecs (LZSS, split-packet, OOB), netchan
reliability/fragmentation, GoldSrc `IProtocolDriver`, master list satellite,
and the **delta encoder** (tables, field codec, delta.lst parser, wire
formats — see `legacy-survey/deep-dive-delta-encoder.md`). Wired into
`EngineContext` 2026-07-03. DNS resolve + bz2 remain deferred.*

**Subsystems**: `networking`\
**Depends on**: utilities, memory, platform *(all done)*\
**Recon/Boundary**: boundary spec `docs/boundaries/networking-boundary.md`; deep dive `legacy-survey/deep-dive-delta-encoder.md`\
**Legacy reference**: `engine/common/net_ws.c`, `net_buffer.c`, `net_chan.c`, `net_encode.c`

______________________________________________________________________

### Chunk 3 — host completion ✅ DONE

**Subsystems**: `host`, `core` (Clock), `map_loader` (FSM scaffold), `abi` (Host_Error shim)\
**Depends on**: cmd_cvar, filesystem, memory, platform *(all done)*\
**Recon/Boundary**: boundary spec `docs/boundaries/host-boundary.md`\
**What shipped**:

- `core::Clock` — timing cvars (`host_maxfps`, `fps_override`, `host_framerate`, `sys_ticrate`, etc.), `tick()` with FPS gate, renderer-thread-safe atomics
- `Host` pimpl restructured with injected-dep `HostInitParams`; `RunFrame`; `signal_frame_abort` with recursive-abort guard
- `EngineContext` flat-owner struct; `init/shutdown` sequencing; `set_current_engine_context` bookend
- `map_loader` FSM scaffold (state machine + observer slots; no BSP loading yet)
- `abi/engine_funcs.cpp` — `Host_Error` direct-export shim\
**ABI surfaces touched**: none\
**Tests**: `test_host.cpp` (init/shutdown, signal_frame_abort, bugcomp pass-through) — green

______________________________________________________________________

### Chunk 4 — *(tombstone)*

Folded into **Chunk 2** (pre-map_loader renumbering artifact — both were
"networking primitives"). The number is retained so existing
cross-references stay valid; see Chunk 2 for the delivered scope. Do not
renumber chunks.

______________________________________________________________________

### Chunk 5 — map loader (BSP + world queries) — ✅ DONE 2026-07-04

*Delivered: BSP v29/v30/BSP2/BSP30ext → immutable `WorldData`
(`load_world_data`, span + Filesystem overloads incl. the `.ent` patch);
full PVS query surface; the pm_trace clip-hull kernel float-exact to legacy
(golden vectors + 18k-trace bit-exact cross-check — the Q-18 gate);
edict-free trace API for Chunk 6; MapLoader FSM loads/owns the world; map
CRC for the protocol. Verification: format-watchdog CLEAR, loader +
kernel parity audits (findings fixed / PARITY-CONFIRMED), reviewer sweep +
finish-subsystem gates closed. See `docs/boundaries/map_loader-boundary.md`
and `docs/architecture/map_loader/`.*

**Subsystems**: `map_loader` (promote scaffold to real parser)\
**Depends on**: filesystem, utilities, memory *(all done)*\
**Recon/Boundary**: boundary spec `docs/boundaries/map_loader-boundary.md`; deep dives `legacy-survey/deep-dive-bsp-loader.md`, `deep-dive-trace-pvs.md`\
**Legacy reference**: `engine/common/model.c`, `mod_bmodel.c`, `engine/common/world.c`, `engine/common/pm_trace.c`, `pm_surface.c`\
**Complexity note**: BSP v30 format loading is straightforward; the hard part is the spatial query API (point-in-leaf, PVS decompression, clip-node trace) that server physics and networking (multicast/PVS) depend on. The trace interface must be decoupled from raw edict pointers now — otherwise the server ABI refactor in Chunk 6 is impossible.\
**ABI surfaces touched**: `pm_shared/` — determinism requirement; **float vs. fixed-point must be decided before this chunk ships**\
**Deliverable**: `MapLoader::load_bsp()` parses a real BSP v30 file; leaf/PVS queries return correct results; float/fixed decision committed to a doc

______________________________________________________________________

### Chunk 6 — server *(dedicated-server milestone — IN PROGRESS 2026-07-04)*

**Subsystems**: `server` (incl. the server-side pmove bridge `sv_pmove.c`, moved in from Chunk 11 per the boundary spec's satellite table)\
**Depends on**: cmd_cvar, networking (Chunk 2), map_loader (Chunk 5), host, filesystem, memory, platform\
**Recon/Boundary**: ✅ done 2026-07-04 (commit `43b07bb7`) — boundary spec `docs/boundaries/server-boundary.md`; deep dives `legacy-survey/deep-dive-server-{lifecycle,game-dll-bridge,clients,physics,world-frame,save-boundary}.md`. **Scaffold OQs decided 2026-07-04**: `server-boundary#OQ-1` → **Q-19 PHS_PLACEMENT** (PHS lands as a map_loader `phs` query module, in Chunk 6 scope) and `#OQ-5` → **Q-20 EDICT_STORE** (ABI-exact edict array as single store behind a zero-cost access seam); crosswalk in `decisions-architecture.md` §3a. **Scaffold unblocked.**\
**Legacy reference**: `engine/server/sv_main.c`, `sv_game.c` (159-slot `enginefuncs_t`), `sv_world.c`, `sv_phys.c`, `sv_pmove.c`, `sv_frame.c`, `sv_client.c`\
**Complexity note**: The `enginefuncs_t` table is 159 function pointers and `entvars_t` layout is byte-exact frozen — highest ABI risk in the entire rewrite. Getting `sv_game.c` to load and call a real HL game DLL without crashing is the integration milestone; `entvars_t` layout must be ABI-exact at the boundary even if internal entity storage differs.\
**ABI surfaces touched**: `engine/eiface.h`, `engine/edict.h` — **FROZEN Game DLL ABI**\
**Deliverable**: Dedicated server starts, loads HL `dlls/hl.dll`, runs a single map frame; server ctest green — **dedicated-server milestone**\
**Session ladder** (one green commit + checkpoint per step; slice order refined by S2 `plan-implementation` 2026-07-04 — world before bridge so the trace-family enginefuncs slots bind real code): S1 scaffold ✅ → S2 plan-implementation ✅ → S3 map_loader `phs` module (Q-19, golden-gated) ✅ → S4 edict arena + string pool + vendored ABI headers (Q-20) ✅ → S5 world interaction (areanodes/link/move/contents/lightstyles; SetAbsBox as injected seam) ✅ → S6 game-DLL bridge (enginefuncs ×159, DLL_FUNCTIONS resolution, in-tree fake-DLL test double) ✅ → S7 lifecycle (spawn/activate/changelevel seams, EngineContext wiring) ✅ → S8 frame loop + sv_phys + pmove bridge ◑ *(landed+reconciled+gated 2026-07-04; sv_pmove bridge + sv_move.c locomotion deferred)* → S9 clients/messaging/satellites (OQ-8 stub markers) ◑ *(landed+reconciled+gated 2026-07-04 — built in parallel with S8 in isolated worktrees; snapshot/delta pipeline + the S8↔S9 frame-loop seam deferred, see checkpoint S8+S9-parallel-reconcile)* → S10 sweep-module → S11 analyse-threading (OQ-9) → S12 document-architecture → S13 legacy-parity audit → S14 finish-subsystem + pre-pr → S15 hl.dll milestone smoke (needs a local HL install — user-provided)

**S8/S9 completion sequence** *(decided 2026-07-05, after the parallel-agent landing)*: S8/S9 landed their independent bulk but deferred the interdependent pieces. Remaining Chunk-6 order — **(1)** S9 snapshot/delta pipeline (frames ring `SV_UPDATE_BACKUP`, baselines, `AddToFullPack`/`SetupVisibility`, `entity_state_t` delta) — biggest gap, unblocks the send; **(2)** the S8↔S9 seam splice — wire `Host_ServerFrame`'s client/net steps + the ~11 marked `S8-seam` stubs + the `clc_move`/usercmd parse path; **(3)** the pmove bridge *last* (`sv_pmove.c` 1014 L + port `engine/common/pm_trace.c` 889 L + `pm_surface.c` 382 L + vendor `playermove_t`/`physent_t` from `pm_shared/pm_defs.h`). **The pmove bridge is NOT blocked on Chunk 11**: `PM_Move` is the game DLL's `NEW_DLL_FUNCTIONS` export (mods statically link `pm_shared`; repo-root `pm_shared/` is headers only), and the trace family is engine code composing over the Chunk-5 kernel. Chunk 11 owns only the client-prediction path (`cl_pmove.c`) + the both-paths determinism test; pmove *parity validation* ticks at S15/Chunk 11 (needs a real `PM_Move`). The shared `pm_trace`/`pm_surface` family likely lands in the `physics` subsystem (reused by Chunk 11's client path), not `src/server/` — settle at implementation time. **Snapshot-pipeline progress (step 1)**: 1a scaffold ✅ (`ae286fb1`) → 1b `SV_CreateBaseline` fill + instanced baselines ✅ (`aec40d29`) → 2 `SV_WriteEntitiesToClient` gather + `packet_entities` ring + `SV_EmitPacketEntities` delta-merge + per-client frames ring ✅ (`f43900e3`) → 3 `SV_WriteClientdataToMessage` + `SV_SendClientDatagram` datagram body (svc_time + clientdata/weapondata delta + entities) ✅ → 3b signon buffer (`sv.signon`, `MAX_INIT_MSG`) + the `SV_CreateBaseline` signon-write half (`svc_spawnbaseline` + per-edict baseline deltas + instanced list) ✅ → 4 `SV_EmitEvents` + `SV_EmitPings` datagram-riders — complete `SV_WriteEntitiesToClient`'s per-frame body (svc_event queue drain + packet_index resolution + args delta; svc_pings from the 2s-cached `SV_GetPlayerStats`/`SV_CalcPing` over the frame ring) ✅. **Snapshot/delta pipeline substantially complete.** Deferred to the **S8↔S9 seam splice** (each lacks its producer/destination until then): the event *producer* (`pfnPlaybackEvent`/`SV_PlaybackEventFull` fills `cl.events`), `SV_UpdateToReliableMessages`'s reliable fan-out (`sv.reliable_datagram` → per-client `netchan.message`), and the Netchan transmit + choke/rate send-gate. All wire encoding rides the existing networking `DeltaTables`. **Seam-splice progress (step 2)**: the networking foundation is decided — the host owns the single `NetworkContext` + its UDP sockets (`EngineContext` declares `networking` ahead of `server`); the server holds a non-owning `rt.net` handle and pulls its server socket each frame (the "host-routes" model — *not* a fresh register decision, it follows Q-2/Q-4 + the committed `IOobSink` seam). The messaging seam is a functional bidirectional netchan loop as of `41bc9d2f`: **A** ✅ (`d3b3f8b9`) `ServerInitParams.net` DI + `read_packets` connectionless ingress routing OOB → `handle_connectionless`, replies via a `NetworkContext`-backed `IOobSink`, wired into `host_server_frame`; **B** ✅ (`b7c4f192`) per-client `Netchan` + `Netchan_Setup` at connect / `Netchan_Clear` at drop, backed by new `NetworkContext::protocol_driver()`/`fragment_pool()` accessors; **C** ✅ (`1421fd91`) in-session demux (`Netchan_Process` + `SV_ExecuteClientMessage` clc loop + `SV_ParseClientMove` usercmd decode filling lastcmd/packet_loss/ping) — also fixed two latent bugs (connect wiping `cl.frames`; missing `usercmd_t` in the fixture `delta.lst`); **D** ✅ (`a7943b8e`) `SV_SendClientMessages` send-gate + `send_client_datagram`→`transmit_bits`→`send_packet`, wired into `host_server_frame`; **E** ✅ (`41bc9d2f`) `SV_UpdateToReliableMessages` reliable broadcast fan-out. **F** ✅ (`a2ab0011`) the event producer (`pfnPlaybackEvent`/`SV_PlaybackEventFull` fills `cl.events` — unreliable queue `FEV_UPDATE` slot-merge + `FEV_RELIABLE` `svc_event_reliable` staging into `cl.reliable` via `write_delta_event`; a new non-owning `EngineBridge::delta` carries the tables; the recipient PHS cull + groupinfo filter ride the same `S8-seam` visibility gate as `SV_Multicast`), consumed by the existing `emit_events`. **The entire S8↔S9 messaging + event seam is now complete** — a bidirectional netchan loop plus the closed event producer→consumer path. Remaining Chunk-6 work before the frame loop is fully fleshed: the **pmove bridge** (`SV_RunCmd`, ordered last). Deferred seams inside the landed slices are marked `XASH3DPP-STUB(chunk6-S9)` / `XASH3DPP-STUB(S8-seam)`: command checksum, freeze/pause + `SV_RunCmd` (pmove), `SV_CalcClientTime` unlag, `host_limitlocal`/`sv_failuretime` send-gates, `FCL_RESEND_USERINFO`/`MOVEVARS` resends, the unreliable `sv.datagram` per-client append, the recipient PHS/groupinfo cull (`SV_CheckClientVisiblity`, shared with `SV_Multicast`), and fragment reassembly.

**pmove-bridge decomposition (step 3, the last frame-loop piece)** *(planned 2026-07-05, recon from `deep-dive-server-physics.md` §4/§9 + `pm_shared/pm_defs.h` + `common/pmove.h`)*: ~2285 legacy lines (`sv_pmove.c` 1015 + `engine/common/pm_trace.c` 889 + `pm_surface.c` 382) + two frozen ABI structs. Slices: **P1** ✅ *(2026-07-05)* — vendor the pmove ABI into `include/xash3dpp/abi/pm_defs.hpp`: `pmplane_t` (16 B: `vec3 normal`+`float dist`), `pmtrace_t` (68 B: allsolid/startsolid/inopen/inwater/fraction/endpos/plane/ent/deltavelocity/hitgroup — `common/pmove.h:26-47`), `physent_t` (`pm_defs.h:37-77`), `playermove_t` (`pm_defs.h:79-215`: state block + `physents[600]`/`moveents[64]`/`visents[600]` + `usercmd_t cmd` + `pmtrace_t touchindex[600]` + `char physinfo[256]` + `movevars_t*` + `player_mins/maxs[4]` + ~30 fn-pointers). Caps: `MAX_PHYSENTS 600`/`MAX_MOVEENTS 64`/`MAX_CLIP_PLANES 5`/`MAX_PHYSINFO_STRING 256`. `movevars_t` already vendored (`pm_movevars.hpp`). Layout `static_assert`s per the `pm_movevars.hpp`/`event_state.hpp` pattern; opaque engine types (`model_s`/`hull_s`/`msurface_s`/`trace_t`) forward-declared for the fn-pointer slots (the DLL calls through them, so signatures must stay ABI-exact). **Gate: abi-watchdog CLEAR** — `tests/server/abi/test_pmove_layout.cpp` includes the real `pm_defs.h` in a sealed namespace and X-macro-pins every field offset **and** member size vs the vendored structs (both pointer widths); `model_s`/`hull_s`/`msurface_s` tag-fwd-declared, `trace_t` kept opaque (it is an anon-struct typedef with no tag). **P2** — `SV_SetupPMove`/`SV_FinishPMove` (`sv_pmove.c:521-664`) + physent gather (`SV_AddLinksToPmove`/`AddLaddersToPmove`/`CopyEdictToPhysEnt` :42-322) over the S6 areanodes; the `usehull=FL_DUCKING?1:0`, multiplayer `onground=-1`, `pmove->time=timebase*1000` ms, `waterjumptime↔teleport_time` quirks (§8 #25-26). **P3** — `SV_InitClientMove` (30 fn-ptrs) + the `PM_*` trace family (`pm_trace.c`/`pm_surface.c`) composing over the map_loader BSP kernel; **the server/physics-vs-shared placement decision lands here** (lean: build in `src/server/physics/` — where `physics.cpp`/`movevars.cpp` already live — structured so Chunk 11's client path can extract it, rather than scaffolding an empty physics subsystem now). **P4** — `SV_RunCmd` (`:887-1014`: speedhack guard, `msec>50` split, `CmdStart→PlayerPreThink→PM_Move→touches(deltavelocity)→PlayerPostThink→CmdEnd`) → fills the `XASH3DPP-STUB(chunk6-S9/pmove)` seam at `client_state.cpp:604-607` + `pfnRunPlayerMove` fakeclient path (`engine_table.cpp:1577`). Gates: reviewer + `legacy-parity-auditor` spot-check. **P5 (defer)** — lag-comp (`SV_SetupMoveInterpolant`/`Restore`, §4), stubbed for the milestone; pmove *parity* ticks at S15/Chunk 11. Current pmove stubs: `client_state.cpp:604`, `game_host.cpp:205/344`, `engine_table.cpp:1577`.

______________________________________________________________________

### Chunk 7 — content pipeline (model & image loaders)

**Subsystems**: `content`\
**Depends on**: filesystem, utilities, memory *(all done)*\
**Recon/Boundary**: none yet — run `/analyse-subsystem content` before scaffold\
**Legacy reference**: `engine/common/imagelib/` (BMP, DDS, TGA, KTX2, WAD), `mod_studio.c`, `mod_alias.c`, `mod_sprite.c`\
**Complexity note**: Studio model has bone/sequence/mesh/texture sub-formats; the hardest part is texture format normalisation (WAD→RGBA, DXT→RGBA) done identically to GoldSrc for visual compat. `common/com_model.h` struct layout is frozen for client DLL reads.\
**ABI surfaces touched**: `common/com_model.h` structs read by client DLL — layout frozen.\
**Deliverable**: Typed model handle lookup; WAD texture pack/unpack test; studio header parse test

______________________________________________________________________

### Chunk 8 — save/restore

**Subsystems**: `save`\
**Depends on**: server (Chunk 6), filesystem, map_loader\
**Recon/Boundary**: partial — `legacy-survey/deep-dive-server-save-boundary.md` maps the server↔save seam (Chunk 6 keeps the `SV_ChangeLevel` orchestration + `pSaveData`/`svc_restore` seams and stubs the four save primitives `SaveGameState`/`LoadGameState`/`LoadAdjacentEnts`/`ClearSaveDir`; Chunk 8 owns serialization only, per `server-boundary.md` satellites). Format-internals recon still needed at Chunk 8 start.\
**Legacy reference**: `engine/server/sv_save.c` (format tag 0x71, ~2 500 lines)\
**Complexity note**: Binary format assumes exact `entvars_t` field ordering — any field added to server edict layout must be mirrored here; the field-map serialiser is fiddly but well-bounded.\
**ABI surfaces touched**: none frozen externally, but save files from the legacy engine must remain loadable.\
**Deliverable**: Round-trip save/load of a minimal game state; reject-gracefully on version mismatch

______________________________________________________________________

### Chunk 9 — sound *(isolatable, slot anywhere after foundation)*

**Subsystems**: `sound`\
**Depends on**: filesystem, memory, platform *(all done — can run alongside other chunks)*\
**Recon/Boundary**: none yet — run `/analyse-subsystem sound` before scaffold\
**Legacy reference**: `engine/client/sound/s_main.c`, `s_load.c`, `s_mix.c`, `s_dsp.c`\
**Complexity note**: Needs a null-output device backend for CI so the mixer can be tested without hardware.\
**ABI surfaces touched**: none\
**Deliverable**: `xash3dpp_sound` builds; wav decode + mix test passes with null output device

______________________________________________________________________

### Chunk 10 — input *(isolatable, slot anywhere after foundation)*

**Subsystems**: `input`\
**Depends on**: platform *(done — can run alongside other chunks)*\
**Recon/Boundary**: none yet — run `/analyse-subsystem input` before scaffold\
**Legacy reference**: `engine/client/input/input.c`, `in_keys.c`, `in_joy.c`\
**Complexity note**: Abstract the SDL/Win32/touch event sources behind an interface so the subsystem is testable without a window.\
**ABI surfaces touched**: none\
**Deliverable**: `xash3dpp_input` builds; key/button event tests pass with mock event source

______________________________________________________________________

### Chunk 11 — physics (pm_shared)

**Subsystems**: `physics`\
**Depends on**: map_loader (Chunk 5), server (Chunk 6 — edict query interface)\
**Recon/Boundary**: partial — `design/pm-determinism-decision.md` (Q-18) + `legacy-survey/deep-dive-trace-pvs.md`; full boundary spec needed at chunk start. Note: the server-side pmove bridge (`sv_pmove.c`) moved to Chunk 6 per `server-boundary.md` — this chunk covers the client-prediction path and the both-paths determinism test.\
**Legacy reference**: `pm_shared/pm_move.c`, `pm_shared/pm_trace.c`, `engine/client/dll_int/cl_pmove.c`\
**Complexity note**: Client prediction and server authority must produce bit-identical results — determinism is the hardest constraint; float/fixed choice from Chunk 5 is locked in here.\
**ABI surfaces touched**: `pm_shared/` — **FROZEN** (shared client ↔ server)\
**Deliverable**: `PM_Move` runs identically on both paths; determinism regression test passes

______________________________________________________________________

### Chunk 12 — client

**Subsystems**: `client`, `demo` stub, `ui` stub\
**Depends on**: all server-path chunks + sound (Chunk 9) + input (Chunk 10)\
**Recon/Boundary**: none yet (broad survey only: `legacy-survey/engine-client.md`) — run `/analyse-subsystem client` before scaffold\
**Legacy reference**: `engine/client/cl_main.c`, `cl_frame.c`, `parse/cl_parse.c`, `dll_int/cl_game.c`, `dll_int/cl_gameui.c`\
**Complexity note**: Client DLL bridge (`cdll_int.h` / `cdll_exp.h`) plus prediction wiring are the largest surfaces; scope to connect → parse → predict → render-one-frame; defer VGUI/UI to Chunk 13. The thread-model decision (whether network I/O or rendering run off-main) must be made before this chunk starts.\
**ABI surfaces touched**: `engine/cdll_int.h`, `engine/cdll_exp.h` — **FROZEN Client DLL ABI**\
**Deliverable**: Client connects to local server, loads HL `cl_dlls/client.dll`, parses one SVC_PRINT frame; Chunk 12a scope only — rendering deferred to Chunk 13

______________________________________________________________________

### Chunk 13 — renderer

**Subsystems**: `renderer`\
**Depends on**: map_loader, content, platform, client (Chunk 12)\
**Recon/Boundary**: none yet (broad survey only: `legacy-survey/renderers.md`)\
**Legacy reference**: `ref/gl/gl_rmain.c`, `gl_studio.c`, `gl_rsurf.c`; `engine/ref_api.h` (v17) as a compat reference\
**Complexity note**: The renderer ABI is fully internal — `ref_api.h` can be replaced with any shape — but GoldSrc visual output (BSP lightmaps, studio model rendering, water warp) must match. **Vulkan vs. GL vs. multi-backend decision needed before this chunk starts.**\
**ABI surfaces touched**: none frozen — internal\
**Deliverable**: GL stub renders world geometry of a HL map; null renderer passes through client frame loop

______________________________________________________________________

### Chunk 14 — leaves (demo, ui)

**Subsystems**: `demo`, `ui`\
**Depends on**: client (Chunk 12) — one at a time after respective parents\
**Recon/Boundary**: none yet\
**Legacy reference**: `cl_demo.c`, `cl_gameui.c`\
**Complexity note**: Each is a leaf with no unimplemented dependents; scope individually. Demo format is internal and a breaking change is acceptable.\
**ABI surfaces touched**: demo format (internal, breaking change OK)\
**Deliverable**: Demo record/playback test; MainUI DLL loads

______________________________________________________________________

## Open Questions / Landmines

- **pm_shared float vs. fixed-point** — **DECIDED** (Q-18 PM_FP_MODEL,
  2026-07-04): `float` matching the frozen `playermove_t` ABI, with the FP
  model pinned strict-by-default in CMake (`/fp:precise`,
  `-ffp-contract=off`; fast-math forbidden for simulation-critical targets).
  Fixed-point was rejected because mods compile pm_shared into their own
  DLLs — engine-side fixed-point cannot achieve system determinism and
  worsens parity. Presentation-side targets may later opt out per target via
  `xash3dpp_relax_fp()` (`cmake/fp_model.cmake`). Golden trace fixtures gate
  Chunk 5. See `docs/design/pm-determinism-decision.md`.

- **Networking wire-compat target** — **DECIDED**: GoldSrc-compatible
  protocol is the default and is wire-frozen (netchan framing, delta-encoder
  field tables, game-protocol message IDs match the legacy bytes exactly).
  Per-client protocol selection is supported via an `IProtocolDriver` seam in
  netchan so a newer / experimental driver can be registered without
  touching the default path. See `boundaries/networking-boundary.md`
  §"Pluggable game protocol per client".

- **`entvars_t` internal representation** — now tracked as
  `server-boundary#OQ-5` (with a recommendation: keep the ABI-exact edict
  array as the single authoritative store in Chunk 6; handleization is a
  post-parity refactor). Must be resolved before the server scaffold. The
  server must present `entvars_t` at the exact ABI-specified layout when
  calling the game DLL; any non-literal internal storage needs a
  projection/copy step at every DLL call boundary and reshapes Chunk 8.

- **Renderer backend choice** — `ref_api.h` is internal and free to redesign. Decision needed before Chunk 13: Vulkan-first (modern, mobile-hostile), GL-compat (broadest reach), or abstracted multi-backend (most work). Also needs to settle on desktop vs. mobile as the primary target. Can safely be deferred until after Chunk 12 (client) is green.

- **Thread model** — The legacy engine is single-threaded. `core::Clock` already uses atomics for renderer-thread reads. A decision on whether networking I/O or rendering move to separate threads is needed before Chunk 12 (client), because it determines whether `cl_parse` and `S_Update` can run concurrently.

- **HTTP downloader + master-server placement** — **DECIDED** (Q-11
  satellite-placement test): HTTP is a separate `xash3dpp_http` target with
  its own boundary spec; master-server list stays in `xash3dpp_networking`.
  A later grouping pass may move related targets into a shared `src/net/`
  subdirectory. See `decisions-architecture.md §Q-11`.

- **Platform sockets layer** — **DONE**: `xash3dpp_platform` ships
  `IPlatformSockets` + the `os_socket` layer (see the status table); the
  requirements doc lives at `architecture/platform/sockets.md`.

- **Legacy survey gaps** — Boundary specs still needed for `client`,
  `content`, `sound`, `input`, `physics`, and `save` (`server` is done —
  `boundaries/server-boundary.md`). Each chunk starts with a
  `/analyse-subsystem` pass before writing any code. Live boundary-spec
  open questions are indexed in the **OQ crosswalk** in
  `docs/design/decisions-architecture.md`.

______________________________________________________________________

## Tooling / automation follow-ups

Deferred items from the 2026-07-04 workflow-hardening pass (tracked here so
they don't get lost; pick up opportunistically or when the trigger fires):

- **Shared legacy-header include prelude for layout-parity tests** — the
  guard pre-defines + typedef prelude for including real legacy headers in
  a sealed namespace now exists twice (`tests/server/abi/
  test_edict_layout.cpp`, `test_eiface_layout.cpp`). Extract
  `tests/legacy_abi_prelude.hpp` before writing the third copy.
  **Trigger**: S8 vendors `playermove_t`; S9 vendors `entity_state_t`/
  `usercmd_t`.
- **cmd_cvar NS_QUALIFY sweep** — ~115 candidate warnings (relative
  `memory::`/`utilities::`/`limits::` sibling refs) because the
  NS_QUALIFY rule postdates Chunk 1; plus a handful of thread-assert and
  pre-reserve candidates. Mechanical hygiene pass; found by the first
  cross-subsystem `compliance_scan --slice` run (S7a touched
  `cvar_ops.cpp`). Do alongside another cmd_cvar-touching session or the
  S10 sweep. Known remaining stub: `cvar_write_variables` (surfaced by
  the stub-debt report; its consumer is host config write-out).
- **Crash-stack capture tool** *(deferred by decision, 2026-07-04)* — a
  `crash_run` xtool that reruns a failing test under the Windows SDK
  `cdb` and returns the stack + loaded modules as JSON, for native
  crashes the `test.py` exit-code decoder can't localize.
  **Trigger**: the first unexplained native crash inside a real game DLL
  (S13 parity fixes / S15 hl.dll smoke test).
- **Q-18 golden-vector generator** *(deferred by decision)* — a committed
  CMake target/script that compiles the frozen legacy kernels
  (`pm_trace.c` hull check, CRC32, PVS decompress, delta field codec) and
  emits bit-exact expected values for a table of inputs, replacing the
  uncommitted throwaway harness behind the Chunk 5 golden literals.
  **Trigger**: the next Q-18-gated subsystem (Chunk 6 server physics /
  Chunk 11 pm determinism).
- **`cxx_std_20` → `cxx_std_23` normalization** — most targets declare
  `target_compile_features(... cxx_std_20)` minimums while the project
  standard is C++23 (root `CMAKE_CXX_STANDARD 23`; map_loader already
  declares 23). Harmless today; normalize in a mechanical pass.
- **Session 2 of the hardening pass** — full adapter parity (9 remaining
  Claude command adapters, 21 opencode command adapters, 3 opencode agent
  adapters, shared `.claude/settings.json` + gitignored local settings),
  twin root `AGENTS.md`/`CLAUDE.md` with SYNC-CORE blocks,
  `.github/AGENT-SETUP.md`, and the WORKFLOW.md upgrades (recon front-end
  in the pipeline diagram, session-scoping/token-budget section, tooling
  section). Fully specified in the approved hardening plan; gate =
  `tools/workflow_sync.py` (full stage) exit 0.
