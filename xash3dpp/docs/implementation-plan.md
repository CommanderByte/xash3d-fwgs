# xash3dpp — Implementation Status and Work Plan

*Generated: 2026-05-15 — Updated: 2026-07-20*

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
| utilities | 10 | ✓ | ✓ | **Complete** |
| memory | 1 | ✓ | ✓ | **Complete** |
| filesystem | 10 | ✓ | ✓ | **Complete** |
| platform | 14 | ✓ | ✓ | **Complete** (incl. os_socket + IPlatformSockets) |
| core | 4 | ✓ | ✓ | **Complete** |
| cmd_cvar | 11 | ✓ | ✓ | **Complete** |
| host | 3 | ✓ | ✓ | **Complete** (Chunk 3 ✅; EngineContext owns networking; frame pump — Clock::tick + Server::frame — wired 2026-07-19) |
| abi | 1 | ✓ | ✗ | **Partial** (`Host_Error` shim + accessor only) |
| map_loader | 10 | ✓ | ✓ | **Complete** (BSP v29/30/BSP2/30ext → immutable WorldData; PVS + trace kernel, Q-18 golden-gated; FSM loads worlds; PHS module per Q-19) |
| launcher | 1 | ✗ | ✗ | **Partial** (thin argv bootstrap, no tests) |
| networking | 24 | ✓ | ✓ | **Complete** (Layers 0–4 incl. delta encoder + satellites, wired into EngineContext; DNS/bz2 deferred) |
| server | 30 | ✓ | ✓ | **Complete** (Chunk 6 — dedicated-server milestone ACHIEVED 2026-07-05: real 32-bit `hl.dll` loads + `c0a0` spawns + one map frame runs clean; OQ-8 milestone-trimmed backlog tracked in the deferred inventory) |
| client | 0 | ✓ | ✗ | **Skeleton** (include stub exists) |
| content | 13 | ✓ | ✓ | **Complete** (model cache + 3 loaders + 7 image codecs + studio **bone solver** [OQ-5 ✅ bit-exact vs Q-18 goldens] + pose pfns + layout tripwire; the `SV_ClipMoveToEntity` studio hitbox trace-loop remains gated on the hl.dll smoke — see Chunk 7. HB-10 reconciliation 2026-07-19) |
| demo | 0 | ✗ | ✗ | **Skeleton** |
| input | 9 | ✓ | ✓ | **Complete** (Chunk 10, mock-source stop-line) |
| physics | 0 | ✗ | ✗ | **Skeleton** |
| renderer | 0 | ✗ | ✗ | **Skeleton** |
| save | 13 | ✓ | ✓ | **Complete** (Chunk 8 — save/restore ACHIEVED 2026-07-20: `.sav`/`.HL1-3` codec + save-directory + `SV_GetSaveComment` + landmark-transition machinery, wired behind `ILevelChangeExecutor`; real-`hl.dll` save→load round trip on `c0a0` witnessed; SAV-OQ-1/2/3 landed) |
| sound | 10 | ✓ | ✓ | **Complete** (Chunk 9, null/sink device stop-line) |
| ui | 0 | ✗ | ✗ | **Skeleton** |
| world | 4 | ✓ | ✓ | **Complete** (promoted out of `server` 2026-07-20 — BSP spatial queries, entity linking, SV_Move/SV_ClipMoveToEntity over the map_loader trace kernel. Written during Chunk 6 S5 with zero `ServerRuntime` coupling, so the promotion was a pure file move; Chunk 12 client prediction is the second consumer that made it load-bearing) |

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

**Subsystems**: `server`, `world` (incl. the server-side pmove bridge `sv_pmove.c`, moved in from Chunk 11 per the boundary spec's satellite table; `world` was built here as `src/server/world/` and promoted to its own target 2026-07-20)\
**Depends on**: cmd_cvar, networking (Chunk 2), map_loader (Chunk 5), host, filesystem, memory, platform\
**Recon/Boundary**: ✅ done 2026-07-04 (commit `43b07bb7`) — boundary spec `docs/boundaries/server-boundary.md`; deep dives `legacy-survey/deep-dive-server-{lifecycle,game-dll-bridge,clients,physics,world-frame,save-boundary}.md`. **Scaffold OQs decided 2026-07-04**: `server-boundary#OQ-1` → **Q-19 PHS_PLACEMENT** (PHS lands as a map_loader `phs` query module, in Chunk 6 scope) and `#OQ-5` → **Q-20 EDICT_STORE** (ABI-exact edict array as single store behind a zero-cost access seam); crosswalk in `decisions-architecture.md` §3a. **Scaffold unblocked.**\
**Legacy reference**: `engine/server/sv_main.c`, `sv_game.c` (159-slot `enginefuncs_t`), `sv_world.c`, `sv_phys.c`, `sv_pmove.c`, `sv_frame.c`, `sv_client.c`\
**Complexity note**: The `enginefuncs_t` table is 159 function pointers and `entvars_t` layout is byte-exact frozen — highest ABI risk in the entire rewrite. Getting `sv_game.c` to load and call a real HL game DLL without crashing is the integration milestone; `entvars_t` layout must be ABI-exact at the boundary even if internal entity storage differs.\
**ABI surfaces touched**: `engine/eiface.h`, `engine/edict.h` — **FROZEN Game DLL ABI**\
**Deliverable**: Dedicated server starts, loads HL `dlls/hl.dll`, runs a single map frame; server ctest green — **dedicated-server milestone**\
**Session ladder** (one green commit + checkpoint per step; slice order refined by S2 `plan-implementation` 2026-07-04 — world before bridge so the trace-family enginefuncs slots bind real code): S1 scaffold ✅ → S2 plan-implementation ✅ → S3 map_loader `phs` module (Q-19, golden-gated) ✅ → S4 edict arena + string pool + vendored ABI headers (Q-20) ✅ → S5 world interaction (areanodes/link/move/contents/lightstyles; SetAbsBox as injected seam) ✅ → S6 game-DLL bridge (enginefuncs ×159, DLL_FUNCTIONS resolution, in-tree fake-DLL test double) ✅ → S7 lifecycle (spawn/activate/changelevel seams, EngineContext wiring) ✅ → S8 frame loop + sv_phys + pmove bridge ✅ *(landed+reconciled+gated 2026-07-04; pmove bridge P1–P4 complete 2026-07-05 — SV_RunCmd drives both bot + real-client paths; P5 lag-comp + sv_move.c locomotion deferred to the S10 inventory)* → S9 clients/messaging/satellites (OQ-8 stub markers) ◑ *(landed+reconciled+gated 2026-07-04 — built in parallel with S8 in isolated worktrees; snapshot/delta pipeline + the S8↔S9 frame-loop seam substantially complete; the residual client/messaging backlog is catalogued in the S10 deferred-stub inventory)* → S10 sweep-module ✅ *(2026-07-05 — compliance clean (0 findings after the world_hooks thread-assert fix); the 149-marker deferral backlog consolidated into the "Chunk 6 — deferred stub inventory" section above)* → S11 analyse-threading (OQ-9) ✅ *(2026-07-05 — `threading-analysis/server-threading.md`; OQ-9 main-thread-only, safe-by-contract)* → S12 document-architecture ✅ *(2026-07-05 — `architecture/server/` README+index+6 concept pages)* → S13 legacy-parity audit ✅ *(2026-07-05 — 4-way parallel auditor fan-out over lifecycle/physics/world/snapshot; 9 CONFIRMED divergences, 7 fixed (`e557290b` messaging/lifecycle/water-timer), the rotated-brush matrix Q-18 + PLAUSIBLE set tracked in the deferred inventory)* → S14 finish-subsystem + pre-pr ✅ *(2026-07-05 — SHIP: finish_check 8/9 (item 2 limits = pre-adjudicated wire-frozen opcodes), compliance prepr 0/0/0/0, reviewer SHIP on the S13 batch)* → S15 hl.dll milestone smoke ✅ *(2026-07-05 — **the milestone**: the real retail 32-bit `valve/dlls/hl.dll` loads through the production `GiveFnptrsToDll` handshake, `c0a0.bsp` loads via MapLoader, the full `SV_SpawnServer → spawn_entities → SV_ActivateServer` chain + one `Host_ServerFrame` run clean with zero `Host_Error` — 10/10 asserts. Harness `tests/server/lifecycle/test_hl_smoke.cpp`, driven by the MapLoader FSM through the `ILevelChangeExecutor` seam exactly as the live engine drives `map c0a0`. Asset-gated (`XASH_HL_ROOT`) + arch-gated (x86 only; `hl.dll` is 32-bit) — a SKIP is a pass, so CI / x64 / HL-less machines stay green (suite 89/89 both arches). The x86 build/test loop is a first-class MCP `arch` axis (`b9e6c6ff`).)* — **dedicated-server milestone ACHIEVED**

**S8/S9 completion sequence** *(decided 2026-07-05, after the parallel-agent landing)*: S8/S9 landed their independent bulk but deferred the interdependent pieces. Remaining Chunk-6 order — **(1)** S9 snapshot/delta pipeline (frames ring `SV_UPDATE_BACKUP`, baselines, `AddToFullPack`/`SetupVisibility`, `entity_state_t` delta) — biggest gap, unblocks the send; **(2)** the S8↔S9 seam splice — wire `Host_ServerFrame`'s client/net steps + the ~11 marked `S8-seam` stubs + the `clc_move`/usercmd parse path; **(3)** the pmove bridge *last* (`sv_pmove.c` 1014 L + port `engine/common/pm_trace.c` 889 L + `pm_surface.c` 382 L + vendor `playermove_t`/`physent_t` from `pm_shared/pm_defs.h`). **The pmove bridge is NOT blocked on Chunk 11**: `PM_Move` is the game DLL's `NEW_DLL_FUNCTIONS` export (mods statically link `pm_shared`; repo-root `pm_shared/` is headers only), and the trace family is engine code composing over the Chunk-5 kernel. Chunk 11 owns only the client-prediction path (`cl_pmove.c`) + the both-paths determinism test; pmove *parity validation* ticks at S15/Chunk 11 (needs a real `PM_Move`). The shared `pm_trace`/`pm_surface` family likely lands in the `physics` subsystem (reused by Chunk 11's client path), not `src/server/` — settle at implementation time. **Snapshot-pipeline progress (step 1)**: 1a scaffold ✅ (`ae286fb1`) → 1b `SV_CreateBaseline` fill + instanced baselines ✅ (`aec40d29`) → 2 `SV_WriteEntitiesToClient` gather + `packet_entities` ring + `SV_EmitPacketEntities` delta-merge + per-client frames ring ✅ (`f43900e3`) → 3 `SV_WriteClientdataToMessage` + `SV_SendClientDatagram` datagram body (svc_time + clientdata/weapondata delta + entities) ✅ → 3b signon buffer (`sv.signon`, `MAX_INIT_MSG`) + the `SV_CreateBaseline` signon-write half (`svc_spawnbaseline` + per-edict baseline deltas + instanced list) ✅ → 4 `SV_EmitEvents` + `SV_EmitPings` datagram-riders — complete `SV_WriteEntitiesToClient`'s per-frame body (svc_event queue drain + packet_index resolution + args delta; svc_pings from the 2s-cached `SV_GetPlayerStats`/`SV_CalcPing` over the frame ring) ✅. **Snapshot/delta pipeline substantially complete.** Deferred to the **S8↔S9 seam splice** (each lacks its producer/destination until then): the event *producer* (`pfnPlaybackEvent`/`SV_PlaybackEventFull` fills `cl.events`), `SV_UpdateToReliableMessages`'s reliable fan-out (`sv.reliable_datagram` → per-client `netchan.message`), and the Netchan transmit + choke/rate send-gate. All wire encoding rides the existing networking `DeltaTables`. **Seam-splice progress (step 2)**: the networking foundation is decided — the host owns the single `NetworkContext` + its UDP sockets (`EngineContext` declares `networking` ahead of `server`); the server holds a non-owning `rt.net` handle and pulls its server socket each frame (the "host-routes" model — *not* a fresh register decision, it follows Q-2/Q-4 + the committed `IOobSink` seam). The messaging seam is a functional bidirectional netchan loop as of `41bc9d2f`: **A** ✅ (`d3b3f8b9`) `ServerInitParams.net` DI + `read_packets` connectionless ingress routing OOB → `handle_connectionless`, replies via a `NetworkContext`-backed `IOobSink`, wired into `host_server_frame`; **B** ✅ (`b7c4f192`) per-client `Netchan` + `Netchan_Setup` at connect / `Netchan_Clear` at drop, backed by new `NetworkContext::protocol_driver()`/`fragment_pool()` accessors; **C** ✅ (`1421fd91`) in-session demux (`Netchan_Process` + `SV_ExecuteClientMessage` clc loop + `SV_ParseClientMove` usercmd decode filling lastcmd/packet_loss/ping) — also fixed two latent bugs (connect wiping `cl.frames`; missing `usercmd_t` in the fixture `delta.lst`); **D** ✅ (`a7943b8e`) `SV_SendClientMessages` send-gate + `send_client_datagram`→`transmit_bits`→`send_packet`, wired into `host_server_frame`; **E** ✅ (`41bc9d2f`) `SV_UpdateToReliableMessages` reliable broadcast fan-out. **F** ✅ (`a2ab0011`) the event producer (`pfnPlaybackEvent`/`SV_PlaybackEventFull` fills `cl.events` — unreliable queue `FEV_UPDATE` slot-merge + `FEV_RELIABLE` `svc_event_reliable` staging into `cl.reliable` via `write_delta_event`; a new non-owning `EngineBridge::delta` carries the tables; the recipient PHS cull + groupinfo filter ride the same `S8-seam` visibility gate as `SV_Multicast`), consumed by the existing `emit_events`. **The entire S8↔S9 messaging + event seam is now complete** — a bidirectional netchan loop plus the closed event producer→consumer path. Remaining Chunk-6 work before the frame loop is fully fleshed: the **pmove bridge** (`SV_RunCmd`, ordered last). Deferred seams inside the landed slices are marked `XASH3DPP-STUB(chunk6-S9)` / `XASH3DPP-STUB(S8-seam)`: command checksum, freeze/pause + `SV_RunCmd` (pmove), `SV_CalcClientTime` unlag, `host_limitlocal`/`sv_failuretime` send-gates, `FCL_RESEND_USERINFO`/`MOVEVARS` resends, the unreliable `sv.datagram` per-client append, the recipient PHS/groupinfo cull (`SV_CheckClientVisiblity`, shared with `SV_Multicast`), and fragment reassembly.

**pmove-bridge decomposition (step 3, the last frame-loop piece)** *(planned 2026-07-05, recon from `deep-dive-server-physics.md` §4/§9 + `pm_shared/pm_defs.h` + `common/pmove.h`)*: ~2285 legacy lines (`sv_pmove.c` 1015 + `engine/common/pm_trace.c` 889 + `pm_surface.c` 382) + two frozen ABI structs. Slices: **P1** ✅ *(2026-07-05)* — vendor the pmove ABI into `include/xash3dpp/abi/pm_defs.hpp`: `pmplane_t` (16 B: `vec3 normal`+`float dist`), `pmtrace_t` (68 B: allsolid/startsolid/inopen/inwater/fraction/endpos/plane/ent/deltavelocity/hitgroup — `common/pmove.h:26-47`), `physent_t` (`pm_defs.h:37-77`), `playermove_t` (`pm_defs.h:79-215`: state block + `physents[600]`/`moveents[64]`/`visents[600]` + `usercmd_t cmd` + `pmtrace_t touchindex[600]` + `char physinfo[256]` + `movevars_t*` + `player_mins/maxs[4]` + ~30 fn-pointers). Caps: `MAX_PHYSENTS 600`/`MAX_MOVEENTS 64`/`MAX_CLIP_PLANES 5`/`MAX_PHYSINFO_STRING 256`. `movevars_t` already vendored (`pm_movevars.hpp`). Layout `static_assert`s per the `pm_movevars.hpp`/`event_state.hpp` pattern; opaque engine types (`model_s`/`hull_s`/`msurface_s`/`trace_t`) forward-declared for the fn-pointer slots (the DLL calls through them, so signatures must stay ABI-exact). **Gate: abi-watchdog CLEAR** — `tests/server/abi/test_pmove_layout.cpp` includes the real `pm_defs.h` in a sealed namespace and X-macro-pins every field offset **and** member size vs the vendored structs (both pointer widths); `model_s`/`hull_s`/`msurface_s` tag-fwd-declared, `trace_t` kept opaque (it is an anon-struct typedef with no tag). **P2** ✅ *(2026-07-05, `physics/pmove.cpp`)* — `SV_SetupPMove`/`SV_FinishPMove` (`sv_pmove.c:521-664`) + physent gather (`SV_AddLinksToPmove`/`AddLaddersToPmove`/`CopyEdictToPhysEnt` :42-322) over the S6 areanodes; the `usehull=FL_DUCKING?1:0`, multiplayer `onground=-1`, `pmove->time=timebase*1000` ms, `waterjumptime↔teleport_time` quirks (§8 #25-26). `rt.pmove` = pool-owned `playermove_t` (allocated in `load_progs`); `ServerClient.timebase` + `k_dead_dead` added; the visents-before-skip / physents-last gather ordering preserved. Lag-comp (`SV_GetTrueOrigin`) and the `pe->model`/studio-hitbox handle binding are the P5 / P3 / Chunk-7 seams (marked `pmove-P3` / `chunk7/OQ-2`). Gates: reviewer SHIP, legacy-parity-auditor PARITY-CONFIRMED (41/41). **P3a** ✅ *(2026-07-05, `physics/pm_trace.cpp`)* — the `PM_*` trace/point-contents family (`pm_trace.c` groups a/b/d): `PM_PlayerTraceExt`, `PM_TestPlayerPosition`, `PM_TraceModel`, `PM_TraceLine`/`Ex`, `PM_PointContents`/`TruePointContents`/`PointContentsPmove`, `PM_HullForBsp`, `PM_StuckTouch` — composed over the **existing edict-free map_loader kernel** (`recursive_hull_check`/`hull_point_contents`/`world_hull`/`hull_for_bsp`/`BoxHull`) and the S6 rotated-brush transforms (`world_transform_aabb`/`transform_positive_plane`), sourced from the gathered physent list (usehull-indexed player bounds) via a `PmTraceEnv`. Each physent's brush submodel is resolved through the arena + `IModelResolver` (`pe->info`→edict→`modelindex`), retiring the P2 `pe->model` binding TODO — no opaque engine handle is stashed. **Placement decision settled**: built in `src/server/physics/` (alongside `physics.cpp`/`movevars.cpp`), structured so Chunk 11's client path can extract it; no empty `physics` subsystem scaffolded. Vendored the `PM_*` trace flags (`k_pm_world_only`/`glass_ignore`/`custom_ignore`/`studio_ignore`/`traceline_*`) into `pm_defs.hpp`. **Group (c)** — the surface/texture trace family (`PM_TraceSurface`/`PM_TraceTexture`/`PM_RecursiveSurfCheck`) — is **deferred to Chunk 7** (needs `mfacebevel_t` facet bevels + miptex original buffers `WorldData` does not carry until the content pipeline; same dependency class as the studio hitbox hulls) and stubbed at the P3b fn-ptr table. Studio hitbox hulls fall back to the bbox (Chunk 7 / OQ-2); SOLID_CUSTOM is the S8 physics-interface sweep seam (no-hit for the milestone). Tests cross-check `pm_player_trace_ext` against the raw kernel primitive (oracle) + deterministic single-box physent scenarios + hand-derived hull-0 point-contents. Gates: reviewer + parity spot-check. **P3b** ✅ *(2026-07-05, `physics/init_client_move.cpp`)* — `SV_InitClientMove`: installs the ~30-entry PM_* callback table into `rt.pmove` via context-free pfn shims that reach the installed `EngineBridge` (added `playermove_t* pmove` + `const HullBoundsTable* player_bounds` to the bridge), sets `server`/`movevars`/hull-bounds table, and runs the DLL's `pfnPM_Init` (a `DLL_FUNCTIONS` export, null-guarded); wired from `load_progs` right after the pmove allocation. **Wired to real impls**: the trace family (`PM_PlayerTrace`/`Ex`, `PM_TestPlayerPosition`/`Ex`, `PM_TraceLine`/`Ex`, `PM_TraceModel` via the legacy `(pmtrace_t*)trace` pun), point contents (`PM_PointContents`/`TruePointContents`), `PM_StuckTouch`, the utilities (`Info_ValueForKey`, `Con_*`, `Sys_FloatTime`→`platform::get_time`, `RandomLong`/`Float` xorshift), and `PM_PlaybackEventFull`→the S9 `playback_event_full` (forced `FEV_NOTHOST`). **Marked stubs** (safe defaults, each naming its owner): `PM_Particle`/`PM_PlaySound` (S9 / Chunk 9 sound), `PM_GetModelType`/`Bounds` + `PM_TraceTexture`/`PM_TraceSurface` (Chunk 7 — model handles / group-c miptex), `PM_HullForBsp`/`PM_HullPointContents` (opaque-hull round-trip, off the `pm_shared PM_PlayerMove` path), `COM_*`/`memfgets` (Chunk 7 material files; a null load degrades a real `pfnPM_Init` to default texture types). Test drives the full round-trip (install → bridge → context-free `PM_PointContents` → water). Gate: abi-watchdog + reviewer. **P3 complete — the pmove callback surface the game DLL binds against is up.** **P4a** ✅ *(2026-07-05, `physics/run_cmd.cpp`)* — `SV_RunCmd` core (`sv_pmove.c:887-1014`): the speed-hack clock (dormant until the S9 `SV_CheckCmdTimes` arms `cl.ignorecmdtime`), the `msec>50` split-recurse (second half impulse-zeroed), then the chain `pfnCmdStart → PM_CheckMovingGround → viewangle latch → pfnPlayerPreThink → SV_PlayerRunThink → SetupPMove → pfnPM_Move → FinishPMove → touch dispatch (deltavelocity → PM_ConvertTrace → SV_Impact) → pfnPlayerPostThink → pfnCmdEnd`. Composes the sv_phys helpers `SV_UpdateBaseVelocity` + `SV_Impact` (moved out of `physics.cpp`'s anonymous namespace and exposed via `physics.hpp`); `SV_PlayerRunThink`/`PM_CheckMovingGround`/`PM_ConvertTrace` are ported locally. Placed in a dedicated pmove-bridge TU (raw `edict->v.` access sanctioned, Q-20). Added the `cl.ignorecmdtime`/`cmdtime`/warn fields to `ServerClient`. **NB**: the legacy `state <= cs_zombie` guard is spelled out semantically (our `ClientState` enum orders `Zombie` after `Spawned`). Tested directly against the fake DLL (new `pfnPM_Move`/`pfnCmdStart`/etc. probes): chain order + seed passthrough + FinishPMove copyback, `msec>50` split, touch dispatch (velocity save/restore), zombie skip. Gates: reviewer + parity spot-check. **P4b** ✅ *(2026-07-05)* — wired `sv_run_cmd` into its two call paths. **(1)** `pfnRunPlayerMove` (the fakeclient/bot mover, `sv_game.c:3823`, `engine_table.cpp`): resolve client → reject non-fakeclients → synthesize timebase + `usercmd_t` → seed → `sv_run_cmd` → `lastcmd`. Needs the full runtime, so `EngineBridge` gained a null-guarded `ServerRuntime *runtime` back-pointer (wired in `load_progs`; only full-orchestration slots use it). `sv.current_client` save/restore skipped (untracked; PM callbacks resolve their edict directly). **(2)** `SV_ParseClientMove` (real-client path, `sv_client.c:3305`, `client_state.cpp`): the freeze/pause zeroing + viewangle latch + `SV_EstablishTimeBase` + the fresh-cmd `sv_run_cmd` loop (seed = netchan `incoming_sequence − i`) now run; file-local `establish_timebase`/`player_is_frozen` helpers added. EntityView-scope, so it uses `EntityView` (new `set_v_angle`) — no raw `->v.`. `CL_IsInGame()` is true on dedicated, so the pause gate reduces to `paused || SV_PlayerIsFrozen` (listen-server = OQ-4 hook). Vendored `k_fl_frozen` (`FL_FROZEN`=1<<12). **Deferred (marked)**: the `net_drop` dropped-packet replay (`XASH3DPP-STUB(S8-seam)` — `net_drop = netchan.dropped − (numcmds−1)` needs the netchan.dropped mirror, so `net_drop` is pinned to 0 = the no-loss path, incl. the `numcmds==0` lastcmd-replay edge); the studio animtime clamp (`XASH3DPP-STUB(chunk7)`, needs the model cache). Tests: `test_run_player_move_fakeclient` (installed `pfnRunPlayerMove` → chain + timebase synthesis + copyback) + `test_run_player_move_rejects_real_client`; the `test_net_io` clc_move path exercises the real-client parse seam. Gates: reviewer SHIP; legacy-parity-auditor PARITY-CONFIRMED 22/22 (incl. a bit-identical proof of the ping-adjust double→float narrowing). **P4 complete — SV_RunCmd drives both the bot and real-client paths; the pmove-bridge frame-loop side is closed.** **P5 (defer)** — lag-comp (`SV_SetupMoveInterpolant`/`Restore`, §4), stubbed for the milestone; pmove *parity* ticks at S15/Chunk 11. Current pmove stubs: `client_state.cpp:604`, `game_host.cpp:205/344`, `engine_table.cpp:75`.

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

### Chunk 7 — content pipeline (model & image loaders) — ✅ DONE *(bone solver 2026-07-06; SV_ClipMoveToEntity trace-loop 2026-07-19)*

**Subsystems**: `content` (+ `utilities`/`map_loader`/`server` studio wiring)\
**Depends on**: filesystem, utilities, memory *(all done)*\
**Recon/Boundary**: `docs/boundaries/content-boundary.md` + `docs/modernization-opportunities/content-modernization.md` (OQ-1/2/3/4/6/8 decided; OQ-5 bone-math ✅ done; OQ-7 residue open)\
**Legacy reference**: `engine/common/imagelib/` (BMP, DDS, TGA, KTX2, WAD), `mod_studio.c`, `mod_alias.c`, `mod_sprite.c`, `public/xash3d_mathlib.c`/`matrixlib.c` (bone math)\
**ABI surfaces touched**: `common/com_model.h` / `engine/studio.h` structs read by DLL — layout frozen (read by hardcoded offset, never vendored).\
**Deliverable**: Typed model handle lookup; WAD texture pack/unpack test; studio header parse test — **all done**, plus the full studio bone solver.\
**What shipped**: `ModelCache` handle registry + 3 non-brush loaders + magic dispatch + CRC (OQ-6) + purge FSM (OQ-8) + `IModelPostProcess` (OQ-4); 7 image codecs (WAD/TGA/BMP/DDS/KTX2/MIP/PNG) + palette machinery; the **studio bone solver (OQ-5)** — `utilities` quaternion/matrix primitives (bit-exact vs the Q-18 verbatim-legacy goldens), `content` merged RLE `calc_bones` + `setup_bones` driver + `IBoneSolver` seam, the bone-position / attachment pose queries + studio hitbox hull-planes; the content pipeline wired into the server (lazy `ModelCache` in `ModelResolver`, `IModelResolver::studio_bytes`), the three studio game-DLL pfns unstubbed (`pfnGetModelPtr`/`GetBonePosition`/`GetAttachment`), and `map_loader BoxHull::set_planes` (the oriented-box hull for hitboxes).\
**Deferred (recorded)**:

- **Studio server hitbox trace-loop** — **DONE 2026-07-19** (the hl.dll smoke gate opened the same day): `SV_ClipMoveToEntity` per-hitbox loop (`world/clip.cpp`), `EntityView` pose accessors, `SV_HullForStudioModel` gating (`world/hulls.cpp`: size scaling, `sv_clienttrace`, `SV_StudioPlayerBlend`, FTRACE_SIMPLEBOX), the `pm_trace.cpp` mirror (trace + test-position + `PM_AllowHitBoxTrace` gating) and the legacy pooled 16-entry pose cache (`StudioHullCache` in `ModelResolver`, cleared on level change). **Closes server-boundary OQ-2.** No new float kernels (bone solve + `set_planes` + trace kernel all previously golden-verified) — verified by unit fixtures + a composition oracle (`tests/server/world/test_studio_trace.cpp`), both-arch suites, the re-run hl.dll smoke, and the parity/reviewer/watchdog gate agents. Adjudicated quirks recorded in `server-boundary.md`: shield-skip stale-slot compaction, the unconditional miss hitgroup stamp, shield state outside the cache key.
- **Renderer (Chunk 13)**: internal `Image`↔`rgbdata_t` adapter (OQ-1); `Image_Process` resample/flip/quantise (NeuQuant); MDL/SPR/LMP/FNT/PAL **image-lump** codecs; the `content → imagelib` + `content → map_loader` CMake links; lightmaps/glpolys; texture upload.
- **OQ-7 residue**: `XASH_LOW_MEMORY` texel truncation, external `…T.mdl` texture merge, dedicated-server sprite half-load, the Quake sprite pitch-inversion bug, and **external seqgroups** (`seqgroup>0` `…NN.mdl` — the bone solver degrades these to bind pose).
- **Host feature-flag wiring**: `host.features` (`ENGINE_COMPENSATE_QUAKE_BUG` / `ENGINE_COMPUTE_STUDIO_LERP`) is unwired (== 0); the studio pfns/hull consume it via the legacy defaults (flip on, attachment angles untouched) until it lands (already deferred in `physics.cpp:473`).
- **Studio ABI layout tripwire** — ~~a `tests/…/test_studio_layout.cpp` pinning the `studiohdr_t` / `mstudio*` offsets + strides~~ **DONE 2026-07-19 (consolidation audit)**: `tests/content/test_studio_layout.cpp` pins all 7 strides (`k_studio_*`), 26 `studiohdr_t` field offsets, and the bone/bbox/attachment sub-struct offsets against a sealed `#include <engine/studio.h>` with `offsetof`/`sizeof` `static_assert`s — matching the `test_edict_layout` / `test_eiface_layout` / `test_pmove_layout` pattern. Every hand-transcribed value compiled clean on first build (confirming the abi-watchdog hand-check).
- **utilities `AngleVectors`/`VectorAngles` float-trig** (pre-existing, not a bone-solver defect): `angle_vectors` / `vec_to_yaw` / `vector_angles` (`matrix.cpp`) use single-precision `sinf`/`atan2f`, diverging from the legacy double `SinCos`. NOT on the studio-bone path (studio bones route through the fixed double `sincos`); tolerance-tested. A utilities parity tick if a server pose path ever routes angles through them.

______________________________________________________________________

### Chunk 8 — save/restore ✅ DONE *(save/load/changelevel live 2026-07-20; real-hl.dll round-trip witness)*

**Subsystems**: `save`\
**Depends on**: server (Chunk 6), filesystem, map_loader\
**Recon/Boundary**: `boundaries/save-boundary.md` (refreshed 2026-07-20, as-built) + `legacy-survey/deep-dive-server-save-boundary.md` (format-internals recon, format tag 0x71 append). Server keeps the `SV_ChangeLevel` orchestration + `pSaveData`/`svc_restore` seams behind `ILevelChangeExecutor`; Chunk 8 owns serialization + the four save primitives.\
**Legacy reference**: `engine/server/sv_save.c` (format tag 0x71, ~2 500 lines)\
**Complexity note**: Binary format assumes exact `entvars_t` field ordering — any field added to server edict layout must be mirrored here; the field-map serialiser is fiddly but well-bounded.\
**ABI surfaces touched**: none frozen externally, but save files from the legacy engine must remain loadable.\
**Deliverable**: **achieved** — round-trip save/load of a minimal game state, reject-gracefully on version mismatch/corrupt input, verified end-to-end against a REAL retail 32-bit `hl.dll` (save→load on `c0a0`, `tests/server/lifecycle/test_hl_smoke.cpp`).\
**What shipped**: the four primitives (`SaveGameState`/`LoadGameState`/`LoadAdjacentEnts`/`ClearSaveDir`) + `SV_GetSaveComment` landed behind `ILevelChangeExecutor` (`server/lifecycle/save_bridge.cpp`'s `exec_load_game`/`exec_change_level`) — the `.sav` container + `.HL1` (level state) + `.HL2` (client state) + `.HL3` (entity patch) codec, the token-table/field-record framing (`IFieldSink`, SAV-OQ-2), the `.HLX` extension door (SAV-OQ-1, skip-logic only, no producer), save-directory management (aging, comments, latest-save lookup), the landmark-transition machinery (`LoadAdjacentEnts`/`CreateEntityTransitionList`, global-entity merge, `EntityInSolid` pruning), and the `FIELD_FUNCTION` symbol↔ordinal glue over the platform reverse-lookup primitive (SAV-OQ-3). 13 session slices (S8.1-S8.8); 8 legacy-parity gate cycles caught 12 divergences + 1 blocker pre-commit (ledger `docs/audits/2026-07-chunk8-10-campaign.md`).\
**Deferred (recorded, tagged `chunk11`/`chunk12`)**: the physint veto/override hooks (`SV_AllowSaveGame`, `pfnCreateEntitiesInRestoreList` — no physics-interface negotiation exists before Chunk 11); the `pfnSaveGlobalState`/`pfnRestoreGlobalState` global-state blob producer (container_codec.hpp's `ISaveGlobalState` seam exists, wired to `nullptr` — no game DLL exports it here, Chunk 11); the `saveshot` client-side preview-image hook (`Cbuf_AddTextf("saveshot ...")`, client-renderer-owned, Chunk 12); `SV_InactivateClients`/`SV_FinalMessage` changelevel teardown steps (client-machinery, S9/Chunk 12); the global-merge repoint target (`SV_FindGlobalEntity`'s global-entity registry, Chunk 12); and the dead-player/intermission save-veto gate (`IsValidSave`'s live-player dead/health screening, client-connection-owned, Chunk 12).

______________________________________________________________________

### Chunk 9 — sound ✅ DONE *(full ladder S9.0-S9.8, 2026-07-19..20; byte-identical cross-arch PCM witness)*

**Subsystems**: `sound` (+ `core` queue family, + `platform` thread primitives)\
**Depends on**: filesystem, memory, platform *(all done)*\
**Recon/Boundary**: `boundaries/sound-boundary.md` (as-built 2026-07-20: topology, assert map, adjudicated deviations) + `legacy-survey/deep-dive-sound.md`; thread design in `design/thread-spawn-and-inbox-brief.md` (HB-4 discharged, Q-24).\
**Legacy reference**: `engine/client/sound/s_main.c`, `s_load.c`, `s_mix.c`, `s_dsp.c`, `s_vox.c`, `soundlib/`\
**ABI surfaces touched**: `sound_api.h` vendored layout-pinned (`abi/sound_api.hpp`); no frozen surface modified.\
**Deliverable**: **achieved and exceeded** — `xash3dpp_sound` builds both arches; the S9.8 witness drives a console-scripted sequence through channel alloc, VOX, DSP, the S9.7b thread topology, and `SinkDevice` with byte-identical PCM across runs AND arches.\
**What shipped**: S9.0 `platform::spawn_thread`/`JoinHandle` (first thread primitives in the tree); S9.1 seams + `Sound` Q-22 class + pull-shaped `IAudioDevice` (`NullDevice`/`SinkDevice`) + SND-OQ-1 providers; S9.2 `IAudioCodec` + wav (cue loops); S9.3 the 12-instantiation mix-kernel template + bit-exact vectors + `compute_channel_pitch` float-chain; S9.4 VOX (legacy tests ported verbatim, lazy word resolution); S9.5 room DSP (both preset tables, SND-OQ-6 sentinel row, the dual-coercion `dsp_coeff_table` float); S9.6 entry surface (registry, channel alloc/steal rules, spatialize, stop paths, 25 cvars + 14 commands via the B5 context overload, `channels_snapshot()` P-4); S9.7a `core::MpscQueue`/`SpscRing` (adversarially reviewed, 64-bit counters); S9.7b the thread topology (MPSC command stream → `xash-audio-decoder` → int16 SPSC ring → `AudioCallback`; SND-OQ-2 resolved, SND-OQ-3 amended; four-dimension gate, ledger #40); S9.8 the determinism witness (CONC-6 resolved: `internal_pump` derived from `IAudioDevice::drives_own_callback()`). Gate record: ledger #35-#41 in `docs/audits/2026-07-chunk8-10-campaign.md`.\
**Deferred (recorded, tagged)**: music streaming / `IAudioStream` vend (SND-OQ-4, fenced); soundfade curve + `MixGateSnapshot` producer + `pitch_mult` (chunk12); ambient channels + `S_ClearBuffer` (chunk12); sfx handle-0 `*default` reservation (Chunk 12 precache wiring); mp3/ogg decoders; SDL audio device (Chunk 12/13 per Q-23); voice chat (fenced).

______________________________________________________________________

### Chunk 10 — input ✅ DONE *(implementation landed 2026-07-19 `8720af2b`; boundary spec reconciled to as-built 2026-07-20 by the close-out audit — the earlier "doc closure" claim was premature and is corrected here)*

**Subsystems**: `input`\
**Depends on**: platform *(done)*\
**Recon/Boundary**: `boundaries/input-boundary.md` + `legacy-survey/deep-dive-input.md`.\
**Legacy reference**: `engine/client/input/input.c`, `in_keys.c`, `in_joy.c`, `in_touch.c`\
**ABI surfaces touched**: key codes pinned to `keydefs.h` values by static_assert (config-format + ABI load-bearing); no frozen surface modified.\
**Deliverable**: **achieved** — `xash3dpp_input` builds both arches behind the ratified mock-source stop-line (Q-23: `IEventSource` + `IWindowControls` null-backed, no window); scripted event sequences drive key state, bindings, key_dest routing, joy/gyro processing, and move assembly to usercmd goldens.\
**What shipped** (worktree lane S10.1-S10.6, merged + double-gated): `InputEvent` trivially-copyable variant + `Key` strong typedef with `keydefs.h` static_assert pins; `IEventSource` (+ polled `pointer_delta`) split from `IWindowControls` (full legacy null-seam surface incl. Joy*/Vibrate/PreCreateMove); `MockEventSource`; keys[265] + Key_Event + key_dest; bindings + commands via the B5 context overload (`FCMD_PRIVILEGED` parity, `bindings_snapshot()`); joy deadzone/trigger/gyro (`joy_tunables` split); `IN_EngineAppendMove` assembly; touch/OSK event model (drawing stays Chunk 12/13); 6 test executables. Post-merge parity gate fixed 6 divergences pre-commit (ledger #34).\
**Deferred (recorded, tagged `chunk12`)**: SDL event pump + window binding (grab/warp/cursor/text-input reach a real window at the window chunk per Q-23); touch/OSK rendering; `makehelp` body.
______________________________________________________________________

### Chunk 11 — physics (pm_shared)

**Subsystems**: `physics`\
**Depends on**: map_loader (Chunk 5), server (Chunk 6 — edict query interface)\
**Recon/Boundary**: partial — `design/pm-determinism-decision.md` (Q-18) + `legacy-survey/deep-dive-trace-pvs.md`; full boundary spec needed at chunk start. Note: the server-side pmove bridge (`sv_pmove.c`) moved to Chunk 6 per `server-boundary.md` — this chunk covers the client-prediction path and the both-paths determinism test.\
**Legacy reference**: `pm_shared/pm_move.c`, `pm_shared/pm_trace.c`, `engine/client/dll_int/cl_pmove.c`\
**Complexity note**: Client prediction and server authority must produce bit-identical results — determinism is the hardest constraint; float/fixed choice from Chunk 5 is locked in here.\
**ABI surfaces touched**: `pm_shared/` — **FROZEN** (shared client ↔ server)\
**Deliverable**: `PM_Move` runs identically on both paths; determinism regression test passes

**Entry gate** *(modernization audit 2026-07-20)*:

1. **Write the boundary spec.** This entry already says "full boundary spec
   needed at chunk start" and none exists — `physics` is one of six 0-TU
   skeletons with no spec, which is why `q21_scan`'s 16/16 is silence rather
   than health.
2. **Correct the HB-2 fence anchor first.** `decisions-architecture.md:946-954`
   names `clip.cpp:219` for the rotated-brush ULP kernel; the real anchor is
   the `if (rotated)` block at **`clip.cpp:245-281`** (marked `TODO(Q-18)`
   in-code). Chunk 11's parity work runs directly through it, so fix the anchor
   before, not after.
3. **Reuse one shared RNG instance — do not copy the placeholder a third
   time.** Legacy wires the *literal same* `COM_RandomLong`/`COM_RandomFloat`
   pointer into both the engfuncs slot and the pmove slot on both paths,
   drawing from one process-wide generator; there is **no such thing as
   "independent streams"** in GoldSrc. The tree currently has **three**
   separately-seeded, algorithmically-different stand-ins (HB-12's own
   inventory lists only two — the third is in `sound/dsp.cpp`).
4. **Sever the pmove trace layer's one edict-store reach — mechanical, not a
   decision.** *(Corrected 2026-07-20 after a per-TU read; the earlier framing
   below was a false trichotomy — see `boundaries/physics-boundary.md` §3.)*
   The shared kernel `pm_trace.cpp` has **exactly one** role-owned reach in 800
   lines: `physent_modelindex` at `pm_trace.cpp:96` resolves `pe->info → edict
   → v.modelindex` through `env.arena->edict_num(...)`. **All the shared code
   wants from the server is one `int`** (a model index), which it hands to the
   neutral `IModelResolver`. It is not "the edict store" in any deep sense.
   The fix is legacy-precedented and mechanical: legacy's `SV_CopyEdictToPhysEnt`
   resolves the model *at gather time*; our OQ-2 (2026-07-19) chose to re-resolve
   at trace time, which is what dragged the store into the shared code. Restore
   the gather-time shape in **neutral** form — each role's gather fills a
   per-physent `int` model index (server from `edict→modelindex`, client from
   `cl_entity→modelindex`) — and the `arena` field leaves `PmTraceEnv`. **No
   virtual call in the trace loop, no reopening Q-20, no touching the byte-exact
   arithmetic.** Caveat: the client half of the gather is Chunk 12 code and does
   not exist yet, so "the client can fill the same int" is reasoned from legacy,
   not verified against our tree — confirm when that gather is written. The
   measured split is in `src/physics/CMakeLists.txt`: the shared surface is
   `pm_trace.cpp` alone (800 lines, 0 `ServerRuntime` refs); `init_client_move.cpp`
   (495, 2) and `movevars.cpp` (105, 2) are server producers/harness, not shared
   code; `physics.cpp` (1808, **39**, `SV_Physics` world-sim), `pmove.cpp` (541,
   7, the gather) and `run_cmd.cpp` (281, 3) are server-only. Legacy splits it
   the same way: `engine/common/pm_trace.c` shared, versus the server-only
   `engine/server/sv_phys.c` and `sv_pmove.c`.
5. **Decide explicitly whether narrowing the `ServerRuntime &` corridor in
   `src/server/physics/` is in or out of scope.** Narrowing signatures and
   porting parity math in the same wave is the collision to avoid. Either
   answer is fine; no answer is not.
6. **State where the consolidated world/physics micro-predicates live** before
   any new physics TU is written, or the duplication regrows: 16 definitions
   under 4 competing names today, 3 of which gate the fenced block.
7. The four `switch(movetype)` sites are **parity-shaped by choice** and are
   the slot the deferred `physFuncs` override hooks fit into. Explicitly **not**
   to be table-ified — this was proposed once and correctly refused.
8. Record the already-ratified threading contract (single global `pmove_t`,
   players sequential, no new global physics state). Transcription, not a
   decision.
9. Note that `src/physics/` is still an **empty placeholder** — the
   `pm_shared` work lives in `src/server/physics/`, and item 4 above is what
   has to be decided before the target can exist. *(This item also named
   `src/world/`; that one was resolved on 2026-07-20 — the world code was
   already decoupled and is now the `xash3dpp_world` target with its own
   boundary spec, so `physics` is the last misleading skeleton of the two.)*

______________________________________________________________________

### Chunk 12 — client

**Subsystems**: `client`, `demo` stub, `ui` stub\
**Depends on**: all server-path chunks + sound (Chunk 9) + input (Chunk 10)\
**Recon/Boundary**: none yet (broad survey only: `legacy-survey/engine-client.md`) — run `/analyse-subsystem client` before scaffold\
**Legacy reference**: `engine/client/cl_main.c`, `cl_frame.c`, `parse/cl_parse.c`, `dll_int/cl_game.c`, `dll_int/cl_gameui.c`\
**Complexity note**: Client DLL bridge (`cdll_int.h` / `cdll_exp.h`) plus prediction wiring are the largest surfaces; scope to connect → parse → predict → render-one-frame; defer VGUI/UI to Chunk 13.\
**ABI surfaces touched**: `engine/cdll_int.h`, `engine/cdll_exp.h` — **FROZEN Client DLL ABI**\
**Deliverable**: Client connects to local server, loads HL `cl_dlls/client.dll`, parses one SVC_PRINT frame; Chunk 12a scope only — rendering deferred to Chunk 13

**Entry gate** *(modernization audit 2026-07-20; packet:
`design/thread-model-decision-packet.md`)*:

1. **Thread model — NETWORK I/O ONLY.** The former wording bundled "network
   I/O **or rendering**", which cannot be discharged here: `threading-model.md`
   §3.6 already assigns the render thread to **Chunk 13**, Q-6 makes the spawn
   a function of the backend's declared `wants_render_thread`, and Q-10 scopes
   the backend choice to "before Chunk 13". The render half now lives in Chunk
   13's entry gate. The NetIO half closes **by citation**: §3.5 states verbatim
   *"Chunks 8/10/11/12 — Model B unchanged. No new threads."* and §3.7 defers
   `T_NetIO` to §7.4's two triggers, **both unfired**. Record the decision or
   cite the default. Scope if it is ever taken: **22 waivers, all in
   networking**, and the cut line is **transport-vs-delta**, not
   subsystem-vs-subsystem.
2. **This chunk's obligations are NOT in `src/client/`** — they are spread
   across **nine** existing subsystems (server, save, sound, input, platform,
   cmd_cvar, networking, abi, host). A `/plan-implementation client` run scoped
   to the client directory finds almost none of them.
3. **Fix the launcher's null-dependency bypass in the same wave** as the
   sound/input link edges. `launcher/main.cpp:90` is
   `xash::Host host; return host.Main(args);`, and `Host::Main` builds init
   params with "dep pointers intentionally left null (standalone path)" — the
   fully-wired path is exercised by one test file, not the shipped binary.
   Adding CMake link edges on top of that yields a graph-only wiring.
4. **Ratify cvar storage ownership before wiring the client DLL's cvar
   surface**, or a *third* cvar registry gets written — a game DLL currently
   cannot read a single engine cvar.
5. When client work first opens the delta pipeline, execute the `snapshot_*` →
   `delta_frame_*` rename (**identifiers only** — field widths, ordering and
   codec arithmetic are HB-2 fenced).
6. Server must implement `ITrustOracle`; it is `nullptr` in every production
   path today, so the `stuffcmd` trust gate is permanently disabled.

______________________________________________________________________

### Chunk 13 — renderer

**Subsystems**: `renderer`\
**Depends on**: map_loader, content, platform, client (Chunk 12)\
**Recon/Boundary**: none yet (broad survey only: `legacy-survey/renderers.md`)\
**Legacy reference**: `ref/gl/gl_rmain.c`, `gl_studio.c`, `gl_rsurf.c`; `engine/ref_api.h` (v17) as a compat reference\
**Complexity note**: The renderer ABI is fully internal — `ref_api.h` can be replaced with any shape — but GoldSrc visual output (BSP lightmaps, studio model rendering, water warp) must match. **Vulkan vs. GL vs. multi-backend decision needed before this chunk starts.**\
**ABI surfaces touched**: none frozen — internal\
**Deliverable**: GL stub renders world geometry of a HL map; null renderer passes through client frame loop

**Entry gate** *(modernization audit 2026-07-20; packet:
`design/renderer-backend-decision-packet.md`)*:

1. **The backend decision lives here, and so does the render-thread
   decision** — Q-6 already makes the `T_Render` spawn a function of the
   backend's declared `RendererCaps::wants_render_thread` (false for GL, true
   for Vulkan), and Q-10 already scopes backend policy to "before Chunk 13".
   The Chunk-12 entry previously claimed the render half; that is corrected.
2. **imagelib is this chunk's supplier and has NO in-tree consumer at all** —
   `decode()` and all four save paths have zero production callers, so its
   **compressed-format passthrough contract has never been exercised**.
   Validate it against the chosen backend's real format support. This is a
   forward obligation, **not dead code**.
3. **Wire `ImageDecoder` into `content::InitParams`** and add the
   content→imagelib link. The marker is **stale-tagged `TODO(Chunk 7)`** in
   `src/content/CMakeLists.txt` though this chunk owns it — a stale tag is
   worse than an untagged TODO, because it hides behind a discharged chunk
   number and is filtered out by any gate scoped to open chunks.
4. **Land imagelib's key-column dispatch conversion BEFORE linking it here.**
   Today the blast radius is 9 sites inside one target with test-only linkage;
   once the renderer is a second caller it only grows.
5. **The backend selector is the first new dispatch site built after this
   audit.** It must be born as a keyed `constexpr` table compared once with
   `ci_equal` — not a per-backend `handles()` virtual, not a function-local
   pointer array — or it becomes the fifth incompatible shape.
6. Become the production implementer of `content::IModelPostProcess`; do
   **not** add a second post-load hook.
7. **If `T_Render` is adopted**: the cmd_cvar retrofit (all four parts, not
   just a mutex) and the 8 plain/mixed stats structs become hard
   preconditions, and HB-5 must be **built** rather than briefed. Sequence
   them ahead of renderer work, not alongside it.
8. `RenderFrame` is HB-5's cited P-2 reference implementation and is **zero
   code**. Either build it to the shape already specified in
   `threading-model.md` §6.3, or stop citing it as a reference.
9. **Conditional on the backend naming mobile a primary target**: imagelib's
   `PixelFormat` set has **no ETC2/ASTC entries**. Do not add speculatively.

______________________________________________________________________

### Chunk 14 — leaves (demo, ui)

**Subsystems**: `demo`, `ui`\
**Depends on**: client (Chunk 12) — one at a time after respective parents\
**Recon/Boundary**: none yet\
**Legacy reference**: `cl_demo.c`, `cl_gameui.c`\
**Complexity note**: Each is a leaf with no unimplemented dependents; scope individually. Demo format is internal and a breaking change is acceptable.\
**ABI surfaces touched**: demo format (internal, breaking change OK)\
**Deliverable**: Demo record/playback test; MainUI DLL loads

**Entry gate** *(added 2026-07-20; the modernization audit wrote gates for
Chunks 11/12/13 and left this one without, so any Chunk-14 obligation had no
landing surface at all)*:

1. **Write both boundary specs first.** `demo` and `ui` are two of the six
   0-TU skeletons with no spec, which is why `q21_scan`'s 16/16 is silence
   rather than health — it globs `docs/boundaries/*-boundary.md` and a
   subsystem with no doc contributes no denominator slot.
2. **Decide the `q21_scan` denominator before writing them**, not after. It
   is the same open question Chunk 11's gate raises: adopt
   `xtools.subsystems()` (22 directories, so 22 slots) or delete the unbuilt
   skeleton directories. The two are mutually exclusive and L10/L11 of the
   audit disagreed; whoever writes these specs settles it.
3. **The demo format is internal and a breaking change is explicitly OK** —
   it is NOT under the HB-2 byte-exact fence, unlike the networking wire
   codec it superficially resembles. Do not extend the fence to it by
   analogy.
4. **Check the ledger before planning.** Obligations owed to this chunk live
   in `audits/2026-07-modernization-audit.ledger.md`; the audit's own lesson
   was that Chunk 12's obligations are spread across nine *other*
   subsystems, so a scan scoped to `src/demo/` + `src/ui/` will miss them.
5. `ui` loads the MainUI DLL — a second game-DLL-shaped ABI consumer. Reuse
   the `GameDll` loader seam rather than growing a parallel one, and record
   the decision either way.

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

______________________________________________________________________

## Harmonization backlog (2026-07-06, unscheduled)

> **These are candidate follow-ups, NOT scheduled chunks.** They come from the
> cross-cutting synthesis of the 13-subsystem doc refresh (see
> `legacy-survey/overview.md` §"As-built synthesis (2026-07-06)"). The chunk
> numbering above stays authoritative; nothing here is promoted until it earns
> a chunk. Every item flagged **needs design brief** must get its own brief
> *before* any code lands (precedent: `design/extension-goals.md` §1 and
> `design/pm-determinism-decision.md`) because it has ABI or cross-subsystem
> impact. Each item cites the refreshed per-subsystem docs it came from and
> tags the relevant G-/P-/Q-/OQ- IDs. Nothing here invents scope: every item
> traces to a finding already recorded in the refreshed docs.

> **Status 2026-07-19 (consolidation audit fix wave):** HB-1 ✅ resolved
> (utilities `ci_compare` ebeb1763 + filesystem b1a7b4de + cmd_cvar
> 5befba72); HB-2 ✅ (no-touch set recorded in
> `decisions-architecture.md` §Q-21); HB-3 ✅ (host 93860120 + map_loader
> feed7557; server was already the clean reference); HB-8 ✅ (ownership
> recorded in the filesystem/content boundaries — no code change needed);
> HB-9 ✅ (abi/launcher boundary P-8 rows record the by-role posture);
> HB-10 ✅ (status table reconciled; the core cvar public-name choice
> stays recorded in `core-boundary.md`); HB-11 ◑ inventory current — the
> one candidate addition, the host frame pump, was WIRED instead of
> listed (93860120); HB-12 ⏳ deferred (owner: a future harmonization
> session); HB-4..HB-7 ⏳ deferred pending their design briefs (DOOR
> class). Full adjudication ledger:
> `docs/audits/2026-07-consolidation-audit.md`.

### Security (act first)

- **HB-1 — Bounded `ci_compare(string_view, string_view)`** *(SEC)* —
  `string_view.data()` is handed to C-string `strnicmp`/`strncmp`, a latent
  OWASP buffer-over-read (in-tree callers currently pass NUL-terminated
  literals, so it is latent, not live). Present in **utilities** (M-4
  `ci_less`), **filesystem** (M-7 `ci_find_by_name`), **cmd_cvar** (M-5, 6
  sites); **verified absent** in the other 9 subsystems checked (memory is
  N/A — it has no string-compare sites). Fix: add one bounded
  `ci_compare` in `utilities`, adopt at the 3 call sites; delete the
  per-subsystem M-4/M-7/M-5 findings on adoption. *Docs*:
  `modernization-opportunities/{utilities,cmd_cvar}-modernization.md` +
  `modernization-opportunities/filesystem-modernization.md`.
  *Tags*: no ABI impact; a shared `utilities` helper so coordinate the three
  consumers. **No design brief needed** (bounded change, no ABI surface).

### Do-not-modernize invariants (name, don't touch)

- **HB-2 — Name the tree-wide float/byte-EXACT no-touch set** *(INVARIANT)* —
  five subsystems independently flagged the same prohibition: map_loader
  (Q-18 trace/PVS/CRC kernel), content (studio bone math), networking (wire
  bit-codec + delta widths + LZSS + OOB magic), server (rotated-brush ULP
  `clip.cpp:219`), utilities (double-precision studio math). Record the union
  once (a `decisions-architecture.md` note or an `extension-goals.md`-style
  invariant list) so no future modernization pass (FMA/reassoc/`std::ranges`
  rewrite) silently perturbs a golden-gated kernel. *Docs*:
  `modernization-opportunities/{map_loader,networking,content,server}-modernization.md`,
  `legacy-survey/deep-dive-{trace-pvs,delta-encoder,content}.md`. *Tags*:
  Q-18; wire-compat (Networking DECIDED §above); no code, a documentation
  invariant. **No design brief needed** (records existing decisions).

### P-8 thread-role conformance

- **HB-3 — P-8 `assert_thread_role` under-guard sweep** *(P-8)* — transition
  and frame-entry points are under-guarded relative to their header
  contracts: **host** `RunFrame`/`RequestShutdown`/`signal_frame_abort`
  (header claims Main-only), **map_loader** `new_game`/`change_level`/
  `clear_world`. Server is the clean reference (92 uniform asserts, no gap).
  These are exactly the spots where "marshal back to Main" is anchored, so
  the gap matters for G-1/G-3. Fold into the **Chunk 6B** hardening retrofit
  rather than scheduling separately. *Docs*:
  `threading-analysis/{host,map_loader}-threading.md`. *Tags*: P-8; Chunk 6B.
  Exclude the enforcement-free-by-role subsystems (HB-9) from the sweep.
  **No design brief needed** (adds asserts to match existing contracts).

### Extension-door infrastructure (design brief first)

- **HB-4 — Thread-spawn + `ThreadRole`-register primitive** *(DOOR, P-1)* —
  **✅ BRIEF DELIVERED 2026-07-19** (`design/thread-spawn-and-inbox-brief.md`,
  chunks-8/9/10 campaign B2; register entry Q-24): spawn/naming/priority →
  platform (lands Chunk 9 slice S9.0), MpscQueue/SpscRing → core (S9.7a),
  host RunFrame inbox-drain slot designed-NOT-built (first consumer G-1/G-3).
  Original text follows for the record —
  no OS thread-spawn primitive exists yet: core owns the `ThreadRole` enum +
  `assert_thread_role`, platform hosts `thread_role.cpp` (natural owner, no
  spawn), host owns the P-1 inbox-drain slot (`RunFrame`, marked by
  `cbuf_execute`), launcher establishes Main as a pure provider. Every
  north-star off-main thread (G-1 MCP listener, G-2 NetIO, G-3 debug thread,
  P-1 worker pool) needs it. Design the spawn + main-thread inbox (MPSC) as
  **one unit at Chunk 7** (the plan already hooks the worker-pool inbox
  there). *Docs*: `boundaries/{platform,core,host}-boundary.md`,
  `threading-analysis/host-threading.md`. *Tags*: P-1, G-1/G-2/G-3/G-5;
  Chunk 7 hook. **Needs design brief** (cross-subsystem: core/platform/host).

- **HB-5 — One shared P-2 published-snapshot idiom** *(DOOR, P-2)* —
  ✅ **BRIEF DELIVERED 2026-07-20** (`design/published-snapshot-brief.md`,
  modernization audit lens L1). **Analysis half DISCHARGED; primitive half
  GATED on the Chunk-12 thread-model decision** — it is no longer an open
  build item. Verdict: **no shared primitive is warranted today.** The tree
  has exactly ONE cross-thread structured publisher (sound's channel
  handshake) and it has zero production consumers; there are two production
  thread spawns tree-wide, both in `src/sound/topology.cpp`.
  **Three of this entry's own former references were wrong** and are
  corrected in the brief: map_loader's `WorldData` swap is Main-only (its own
  sentence says so — a finding claiming otherwise was refuted); server's
  `snapshot_*` family is the **wire entity-delta pipeline**, not P-2, and two
  packs mis-read it as evidence *because of this entry's wording*; and Chunk
  13's `RenderFrame` "reference implementation" is **zero code**. What ships
  instead is a three-term vocabulary rule (`publish` / `snapshot` /
  `delta_frame`) plus eight shape constraints binding on whoever eventually
  builds it. *Tags*: P-2, G-1/G-3.

- **HB-6 — Name the "one introspection layer" (P-4) channels** *(DOOR, P-4)* —
  core is the substrate (logs, `Clock::stats`, `error_code_name`); memory
  (`get_stats`/`for_each_pool`), content (`model_infos`/`StudioView`),
  map_loader (world queries), and server (`EntityView`) are its channels.
  Enumerate them as a single typed introspection layer (feeds G-1/G-3/G-4)
  so no frontend grows a private backdoor (the P-4 door rule). *Docs*:
  `boundaries/core-boundary.md` + each subsystem boundary. *Tags*: P-4,
  G-1/G-3/G-4.
  ✅ **BRIEF DELIVERED 2026-07-20** (`design/introspection-channels-brief.md`,
  lens L3). **Has a buildable first slice with an in-tree consumer**: the
  `diagnostics_dump` aggregator, overdue since `debug-stats-design.md` §6.4's
  ≥3-stats-structs trigger (the tree has 14). Two corrections this entry
  needs: (a) **`EntityView` is NOT a channel** — it is a private-header,
  zero-cost Q-20 accessor over a *live* edict, unreachable from any frontend
  and not a snapshot; (b) §6.2's positional `diagnostics_dump(CmdCvarContext &,
  MemorySubsystem &, ...)` is **unbuildable**, not merely unscalable — `host`
  does not link sound/input/content/imagelib, so it forces four new link edges
  into the composition root for a debug command, while a channel-span form
  forces zero. The brief also finds the real G-3 blocker is not "9 plain
  structs" but **6 accessors returning a live reference into plain memory**,
  three of which are deletable outright.

- **HB-7 — Shared aligned-allocation door** *(DOOR, P-7/G-5)* — memory H-1
  (lift the `alignof(T) ≤ 8` ceiling on `pool_new`) simultaneously serves
  P-7 over-aligned classes, the G-5 script-allocator bridge, and the future
  `IAllocatorBackend` seam (the strategy fn-ptr triple is that seam). Cover
  all three in one brief rather than solving H-1 in isolation. *Docs*:
  `modernization-opportunities/memory-modernization.md`,
  `boundaries/memory-boundary.md`. *Tags*: P-7, G-5, Q-2; Arena-time.
  ✅ **BRIEF DELIVERED 2026-07-20** (`design/allocation-seam-brief.md`, lens
  L6). **STAYS OPEN AND UN-BUILT — and the brief corrects this entry's own
  premise.** The alignment lift is **not** a G-5 precondition: all three
  shortlisted runtimes bridge to `mem_alloc`/`mem_free` today at ≤8-byte
  alignment. AngelScript's gap is **per-VM attribution** (a global,
  context-less hook needing a TLS-routing shim), not alignment — so HB-7 must
  not sit on the critical path of the G-5 runtime pick or its spike. A
  Phase-1 finding proposing the lift as *work* was refuted by the
  gold-plating critic (self-declared speculative consumer; all 11 `pool_new`
  sites are under-aligned). The brief records the **additive two-path shape**
  — never grow the shared 8-byte `AllocHeader`; add
  `mem_alloc_aligned`/`mem_free_aligned` as a paired path, because
  `mem_free`'s fixed `ptr - 1` cannot disambiguate — and names **HB-5's
  cacheline-padded double buffer** as a more concrete future requirer than
  G-5. Separately: the one real allocation defect found is **adoption, not
  shape** — content, imagelib and map_loader each create and destroy a pool
  with zero allocations routed through it.

### Housekeeping (bounded, no brief)

- **HB-8 — Record `xash3dpp_miniz` shared-target ownership** *(HOUSE)* —
  `xash3dpp_miniz` (built from `../public/miniz.c`) is linked PRIVATE by
  **filesystem** and **content**, and is **not** owned by utilities. Already
  reflected in those two boundary/deep-dive docs; noted here so it is not
  mis-attributed. *Docs*: `boundaries/{filesystem,content}-boundary.md`.
  *Tags*: none (build-graph fact).

- **HB-9 — Flag enforcement-free-by-role subsystems** *(HOUSE)* — **abi**
  (C-ABI shim) and **launcher** (pure Main provider) carry 0
  `assert_thread_role` **by design** — the deliberate counterpoint to
  consumer subsystems. Any P-8 sweep (HB-3) must exclude them, not
  back-fill them. *Docs*: `threading-analysis/abi-threading.md`,
  `boundaries/{abi,launcher}-boundary.md`. *Tags*: P-8 (exclusion list).

- **HB-10 — Reconcile status/plan drift** *(HOUSE)* — the Status Table above
  lags `status_table.py`: **content** is structurally Complete (table still
  reads Partial). **Core cvar-name drift** is a GoldSrc config-compat gap:
  `host_maxfps` vs legacy `fps_max`, `host_sleeptime` vs `sleeptime`, and
  `sys_timescale` set FCVAR_CHEAT vs legacy FCVAR_FILTERABLE. *Docs*:
  `boundaries/{content,core}-boundary.md`. *Tags*: OQ-2 (content finish);
  GoldSrc config compat. **No design brief** (status reconcile + cvar
  metadata; the cvar rename is a parity decision to confirm, not new scope).

- **HB-11 — Track unfinished code inside "Complete" subsystems** *(HOUSE)* —
  chunk-inherited deferrals inside structurally-Complete subsystems, each
  already carrying an inline `stub_scan` marker: utilities `gameinfo_parser`
  stubs + `swap_struct` undefined; server `ITrustOracle`/`ICompatPolicy`
  (Chunk 7) + save paths (Chunk 8); networking DNS + bz2; cmd_cvar
  `$`-substitution / `if`-`else` / stuffcmd prefix-filter / `base_cmd` sorted
  autocomplete; launcher Optimus/PowerXpress `dllexport` port gap. Live source
  of truth remains `stub_scan.py`. *Docs*: each subsystem boundary. *Tags*:
  Chunk 7/8 (server), console-UI chunk (cmd_cvar autocomplete). **No brief**
  (inventory of already-tracked deferrals).

- **HB-12 — One shared deterministic RNG** *(HOUSE)* — multiple RNG stubs
  await unification: server `s_rng_state`/`s_pm_rng`, plus the
  `COM_RandomLong`/`Float` xorshift stubs in `engine_table.cpp` +
  `init_client_move.cpp` (see the Chunk 6 deferred inventory above). Unify
  into one idtech-parity RNG stream. *Docs*: `boundaries/server-boundary.md`,
  the deferred-stub inventory above. *Tags*: determinism (parity — the shared
  stream must reproduce idtech bytes). **No design brief** (parity port, but
  the byte-exact requirement ties it to HB-2's invariant set).
