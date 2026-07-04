# map_loader — Architecture Overview

> **Source**: `xash3dpp/src/map_loader/`\
> **Public API**: `xash3dpp/include/xash3dpp/map_loader/`\
> **Legacy reference**: `engine/common/mod_bmodel.c`, `engine/common/pm_trace.c`, `common/bspfile.h`, `public/crclib.c`\
> **Boundary spec**: [`docs/boundaries/map_loader-boundary.md`](../../boundaries/map_loader-boundary.md)\
> **Threading**: [`docs/threading-analysis/map_loader-threading.md`](../../threading-analysis/map_loader-threading.md)\
> **Deep dives**: [`legacy-survey/deep-dive-bsp-loader.md`](../../legacy-survey/deep-dive-bsp-loader.md), [`legacy-survey/deep-dive-trace-pvs.md`](../../legacy-survey/deep-dive-trace-pvs.md)

## Purpose

`xash3dpp_map_loader` owns the world: it parses BSP v29/v30/BSP2/BSP30ext
files into an immutable `WorldData` model and answers the two spatial
question families everything downstream depends on — **visibility** (PVS
decompression, point→leaf, box→clusters, fat PVS) and **collision**
(clip-hull point contents and the segment-sweep trace kernel). It also runs
the map-load FSM (`MapLoader`, host-boundary OQ-2) that Host drives per
frame and client/server observe.

It deliberately does **not** own: entity dictionaries (the game DLL parses
the raw entity text), physent iteration and hit-entity recording (server,
Chunk 6), PHS precomputation (server), texture texels/lightmaps/surface
extents (content pipeline, Chunk 7), or studio hitbox hulls.

## Design in three layers

1. **BSP loader (cold path)** — `src/map_loader/bsp/*.cpp` behind
   `private/map_loader/bsp/*.hpp`. Raw file bytes → validated, normalized
   `WorldData`. Every piece of on-disk variance is resolved here, once:
   16/32-bit record widths (clipnodes are widened to `ClipNode32`
   permanently), the Blue-Shift lump swap, the BSP30ext extended-clipnode
   guess and per-hull remap, broken-compiler fix-ups (aguirRe, darkfuture),
   ZHLT empty hulls, texture-name-derived surface flags, the water-alpha
   probe and the wire-frozen map CRC.
2. **Query kernels (hot path)** — `pvs.cpp` and `trace.cpp`, pure free
   functions over `const WorldData&` / non-owning `TraceHull` views. No
   globals (legacy `world.version` and the shared `pm_boxhull`/`g_visdata`
   statics are gone), no version branches, no edict pointers. Q-18 applies
   in full to `trace.cpp`: float-for-float legacy parity, gated by golden
   vectors and a bit-exact cross-check against the verbatim legacy kernel.
3. **MapLoader FSM** — `map_loader.cpp`. Owns the active world
   (`load_world`/`clear_world`/`world()`), loads synchronously in the
   `LoadLevel` state, notifies `IMapLoaderObserver`s with the real outcome,
   and applies the `maps/<name>.ent` entity-patch override on world loads.

## Data flow

```text
Filesystem::load_file ─→ parse_header ─→ resolve_lump (validation ladder)
                                              │
        entities/planes/submodels/textures/…  ▼   (legacy stage order)
                                     WorldDataFill stages ─→ WorldData (immutable)
                                              │
              ┌───────────────────────────────┼──────────────────────────┐
              ▼                               ▼                          ▼
       pvs.hpp queries                 trace.hpp kernel           MapLoader::world()
  (point_leaf, fat_pvs, …)      (hull_point_contents,          (borrowed by host/
                                 trace_hull, finalize_trace)    server/client)
```

## Key contracts

- **Immutability (Q-6)**: after `load_world_data` returns, `WorldData` never
  changes; all queries are concurrent-read-safe.
- **Edict-free tracing**: the Chunk 6 server composes per-entity traces from
  `hull_for_bsp`/`BoxHull` + `trace_hull` + `finalize_trace`; this subsystem
  never sees an entity.
- **Tie-break asymmetry** (legacy-exact): `point_leaf` sends on-plane points
  to the *back* child (`<= 0`); hull walkers send them to the *front*
  (`< 0`).
- **Error model (Q-5)**: `std::expected<WorldData, core::ErrorCode>` with
  `BspUnsupportedVersion`/`BspCorruptLump`/`BspBadWorld`; every failure is
  logged at tag `map_loader`. Legacy `Host_Error` process-kills became error
  returns (documented Known Deviation).

## Module map

| Piece | Files | Legacy counterpart |
|---|---|---|
| Disk format (size-pinned) | `private/…/bsp/disk_format.hpp` | `common/bspfile.h` |
| Header/quirk detection + lump validation | `bsp/bsp_loader.{hpp,cpp}` | `Mod_LoadBmodelLumps`, `Mod_LoadLump` |
| Core lump stages | `bsp/bsp_lumps.cpp` | `Mod_Load{Entities,Planes,Submodels,Visibility,MarkSurfaces,Leafs,Nodes}` |
| Hull construction | `bsp/bsp_hulls.cpp` | `Mod_LoadClipnodes`, `Mod_MakeHull0`, `Mod_SetupSubmodels/Hull` |
| Texture-name flags + water-alpha | `bsp/bsp_flags.cpp` | `Mod_LoadTexture/TexInfo/Surfaces`, `Mod_CheckWaterAlphaSupport` |
| Map checksum | `bsp/map_crc.{hpp,cpp}` | `CRC32_MapFile` |
| PVS queries | `pvs.{hpp,cpp}` | `Mod_DecompressPVS`, `Mod_PointInLeaf`, `Mod_FatPVS`, `Mod_Box*` |
| Trace kernel + hull selection | `trace.{hpp,cpp}`, `private/…/trace_math.hpp` | `pm_trace.c`, `PlaneDiff`/`BoxOnPlaneSide` |
| FSM + world ownership | `map_loader.{hpp,cpp}` | `host_state.c` (OQ-2) + `Mod_LoadWorld` seam |

Sub-feature bundling (Q-11): bsp/pvs/trace score below the satellite
threshold — they share `WorldData`, add no dependency, carry no independent
state machine and are useless without the parent — so they are subfolders of
the single `xash3dpp_map_loader` target (same verdict pattern as
networking's master-list satellite analysis).

## Verification posture

Format watchdog CLEAR (disk structs vs `bspfile.h`); adversarial legacy
parity audits on the loader (findings fixed) and the trace kernel
(PARITY-CONFIRMED); 18,156-trace bit-exact cross-check against the verbatim
legacy kernel; golden trace fixtures in `tests/map_loader/trace/` are the
standing Q-18 determinism gate.

## Index of concepts

- [index.md](./index.md) — full file/symbol/test index
- [bsp-loading.md](./bsp-loading.md) — header/quirk detection, validation ladder, stage pipeline
- [world-data.md](./world-data.md) — the immutable model and its normalisations
- [hulls.md](./hulls.md) — clipnode widening, MakeHull0, per-submodel hull wiring
- [pvs.md](./pvs.md) — visibility queries (decompress, point/box/fat)
- [trace.md](./trace.md) — the Q-18 kernel, hull selection, BoxHull
- [fsm.md](./fsm.md) — MapLoader FSM, world ownership, the `.ent` patch
