# xash3dpp — Implementation Status and Work Plan

*Generated: 2026-05-15*

---

## Status Table

| Subsystem  | src/ files | include/ | tests/ | Status   |
|------------|-----------|----------|--------|----------|
| utilities  | 9         | ✓        | ✓      | **Complete** |
| memory     | 1         | ✓        | ✓      | **Complete** |
| filesystem | 10        | ✓        | ✓      | **Complete** |
| platform   | 11        | ✓        | ✓      | **Complete** |
| host       | 1         | ✓        | ✗      | **Partial** (pimpl skeleton, no tests) |
| launcher   | 1         | ✗        | ✗      | **Partial** (thin argv bootstrap, no tests) |
| networking | 0         | ✓        | ✗      | **Skeleton** (include stub exists) |
| server     | 0         | ✓        | ✗      | **Skeleton** (include stub exists) |
| client     | 0         | ✓        | ✗      | **Skeleton** (include stub exists) |
| cmd_cvar   | 11        | ✓        | ✓      | **Complete** |
| content    | 0         | ✗        | ✗      | **Skeleton** |
| demo       | 0         | ✗        | ✗      | **Skeleton** |
| input      | 0         | ✗        | ✗      | **Skeleton** |
| physics    | 0         | ✗        | ✗      | **Skeleton** |
| renderer   | 0         | ✗        | ✗      | **Skeleton** |
| save       | 0         | ✗        | ✗      | **Skeleton** |
| sound      | 0         | ✗        | ✗      | **Skeleton** |
| ui         | 0         | ✗        | ✗      | **Skeleton** |
| world      | 0         | ✗        | ✗      | **Skeleton** |

---

## Dependency Graph Summary

The legacy DAG flows: **launcher → host → (cmd_cvar + networking + filesystem) → server ↔ client → plugins (renderer, filesystem)**. The five completed subsystems (`utilities`, `memory`, `filesystem`, `platform`, `cmd_cvar`) form the entire foundation layer — **Chunk 1 is done**. The two skeleton subsystems with existing include stubs (`networking`, `server`, `client`) are the next tier. `world` (BSP/trace/model loaders) is the next hub after networking, blocking both server and physics. The server path (Chunks 2–5) is significantly more isolated than the client path because it only needs the Game DLL ABI; client adds sound, input, content, rendering, and UI on top.

---

## Subsystem Scores

| Subsystem  | Isolation | Foundational | Complexity                                        | ABI risk                    |
|------------|-----------|--------------|---------------------------------------------------|-----------------------------|
| cmd_cvar   | High      | **High**     | Medium (~5 legacy files, global linked list)      | Low                         |
| networking | High      | **High**     | Medium (~7 legacy files, UDP + channel layer)     | Low                         |
| world      | Medium    | **High**     | High (~8 files, trace + PM callbacks)             | Medium (`pm_shared`)        |
| server     | Low       | **High**     | Very High (~15 files, 100+ `eiface` callbacks)    | **High** (Game DLL FROZEN)  |
| content    | Medium    | Medium       | Medium (~5 format loaders)                        | Low                         |
| sound      | High      | Low          | Medium (~5 files, DSP/mix)                        | Low                         |
| input      | High      | Low          | Small (~5 files)                                  | Low                         |
| physics    | Medium    | High         | Medium (`pm_shared`, determinism)                 | **High** (`pm_shared` FROZEN) |
| save       | Medium    | Low          | Small (1 file)                                    | Medium (binary format)      |
| demo       | High      | Low          | Small (1 file)                                    | Medium (delta encoding)     |
| client     | Low       | **High**     | Very High (30+ files, Client DLL ABI)             | **High** (Client DLL FROZEN)|
| renderer   | Low       | Low          | Very High (multiple backends)                     | Low (internal — free to redesign) |
| ui         | Medium    | Low          | Small (`cl_gameui.c`)                             | Low                         |

---

## Recommended Work Plan

### Chunk 1 — cmd/cvar ✅ DONE

**Subsystems**: `cmd_cvar`  
**Depends on**: utilities, memory, platform *(all done)*  
**Status**: 11 `.cpp` files, 4 public headers, 5 test files — `xash3dpp_cmd_cvar` builds and tests pass.

---

### Chunk 2 — networking primitives

**Subsystems**: `networking`  
**Depends on**: utilities, memory, platform *(all done)*  
**Legacy reference**: `engine/common/net_ws.c`, `net_buffer.c`, `net_chan.c`, `net_encode.c`  
**Complexity note**: `net_chan.c` (reliability/fragmentation) and `net_ws.c` (sockets) are entangled in the legacy code; the key design decision is splitting them into a pluggable transport layer and a protocol layer.  
**ABI surfaces touched**: none — internal; wire-compat with GoldSrc is a strategic choice (see Open Questions)  
**Deliverable**: `xash3dpp_networking` builds; loopback transport + Netchan reliability tests pass

---

### Chunk 3 — host completion

**Subsystems**: `host`  
**Depends on**: cmd_cvar, networking, filesystem, memory, platform  
**Legacy reference**: `engine/common/host.c`, `host_state.c`  
**Complexity note**: Splitting `host_parm_t` (40+ fields) into timing service, frame scheduler, and game-state machine without producing new god-objects.  
**ABI surfaces touched**: none  
**Deliverable**: `Host::Main` drives a proper frame loop; launcher wires up correctly; host tests green

---

### Chunk 4 — world (BSP + model loaders + trace)

**Subsystems**: `world`, `content`  
**Depends on**: utilities, memory, filesystem, platform *(all done)*  
**Legacy reference**: `engine/common/model.c`, `mod_bmodel.c`, `mod_studio.c`, `world.c`, `pm_trace.c`, `pm_surface.c`  
**Complexity note**: The trace/query interface must be decoupled from raw edict pointers now — otherwise the server ABI refactor in Chunk 5 is impossible.  
**ABI surfaces touched**: `pm_shared/` — determinism requirement; **float vs. fixed-point must be decided before this chunk ships**  
**Deliverable**: BSP load + trace tests pass; model format tests pass; float/fixed decision committed as a doc

---

### Chunk 5 — server *(dedicated-server milestone)*

**Subsystems**: `server`  
**Depends on**: cmd_cvar, networking, world, host, filesystem, memory, platform  
**Legacy reference**: `engine/server/sv_main.c`, `sv_game.c` (100+ `enginefuncs_t` callbacks), `sv_world.c`, `sv_phys.c`, `sv_frame.c`  
**Complexity note**: Getting `sv_game.c` to load and call a real Half-Life game DLL without crashing is the integration milestone; `entvars_t` layout must be ABI-exact at the boundary even if internal entity storage differs.  
**ABI surfaces touched**: `engine/eiface.h`, `engine/edict.h` — **FROZEN Game DLL ABI**  
**Deliverable**: Dedicated server starts, loads HL `dlls/hl.dll`, runs a single map frame; server ctest green — **dedicated-server milestone**

---

### Chunk 6 — sound *(isolatable, slot anywhere after foundation)*

**Subsystems**: `sound`  
**Depends on**: filesystem, memory, platform *(all done — can run in parallel with Chunks 1–5)*  
**Legacy reference**: `engine/client/sound/s_main.c`, `s_load.c`, `s_mix.c`, `s_dsp.c`  
**Complexity note**: Needs a null-output device backend for CI so the mixer can be tested without hardware.  
**ABI surfaces touched**: none  
**Deliverable**: `xash3dpp_sound` builds; wav decode + mix test passes with null output device

---

### Chunk 7 — input *(isolatable, slot anywhere after foundation)*

**Subsystems**: `input`  
**Depends on**: platform *(done — can run alongside Chunks 1–5)*  
**Legacy reference**: `engine/client/input/input.c`, `in_keys.c`, `in_joy.c`  
**Complexity note**: Abstract the SDL/Win32/touch event sources behind an interface so the subsystem is testable without a window.  
**ABI surfaces touched**: none  
**Deliverable**: `xash3dpp_input` builds; key/button event tests pass with mock event source

---

### Chunk 8 — physics (pm_shared)

**Subsystems**: `physics`  
**Depends on**: world, server (edict query interface)  
**Legacy reference**: `pm_shared/pm_move.c`, `pm_shared/pm_trace.c`, `engine/server/sv_pmove.c`, `engine/client/dll_int/cl_pmove.c`  
**Complexity note**: Client prediction and server authority must produce bit-identical results — determinism is the hardest constraint; float choice from Chunk 4 is locked in here.  
**ABI surfaces touched**: `pm_shared/` — **FROZEN** (shared client ↔ server)  
**Deliverable**: `PM_Move` runs identically on both paths; determinism regression test passes

---

### Chunk 9 — client

**Subsystems**: `client`, `demo` stub, `ui` stub  
**Depends on**: all of Chunks 1–8 + sound + input  
**Legacy reference**: `engine/client/cl_main.c`, `cl_frame.c`, `parse/cl_parse.c`, `dll_int/cl_game.c`  
**Complexity note**: Client DLL bridge (`cdll_int.h` / `cdll_exp.h`) plus prediction wiring are the largest surfaces; scope to connect → parse → predict → render-one-frame; defer VGUI/UI to Chunk 11.  
**ABI surfaces touched**: `engine/cdll_int.h`, `engine/cdll_exp.h` — **FROZEN Client DLL ABI**  
**Deliverable**: Client connects to local server, loads HL `cl_dlls/client.dll`, renders one frame without crash

---

### Chunk 10 — renderer

**Subsystems**: `renderer`  
**Depends on**: world, content, platform, client  
**Legacy reference**: `ref/gl/` or `ref/vk/` legacy backends as references for what must be reproduced  
**Complexity note**: The renderer ABI is fully internal — `ref_api.h` can be replaced with any shape — but GoldSrc visual output (BSP lightmaps, studio model rendering) must match.  
**ABI surfaces touched**: none frozen — internal; **Vulkan vs. GL vs. multi-backend decision needed before this chunk starts**  
**Deliverable**: GL stub renders world geometry of a HL map at playable framerate

---

### Chunk 11 — leaves (save, demo, ui)

**Subsystems**: `save`, `demo`, `ui`  
**Depends on**: server (save), client (demo, ui) — one at a time after respective parents  
**Legacy reference**: `sv_save.c`, `cl_demo.c`, `cl_gameui.c`  
**Complexity note**: Each is a leaf with no unimplemented dependents; scope individually.  
**ABI surfaces touched**: save format (internal, v2 opportunity); demo format (internal, breaking change OK)  
**Deliverable**: Save/load round-trip test; demo record/playback test; MainUI DLL loads

---

## Open Questions / Landmines

- **pm_shared float vs. fixed-point** — Must be decided before Chunk 4 ships. Fixed-point eliminates cross-platform FPU divergence (important for netplay) but requires a PM layer rewrite. Float is cheaper upfront but may require reproducible-FP compiler flags.

- **cmd/cvar: singleton vs. context object** — The legacy system is a global linked list. If the rewrite passes a context through the call stack (the cleaner path), every other subsystem's constructor signature changes. This decision dominates Chunk 1 and echoes through everything above it.

- **Wire compatibility with GoldSrc clients** — If the rewrite must interoperate with vanilla Half-Life clients during development, `net_chan.c` delta encoding and `sv_frame.c` entity packing must match the legacy protocol byte-for-byte. This locks many internal data-layout decisions that would otherwise be free.

- **Renderer backend choice** — `ref_api.h` is internal and free to redesign. Decision needed before Chunk 10: Vulkan-first (modern, mobile-hostile), GL-compat (broadest reach), or abstracted multi-backend (most work). Also needs to settle on desktop vs. mobile as the primary target.

- **`entvars_t` internal representation** — The server must present `entvars_t` at the exact ABI-specified layout when calling the game DLL. If Chunk 5 uses SoA or any non-literal struct storage internally, it needs a projection/copy step at every DLL call boundary. Decide early whether to store ABI-literal structs natively or maintain ABI views.

- **Dedicated-server milestone scope** — Chunks 1–5 can produce a working dedicated server. Is that the explicit first public milestone? If so, Chunks 6–7 (sound/input) can be deferred past Chunk 5 rather than slotted in earlier.

- **Legacy survey gaps** — There are no legacy-survey docs yet for `cmd_cvar`, `networking`, `world`, `content`, `sound`, `input`, `physics`, `ui`, or `save`. Each chunk above should start with a `/analyse-subsystem` pass to produce a boundary spec before writing any code.
