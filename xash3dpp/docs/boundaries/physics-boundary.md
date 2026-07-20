# physics — boundary spec

> Written 2026-07-20, as Chunk 11's entry-gate item 1. This spec is unusual:
> the `physics` **target does not exist yet** and `src/physics/` is an empty
> placeholder. The code it documents lives today in `src/server/physics/`
> (built into `xash3dpp_server`), and the shared kernel becomes its own target
> when the gather severance in §3 lands. This spec is also the **first use of
> the Role & parity section** — physics is the one subsystem whose shared
> obligation is load-bearing, so it is where that discipline is prototyped.
> Legacy reference: `pm_shared/pm_move.c` (the mover, game-DLL side),
> `engine/common/pm_trace.c` + `pm_surface.c` (the shared trace),
> `engine/client/dll_int/cl_pmove.c` + `engine/server/sv_pmove.c` (the gathers).

## 1. Scope / Responsibility

**The shared-deterministic player-movement trace kernel** — the collision and
contents queries that the movement integrator (`PM_Move`, which lives in the
game DLL across the frozen ABI) calls back into. Chunk 11 proves the neutral
seam with two synthetic role-shaped fixtures; Chunk 12 supplies the real client
prediction caller that must then run identically with server authority.

Concretely, one file: `pm_trace.cpp` — `pm_player_trace_ext`,
`pm_test_player_position`, `pm_trace_model`, `pm_trace_line[_ex]`, and the
`pm_point_contents` family, plus the per-physent hull/model selection under
them (brush hull select, studio hitbox path, the rotated-brush transform).

**Not in scope — and this is the naming trap the spec exists to defuse:**
`physics` does *not* mean "all physics." Two large neighbours share the domain
word and are deliberately elsewhere, both server-boundary's:

- **`SV_Physics` world simulation** (`src/server/physics/physics.cpp`, 1808
  lines) — the authoritative per-`MOVETYPE` dispatch for every *non-player*
  entity (pushers, grenades, monsters). It is **100% server-authoritative and
  never predicted**: the client receives these entities as snapshots and
  interpolates them. It carries no shared obligation and stays in the server.
- **The player-move gather + harness** (`pmove.cpp`, `run_cmd.cpp`,
  `init_client_move.cpp`) — the **server half** of a symmetric pair. These
  produce the neutral snapshot the shared kernel consumes; the client half
  (`CL_*`) is Chunk 12. The gather is role-specific by definition and stays
  with its role. See §Role & parity.

`movevars.cpp` is a shared *input* producer, not shared code — see §3. Also out
of Chunk 11: `ServerRuntime` narrowing, Q-18 arithmetic changes, lag
compensation/P5, physics-interface overrides, `SV_Physics`, movevars transport,
and unrelated pmove callback stubs.

## 2. Exposed surface (Interface)

Public header `xash3dpp/private/server/pm_trace.hpp` (relocates with the target
when it is created), namespace `xash::server`:

| Symbol | Provides |
|---|---|
| `PmTraceEnv` | the injected context (world, model resolver, hull bounds, cvars, pusher flag — and today one server leak, §3) |
| `pm_player_trace_ext` | sweep `[start,end]` against `ents[0..numents)` with the player hull; nearest impact |
| `pm_test_player_position` | is the player hull free at a point; returns the blocking physent |
| `pm_trace_model` | trace against one named physent's model |
| `pm_trace_line`, `pm_trace_line_ex` | line trace with `k_pm_*` selectors |
| `pm_point_contents`, `pm_true_point_contents`, `pm_point_contents_pmove` | contents at a point |

The mover itself (`PM_Move`) is **not** in this surface — it is game-DLL code
behind the frozen `pm_shared` ABI. This target owns the trace kernel the mover
calls, not the mover.

## 2a. Dependencies

`map_loader` (the edict-free trace kernel — `world_hull`, `hull_for_bsp`,
`TraceResult`, `WorldData`) and `utilities` (`Vec3`, matrix math) are PUBLIC.
`world` (the `IModelResolver` implementation surface), `content` (studio
hulls/bytes), `cmd_cvar` (the live `r_studiocache` read), and `core` (thread
assertion) are PRIVATE. The public header forward-declares `IModelResolver` and
never exposes `BrushModel` or `StudioHullPose` by value.

None of these depends on the server, so a `physics` target sits **below** the
server in the stack, linked by it (and, at Chunk 12, by the client) — the same
shape `world` has.

**The one dependency that must not exist:** the edict store. See §3.

## 2b. Owned state

**None.** Every entry point operates on injected context + caller-owned buffers;
some mutate the caller's `playermove_t` temporarily and the production model
resolver lazily fills caches. No file-scope mutable state; `pm_trace.cpp`
carries **zero** direct `ServerRuntime` references.
<!-- verify: grep-count(ServerRuntime, xash3dpp/src/server/physics/pm_trace.cpp) == 0 -->

## 3. Invariants and Quirks

- **The neutral seam, and its single leak.** `PmTraceEnv` (`pm_trace.hpp:53`)
  has six fields; five are role-neutral (`world`, `models`, `player_bounds`,
  `pusher_ext`, `cvars`). The sixth — `arena` (`EdictArena *`, the server edict
  store) — is the **only** role-owned reach in all 800 lines, used in exactly
  one place: `physent_modelindex` at `pm_trace.cpp:96`, which resolves
  `pe->info → edict → v.modelindex`. **All the shared kernel wants from the
  server is one `int`** (a model index), which it then feeds to the neutral
  `models` resolver.
- **How the leak is severed (Chunk 11 mechanical work, not a decision).**
  Legacy never has this reach: `SV_CopyEdictToPhysEnt` resolves the model *at
  gather time* and stores it in the physent. Our OQ-2 (2026-07-19) chose to
  keep the physent free of a server `model_t *` and re-resolve at trace time —
  which is what dragged the store into the shared code. The fix restores the
  legacy shape in neutral form: **resolve the model index at gather time into
  neutral storage** (a per-physent `int` filled by each role's gather — server
  from `edict→modelindex`, client from `cl_entity→modelindex`), so the kernel
  reads the int and the `arena` field leaves `PmTraceEnv`. No virtual call in
  the trace loop, no reopening Q-20, no touching the byte-exact arithmetic.
- **`PM_TraceModel` resolves at the role adapter.** Legacy
  `engine/common/pm_trace.c:743-788` ignores `physent_t::info` and dereferences
  the supplied `pe->model`; this rewrite deliberately keeps that opaque ABI
  pointer null. The server callback therefore validates `pe->info`, resolves
  the model through the server arena, and passes one explicit integer to
  `pm_trace_model`. This preserves copied/non-list valid physents without
  putting `EdictArena` in the shared kernel. Chunk 12 supplies the analogous
  client adapter.
- **Parity fence is determinism, not one ULP block here.** The ULP-exact
  rotated-brush kernel this code reaches is HB-2 fenced at
  `world/clip.cpp:245-281`, reached indirectly via the map_loader trace. The
  *live* divergence risk in the movement path is the **RNG**:
  `init_client_move.cpp` installs a non-parity xorshift in the pmove RNG slots
  (`XASH3DPP-STUB`) where legacy wires the single shared `COM_RandomLong` —
  Chunk 11 gate item 3.
- **`movevars_t` is a shared input, not shared code.** The physics subset
  (gravity, friction, accelerate, stopspeed, maxvelocity, stepsize, …) must be
  bit-identical on both sides. The server *produces* it from `sv_*` cvars
  (`movevars.cpp`) and is authoritative; the client *receives* it over the
  delta channel and is a passive receiver. Same struct, opposite roles — which
  is exactly why it is a server-boundary producer, not a member of this target.
- **Studio hull gating** reads `r_studiocache` live per call (`PmTraceEnv.cvars`)
  and follows `PM_AllowHitBoxTrace` (flag or `usehull==2`) — no `sv_clienttrace`
  gate, unlike the server world trace (OQ-2).

## 6. Threading

Main-thread only today, enforced at the production trace entry points. The
kernel owns no state, but its injected production dependencies are not an
immutable snapshot: `r_studiocache` is read live and `IModelResolver` lazily
loads studio bytes and mutates its pose cache. Chunk 12 prediction remains on
Main and uses its own `PmTraceEnv`/physent snapshot. Off-main use requires an
immutable cvar/resolver snapshot and exclusive caller buffers; that is a later
design event, not an assertion removal. The single-`pmove_t`,
players-sequential contract belongs to each role harness.

## 9. As-built reconciliation (2026-07-20)

- The subsystem's shared surface is **one file, `pm_trace.cpp`, ~800 lines**,
  living in `src/server/physics/` and built into `xash3dpp_server`. The other
  five TUs there are server-boundary's (world-sim, gather, harness, input
  producer) — see the measured split in `src/physics/CMakeLists.txt`.
- **Extraction is one `int` away, not a design decision.** The earlier gate
  framing (a virtual-call interface vs reopening Q-20) was a false trichotomy;
  the reads on 2026-07-20 found the reach is a single site wanting a single
  model index, severable at the gather in the legacy-precedented way above.
- The `physics` target is created when that severance lands. Dependencies will
  be `map_loader` and `utilities` PUBLIC; `world`, `content`, `cmd_cvar`, and
  `core` PRIVATE.
- **Client-side caveat.** "The client gather can fill the same neutral int" is
  reasoned from legacy (`CL_CopyEntityToPhysEnt`), not from our code — the
  client half is Chunk 12 and does not exist yet. It is the one part of the
  severance that wants confirmation when that gather is written.

## Role & parity

*(Prototype of the discipline's required section. For a server-authoritative or
client-only subsystem this collapses to one line — "Role: server-authoritative;
no cross-role parity obligation." Physics is the case that exercises the full
form.)*

- **Role:** **shared-deterministic.** Server authority (`SV_RunCmd`,
  `src/server/physics/run_cmd.cpp`) is live. Chunk 11 supplies a synthetic
  client-role fixture over an independent neutral snapshot; Chunk 12 adds the
  real `CL_RunCmd` caller and the eventual per-command comparison.
- **Counterpart path:** client prediction, `src/client/` (Chunk 12, not yet
  written). Server path is live today.
- **Neutral seam:** each role gathers its own entities into `physent_t[]`
  (server `SV_CopyEdictToPhysEnt`; client `CL_CopyEntityToPhysEnt`) and hands
  the kernel a role-neutral `PmTraceEnv` plus aligned, non-ABI model-index
  sidecars. The single-model ABI callback resolves at its role adapter (§3).
- **Parity fence:** determinism. The ULP-exact block is HB-2 at
  `world/clip.cpp:245-281` (reached indirectly); the open divergence risk is
  the RNG stub (gate item 3); `movevars_t` is a shared input both sides must
  agree on bit-for-bit.
- **Annotation:** when the target is created, `pm_trace.cpp` (and any TU that
  joins it) carries the `ROLE: shared-deterministic` marker so the obligation
  is legible at the edit site, not only here.

## Extension axes (Q-21)

Evaluated against `docs/design/extension-goals.md`.

| Goal / primitive | Applies? | Required seam or door — door-keep verdict |
|------------------|----------|-------------------------------------------|
| **G-1** in-engine MCP service | Consumer via P-4 | Trace/contents queries can run off a deliberately published immutable context; the live production resolver is not that snapshot. |
| **G-2** Game ABI v2 | Indirect | The mover is game-DLL ABI (`pm_shared`, frozen); a v2 flavour swaps behind that boundary, not here. |
| **G-3** dedicated debug thread | **Door recorded, not open today** | A reader needs its own physent buffers plus immutable cvar/model snapshots. The live resolver mutates caches, so current entry points stay Main-only. |
| **G-4** expanded in-game debugging | Consumer via P-4 | Movement/trace visualisation consumes the same entry points. Nothing owed. |
| **G-5** scripting runtime | Nothing owed now | Script movement queries go through P-2/P-4; determinism + parity precedence cap everything. |
| **P-1** main-thread inbox | Not applicable | Called, does not pump. |
| **P-2** published-snapshot reads | **Shape reserved** | Caller buffers are neutral, but live cvar/resolver dependencies must be snapshotted before off-main use. |
| **P-3** context-first, no new file-scope state | **✅ met — zero owned state** | No file-scope mutable state, no `ServerRuntime`. |
| **P-4** typed introspection | Conforms after severance | Hot-list model identity is an aligned typed sidecar; the role-owned `PM_TraceModel` adapter resolves its single explicit model index. |
| **P-5** narrowest-state signatures | **Debt closes at extraction** | The kernel takes `PmTraceEnv`, paired views, and an explicit single-model index—never `ServerRuntime` or `EdictArena`. Broad server signature narrowing is out of scope. |
| **P-6** services are satellites | Yes — held | Sits below the server; links toward no service. |
| **P-7** pool-owned RAII lifecycle | N/A | Owns no allocations; operates on caller buffers and frozen ABI PODs. |
| **P-8** annotation discipline | Inherited | Counts re-derive from `census physics` once the target exists; do not hand-count from the server row. |
