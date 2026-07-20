# Threading and Invariants

> **Cross-cutting concern** — spans every source file under `src/server/`\
> **Namespace**: `xash::server`\
> **Full analysis**:
> [docs/threading-analysis/server-threading.md](../../threading-analysis/server-threading.md)

## Overview

The server has no synchronisation primitives because it needs none: it is
**single-threaded, main-thread-only**, and that contract is *enforced* rather
than merely assumed. This page summarises the two cross-cutting rules that shape
every concept — the OQ-9 threading posture and the Q-20 raw-entvars-access
confinement — and collects the module-wide invariants. It is a summary; the
authoritative threading document is the companion analysis linked above.

______________________________________________________________________

## OQ-9 — main-thread-only, enforced

Every public entry point that mutates server state opens with
`assert_thread_role(ThreadRole::Main)` — **92 assertions across 26 source
files** at the time of writing (the 6B S9a conformance pass added the
frame/runtime mutators `set_server_state`, `run_think`, `update_base_velocity`,
`LightStyles::run_frame`, and the two bridge-state ABI shims
`set_external_cvar_string` / `set_min_max_size`). Two mutator-shaped helpers
intentionally do **not** assert and carry a `compliance-allow(thread-assert)`
marker: `set_axis` (a stateless writer over a caller-owned `Vec3` — thread
affinity belongs to the caller) and the `set_min_max_size` forward declaration
(the assert lives at its definition). The covered surfaces are the lifecycle
(`load_progs`/`unload_progs`/`spawn_server`/`activate_server`/`deactivate_server`),
the frame loop (`host_server_frame`/`sv_physics`/`sv_run_game_frame`/
`sv_update_movevars`), the pmove bridge (`sv_setup_pmove`/`sv_finish_pmove`/
`sv_run_cmd` + the two usehull trace mutators), world interaction (`link_edict`/
`move`/`set_abs_box`/the lightstyle mutators), and the client/messaging surface
(`execute_client_message`/`drop_client`/`check_timeouts`/`read_packets`/
`send_client_messages`).

### The ABI-slot transitive-coverage argument

The ~159 `enginefuncs_t` slots and ~30 `playermove_t` callbacks are the one
surface that does **not** self-assert. They are plain C function pointers the
game DLL calls **synchronously from inside an already-asserted engine call**
(`pfnPM_Move` inside `sv_run_cmd`, `pfnTouch` inside `sv_impact`, `pfnKeyValue`
inside `spawn_entities`), so they inherit the Main-thread context of their caller
— the assertion at the engine entry point covers them transitively. No server
code is reachable from a signal handler or an I/O thread (the host layer owns
those; the server never registers one).

This argument only holds while **every** engine entry point asserts. When new
mutating entry points land (S9 completion, `sv_move.c` locomotion), each must
open with the same assert.

### Safe-by-contract state

All shared mutable state is correct *precisely because* the single-thread rule
holds; none would be safe under concurrent entry. The items that would race if
OQ-9 were broken:

| Symbol | File | Class | Notes |
|--------|------|-------|-------|
| `ServerRuntime rt` | (aggregate) | Safe-by-contract | Heap-owned by the one `Server`; reached only through Main-thread entry points |
| `g_bridge` | `abi/engine_table.cpp` | Race-shared (contained) | Install/detach only during operation; process-global ⇒ also assumes a single live server. The one sanctioned engine-state singleton: the ~30 context-free pfn shims have fixed ABI signatures with no userdata slot, so they reach engine state through this file-scope pointer (Q-20 ABI-slot carve-out). Adjudicated at its definition with a `compliance-allow(mutable-global, di-global-ref)` marker |
| ABI static return buffers (`s_value`, `s_empty`, static `""`) | `engine_table.cpp`, `init_client_move.cpp` | Race-static-buf (contained) | Frozen slot contract; a second concurrent caller would clobber the in-flight result |
| canonical random callbacks | `engine_table.cpp`, `init_client_move.cpp` | Main-only by production contract | Both surfaces hold identical addresses and draw from the `EngineContext`-owned `LegacyRandom`; server owns no RNG static |

### Required caller contracts

1. **All server entry points are called from the Main thread only.** A future
   threaded content loader (Chunk 7) or listen-server client path (Chunk 12) must
   marshal onto Main — it must not call in directly. The fix, if a real off-Main
   caller ever appears, is a Main-thread marshal at the boundary, **not**
   per-buffer `thread_local` (which would not fix `g_bridge` and would mask the
   contract violation).
2. **One live `Server` per process.** `g_bridge` is a single process-global.
3. **Do not retain an ABI static-buffer pointer** across another ABI call — the
   next call reuses the same buffer (the legacy contract, unchanged).

`Mod_CalcPHS` may parallelize internally at world-load time only (inside
`map_loader`, not the server) — that is the sole exception and it is off the
server's surface.

______________________________________________________________________

## Q-20 — raw-entvars-access confinement

The ABI-exact `edict_t` array is the single authoritative entity store (no
projection). Engine-**internal** server code reads/writes entvars through the
zero-cost `EntityView` facade, never via `->v.` directly. Raw `edict->v.` access
is **confined** to three sites, enforced by a compliance-scan rule:

- `src/server/game/` — the ABI shim (the store itself + the accessor seam);
- the **pmove bridge** (`physics/pmove.cpp`, `init_client_move.cpp`,
  `run_cmd.cpp`) — the state copy is field-for-field with
  legacy, so it reads raw;
- the Chunk 8 save serializer (future).

`EntityView` accessors are value-semantic (copies, not references into the
store), so a future ABI flavor / handleization can swap the backing arena behind
the seam without touching callers. Entity cross-links stay `edict_t*` — the
pointer identity **is** the ABI handle.

______________________________________________________________________

## Frozen-ABI invariants

- `enginefuncs_t` (159 slots), `DLL_FUNCTIONS` (50), `NEW_DLL_FUNCTIONS` (5),
  `edict_t`, `entvars_t`, `globalvars_t`, `playermove_t`, `physent_t`, and the
  wire structs are byte-frozen — vendored verbatim, never redeclared.
  Layout-parity tests include the real legacy headers in a sealed namespace.
- The edict array base never moves (game DLLs do byte-offset arithmetic against
  it). Slot 0 = world; 1..maxclients = clients.
- `build_engine_table` hands the DLL a **copy** of the table, not the master.
- Connect accepts protocol **49 only**; `PROTOCOL_VERSION` = 49.
- Private-data size is rounded up to the next 16-byte multiple (the Poke646
  over-write workaround).

## Lifecycle-ordering invariants

- The game DLL is loaded once and survives map changes.
- Spawn epoch is `sv.time = 1.0`.
- Spawn → activate order: world load → submodel precache → entity parse →
  `ServerActivate` → string pool to dynamic mode → settle physics (SP 2 / MP 8)
  → **baselines after settling** → `ss_active`.
- `svs.initialized` is set early in `spawn_server` (before world load), cleared
  only at shutdown.
- All pool-allocated state (snapshot rings, string pool, edict arena, external
  cvar strings) is freed **before** `game_pool` destruction (the pool asserts on
  leaks).

## Behavioural-parity invariants

The Quake-lineage constants and bugs are contract, not accidents — ClipVelocity
snap-to-zero at ±1.0, whole-vector maxvelocity clamp, 4-bump `FlyMove`, the
pusher `ltime` ±3600 wrap, the `1/(sv_fps-0.01)` fudge, stale-field edict reuse,
the string-pool overflow wrap, the `SV_Move` fraction-compose (entity × world),
the `SOLID_NOT` water-brush `skin < CONTENTS_EMPTY` test, the zero-physics-frames
early return, and the known-broken security bits (`SV_CheckRate` no-op, inverted
master-info `password`, rcon plain-strcmp). See the per-concept pages and the
boundary spec's "Quirks and invariants" for the full catalogue.

## Milestone-trim invariants (OQ-8)

The subsystem is feature-complete for the dedicated milestone; every trim carries
an inline `XASH3DPP-STUB(<tag>)` / `TODO(<tag>)` marker naming its owning future
chunk (149 markers at the S10 gate — `stub_scan server` is the live inventory).
The grouped backlog is the "Chunk 6 (server) — deferred stub inventory" section
of `docs/implementation-plan.md`: ~90 for S9 completion, ~12 for Chunk 7 content,
~6 for Chunk 8 save, ~4 for Chunk 9 sound, 2 for Chunk 12 client, ~15 cross-cutting
engine-integration residue, plus P5 pmove lag-compensation. **None block the S15
`hl.dll` smoke test** — they are the post-milestone completion backlog, not
missing or broken code.

## See also

- [docs/threading-analysis/server-threading.md](../../threading-analysis/server-threading.md)
  — the authoritative analysis (ownership model, hazard table, recommendations)
- [abi-bridge.md](./abi-bridge.md) — `g_bridge`, the static return buffers, the
  `EntityView` seam
- `docs/boundaries/server-boundary.md` — the frozen-ABI contract + the Q-20/OQ-8/
  OQ-9 decision records
- `docs/design/decisions-architecture.md` — Q-20 (EDICT_STORE), Q-2 (no globals),
  Q-5 (error model)
