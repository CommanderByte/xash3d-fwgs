# xash3dpp — Implementation Status and Work Plan

*Generated: 2026-05-15 — Updated: 2026-07-06*

> Status-table labels: **Complete / Partial / Skeleton** (structural, as
> reported by `xash3dpp/tools/status_table.py --check`); chunk headings use
> **✅ DONE / IN PROGRESS / TODO** (workflow status incl. gates). A
> subsystem can be structurally Complete while its chunk still has open
> gates. After Chunk 6B, **Complete additionally means Q-22/QN-conformant**
> (lifecycle + annotation discipline) — the retrofit wave re-baselines every
> completed subsystem without changing the column values.

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
| server | 1 | ✓ | ✓ | **Complete** (Chunk 6 — dedicated-server milestone ACHIEVED 2026-07-05: real 32-bit `hl.dll` loads + `c0a0` spawns + one map frame runs clean; OQ-8 milestone-trimmed backlog tracked in the deferred inventory) |
| client | 0 | ✓ | ✗ | **Skeleton** (include stub exists) |
| content | 13 | ✓ | ✓ | **Partial** (model cache + 3 loaders + 7 image codecs + studio **bone solver** [OQ-5 ✅ bit-exact vs Q-18 goldens] + pose pfns; `SV_ClipMoveToEntity` studio hitbox trace-loop gated on the hl.dll smoke — see Chunk 7) |
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

### Chunk 6 — server ✅ DONE *(dedicated-server milestone — ACHIEVED 2026-07-05)*

**Subsystems**: `server` (incl. the server-side pmove bridge `sv_pmove.c`, moved in from Chunk 11 per the boundary spec's satellite table)\
**Depends on**: cmd_cvar, networking (Chunk 2), map_loader (Chunk 5), host, filesystem, memory, platform\
**Recon/Boundary**: ✅ done 2026-07-04 (commit `43b07bb7`) — boundary spec `docs/boundaries/server-boundary.md`; deep dives `legacy-survey/deep-dive-server-{lifecycle,game-dll-bridge,clients,physics,world-frame,save-boundary}.md`. **Scaffold OQs decided 2026-07-04**: `server-boundary#OQ-1` → **Q-19 PHS_PLACEMENT** (PHS lands as a map_loader `phs` query module, in Chunk 6 scope) and `#OQ-5` → **Q-20 EDICT_STORE** (ABI-exact edict array as single store behind a zero-cost access seam); crosswalk in `decisions-architecture.md` §3a. **Scaffold unblocked.**\
**Legacy reference**: `engine/server/sv_main.c`, `sv_game.c` (159-slot `enginefuncs_t`), `sv_world.c`, `sv_phys.c`, `sv_pmove.c`, `sv_frame.c`, `sv_client.c`\
**Complexity note**: The `enginefuncs_t` table is 159 function pointers and `entvars_t` layout is byte-exact frozen — highest ABI risk in the entire rewrite. Getting `sv_game.c` to load and call a real HL game DLL without crashing is the integration milestone; `entvars_t` layout must be ABI-exact at the boundary even if internal entity storage differs.\
**ABI surfaces touched**: `engine/eiface.h`, `engine/edict.h` — **FROZEN Game DLL ABI**\
**Deliverable**: Dedicated server starts, loads HL `dlls/hl.dll`, runs a single map frame; server ctest green — **dedicated-server milestone**\
**Session ladder** (one green commit + checkpoint per step; slice order refined by S2 `plan-implementation` 2026-07-04 — world before bridge so the trace-family enginefuncs slots bind real code): S1 scaffold ✅ → S2 plan-implementation ✅ → S3 map_loader `phs` module (Q-19, golden-gated) ✅ → S4 edict arena + string pool + vendored ABI headers (Q-20) ✅ → S5 world interaction (areanodes/link/move/contents/lightstyles; SetAbsBox as injected seam) ✅ → S6 game-DLL bridge (enginefuncs ×159, DLL_FUNCTIONS resolution, in-tree fake-DLL test double) ✅ → S7 lifecycle (spawn/activate/changelevel seams, EngineContext wiring) ✅ → S8 frame loop + sv_phys + pmove bridge ✅ *(landed+reconciled+gated 2026-07-04; pmove bridge P1–P4 complete 2026-07-05 — SV_RunCmd drives both bot + real-client paths; P5 lag-comp + sv_move.c locomotion deferred to the S10 inventory)* → S9 clients/messaging/satellites (OQ-8 stub markers) ◑ *(landed+reconciled+gated 2026-07-04 — built in parallel with S8 in isolated worktrees; snapshot/delta pipeline + the S8↔S9 frame-loop seam substantially complete; the residual client/messaging backlog is catalogued in the S10 deferred-stub inventory)* → S10 sweep-module ✅ *(2026-07-05 — compliance clean (0 findings after the world_hooks thread-assert fix); the 149-marker deferral backlog consolidated into the "Chunk 6 — deferred stub inventory" section above)* → S11 analyse-threading (OQ-9) ✅ *(2026-07-05 — `threading-analysis/server-threading.md`; OQ-9 main-thread-only, safe-by-contract)* → S12 document-architecture ✅ *(2026-07-05 — `architecture/server/` README+index+6 concept pages)* → S13 legacy-parity audit ✅ *(2026-07-05 — 4-way parallel auditor fan-out over lifecycle/physics/world/snapshot; 9 CONFIRMED divergences, 7 fixed (`e557290b` messaging/lifecycle/water-timer), the rotated-brush matrix Q-18 + PLAUSIBLE set tracked in the deferred inventory)* → S14 finish-subsystem + pre-pr ✅ *(2026-07-05 — SHIP: finish_check 8/9 (item 2 limits = pre-adjudicated wire-frozen opcodes), compliance prepr 0/0/0/0, reviewer SHIP on the S13 batch)* → S15 hl.dll milestone smoke ✅ *(2026-07-05 — **the milestone**: the real retail 32-bit `valve/dlls/hl.dll` loads through the production `GiveFnptrsToDll` handshake, `c0a0.bsp` loads via MapLoader, the full `SV_SpawnServer → spawn_entities → SV_ActivateServer` chain + one `Host_ServerFrame` run clean with zero `Host_Error` — 10/10 asserts. Harness `tests/server/lifecycle/test_hl_smoke.cpp`, driven by the MapLoader FSM through the `ILevelChangeExecutor` seam exactly as the live engine drives `map c0a0`. Asset-gated (`XASH_HL_ROOT`) + arch-gated (x86 only; `hl.dll` is 32-bit) — a SKIP is a pass, so CI / x64 / HL-less machines stay green (suite 89/89 both arches). The x86 build/test loop is a first-class MCP `arch` axis (`b9e6c6ff`).)* — **dedicated-server milestone ACHIEVED**

**S8/S9 completion sequence** *(decided 2026-07-05, after the parallel-agent landing)*: S8/S9 landed their independent bulk but deferred the interdependent pieces. Remaining Chunk-6 order — **(1)** S9 snapshot/delta pipeline (frames ring `SV_UPDATE_BACKUP`, baselines, `AddToFullPack`/`SetupVisibility`, `entity_state_t` delta) — biggest gap, unblocks the send; **(2)** the S8↔S9 seam splice — wire `Host_ServerFrame`'s client/net steps + the ~11 marked `S8-seam` stubs + the `clc_move`/usercmd parse path; **(3)** the pmove bridge *last* (`sv_pmove.c` 1014 L + port `engine/common/pm_trace.c` 889 L + `pm_surface.c` 382 L + vendor `playermove_t`/`physent_t` from `pm_shared/pm_defs.h`). **The pmove bridge is NOT blocked on Chunk 11**: `PM_Move` is the game DLL's `NEW_DLL_FUNCTIONS` export (mods statically link `pm_shared`; repo-root `pm_shared/` is headers only), and the trace family is engine code composing over the Chunk-5 kernel. Chunk 11 owns only the client-prediction path (`cl_pmove.c`) + the both-paths determinism test; pmove *parity validation* ticks at S15/Chunk 11 (needs a real `PM_Move`). The shared `pm_trace`/`pm_surface` family likely lands in the `physics` subsystem (reused by Chunk 11's client path), not `src/server/` — settle at implementation time. **Snapshot-pipeline progress (step 1)**: 1a scaffold ✅ (`ae286fb1`) → 1b `SV_CreateBaseline` fill + instanced baselines ✅ (`aec40d29`) → 2 `SV_WriteEntitiesToClient` gather + `packet_entities` ring + `SV_EmitPacketEntities` delta-merge + per-client frames ring ✅ (`f43900e3`) → 3 `SV_WriteClientdataToMessage` + `SV_SendClientDatagram` datagram body (svc_time + clientdata/weapondata delta + entities) ✅ → 3b signon buffer (`sv.signon`, `MAX_INIT_MSG`) + the `SV_CreateBaseline` signon-write half (`svc_spawnbaseline` + per-edict baseline deltas + instanced list) ✅ → 4 `SV_EmitEvents` + `SV_EmitPings` datagram-riders — complete `SV_WriteEntitiesToClient`'s per-frame body (svc_event queue drain + packet_index resolution + args delta; svc_pings from the 2s-cached `SV_GetPlayerStats`/`SV_CalcPing` over the frame ring) ✅. **Snapshot/delta pipeline substantially complete.** Deferred to the **S8↔S9 seam splice** (each lacks its producer/destination until then): the event *producer* (`pfnPlaybackEvent`/`SV_PlaybackEventFull` fills `cl.events`), `SV_UpdateToReliableMessages`'s reliable fan-out (`sv.reliable_datagram` → per-client `netchan.message`), and the Netchan transmit + choke/rate send-gate. All wire encoding rides the existing networking `DeltaTables`. **Seam-splice progress (step 2)**: the networking foundation is decided — the host owns the single `NetworkContext` + its UDP sockets (`EngineContext` declares `networking` ahead of `server`); the server holds a non-owning `rt.net` handle and pulls its server socket each frame (the "host-routes" model — *not* a fresh register decision, it follows Q-2/Q-4 + the committed `IOobSink` seam). The messaging seam is a functional bidirectional netchan loop as of `41bc9d2f`: **A** ✅ (`d3b3f8b9`) `ServerInitParams.net` DI + `read_packets` connectionless ingress routing OOB → `handle_connectionless`, replies via a `NetworkContext`-backed `IOobSink`, wired into `host_server_frame`; **B** ✅ (`b7c4f192`) per-client `Netchan` + `Netchan_Setup` at connect / `Netchan_Clear` at drop, backed by new `NetworkContext::protocol_driver()`/`fragment_pool()` accessors; **C** ✅ (`1421fd91`) in-session demux (`Netchan_Process` + `SV_ExecuteClientMessage` clc loop + `SV_ParseClientMove` usercmd decode filling lastcmd/packet_loss/ping) — also fixed two latent bugs (connect wiping `cl.frames`; missing `usercmd_t` in the fixture `delta.lst`); **D** ✅ (`a7943b8e`) `SV_SendClientMessages` send-gate + `send_client_datagram`→`transmit_bits`→`send_packet`, wired into `host_server_frame`; **E** ✅ (`41bc9d2f`) `SV_UpdateToReliableMessages` reliable broadcast fan-out. **F** ✅ (`a2ab0011`) the event producer (`pfnPlaybackEvent`/`SV_PlaybackEventFull` fills `cl.events` — unreliable queue `FEV_UPDATE` slot-merge + `FEV_RELIABLE` `svc_event_reliable` staging into `cl.reliable` via `write_delta_event`; a new non-owning `EngineBridge::delta` carries the tables; the recipient PHS cull + groupinfo filter ride the same `S8-seam` visibility gate as `SV_Multicast`), consumed by the existing `emit_events`. **The entire S8↔S9 messaging + event seam is now complete** — a bidirectional netchan loop plus the closed event producer→consumer path. Remaining Chunk-6 work before the frame loop is fully fleshed: the **pmove bridge** (`SV_RunCmd`, ordered last). Deferred seams inside the landed slices are marked `XASH3DPP-STUB(chunk6-S9)` / `XASH3DPP-STUB(S8-seam)`: command checksum, freeze/pause + `SV_RunCmd` (pmove), `SV_CalcClientTime` unlag, `host_limitlocal`/`sv_failuretime` send-gates, `FCL_RESEND_USERINFO`/`MOVEVARS` resends, the unreliable `sv.datagram` per-client append, the recipient PHS/groupinfo cull (`SV_CheckClientVisiblity`, shared with `SV_Multicast`), and fragment reassembly.

**pmove-bridge decomposition (step 3, the last frame-loop piece)** *(planned 2026-07-05, recon from `deep-dive-server-physics.md` §4/§9 + `pm_shared/pm_defs.h` + `common/pmove.h`)*: ~2285 legacy lines (`sv_pmove.c` 1015 + `engine/common/pm_trace.c` 889 + `pm_surface.c` 382) + two frozen ABI structs. Slices: **P1** ✅ *(2026-07-05)* — vendor the pmove ABI into `include/xash3dpp/abi/pm_defs.hpp`: `pmplane_t` (16 B: `vec3 normal`+`float dist`), `pmtrace_t` (68 B: allsolid/startsolid/inopen/inwater/fraction/endpos/plane/ent/deltavelocity/hitgroup — `common/pmove.h:26-47`), `physent_t` (`pm_defs.h:37-77`), `playermove_t` (`pm_defs.h:79-215`: state block + `physents[600]`/`moveents[64]`/`visents[600]` + `usercmd_t cmd` + `pmtrace_t touchindex[600]` + `char physinfo[256]` + `movevars_t*` + `player_mins/maxs[4]` + ~30 fn-pointers). Caps: `MAX_PHYSENTS 600`/`MAX_MOVEENTS 64`/`MAX_CLIP_PLANES 5`/`MAX_PHYSINFO_STRING 256`. `movevars_t` already vendored (`pm_movevars.hpp`). Layout `static_assert`s per the `pm_movevars.hpp`/`event_state.hpp` pattern; opaque engine types (`model_s`/`hull_s`/`msurface_s`/`trace_t`) forward-declared for the fn-pointer slots (the DLL calls through them, so signatures must stay ABI-exact). **Gate: abi-watchdog CLEAR** — `tests/server/abi/test_pmove_layout.cpp` includes the real `pm_defs.h` in a sealed namespace and X-macro-pins every field offset **and** member size vs the vendored structs (both pointer widths); `model_s`/`hull_s`/`msurface_s` tag-fwd-declared, `trace_t` kept opaque (it is an anon-struct typedef with no tag). **P2** ✅ *(2026-07-05, `physics/pmove.cpp`)* — `SV_SetupPMove`/`SV_FinishPMove` (`sv_pmove.c:521-664`) + physent gather (`SV_AddLinksToPmove`/`AddLaddersToPmove`/`CopyEdictToPhysEnt` :42-322) over the S6 areanodes; the `usehull=FL_DUCKING?1:0`, multiplayer `onground=-1`, `pmove->time=timebase*1000` ms, `waterjumptime↔teleport_time` quirks (§8 #25-26). `rt.pmove` = pool-owned `playermove_t` (allocated in `load_progs`); `ServerClient.timebase` + `k_dead_dead` added; the visents-before-skip / physents-last gather ordering preserved. Lag-comp (`SV_GetTrueOrigin`) and the `pe->model`/studio-hitbox handle binding are the P5 / P3 / Chunk-7 seams (marked `pmove-P3` / `chunk7/OQ-2`). Gates: reviewer SHIP, legacy-parity-auditor PARITY-CONFIRMED (41/41). **P3a** ✅ *(2026-07-05, `physics/pm_trace.cpp`)* — the `PM_*` trace/point-contents family (`pm_trace.c` groups a/b/d): `PM_PlayerTraceExt`, `PM_TestPlayerPosition`, `PM_TraceModel`, `PM_TraceLine`/`Ex`, `PM_PointContents`/`TruePointContents`/`PointContentsPmove`, `PM_HullForBsp`, `PM_StuckTouch` — composed over the **existing edict-free map_loader kernel** (`recursive_hull_check`/`hull_point_contents`/`world_hull`/`hull_for_bsp`/`BoxHull`) and the S6 rotated-brush transforms (`world_transform_aabb`/`transform_positive_plane`), sourced from the gathered physent list (usehull-indexed player bounds) via a `PmTraceEnv`. Each physent's brush submodel is resolved through the arena + `IModelResolver` (`pe->info`→edict→`modelindex`), retiring the P2 `pe->model` binding TODO — no opaque engine handle is stashed. **Placement decision settled**: built in `src/server/physics/` (alongside `physics.cpp`/`movevars.cpp`), structured so Chunk 11's client path can extract it; no empty `physics` subsystem scaffolded. Vendored the `PM_*` trace flags (`k_pm_world_only`/`glass_ignore`/`custom_ignore`/`studio_ignore`/`traceline_*`) into `pm_defs.hpp`. **Group (c)** — the surface/texture trace family (`PM_TraceSurface`/`PM_TraceTexture`/`PM_RecursiveSurfCheck`) — is **deferred to Chunk 7** (needs `mfacebevel_t` facet bevels + miptex original buffers `WorldData` does not carry until the content pipeline; same dependency class as the studio hitbox hulls) and stubbed at the P3b fn-ptr table. Studio hitbox hulls fall back to the bbox (Chunk 7 / OQ-2); SOLID_CUSTOM is the S8 physics-interface sweep seam (no-hit for the milestone). Tests cross-check `pm_player_trace_ext` against the raw kernel primitive (oracle) + deterministic single-box physent scenarios + hand-derived hull-0 point-contents. Gates: reviewer + parity spot-check. **P3b** ✅ *(2026-07-05, `physics/init_client_move.cpp`)* — `SV_InitClientMove`: installs the ~30-entry PM_* callback table into `rt.pmove` via context-free pfn shims that reach the installed `EngineBridge` (added `playermove_t* pmove` + `const HullBoundsTable* player_bounds` to the bridge), sets `server`/`movevars`/hull-bounds table, and runs the DLL's `pfnPM_Init` (a `DLL_FUNCTIONS` export, null-guarded); wired from `load_progs` right after the pmove allocation. **Wired to real impls**: the trace family (`PM_PlayerTrace`/`Ex`, `PM_TestPlayerPosition`/`Ex`, `PM_TraceLine`/`Ex`, `PM_TraceModel` via the legacy `(pmtrace_t*)trace` pun), point contents (`PM_PointContents`/`TruePointContents`), `PM_StuckTouch`, the utilities (`Info_ValueForKey`, `Con_*`, `Sys_FloatTime`→`platform::get_time`, `RandomLong`/`Float` xorshift), and `PM_PlaybackEventFull`→the S9 `playback_event_full` (forced `FEV_NOTHOST`). **Marked stubs** (safe defaults, each naming its owner): `PM_Particle`/`PM_PlaySound` (S9 / Chunk 9 sound), `PM_GetModelType`/`Bounds` + `PM_TraceTexture`/`PM_TraceSurface` (Chunk 7 — model handles / group-c miptex), `PM_HullForBsp`/`PM_HullPointContents` (opaque-hull round-trip, off the `pm_shared PM_PlayerMove` path), `COM_*`/`memfgets` (Chunk 7 material files; a null load degrades a real `pfnPM_Init` to default texture types). Test drives the full round-trip (install → bridge → context-free `PM_PointContents` → water). Gate: abi-watchdog + reviewer. **P3 complete — the pmove callback surface the game DLL binds against is up.** **P4a** ✅ *(2026-07-05, `physics/run_cmd.cpp`)* — `SV_RunCmd` core (`sv_pmove.c:887-1014`): the speed-hack clock (dormant until the S9 `SV_CheckCmdTimes` arms `cl.ignorecmdtime`), the `msec>50` split-recurse (second half impulse-zeroed), then the chain `pfnCmdStart → PM_CheckMovingGround → viewangle latch → pfnPlayerPreThink → SV_PlayerRunThink → SetupPMove → pfnPM_Move → FinishPMove → touch dispatch (deltavelocity → PM_ConvertTrace → SV_Impact) → pfnPlayerPostThink → pfnCmdEnd`. Composes the sv_phys helpers `SV_UpdateBaseVelocity` + `SV_Impact` (moved out of `physics.cpp`'s anonymous namespace and exposed via `physics.hpp`); `SV_PlayerRunThink`/`PM_CheckMovingGround`/`PM_ConvertTrace` are ported locally. Placed in a dedicated pmove-bridge TU (raw `edict->v.` access sanctioned, Q-20). Added the `cl.ignorecmdtime`/`cmdtime`/warn fields to `ServerClient`. **NB**: the legacy `state <= cs_zombie` guard is spelled out semantically (our `ClientState` enum orders `Zombie` after `Spawned`). Tested directly against the fake DLL (new `pfnPM_Move`/`pfnCmdStart`/etc. probes): chain order + seed passthrough + FinishPMove copyback, `msec>50` split, touch dispatch (velocity save/restore), zombie skip. Gates: reviewer + parity spot-check. **P4b** ✅ *(2026-07-05)* — wired `sv_run_cmd` into its two call paths. **(1)** `pfnRunPlayerMove` (the fakeclient/bot mover, `sv_game.c:3823`, `engine_table.cpp`): resolve client → reject non-fakeclients → synthesize timebase + `usercmd_t` → seed → `sv_run_cmd` → `lastcmd`. Needs the full runtime, so `EngineBridge` gained a null-guarded `ServerRuntime *runtime` back-pointer (wired in `load_progs`; only full-orchestration slots use it). `sv.current_client` save/restore skipped (untracked; PM callbacks resolve their edict directly). **(2)** `SV_ParseClientMove` (real-client path, `sv_client.c:3305`, `client_state.cpp`): the freeze/pause zeroing + viewangle latch + `SV_EstablishTimeBase` + the fresh-cmd `sv_run_cmd` loop (seed = netchan `incoming_sequence − i`) now run; file-local `establish_timebase`/`player_is_frozen` helpers added. EntityView-scope, so it uses `EntityView` (new `set_v_angle`) — no raw `->v.`. `CL_IsInGame()` is true on dedicated, so the pause gate reduces to `paused || SV_PlayerIsFrozen` (listen-server = OQ-4 hook). Vendored `k_fl_frozen` (`FL_FROZEN`=1<<12). **Deferred (marked)**: the `net_drop` dropped-packet replay (`XASH3DPP-STUB(S8-seam)` — `net_drop = netchan.dropped − (numcmds−1)` needs the netchan.dropped mirror, so `net_drop` is pinned to 0 = the no-loss path, incl. the `numcmds==0` lastcmd-replay edge); the studio animtime clamp (`XASH3DPP-STUB(chunk7)`, needs the model cache). Tests: `test_run_player_move_fakeclient` (installed `pfnRunPlayerMove` → chain + timebase synthesis + copyback) + `test_run_player_move_rejects_real_client`; the `test_net_io` clc_move path exercises the real-client parse seam. Gates: reviewer SHIP; legacy-parity-auditor PARITY-CONFIRMED 22/22 (incl. a bit-identical proof of the ping-adjust double→float narrowing). **P4 complete — SV_RunCmd drives both the bot and real-client paths; the pmove-bridge frame-loop side is closed.** **P5 (defer)** — lag-comp (`SV_SetupMoveInterpolant`/`Restore`, §4), stubbed for the milestone; pmove *parity* ticks at S15/Chunk 11. Current pmove stubs: `client_state.cpp:604`, `game_host.cpp:205/344`, `engine_table.cpp:1577`.

______________________________________________________________________

### Chunk 6B — hardening retrofit (Q-22 lifecycle + QN annotation discipline) ✅ DONE *(2026-07-06)*

**Subsystems**: `server`, `networking`, `map_loader`, `host`, `abi`, `cmd_cvar`, `filesystem`, `core`, `platform`, `memory`, `utilities`, `launcher`\
**Depends on**: the north-star v2 standard (Q-21/Q-22/QN/QO — registers, instructions, 8-check audits, annotation-coverage tooling, finish_check item 10, prompt wave) and the D-1 acyclic layer baseline (`design/layer-model.md`)\
**Recon**: none — a conformance retrofit over completed subsystems; the updated audit prompts ARE the recon\
**Scope fence**:

> 6B is **structural/style conformance ONLY**: lifecycle promotion, pool
> routing, annotation backfill, thread asserts, narrowest-state signatures.
> It does **NOT** implement any `XASH3DPP-STUB` backlog item (the 151-marker
> OQ-8/S8-seam/chunk-N inventory stays chunk-inherited). Parity-gated
> subsystems (networking wire format, map_loader Q-18 goldens, server
> S13-blessed behaviour) are behaviour-preserving: no observable-output
> change; targeted gates re-run after every structural refactor.

**Per-session structure** (WORKFLOW.md "Chunk 6B retrofit-wave variant"): audit (detail-audit 8 checks + `compliance_scan` all-sets + `--checks annotation-coverage`) → fix (implement-audit / sweep-module) → SoC lens (PUBLIC/PRIVATE link split minimal; no foreign types leaking into public headers) → doc sync (boundary spec + `architecture/<module>/` where structure changed) → gates (build+test; x86 too for S6/S8/S9; compliance clean; reviewer; parity: S6 netchan/delta spot-audit, S7 Q-18 goldens re-run, S9 full suite both arches + the hl.dll smoke **actually RUN** with `XASH_HL_ROOT` + parity-auditor spot-checks on promoted areas). Every `candidate-*` finding adjudicated as exactly one of **fix · compliance-allow(reason) · false-positive (fix the rule + snippet test) · deferred-with-owner(tag)** — no silent drops.\
**Wave riders**: per-session CMake `cxx_std_20`→`cxx_std_23` normalization (launcher included); when-touched absorptions — S4 `make_os_file`→`create_os_file` + the Q-4 `FilesystemInitParams` check, S5 the tracked ~115-warning NS_QUALIFY sweep + Q-3 raw `Impl*` conformance, S3 the `cmd_cvar ⇄ core` include-inversion adjudication (layer-model census), S9 the `Info_*` consolidation into utilities + the P-5 narrowest-state sweep + promotion of invariant-bearing sub-aggregates (ClientMachinery, SnapshotState, LightStyles, precache; arenas stay raw + annotated; orchestrators stay free functions) with address stability preserved (copy/move deleted per QJ) and crosswalk-recorded renames.\
**Pre-wave prerequisites** *(D0, 2026-07-06)*: ✅ `ThreadRole::Main` registration wired at the launcher entry (was test-only — a debug production run would have aborted at the first asserted entry); registration is same-role idempotent (plain assignment). ✅ detail-audit mechanical dry-run on `utilities` (see S1).\
**Spin-off inventory** *(D0 — production `xash3dpp/` references to repo-root files)*: `xash3dpp_miniz` from `../public/miniz.c` + INTERFACE include `../public` → **vendored-needed** (moves under `xash3dpp/3rdparty/` when it is populated); `tests/server` include dirs `${PROJECT_SOURCE_DIR}/../common` (×3) + the sealed-namespace legacy-header layout pins → **test-only parity reference** (sanctioned); `vfs009.cpp:18` commented-out legacy include → inert, tracked. No other production root-tree dependencies — **no spinoff blockers**.\
**Session ladder** (one green commit per step; S2∥S3, S4∥S5, S6∥S7 ran as worktree-isolated parallel agents per the WORKFLOW multi-agent policy; S1 first — it set the exemplars; S8/S9 serial last): S1 utilities+memory ✅ → S2 platform ✅ → S3 core ✅ → S4 filesystem ✅ → S5 cmd_cvar ✅ → S6 networking ✅ → S7 map_loader ✅ → S8 host+abi+launcher ✅ → S9 server ✅ *(split S9a conformance + S9b promotion-adjudication)*\
**Deliverable** — **MET 2026-07-06**: every subsystem `compliance_scan` all-sets clean (0 violations, adjudicated allows) + annotation coverage 100% every axis, all 12 subsystems; `finish_check` 0-fail everywhere (residual needs-judgment = item 2 QO literal-classification recorded in each boundary spec + item 8 tests-run-by-orchestrator); build+test **x64 AND x86** 89/89; the real 32-bit `hl.dll` smoke **RUN** (c0a0 spawn + frame, not SKIP); `whereami` routes 6 → 6B → 7.

**Completion notes** *(2026-07-06)*:

- **Adjudication outcomes**: filesystem is thread-safe-**by-lock** (`std::shared_mutex`) so its mutators `compliance-allow` the thread-assert; `cmd_cvar`/`core`/`host`/`map_loader`/`server` are main-thread state → real `assert_thread_role(Main)` (+ `ThreadRole::Main` registered in the affected test mains); `networking` is `T_NetIO`-confined single-thread-by-contract → all 55 transport asserts `compliance-allow` (NetIO thread not yet split, G-2). `abi` vendored structs `@annotation-exempt: abi-pod` (comment-only, layout untouched — abi-watchdog CLEAR). `g_bridge` = `engine_table` ABI-slot carve-out (Q-20, context-free pfn shims → file-scope singleton).
- **Wave-rider outcomes**: `make_os_file`→`create_os_file` (S4); the ~106-ref cmd_cvar NS_QUALIFY sweep (S5); the `cmd_cvar⇄core` inversion = already forward-declared, no inversion (S3); `cxx_std_23` normalization (all targets incl. launcher). Q-3 cmd_cvar pimpl = already-conformant (pool_new'd `Impl`; default-deleter `unique_ptr` would be heap corruption — correctly not converted).
- **DEFERRED-with-owner** (refinements, not conformance blockers): **`FilesystemInitParams` Q-4** (owner *post-6B-fs-ergonomics*; 24 fs test-site ripple); **`Info_*` consolidation** `info_string`→utilities (owner *post-6B*, stays server-scoped); **promotions** `SnapshotState` + `ClientMachinery` consolidation (owner *post-6B-server-promotions*; high-churn on the parity-gated snapshot/delta pipeline — `LightStyles` + `PrecacheTables` were **already** classes from Chunk 6, annotated in place); **P-5 narrowest-state sweep** (owner *post-6B*); **`create_master_list_client`** pool migration (owner *Q-22 memory-integration*); the `fat_vis` query-stack `.reserve()` (owner *post-6B*, Q-18-gated). Chunk-inherited `XASH3DPP-STUB` backlog untouched per the scope fence.
- **Tooling hardened** (each with snippet tests): pool-owned `unique_ptr` suppression; `g_*` definition-allow propagation + array/raw-pointer def-shapes; the lifetime rule's statement-keyword + `::`-qualified-member gaps; the `nodiscard`/`prereserve`/`stats-accessor` false-positives; `finish_check` abi test-dir alias + `NODISCARD` allow-honoring; the `compliance_scan` coverage-CLI crash.

______________________________________________________________________

### Chunk 7 — content pipeline (model & image loaders) — ◑ IN PROGRESS (studio bone solver ✅ 2026-07-06)

**Subsystems**: `content` (+ `utilities`/`map_loader`/`server` studio wiring)\
**Depends on**: filesystem, utilities, memory *(all done)*\
**Recon/Boundary**: `docs/boundaries/content-boundary.md` + `docs/modernization-opportunities/content-modernization.md` (OQ-1/2/3/4/6/8 decided; OQ-5 bone-math ✅ done; OQ-7 residue open)\
**Legacy reference**: `engine/common/imagelib/` (BMP, DDS, TGA, KTX2, WAD), `mod_studio.c`, `mod_alias.c`, `mod_sprite.c`, `public/xash3d_mathlib.c`/`matrixlib.c` (bone math)\
**ABI surfaces touched**: `common/com_model.h` / `engine/studio.h` structs read by DLL — layout frozen (read by hardcoded offset, never vendored).\
**Deliverable**: Typed model handle lookup; WAD texture pack/unpack test; studio header parse test — **all done**, plus the full studio bone solver.\
**What shipped**: `ModelCache` handle registry + 3 non-brush loaders + magic dispatch + CRC (OQ-6) + purge FSM (OQ-8) + `IModelPostProcess` (OQ-4); 7 image codecs (WAD/TGA/BMP/DDS/KTX2/MIP/PNG) + palette machinery; the **studio bone solver (OQ-5)** — `utilities` quaternion/matrix primitives (bit-exact vs the Q-18 verbatim-legacy goldens), `content` merged RLE `calc_bones` + `setup_bones` driver + `IBoneSolver` seam, the bone-position / attachment pose queries + studio hitbox hull-planes; the content pipeline wired into the server (lazy `ModelCache` in `ModelResolver`, `IModelResolver::studio_bytes`), the three studio game-DLL pfns unstubbed (`pfnGetModelPtr`/`GetBonePosition`/`GetAttachment`), and `map_loader BoxHull::set_planes` (the oriented-box hull for hitboxes).\
**Deferred (recorded)**:

- **Studio server hitbox trace-loop** — the geometric core is done (`content::studio_hitbox_hulls` + `BoxHull::set_planes`); the `SV_ClipMoveToEntity` per-hitbox loop + `EntityView` pose accessors + `SV_HullForStudioModel` gating (trace-size scaling, `sv_clienttrace`, player-blend, CS shield-skip) + the `pm_trace.cpp` mirror + the 16-entry LRU cache are the finishing step, **gated on the hl.dll smoke** (their parity can't be verified without verbatim-legacy trace goldens). Closes server-boundary OQ-2. Markers: `clip.cpp:180`, `pmove.cpp:159`.
- **Renderer (Chunk 13)**: internal `Image`↔`rgbdata_t` adapter (OQ-1); `Image_Process` resample/flip/quantise (NeuQuant); MDL/SPR/LMP/FNT/PAL **image-lump** codecs; the `content → imagelib` + `content → map_loader` CMake links; lightmaps/glpolys; texture upload.
- **OQ-7 residue**: `XASH_LOW_MEMORY` texel truncation, external `…T.mdl` texture merge, dedicated-server sprite half-load, the Quake sprite pitch-inversion bug, and **external seqgroups** (`seqgroup>0` `…NN.mdl` — the bone solver degrades these to bind pose).
- **Host feature-flag wiring**: `host.features` (`ENGINE_COMPENSATE_QUAKE_BUG` / `ENGINE_COMPUTE_STUDIO_LERP`) is unwired (== 0); the studio pfns/hull consume it via the legacy defaults (flip on, attachment angles untouched) until it lands (already deferred in `physics.cpp:1542`).
- **Studio ABI layout tripwire** (abi-watchdog follow-up): a `tests/…/test_studio_layout.cpp` pinning the `studiohdr_t` / `mstudio*` offsets + strides against a sealed `#include <engine/studio.h>` with `offsetof`/`sizeof` `static_assert`s — matching `test_edict_layout` / `test_eiface_layout` / `test_pmove_layout`. The offsets are hand-transcribed and **verified correct** (abi-watchdog hand-checked every one) but lack a compile-time drift guard; the test needs the repo-root include path the existing layout tests use.
- **utilities `AngleVectors`/`VectorAngles` float-trig** (pre-existing, not a bone-solver defect): `angle_vectors` / `vec_to_yaw` / `vector_angles` (`matrix.cpp`) use single-precision `sinf`/`atan2f`, diverging from the legacy double `SinCos`. NOT on the studio-bone path (studio bones route through the fixed double `sincos`); tolerance-tested. A utilities parity tick if a server pose path ever routes angles through them.

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

- **Extension posture (Q-21)** — the long-term experimental goals (in-engine
  MCP service, multithreading-suitable game ABI v2 + HL SDK rework, dedicated
  debug thread, expanded in-game debugging) are catalogued in
  `design/extension-goals.md`; its door rules bind new code via Q-21 and are
  hooked into the `analyse-subsystem` / `analyse-modernization` prompts.
  Feature work itself is **unscheduled** — promotion to a chunk requires its
  own design brief; see the doc's per-chunk hook table (worker-pool inbox at
  Chunk 7, per-body `PhysicsContext` at Chunk 11, thread-model decision
  before Chunk 12, `RenderFrame` as the snapshot reference at Chunk 13).

______________________________________________________________________

## Chunk 6 (server) — deferred stub inventory *(S10 feature-complete gate, 2026-07-05)*

The server is structurally **feature-complete for the dedicated milestone**: it
loads the game DLL, spawns + activates a level, runs the fixed-step frame loop
(sv_phys movetypes/pushers + the pmove bridge P1–P4), and drives clients over a
bidirectional netchan loop. The remaining behaviour is **milestone-trimmed under
OQ-8** — every trim carries an inline `XASH3DPP-STUB(<tag>)` / `TODO(<tag>)`
marker naming its owner. **Live source of truth: `stub_scan server`** (149
markers at this gate; `--delta` for per-commit churn). The buckets below group
them by *what unblocks them* so each future chunk picks up its inheritance:

- **Server S9 completion — client/messaging + the S8↔S9 frame-splice (~90, tags
  `chunk6-S9` / `S8-seam` / generic `chunk6`)** — the largest bucket, all
  landed-but-deferred seams: the multicast/sound message pipeline
  (`pfnEmitSound`/`pfnEmitAmbientSound`/`pfnParticleEffect`/`SV_BuildSoundMsg`),
  per-client messages (`pfnClientPrintf`/`pfnSetView`/`pfnCrosshairAngle`/
  `pfnFadeClientVolume`), the `net_encode` delta tables
  (`pfnDeltaSet/UnsetField*`, `pfnRegisterEncoders`), voice matrices, player
  stats/auth-ids/physinfo, the `net_drop` dropped-packet replay + command
  checksum + `SV_CalcClientTime` unlag (`client_state.cpp`), the recipient
  PVS/PHS visibility cull (`messages.cpp`, shared with `SV_Multicast`), the
  water-splash sounds + physFuncs override hooks + `SV_CheckCmdTimes` speed-hack
  clock (`physics.cpp`), and the `spawn.cpp` S9 resource/log/datagram setup.
  Not milestone-blocking against the fake DLL; completes the real-client surface.
- **Chunk 7 — content pipeline (~12, tags `chunk7` / `chunk7/OQ-2` / generic)** —
  everything gated on the model cache + miptex: studio extradata
  (`pfnGetModelPtr`/`GetBonePosition`/`GetAttachment`/`pfnModelFrames`), the
  group-(c) surface/texture trace (`pfnTraceTexture` + PM_TraceTexture/Surface),
  studio hitbox hulls (`clip.cpp`/`pmove.cpp`, OQ-2), the `SV_ModelHandle`
  studio animtime clamp (`client_state.cpp`), and `LUMP_LIGHTING`
  (`light.cpp` `SV_LightForEntity`).
- **Chunk 8 — save/restore (~6, tags `chunk8` / `chunk6-S8`)** — the executor
  stubs `Server::exec_load_game`/`exec_change_level`, the save symbol↔ordinal
  table (`pfnFunctionFromName`/`NameForFunction`), and the
  `physFuncs.SV_LoadEntities`/`SV_CreateEntity` override hooks.
- **Chunk 9 — sound (~4, tag `chunk9` + sound-adjacent `chunk6`)** —
  `PM_PlaySound`→`SV_StartSound`, the sentence-sequence files
  (`pfnSequenceGet`/`PickSentence`), and `pfnGetApproxWavePlayLen`.
- **Chunk 12 — client / listen-server (2, tag `chunk12`)** —
  `CL_DisableVisibility()` fold into fullvis (needs the client-state hook).
- **Cross-cutting engine-integration residue (~15, mostly `chunk6-S7`/generic)** —
  the engine cvar-registry unification (`pfnCVarGetPointer`/`GetFloat`
  fall-through), the `Cbuf`/command-context surface (`pfnServerCommand`/
  `ServerExecute`/`CmdArgs/Argv/Argc`/`AddServerCommand`), the filesystem file
  ops (`pfnLoadFileForMe`/`FreeFile`/`CompareFileTime`/`GetFileSize`), the
  `sv_move.c` locomotion family (`SV_MoveToOrigin`/`CheckBottom`/`WalkMove`/
  `MoveToss` — explicitly deferred at S8), the `COM_RandomLong`/`Float` idtech-RNG
  parity port (two xorshift stubs to unify — `engine_table.cpp` + `init_client_move.cpp`),
  `SV_PortalCSG` trace elongation, and the OQ-7 HLMODS compat nudge
  (routes to a future server `ICompatPolicy`, Q-12).
- **P5 pmove lag-compensation** *(deferred by plan)* — `SV_SetupMoveInterpolant`/
  `SV_RestoreMoveInterpolant` are no-ops in `run_cmd.cpp`; pmove *parity* ticks
  at S15 / Chunk 11 (needs a real `PM_Move`).
- **Rotated-brush matrix Q-18** *(S13 parity finding, 2026-07-05)* — the
  `xash::utilities` matrix helpers are pure-float reformulations of legacy's
  double-intermediate `Matrix4x4_*` routines, so every ROTATED SOLID_BSP/PORTAL
  interaction (physics `push_rotate`, world trace/contents, brush-trigger touch)
  is ULP-inexact: (a) `from_angles` float trig vs `CreateFromEntity` double
  `M_PI2/360`+SinCos; (b) `invert_ortho`+`transform_point` = `(v·R−t·R)` vs
  `VectorITransform` `(v−t)·R`; (c) `world_transform_aabb` plain-transpose vs
  `Invert_Simple` ×`1/(row0·row0)`; (d) `transform_positive_plane` omits the
  `sqrt(row0·row0)` scale. Marked `TODO(Q-18)` at `world/clip.cpp`. **No
  map_loader ripple** (server-only callers). Axial/non-rotated paths are
  ULP-clean. Deferred to a focused slice paired with the Q-18 golden-vector
  generator; rotated-geometry parity ticks at S15/Chunk 11.
- **S13 lower-priority parity items** *(PLAUSIBLE, tracked)* — `read_packets`
  omits the netchan `qport` match (NAT co-op edge, needs a qport mirror);
  `send_client_datagram` uses a `<=`-bits headroom test vs legacy's strict
  `<`-bytes; `MSG_SPEC` routes to the reliable stage vs `sv.spec_datagram`
  (inside the HLTV trim); `LightStyles::set` computes `length` from the
  truncated (not source) pattern; per-spawn `Delta_Init` is skipped (matters
  only if a game registers a custom delta encoder — needs a net_encode
  cross-check); `sv_cheats` READ_ONLY unlock omitted (inert until the MP lock
  lands). The `setup_clients` `FCVAR_CHANGED` clear needs a cmd_cvar accessor
  (marked in `spawn.cpp`).

None of these block the S15 dedicated-server smoke test against a real
`hl.dll`; they are the post-milestone completion backlog, inherited by the
chunk named in each bucket.

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
- **Session 2 of the hardening pass** — ✅ completed 2026-07-06 by the
  workflow adapter/config parity commits: Claude/opencode adapters are thin,
  Copilot MCP/tool wiring is clean, twin root `AGENTS.md`/`CLAUDE.md`
  SYNC-CORE blocks are enforced, `.github/AGENT-SETUP.md` and WORKFLOW.md
  were upgraded, and `tools/workflow_sync.py` full stage exits 0.
- **`xash3dpp_script` satellite** *(reserved 2026-07-06, decided-not-built —
  the xash3dpp_http precedent)* — the G-5 scripting runtime target; built
  only after the spike in `design/scripting-runtime-brief.md` picks the
  runtime (verified shortlist: Lua 5.4-as-C / QuickJS-ng / AngelScript).
  Companion build work when it lands: the `xash3dpp_allow_exceptions(target)`
  flag-REPLACE helper (also the tidy for the pre-existing D9025
  default-`/EHsc` noise) and the first `xash3dpp/3rdparty/` vendoring.
- **`TODO(net-base)` transport-types layer** *(deferred 2026-07-06, D-1)* —
  `platform/os_socket.hpp` re-exports networking types (layer inversion; see
  `design/layer-model.md` §2). Trigger: the first satellite needing sockets
  without networking (likely the G-5 spike or the G-1 MCP transport).
