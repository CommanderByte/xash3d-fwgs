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
| map_loader | 9 | ✓ | ✓ | **Complete** (BSP v29/30/BSP2/30ext → immutable WorldData; PVS + trace kernel, Q-18 golden-gated; FSM loads worlds) |
| launcher | 1 | ✗ | ✗ | **Partial** (thin argv bootstrap, no tests) |
| networking | 24 | ✓ | ✓ | **Complete** (Layers 0–4 incl. delta encoder + satellites, wired into EngineContext; DNS/bz2 deferred) |
| server | 0 | ✓ | ✗ | **Skeleton** (include stub; recon done — boundary spec + 6 deep dives committed 2026-07-04) |
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

### Chunk 6 — server *(dedicated-server milestone)*

**Subsystems**: `server` (incl. the server-side pmove bridge `sv_pmove.c`, moved in from Chunk 11 per the boundary spec's satellite table)\
**Depends on**: cmd_cvar, networking (Chunk 2), map_loader (Chunk 5), host, filesystem, memory, platform\
**Recon/Boundary**: ✅ done 2026-07-04 (commit `43b07bb7`) — boundary spec `docs/boundaries/server-boundary.md`; deep dives `legacy-survey/deep-dive-server-{lifecycle,game-dll-bridge,clients,physics,world-frame,save-boundary}.md`. **Scaffold OQs decided 2026-07-04**: `server-boundary#OQ-1` → **Q-19 PHS_PLACEMENT** (PHS lands as a map_loader `phs` query module, in Chunk 6 scope) and `#OQ-5` → **Q-20 EDICT_STORE** (ABI-exact edict array as single store behind a zero-cost access seam); crosswalk in `decisions-architecture.md` §3a. **Scaffold unblocked.**\
**Legacy reference**: `engine/server/sv_main.c`, `sv_game.c` (159-slot `enginefuncs_t`), `sv_world.c`, `sv_phys.c`, `sv_pmove.c`, `sv_frame.c`, `sv_client.c`\
**Complexity note**: The `enginefuncs_t` table is 159 function pointers and `entvars_t` layout is byte-exact frozen — highest ABI risk in the entire rewrite. Getting `sv_game.c` to load and call a real HL game DLL without crashing is the integration milestone; `entvars_t` layout must be ABI-exact at the boundary even if internal entity storage differs.\
**ABI surfaces touched**: `engine/eiface.h`, `engine/edict.h` — **FROZEN Game DLL ABI**\
**Deliverable**: Dedicated server starts, loads HL `dlls/hl.dll`, runs a single map frame; server ctest green — **dedicated-server milestone**

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
