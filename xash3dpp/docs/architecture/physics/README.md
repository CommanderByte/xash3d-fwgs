# physics — Architecture Overview

> **Target:** `xash3dpp_physics`
> **Public API:** `xash3dpp/physics/pm_trace.hpp`
> **Implementation:** `src/physics/pm_trace.cpp`
> **Role:** shared-deterministic

## Purpose

Physics is the role-neutral player-move trace kernel used by the frozen game-DLL
`PM_Move` callback surface. It accepts caller-owned `playermove_t` buffers,
world/model dependencies, and model-index snapshots; it owns no entity store,
runtime aggregate, random stream, or lifecycle.

Server and, in Chunk 12, client are role owners. Each gathers its own frozen ABI
physent lists and aligned model-index sidecars. The shared kernel consumes paired
views. `PM_TraceModel` is adapted at the role boundary because its arbitrary
physent need not belong to an active list.

## Dependency shape

`map_loader` and `utilities` are public value-type dependencies. `world`,
`content`, `cmd_cvar`, and `core` are implementation-only. The public header
forward-declares `world::IModelResolver`; brush/studio model details stay in the
source file. Server links physics privately, and physics never includes or links
server.

## Determinism fence

The kernel preserves legacy expression order, filtering, list ordering, and
float behavior. Shared trace predicates live in `world/trace.hpp`. A standalone,
server-free test runs two sequential role fixtures through a `PM_Move`-signature
probe and compares an explicit deterministic projection. Rotated cases establish
role identity only; Q-18 remains the legacy ULP authority.

Randomness is injected by the role owner. `EngineContext` owns the sole
production `core::LegacyRandom`, whose implementation is differentially tested
against a compiled, source-byte-verified copy of the legacy routine.

## Threading

The target owns no mutable state, but production trace entry points are Main-only
because the model resolver lazily mutates studio caches and cvars are read live.
Each role uses one sequential `playermove_t`. Off-main tracing requires immutable
resolver/cvar snapshots and exclusive caller buffers; simply removing the
assertion would violate the dependency contracts.

## Chunk 12 handoffs

Chunk 12 owns the production client gather and prediction caller, movevars
reception, retail client-DLL integration, command splitting, client-side
`PM_TraceModel` identity resolution, and any end-to-end sound/pmove draw-schedule
witness.
