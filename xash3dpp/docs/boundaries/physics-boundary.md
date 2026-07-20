# physics — boundary spec

> Written 2026-07-20 for Chunk 11 and reconciled after extraction the same day.
> `xash3dpp_physics` now owns the shared player-move trace kernel. Server gather
> and ABI adaptation remain role-owned; Chunk 12 supplies the production client
> gather and prediction consumer. Legacy reference: `pm_shared/pm_move.c`,
> `engine/common/pm_trace.c`, `engine/client/dll_int/cl_pmove.c`, and
> `engine/server/sv_pmove.c`.

## 1. Scope / Responsibility

Physics owns the **shared-deterministic player-movement trace kernel**: the
collision and contents callbacks used by the game DLL's frozen `PM_Move` ABI.
The implementation is `src/physics/pm_trace.cpp`; its public surface is
`include/xash3dpp/physics/pm_trace.hpp`.

The target contains `pm_player_trace_ext`, `pm_test_player_position`,
`pm_trace_model`, `pm_trace_line[_ex]`, `pm_point_contents` and related hull,
model-selection, and stuck-touch operations. It does not own `PM_Move` itself.

The following similarly named work remains outside this boundary:

- `SV_Physics` and non-player entity simulation remain server-authoritative.
- The server gather/harness (`pmove.cpp`, `run_cmd.cpp`,
  `init_client_move.cpp`) remains in `xash3dpp_server`; the client equivalent
  belongs to Chunk 12.
- The server produces `movevars_t`; the client receives it as shared-format
  input. Transport and reception are Chunk 12 work.
- ServerRuntime narrowing, Q-18 arithmetic changes, lag compensation/P5,
  physics-interface overrides, command splitting, and unrelated pmove callback
  stubs are not part of Chunk 11.

The generic subsystem scaffold did not apply: this chunk extracted existing
shared code and added its parity witness rather than constructing a new service.

## 2. Exposed surface (Interface)

Public header `xash3dpp/physics/pm_trace.hpp`, namespace `xash::physics`:

| Symbol | Provides |
|---|---|
| `PmTraceModelIndices` | role-owned model-index snapshots aligned with the frozen `physents`, `visents`, and `moveents` arrays |
| `PmPhysentView` | paired entity/model-index spans for one valid list prefix |
| `PmTraceEnv` | injected world, model resolver, aligned sidecars, hull bounds, live-cvar seam, and pusher flag |
| `pm_player_trace_ext` | player-hull sweep over a paired physent view |
| `pm_test_player_position` | point-in-solid test over the active physent list |
| `pm_trace_model` | single-physent sweep with an explicit role-resolved model index |
| `pm_trace_line[_ex]` | line traces over the physent or visent list |
| `pm_*point_contents*` | world and water/current contents queries |

The public header uses `world::IModelResolver` only as an incomplete pointer
type. `BrushModel` and `StudioHullPose` are implementation details. A standalone
compile-only consumer includes this header without any world header.

## 2a. Dependencies

`map_loader` and `utilities` are PUBLIC. `world`, `content`, `cmd_cvar`, and
`core` are PRIVATE. The required target direction is `server → physics`; there
is no physics-to-server edge. Static-library implementation dependencies may
appear as CMake `$<LINK_ONLY:...>` entries, but `world` must never be a bare
public interface edge; the physics CMake file enforces that distinction.

## 2b. Owned state

**None.** Entry points operate on injected context and caller-owned frozen ABI
buffers. The model-index sidecars belong to each role, not to physics.
`pm_trace.cpp` carries no `ServerRuntime`, `EdictArena`, or private server
include. <!-- verify: grep-count(ServerRuntime|EdictArena|xash3dpp/private/server, xash3dpp/src/physics/**/*.cpp) == 0 -->

## 3. Invariants and Quirks

- **Aligned snapshots are the hot-list identity seam.** The server writes a
  model index at the same successful append point as each `physent_t`, for all
  three lists. Only the corresponding `num*` prefix is valid. A gathered list
  is therefore stable if an edict's live model index changes later.
- **`PM_TraceModel` is the deliberate adapter exception.** Legacy follows the
  supplied physent's model pointer, whereas the rewrite keeps that opaque ABI
  pointer null. The server ABI adapter validates `pe->info`, resolves it through
  its arena, and passes the resulting integer to `pm_trace_model`. The shared
  kernel does not scan sidecar addresses, so copied physents and valid entities
  outside the active lists retain the rewrite's established behavior. Invalid
  role identifiers remain no-hit. Chunk 12 owes the client adapter.
- **The RNG reference is compiled legacy code, not a handwritten sequence.**
  `core::LegacyRandom` is independently compared with a C oracle whose routine
  body is byte-checked against `engine/common/common.c:54-153`. The sole
  production instance lives in `EngineContext`; enginefuncs and pmove receive
  identical callback addresses and draw from it sequentially on Main.
- **Sound's RNG claim is intentionally narrower.** DSP receives the canonical
  random-long callback and has no private generator. Tests prove injection and
  current topology dormancy, not legacy-equivalent sound/pmove scheduling.
- **Shared predicates preserve exact expressions.** `vector_is_null`, strict
  `bounds_intersect`, and `check_angles` live in `world/trace.hpp`; lower-tier
  representation-specific variants remain local.
- **Rotated traces are identity-only in the Chunk 11 role witness.** Q-18 owns
  any legacy ULP change. This extraction changes no trace arithmetic, filtering,
  movetype selection, or callback policy.
- **`movevars_t` is shared input, not shared code.** Chunk 11 injects identical
  values directly into its two fixtures and makes no transport claim.

## 6. Threading

Production tracing is Main-only. Each role owns one sequential `pmove_t` and
reuses it player by player. Physics owns no stream or global state, but the
production resolver lazily fills studio caches and the kernel reads cvars live;
off-main use would require immutable resolver and cvar snapshots plus exclusive
caller buffers.

Canonical random callbacks are valid after `EngineContext` publication and
are Main-only in production. Calls without a published context return the
requested lower bound and consume no draw.

## 9. As-built reconciliation (2026-07-20)

- `xash3dpp_physics` owns exactly one source TU and server links it privately.
  Its public header is self-contained and its link interface is guarded against
  exposing `world`. <!-- verify: census(physics, src_tu_count) == 1 -->
- `PmTraceEnv` contains no `EdictArena`; the role-owned sidecars sever the hot
  trace loop from the server store. The single-model callback resolves its
  explicit index in the server adapter.
- Standalone physics tests do not link server. Two value-initialized fixtures,
  differing only in the role marker, run sequentially through a test-local
  `PM_Move`-signature probe and compare a deterministic projection with raw
  float-bit and callback/RNG diagnostics.
- The witness covers world, bbox, studio, water/current contents, glass and
  filter flags, equal-fraction ordering, all three physent lists, hull choice,
  sidecar snapshots, rotated-role identity, direct movevars, and sequential
  reuse. RNG state is reset between parity legs; shared-stream sequencing is
  tested separately.
- Chunk 12 retains production client gather/prediction, retail client-DLL
  integration, movevars reception, command splitting, client-side
  `PM_TraceModel` resolution, and a captured sound/pmove draw schedule.

## Role & parity

- **Role:** **shared-deterministic.** Server authority is live; Chunk 11 adds a
  server-free synthetic second-role witness; Chunk 12 adds the real client path.
- **Counterpart path:** production client prediction in `src/client/`, Chunk 12.
- **Neutral seam:** each role supplies its frozen ABI arrays plus aligned
  model-index sidecars and a role-neutral `PmTraceEnv`. The single-model ABI
  callback resolves identity in its role adapter.
- **Parity fence:** the Chunk 11 deterministic projection and oracle-verified
  RNG reset establish role-to-role identity for the extracted seam. End-to-end
  production parity remains a Chunk 12 acceptance gate.
- **Annotation:** the shared TU carries `// ROLE: shared-deterministic`.
  <!-- verify: census(physics, role_markers) >= 1 -->

## Q-11 satellite verdict

Not applicable. Physics is a stateless extracted kernel, not a lifecycle-owning
service with optional drivers or codecs. The generic subsystem scaffold and
satellite split were therefore deliberately skipped; role-owned gather and ABI
adaptation remain in server/client targets instead of becoming physics
satellites.

## Extension axes (Q-21)

Evaluated against `docs/design/extension-goals.md`.

| Goal / primitive | Applies? | Required seam or door — door-keep verdict |
|---|---|---|
| **G-1** in-engine MCP service | Consumer via P-4 | Trace/contents queries can run over a deliberately published immutable context; the live production resolver is not that snapshot. |
| **G-2** Game ABI v2 | Indirect | The mover is frozen game-DLL ABI; a v2 flavour swaps behind that boundary. |
| **G-3** dedicated debug thread | **Door recorded, closed today** | Off-main readers need private buffers and immutable resolver/cvar snapshots. |
| **G-4** expanded in-game debugging | Consumer via P-4 | Visualisation consumes the typed trace surface. |
| **G-5** scripting runtime | Nothing owed now | Script movement queries compose through P-2/P-4; determinism takes precedence. |
| **P-1** main-thread inbox | Not applicable | Called, does not pump. |
| **P-2** published-snapshot reads | **Shape reserved** | Caller buffers are neutral; production resolver/cvar inputs remain live. |
| **P-3** context-first, no new file-scope state | **✅ met** | No owned state, `ServerRuntime`, or `EdictArena`. |
| **P-4** typed introspection | **✅ met** | Model identity is an aligned typed sidecar; `PM_TraceModel` takes one explicit index. |
| **P-5** narrowest-state signatures | **✅ met for the shared kernel** | Entry points take `PmTraceEnv`, paired views, or one explicit model index; broader server orchestration is outside this boundary. |
| **P-6** services are satellites | **✅ held** | Physics sits below server and links toward no service. |
| **P-7** pool-owned RAII lifecycle | N/A | Owns no allocation; operates on caller buffers and frozen PODs. |
| **P-8** annotation discipline | **✅ met** | The source carries the shared-deterministic marker and census derives its count. |
