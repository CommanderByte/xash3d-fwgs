# xash3dpp — Implementation Status and Work Plan

*Generated: 2026-05-15 — Updated: 2026-07-03*

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
| map_loader | 1 | ✓ | ✗ | **Partial** (FSM scaffold; no BSP parsing yet) |
| launcher | 1 | ✗ | ✗ | **Partial** (thin argv bootstrap, no tests) |
| networking | 17 | ✓ | ✓ | **Partial** (Layers 0–3 + satellites complete, wired into EngineContext; delta encoder in progress; DNS/bz2 deferred) |
| server | 0 | ✓ | ✗ | **Skeleton** (include stub exists) |
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

The legacy DAG flows: **launcher → host → (cmd_cvar + networking + filesystem) → server ↔ client → plugins (renderer, filesystem)**. The entire foundation tier is now complete: `utilities`, `memory`, `filesystem`, `platform`, `core`, `cmd_cvar`, `host`/`engine_context` — **Chunks 1 and 3 are done**. Chunk 2/4 (networking) shipped as a standalone library in May 2026 (address/MessageBuf, transport, wire codecs, netchan, GoldSrc protocol driver, master list) and was wired into `EngineContext` on 2026-07-03; the **delta encoder (Layer 4) is the remaining networking piece and is in progress**. `map_loader` has an FSM scaffold but no BSP parsing; it must be promoted before server can load maps (gated on the pm_shared float-vs-fixed decision). The server path (Chunks 5, 6) is significantly more isolated than the client path because it only needs the Game DLL ABI; client adds sound, input, content, rendering, and UI on top.

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
**Status**: 11 `.cpp` files, 4 public headers, 5 test files — `xash3dpp_cmd_cvar` builds and tests pass.

______________________________________________________________________

### Chunk 2 — networking primitives

**Subsystems**: `networking`\
**Depends on**: utilities, memory, platform *(all done)*\
**Legacy reference**: `engine/common/net_ws.c`, `net_buffer.c`, `net_chan.c`, `net_encode.c`\
**Complexity note**: `net_chan.c` (reliability/fragmentation) and `net_ws.c` (sockets) are entangled in the legacy code; the key design decision is splitting them into a pluggable transport layer and a protocol layer.\
**ABI surfaces touched**: none — internal; wire-compat with GoldSrc is a strategic choice (see Open Questions)\
**Deliverable**: `xash3dpp_networking` builds; loopback transport + Netchan reliability tests pass

______________________________________________________________________

### Chunk 3 — host completion ✅ DONE

**Subsystems**: `host`, `core` (Clock), `map_loader` (FSM scaffold), `abi` (Host_Error shim)\
**Depends on**: cmd_cvar, filesystem, memory, platform *(all done)*\
**What shipped**:

- `core::Clock` — timing cvars (`host_maxfps`, `fps_override`, `host_framerate`, `sys_ticrate`, etc.), `tick()` with FPS gate, renderer-thread-safe atomics
- `Host` pimpl restructured with injected-dep `HostInitParams`; `RunFrame`; `signal_frame_abort` with recursive-abort guard
- `EngineContext` flat-owner struct; `init/shutdown` sequencing; `set_current_engine_context` bookend
- `map_loader` FSM scaffold (state machine + observer slots; no BSP loading yet)
- `abi/engine_funcs.cpp` — `Host_Error` direct-export shim\
**ABI surfaces touched**: none\
**Tests**: `test_host.cpp` (init/shutdown, signal_frame_abort, bugcomp pass-through) — green

______________________________________________________________________

### Chunk 4 — networking primitives

**Subsystems**: `networking`\
**Depends on**: utilities, memory, platform *(all done)*\
**Legacy reference**: `engine/common/net_ws.c` (~1 900 lines), `net_chan.c` (~1 800 lines), `net_buffer.c`, `net_encode.c`\
**Complexity note**: Four tightly-coupled layers (transport, netchan, codec, delta encoder) with bzip2/LZSS compression; delta encoder is the gnarliest part. Key design decision: splitting socket I/O from the reliability channel so the transport is mockable in tests.\
**ABI surfaces touched**: `netadr_t` binary layout in `common/netadr.h` is frozen (20 bytes, `#pragma pack(1)`); `net_api_t` client-DLL callbacks must be bit-compatible.\
**Deliverable**: `networking::NetworkContext` init/shutdown; loopback send/recv round-trip test; `netadr_t` serialise/deserialise test; `engine_context.hpp` Chunk-2 slot wired

______________________________________________________________________

### Chunk 5 — map loader (BSP + world queries)

**Subsystems**: `map_loader` (promote scaffold to real parser)\
**Depends on**: filesystem, utilities, memory *(all done)*\
**Legacy reference**: `engine/common/model.c`, `mod_bmodel.c`, `engine/common/world.c`, `engine/common/pm_trace.c`, `pm_surface.c`\
**Complexity note**: BSP v30 format loading is straightforward; the hard part is the spatial query API (point-in-leaf, PVS decompression, clip-node trace) that server physics and networking (multicast/PVS) depend on. The trace interface must be decoupled from raw edict pointers now — otherwise the server ABI refactor in Chunk 6 is impossible.\
**ABI surfaces touched**: `pm_shared/` — determinism requirement; **float vs. fixed-point must be decided before this chunk ships**\
**Deliverable**: `MapLoader::load_bsp()` parses a real BSP v30 file; leaf/PVS queries return correct results; float/fixed decision committed to a doc

______________________________________________________________________

### Chunk 6 — server *(dedicated-server milestone)*

**Subsystems**: `server`\
**Depends on**: cmd_cvar, networking (Chunk 4), map_loader (Chunk 5), host, filesystem, memory, platform\
**Legacy reference**: `engine/server/sv_main.c`, `sv_game.c` (100+ `enginefuncs_t` callbacks), `sv_world.c`, `sv_phys.c`, `sv_frame.c`, `sv_client.c`\
**Complexity note**: The `enginefuncs_t` table is 150 function pointers and `entvars_t` layout is byte-exact frozen — highest ABI risk in the entire rewrite. Getting `sv_game.c` to load and call a real HL game DLL without crashing is the integration milestone; `entvars_t` layout must be ABI-exact at the boundary even if internal entity storage differs.\
**ABI surfaces touched**: `engine/eiface.h`, `engine/edict.h` — **FROZEN Game DLL ABI**\
**Deliverable**: Dedicated server starts, loads HL `dlls/hl.dll`, runs a single map frame; server ctest green — **dedicated-server milestone**

______________________________________________________________________

### Chunk 7 — content pipeline (model & image loaders)

**Subsystems**: `content`\
**Depends on**: filesystem, utilities, memory *(all done)*\
**Legacy reference**: `engine/common/imagelib/` (BMP, DDS, TGA, KTX2, WAD), `mod_studio.c`, `mod_alias.c`, `mod_sprite.c`\
**Complexity note**: Studio model has bone/sequence/mesh/texture sub-formats; the hardest part is texture format normalisation (WAD→RGBA, DXT→RGBA) done identically to GoldSrc for visual compat. `common/com_model.h` struct layout is frozen for client DLL reads.\
**ABI surfaces touched**: `common/com_model.h` structs read by client DLL — layout frozen.\
**Deliverable**: Typed model handle lookup; WAD texture pack/unpack test; studio header parse test

______________________________________________________________________

### Chunk 8 — save/restore

**Subsystems**: `save`\
**Depends on**: server (Chunk 6), filesystem, map_loader\
**Legacy reference**: `engine/server/sv_save.c` (format tag 0x71, ~1 400 lines)\
**Complexity note**: Binary format assumes exact `entvars_t` field ordering — any field added to server edict layout must be mirrored here; the field-map serialiser is fiddly but well-bounded.\
**ABI surfaces touched**: none frozen externally, but save files from the legacy engine must remain loadable.\
**Deliverable**: Round-trip save/load of a minimal game state; reject-gracefully on version mismatch

______________________________________________________________________

### Chunk 9 — sound *(isolatable, slot anywhere after foundation)*

**Subsystems**: `sound`\
**Depends on**: filesystem, memory, platform *(all done — can run alongside other chunks)*\
**Legacy reference**: `engine/client/sound/s_main.c`, `s_load.c`, `s_mix.c`, `s_dsp.c`\
**Complexity note**: Needs a null-output device backend for CI so the mixer can be tested without hardware.\
**ABI surfaces touched**: none\
**Deliverable**: `xash3dpp_sound` builds; wav decode + mix test passes with null output device

______________________________________________________________________

### Chunk 10 — input *(isolatable, slot anywhere after foundation)*

**Subsystems**: `input`\
**Depends on**: platform *(done — can run alongside other chunks)*\
**Legacy reference**: `engine/client/input/input.c`, `in_keys.c`, `in_joy.c`\
**Complexity note**: Abstract the SDL/Win32/touch event sources behind an interface so the subsystem is testable without a window.\
**ABI surfaces touched**: none\
**Deliverable**: `xash3dpp_input` builds; key/button event tests pass with mock event source

______________________________________________________________________

### Chunk 11 — physics (pm_shared)

**Subsystems**: `physics`\
**Depends on**: map_loader (Chunk 5), server (Chunk 6 — edict query interface)\
**Legacy reference**: `pm_shared/pm_move.c`, `pm_shared/pm_trace.c`, `engine/server/sv_pmove.c`, `engine/client/dll_int/cl_pmove.c`\
**Complexity note**: Client prediction and server authority must produce bit-identical results — determinism is the hardest constraint; float/fixed choice from Chunk 5 is locked in here.\
**ABI surfaces touched**: `pm_shared/` — **FROZEN** (shared client ↔ server)\
**Deliverable**: `PM_Move` runs identically on both paths; determinism regression test passes

______________________________________________________________________

### Chunk 12 — client

**Subsystems**: `client`, `demo` stub, `ui` stub\
**Depends on**: all server-path chunks + sound (Chunk 9) + input (Chunk 10)\
**Legacy reference**: `engine/client/cl_main.c`, `cl_frame.c`, `parse/cl_parse.c`, `dll_int/cl_game.c`, `dll_int/cl_gameui.c`\
**Complexity note**: Client DLL bridge (`cdll_int.h` / `cdll_exp.h`) plus prediction wiring are the largest surfaces; scope to connect → parse → predict → render-one-frame; defer VGUI/UI to Chunk 13. The thread-model decision (whether network I/O or rendering run off-main) must be made before this chunk starts.\
**ABI surfaces touched**: `engine/cdll_int.h`, `engine/cdll_exp.h` — **FROZEN Client DLL ABI**\
**Deliverable**: Client connects to local server, loads HL `cl_dlls/client.dll`, parses one SVC_PRINT frame; Chunk 12a scope only — rendering deferred to Chunk 13

______________________________________________________________________

### Chunk 13 — renderer

**Subsystems**: `renderer`\
**Depends on**: map_loader, content, platform, client (Chunk 12)\
**Legacy reference**: `ref/gl/gl_rmain.c`, `gl_studio.c`, `gl_rsurf.c`; `engine/ref_api.h` (v17) as a compat reference\
**Complexity note**: The renderer ABI is fully internal — `ref_api.h` can be replaced with any shape — but GoldSrc visual output (BSP lightmaps, studio model rendering, water warp) must match. **Vulkan vs. GL vs. multi-backend decision needed before this chunk starts.**\
**ABI surfaces touched**: none frozen — internal\
**Deliverable**: GL stub renders world geometry of a HL map; null renderer passes through client frame loop

______________________________________________________________________

### Chunk 14 — leaves (demo, ui)

**Subsystems**: `demo`, `ui`\
**Depends on**: client (Chunk 12) — one at a time after respective parents\
**Legacy reference**: `cl_demo.c`, `cl_gameui.c`\
**Complexity note**: Each is a leaf with no unimplemented dependents; scope individually. Demo format is internal and a breaking change is acceptable.\
**ABI surfaces touched**: demo format (internal, breaking change OK)\
**Deliverable**: Demo record/playback test; MainUI DLL loads

______________________________________________________________________

## Open Questions / Landmines

- **pm_shared float vs. fixed-point** — Must be decided before Chunk 5 (map_loader) ships. Fixed-point eliminates cross-platform FPU divergence (important for netplay) but requires a PM layer rewrite. Float is cheaper upfront but may require reproducible-FP compiler flags.

- **Networking wire-compat target** — **DECIDED**: GoldSrc-compatible
  protocol is the default and is wire-frozen (netchan framing, delta-encoder
  field tables, game-protocol message IDs match the legacy bytes exactly).
  Per-client protocol selection is supported via an `IProtocolDriver` seam in
  netchan so a newer / experimental driver can be registered without
  touching the default path. See `boundaries/networking-boundary.md`
  §"Pluggable game protocol per client".

- **`entvars_t` internal representation** — The server must present `entvars_t` at the exact ABI-specified layout when calling the game DLL. If Chunk 6 uses SoA or any non-literal struct storage internally, it needs a projection/copy step at every DLL call boundary. Raw edict pointers everywhere vs. handle map with shim layer shapes all of Chunk 6 and the save-format chunk.

- **Renderer backend choice** — `ref_api.h` is internal and free to redesign. Decision needed before Chunk 13: Vulkan-first (modern, mobile-hostile), GL-compat (broadest reach), or abstracted multi-backend (most work). Also needs to settle on desktop vs. mobile as the primary target. Can safely be deferred until after Chunk 12 (client) is green.

- **Thread model** — The legacy engine is single-threaded. `core::Clock` already uses atomics for renderer-thread reads. A decision on whether networking I/O or rendering move to separate threads is needed before Chunk 12 (client), because it determines whether `cl_parse` and `S_Update` can run concurrently.

- **HTTP downloader + master-server placement** — **DECIDED** (Q-11
  satellite-placement test): HTTP is a separate `xash3dpp_http` target with
  its own boundary spec; master-server list stays in `xash3dpp_networking`.
  A later grouping pass may move related targets into a shared `src/net/`
  subdirectory. See `decisions-architecture.md §Q-11`.

- **Platform sockets layer not yet implemented** — Chunk 4 cannot start
  until `xash3dpp_platform` exposes an `IPlatformSockets` interface and
  `os_socket` free-function layer. Requirements are drafted in
  `architecture/platform/sockets.md`; implementation will be picked up in a
  dedicated session before Chunk 4 begins.

- **Legacy survey gaps** — Boundary specs still needed for `server`, `client`, `content`, `sound`, `input`, `physics`, and `save`. Each chunk above should start with a `/analyse-subsystem` pass to produce a boundary spec before writing any code.
