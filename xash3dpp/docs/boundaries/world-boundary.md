# world — boundary spec

> Written 2026-07-20, when `xash3dpp_world` was promoted out of
> `xash3dpp_server`. The code is Chunk 6 S5 work; only its target changed.
> Legacy reference: `engine/common/world.c`, `engine/server/sv_world.c`,
> `engine/common/pm_surface.c`.

## 1. Scope / Responsibility

The edict-aware half of collision and spatial indexing: it composes the
map_loader trace kernel (which is edict-free by contract) over a set of
entities the caller owns.

- **Trace** — `move` / `move_no_ents` / `clip_move_to_entity`: per-entity hull
  selection, rotated-brush transforms, the 15-step clip filter chain, and the
  `SV_Move` fraction-rescale quirk.
- **Hulls** — `hull_for_entity` / `hull_for_bsp_entity`, including the studio
  hitbox path (OQ-2) and the Quake hull-select variance.
- **Contents** — `point_contents` and the trigger-intersection predicates.
- **Links** — the areanode tree, `link_edict`/`unlink_edict`, and the
  touch/trigger walks.

**Not in scope**, and deliberately so:

- The BSP data itself, PVS/PAS and the trace kernel — those are map_loader.
- `SV_Physics` movetype dispatch, usercmd execution, the pmove bridge — those
  stayed in `xash3dpp_server` (`src/server/physics/`).
- Lightstyle storage. `LightStyles` is server session state and lives at
  `src/server/lifecycle/lightstyles.cpp`; it was briefly moved here during the
  promotion and moved back the same day.

## 2. Exposed surface (Interface)

Public headers, both in namespace `xash::server` (see §9):

| Header | Provides |
|---|---|
| `xash3dpp/world/trace.hpp` | `MoveEnv`, `SvTrace`, `SvHull`, `BrushModel`, `IModelResolver`, `IClipHooks`, `StudioHullPose`; `move`, `move_no_ents`, `clip_move_to_entity`, `hull_for_entity`, `hull_for_bsp_entity`, `point_contents`, `world_transform_aabb`, `transform_positive_plane`, `studio_pose_for_entity`, `studio_player_blend` |
| `xash3dpp/world/links.hpp` | `WorldLinks`, `AreaNode`, `LinkEnv`, `IWorldLinkHooks`, `GroupOp`; `edict_from_area` and the intrusive link helpers |

## 2a. Dependencies

`map_loader` (WorldData + trace kernel), `content` (studio hitboxes/bones),
`utilities` (Vec3/Matrix3x4), `cmd_cvar` (live `sv_clienttrace` /
`mod_studiocache` reads at trace time) — all PUBLIC, since each appears in the
header signatures. `core` is PRIVATE (logging, thread role).

Every one of these was already an `xash3dpp_server` dependency and none of them
depends on the server, so inserting this target introduced no cycle.

## 2b. Owned state

**None.** This is the property that made the promotion possible and it is the
one to preserve: all four TUs carry **zero** references to `ServerRuntime`, and
every entry point takes its world, resolver, areanode root, hooks and cvar
handle through the injected `MoveEnv` / `LinkEnv` context. There is no
file-scope mutable state.

The areanode tree is owned by the caller (`WorldLinks`, held by the server
runtime today); this target only reads and mutates it through the handle it is
given.

## 3. Invariants and Quirks

- **HB-2 fenced.** `clip.cpp:222-258` — the `if ( rotated )` block, marked
  `TODO(Q-18)` in code — is the rotated-brush ULP kernel. It is byte-exact
  no-touch: no FMA, no reassociation, no `std::ranges` rewrite. The 2026-07-20
  promotion was a pure file move; its entire diff was one include path.
- Edict identity stays `edict_t *`; field access goes through `EntityView`
  (Q-20). This target never touches `->v.` directly, which the
  `entvars-confinement` compliance rule enforces.
- `MoveEnv::hooks` is nullable and means "defaults"; it is currently never
  assigned in production (recorded in the audit ledger as an unowned seam).
- Studio hull gating reads `sv_clienttrace` / `mod_studiocache` and
  `trace_flags` **live, per call**, matching the legacy per-call cvar reads
  rather than caching them.

## 6. Threading

Main-thread only today, by the same contract the server carries
(server-boundary OQ-9). Nothing here is inherently main-pinned: the entry
points are pure functions over an injected context, so the constraint comes
from the mutable areanode tree and the edicts, not from this code. If Chunk 12
runs client prediction on another thread it needs its own `MoveEnv` over its
own entity set, not a lock here.

## 9. As-built reconciliation (2026-07-20)

- 4 TUs, ~1,350 lines: `clip.cpp` (609), `links.cpp` (287), `hulls.cpp` (263),
  `contents.cpp` (192). 4 test targets under `tests/world/`.
- The **namespace is still `xash::server`**, deliberately. Renaming it to
  `xash::world` also means moving `EntityView`, `Vec3`, `vec_axis`, `to_vec3`
  and `store_vec3` — which `abi/entity_view.hpp` exports into `xash::server`
  and which the whole server/world tree spells unqualified. That change is
  mechanical, tree-wide and behaviour-free, so it is recorded as a Chunk-12
  obligation rather than bundled into the promotion.
- The `tests/world/` fixtures link `xash3dpp_server` because `EdictArena` is
  the tree's only edict allocator (server-owned by Q-20). That is a fixture
  dependency, not a library one.

## Role & parity

- **Role:** shared-deterministic. The edict-aware trace/link layer is composed
  over the map_loader kernel and is reached by the server world trace today and
  by Chunk-12 client prediction next; results are compared, so a divergence is a
  prediction error.
- **Counterpart path:** server (live) + client prediction (Chunk 12), each over
  its own `MoveEnv` / `LinkEnv` and entity set.
- **Neutral seam:** every entry point is a pure function of `const WorldData &` +
  the injected context + caller-owned edicts; zero owned state (P-3).
- **Parity fence:** HB-2 — the rotated-brush ULP kernel at `clip.cpp:219-255` is
  byte-exact no-touch.
- **Annotation:** `clip.cpp` carries the `// ROLE: shared-deterministic` banner. <!-- verify: census(world, role_markers) >= 1 -->

## Extension axes (Q-21)

Evaluated against `docs/design/extension-goals.md`. This target is unusual in
that its doors are open **by construction** rather than by design effort: it
was written context-first with zero owned state, which is exactly what P-3 and
P-5 ask for, and that is what let it become a target at all.

| Goal / primitive | Applies? | Required seam or door — door-keep verdict |
|------------------|----------|-------------------------------------------|
| **G-1** in-engine MCP service | Consumer via P-4 | Trace/contents queries are pure functions over an injected context, so a service can call them off a published snapshot without touching live state. Nothing owed. |
| **G-2** Game ABI v2 | Indirect | Takes `edict_t *` and reads through `EntityView`, so a v2 entity flavour swaps behind that seam without touching this target. Keep the `EntityView`-only rule. |
| **G-3** dedicated debug thread | **Door open — needs a caller-owned context** | Nothing here is thread-affine; a reader thread needs its own `MoveEnv` over an immutable world plus a snapshot of the entity set. The areanode tree is the mutable part and is caller-owned. |
| **G-4** expanded in-game debugging | Consumer via P-4 | Hull/trace visualisation consumes the same entry points. Nothing owed. |
| **G-5** scripting runtime | Nothing owed now | Script entity queries go through the same P-2/P-4 surfaces as G-1; parity precedence caps everything (HB-2). |
| **P-1** main-thread inbox | Not applicable | No inbox; this target is called, it does not pump. |
| **P-2** published-snapshot reads | **Open by construction** | Every entry point is a pure function of `const WorldData &` + the injected context + caller buffers. |
| **P-3** context-first, no new file-scope state | **✅ met — zero owned state** | Four TUs, no file-scope mutable state, no `ServerRuntime`. This is the axis the promotion was evidence for. <!-- verify: grep-count(ServerRuntime, xash3dpp/src/world/**/*.cpp) == 0 --> |
| **P-4** typed introspection | **✅ door open** | `EntityView` is the only edict-field access path; no raw `->v.` (enforced by `entvars-confinement`). |
| **P-5** narrowest-state signatures | **✅ met** | Entries take `MoveEnv` / `LinkEnv` sub-aggregates, never a whole runtime. |
| **P-6** services are satellites | **Yes — held** | This target links toward no service; it sits below the server. |
| **P-7** pool-owned RAII lifecycle | Conforms where it applies | Owns no allocations; the frozen ABI PODs it receives are exempt per the Q-22 retrofit guard. |
| **P-8** annotation discipline | Inherited | Counts move with the target — re-derive from `census world` rather than hand-counting (the server row's figures were a scanner artifact before 2026-07-20). |
