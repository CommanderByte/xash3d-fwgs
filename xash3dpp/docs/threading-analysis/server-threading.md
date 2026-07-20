# server — Threading Analysis (Chunk 6)

> Boundary spec: `docs/boundaries/server-boundary.md`

> **Refreshed 2026-07-06 (as-built pass).** Re-scanned against the shipped
> `xash3dpp/src/server/` tree (30 TUs, Chunk 6 Complete). The enforcement
> count grew: **92 `assert_thread_role(ThreadRole::Main)` calls across 26
> source files** (was 86/26 at S11 — the S9 client/messaging and world
> completion added sites; the 26-file spread is unchanged). `compliance_scan.py
> server` is **clean**. The structure, ownership model, and every
> Safe-by-contract / Race-static-buf / caller-contract finding below hold
> as-built and are confirmed against current code — the update is the count and
> the per-file distribution table. Nothing is deleted.

*2026-07-05 (S11). Decision refs: OQ-9 (server is main-thread-only),
`server-boundary.md`; `core::assert_thread_role`/`ThreadRole::Main` is the
enforcement primitive. Companion docs: `map_loader-threading.md` (Q-6),
`networking-threading.md`.*

## Ownership model

**Single-threaded, main-thread-only — enforced, not merely assumed.** Every
public entry point that mutates server state opens with
`assert_thread_role(ThreadRole::Main)`: the lifecycle (`load_progs`/
`unload_progs`/`spawn_server`/`activate_server`/`deactivate_server`), the frame
loop (`host_server_frame`/`sv_physics`/`sv_run_game_frame`/`sv_update_movevars`),
the pmove bridge (`sv_setup_pmove`/`sv_finish_pmove`/`sv_run_cmd` + the two
usehull trace mutators), world interaction (`link_edict`/`move`/`set_abs_box`/
lightstyles), and the client/messaging surface (`execute_client_message`/
`drop_client`/`check_timeouts`/`read_packets`/`send_client_messages`). **86
assertions across 26 source files** at the time of writing.

The ~159 `enginefuncs_t` ABI slots and the ~30 `playermove_t` callbacks are the
one surface that does *not* self-assert: they are plain C function pointers the
game DLL calls **synchronously from inside an already-asserted engine call**
(e.g. `pfnPM_Move` runs inside `sv_run_cmd`, `pfnTouch` inside `sv_impact`,
`pfnKeyValue` inside `spawn_entities`). They therefore inherit the Main-thread
context of their caller — the assertion at the engine entry point covers them
transitively. No server code is reachable from a signal handler or an I/O
thread (the host layer owns those; the server never registers one), so **Step 5
signal-handler safety is N/A**.

There is no "init phase then read-only" transition: the server is mutable for
its whole active lifetime, but only ever from one thread.

**Superseded 2026-07-06 (count only):** the "**86 assertions across 26 source
files**" figure above is now **92 across 26 files** as-built. The per-file
distribution (verified 2026-07-06):

| Slice | Files with asserts (count) |
|-------|----------------------------|
| `abi/` | `engine_table.cpp` (5), `string_pool.cpp` (5), `edict_arena.cpp` (4), `game_dll.cpp` (2) |
| `lifecycle/` | `game_host.cpp` (4), `precache.cpp` (4), `entity_parse.cpp` (3), `spawn.cpp` (3), `world_hooks.cpp` (1) |
| `clients/` | `snapshot.cpp` (14), `client_state.cpp` (8), `filter.cpp` (6), `messages.cpp` (6), `net_io.cpp` (2), `log.cpp` (1), `query.cpp` (1) |
| `physics/` | `physics.cpp` (4), `pm_trace.cpp` (2), `pmove.cpp` (2), `init_client_move.cpp` (1), `movevars.cpp` (1), `run_cmd.cpp` (1) |
| `world/` | `light.cpp` (3), `links.cpp` (2), `clip.cpp` (1) |
| facade | `server.cpp` (6) |

The ABI-slot / `playermove_t`-callback transitive-coverage argument is
unchanged: `snapshot.cpp`'s 14 sites bracket every public snapshot-pipeline
entry, and the `abi/` slots remain the deliberately unasserted C surface that
inherits the caller's Main context.

## Safe items

All shared mutable state is **Safe-by-contract** — correct precisely because the
OQ-9 single-thread rule holds; none would be safe under concurrent entry:

> *Reconciled 2026-07-20:* `g_bridge` retains its explicit
> `compliance-allow(mutable-global, di-global-ref)` Q-20 carve-out, and the ABI
> static return buffers remain Safe-by-contract. Chunk 11 removed both former
> RNG statics; the identical enginefuncs/pmove callbacks now draw from the sole
> `EngineContext`-owned `LegacyRandom` on Main.

- **`ServerRuntime rt`** (the `sv`/`svs`/`svgame` aggregate) — heap-owned by the
  `Server` object (one per process), reached only through Main-thread entry
  points. The bulk of server state; not static, not shared across threads.
- **`g_bridge`** (`engine_table.cpp`, `EngineBridge*`) — the file-scope global
  the ABI slots reach for state. Written only by `install_engine_bridge`
  (at `load_progs`) and nulled at `unload_progs`; read by every slot in between.
  Effectively Safe-RO between install and detach. *Process-global*: it assumes a
  single live server (see caller contracts).
- **ABI static return buffers** — the frozen slot contract hands back a
  `const char*`/`float*` that only needs to stay valid for the duration of the
  call, so the shims return pointers into file-static scratch: e.g.
  `pfn_info_key_value` / `pfn_get_info_key_buffer` (`s_value[256]`, `s_empty`),
  `Info_ValueForKey` in `init_client_move.cpp` (`s_value[256]`), and the several
  pfn slots that return a static `""`. Classic *Race-static-buf* shape, made
  safe only by the single-thread contract (a second concurrent caller would
  clobber the first's result before it is consumed).
- **Canonical random stream** (`EngineContext::random`) — enginefuncs and
  pmove use identical no-capture callback addresses. The instance is mutated
  per draw and remains Main-thread-only in production; no server RNG global
  remains.

## Hazards

None **under the OQ-9 contract.** The table records the items that *would* race
if that contract were broken (i.e. the caller contracts the module relies on but
does not itself synchronize):

| Symbol | File | Class | Notes |
|--------|------|-------|-------|
| ABI static return buffers (`s_value`, `s_empty`, static `""`) | `engine_table.cpp`, `init_client_move.cpp` | Race-static-buf (contained by OQ-9) | Safe only because all ABI slots run on Main. Off-thread entry would corrupt in-flight results. |
| `g_bridge` | `engine_table.cpp` | Race-shared (contained by OQ-9) | Install/detach-only during operation; process-global, so it also assumes a single live server. |
| canonical `EngineContext` random stream | host-owned; server callbacks in `engine_table.cpp` / `init_client_move.cpp` | Safe-by-contract | Per-call mutation; production calls are Main-only and one process stream is intentional. |

## Required caller contracts

1. **All server entry points are called from the Main thread only.** This is the
   OQ-9 contract; the `assert_thread_role(Main)` calls fail fast (debug) if a
   future integration violates it. A threaded content loader (Chunk 7) or a
   listen-server client path (Chunk 12) that wants to touch server state must
   marshal onto the Main thread — it must not call in directly.
2. **One live `Server` per process.** `g_bridge` is a single process-global; a
   second concurrent server would share it. Matches the legacy single-`svgame`
   assumption.
3. **Do not retain an ABI static-buffer pointer** (`pfnInfoKeyValue` result,
   etc.) across another ABI call — the next call reuses the same buffer. This is
   the legacy contract, unchanged.

## Recommendations

Ordered; none urgent — the subsystem is correct as-is under OQ-9:

1. **Keep the enforcement uniform.** When new mutating entry points land
   (S9 client/messaging completion, sv_move.c locomotion), open each with
   `assert_thread_role(ThreadRole::Main)` — the ABI-slot transitive-coverage
   argument only holds while every engine entry point asserts.
2. **When the RNG-unification stub is ported** (the tracked `COM_RandomLong`
   idtech parity port), the single shared stream stays Main-thread-only — no new
   hazard, but update the Safe-items note here.
3. **If Chunk 12 introduces a real off-Main caller** (listen-server client), the
   Race-static-buf and `g_bridge` rows graduate from "contained" to real: the
   fix is a Main-thread marshal at the boundary, not per-buffer `thread_local`
   (which would not fix `g_bridge` and would mask the contract violation).
