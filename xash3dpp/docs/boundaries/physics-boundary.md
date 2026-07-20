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
game DLL across the frozen ABI) calls back into, and which **server authority
and client prediction must run identically**.

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

`movevars.cpp` is a shared *input* producer, not shared code — see §3.

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
`TraceResult`, `WorldData`), `content` (studio hulls/bytes via `IModelResolver`),
`utilities` (`Vec3`, matrix math), `cmd_cvar` (the live `r_studiocache` read at
trace time) — all PUBLIC, each appears in the signatures. `core` is PRIVATE.

None of these depends on the server, so a `physics` target sits **below** the
server in the stack, linked by it (and, at Chunk 12, by the client) — the same
shape `world` has.

**The one dependency that must not exist:** the edict store. See §3.

## 2b. Owned state

**None.** Every entry point is a pure function of `const PmTraceEnv &` +
`playermove_t &` (caller-owned) + the ray geometry. No file-scope mutable
state; `pm_trace.cpp` carries **zero** direct `ServerRuntime` references.
<!-- verify: grep-count(ServerRuntime, xash3dpp/src/server/physics/pm_trace.cpp) == 0 -->

## 3. Invariants and Quirks

- **The neutral seam, and its single leak.** `PmTraceEnv` (`pm_trace.hpp:52`)
  has six fields; five are role-neutral (`world`, `models`, `player_bounds`,
  `pusher_ext`, `cvars`). The sixth — `arena` (`EdictArena *`, the server edict
  store) — is the **only** role-owned reach in all 800 lines, used in exactly
  one place: `physent_modelindex` at `pm_trace.cpp:94`, which resolves
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
- **Parity fence is determinism, not one ULP block here.** The ULP-exact
  rotated-brush kernel this code reaches is HB-2 fenced at
  `world/clip.cpp:237-273`, reached indirectly via the map_loader trace. The
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

Main-thread only today, inherited from the server (server-boundary OQ-9).
Nothing here is thread-affine: the kernel is a pure function over an injected
context and caller buffers. Chunk 12 client prediction runs it on its own
`PmTraceEnv` over its own `physent_t[]` snapshot — a second caller, not a lock.
The single-`pmove_t`, players-sequential contract is the *harness's* (server /
client), not this kernel's.

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
  be `world`, `map_loader`, `content`, `utilities`, `cmd_cvar` PUBLIC, `core`
  PRIVATE.
- **Client-side caveat.** "The client gather can fill the same neutral int" is
  reasoned from legacy (`CL_CopyEntityToPhysEnt`), not from our code — the
  client half is Chunk 12 and does not exist yet. It is the one part of the
  severance that wants confirmation when that gather is written.

## Role & parity

*(Prototype of the discipline's required section. For a server-authoritative or
client-only subsystem this collapses to one line — "Role: server-authoritative;
no cross-role parity obligation." Physics is the case that exercises the full
form.)*

- **Role:** **shared-deterministic.** Both server authority (`SV_RunCmd`,
  `src/server/physics/run_cmd.cpp`) and client prediction (`CL_RunCmd`, Chunk
  12) run this trace kernel over the same neutral snapshot; their results are
  **compared every frame**, so a divergence is a prediction error (visible
  rubber-banding), not merely a bug.
- **Counterpart path:** client prediction, `src/client/` (Chunk 12, not yet
  written). Server path is live today.
- **Neutral seam:** each role gathers its own entities into `physent_t[]`
  (server `SV_CopyEdictToPhysEnt`; client `CL_CopyEntityToPhysEnt`) and hands
  the kernel a role-neutral `PmTraceEnv`. The kernel is role-blind — **except
  the one `arena` leak in §3, which is the whole of the remaining debt.**
- **Parity fence:** determinism. The ULP-exact block is HB-2 at
  `world/clip.cpp:237-273` (reached indirectly); the open divergence risk is
  the RNG stub (gate item 3); `movevars_t` is a shared input both sides must
  agree on bit-for-bit.
- **Annotation:** when the target is created, `pm_trace.cpp` (and any TU that
  joins it) carries the `ROLE: shared-deterministic` marker so the obligation
  is legible at the edit site, not only here.

## Extension axes (Q-21)

Evaluated against `docs/design/extension-goals.md`.

| Goal / primitive | Applies? | Required seam or door — door-keep verdict |
|------------------|----------|-------------------------------------------|
| **G-1** in-engine MCP service | Consumer via P-4 | Trace/contents queries are pure functions of an injected context; a service can run them off a published snapshot. Nothing owed. |
| **G-2** Game ABI v2 | Indirect | The mover is game-DLL ABI (`pm_shared`, frozen); a v2 flavour swaps behind that boundary, not here. |
| **G-3** dedicated debug thread | **Door open — needs a caller-owned context** | Nothing here is thread-affine; a reader thread needs its own `PmTraceEnv` + physent snapshot. Same shape as world G-3. |
| **G-4** expanded in-game debugging | Consumer via P-4 | Movement/trace visualisation consumes the same entry points. Nothing owed. |
| **G-5** scripting runtime | Nothing owed now | Script movement queries go through P-2/P-4; determinism + parity precedence cap everything. |
| **P-1** main-thread inbox | Not applicable | Called, does not pump. |
| **P-2** published-snapshot reads | **Open by construction** | Every entry point is a pure function of the neutral env + caller buffers. |
| **P-3** context-first, no new file-scope state | **✅ met — zero owned state** | No file-scope mutable state, no `ServerRuntime`. |
| **P-4** typed introspection | Conforms | Physent field access is through the neutral `physent_t`; edict fields are not reached — save the one `arena` leak (§3), which the gather severance closes. |
| **P-5** narrowest-state signatures | **Debt — the one `arena` leak** | The kernel takes `PmTraceEnv`, not a runtime — but `PmTraceEnv.arena` is the server store. The gather-time-modelindex fix (§3) removes it; until then this is the recorded door-debt. |
| **P-6** services are satellites | Yes — held | Sits below the server; links toward no service. |
| **P-7** pool-owned RAII lifecycle | N/A | Owns no allocations; operates on caller buffers and frozen ABI PODs. |
| **P-8** annotation discipline | Inherited | Counts re-derive from `census physics` once the target exists; do not hand-count from the server row. |
