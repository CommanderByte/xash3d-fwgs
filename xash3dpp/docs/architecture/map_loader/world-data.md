# WorldData — the immutable world model

> **Defined in**: `map_loader/world.hpp`, `src/map_loader/world.cpp`\
> **Namespace**: `xash::map_loader`\
> **Legacy reference**: `common/com_model.h` (`model_t`/`mnode_t`/`mleaf_t`/`hull_t`)

## Overview

`WorldData` is the normalized in-memory form of a loaded BSP: every array a
query needs, exposed only through const spans/views. It replaces the legacy
`model_t` + `world_static_t` globals with a value type that is **immutable
after `load_world_data` returns** — the property that makes every query
concurrent-read-safe (Q-6) with zero synchronisation.

## Record types (normalized)

| Type | Legacy counterpart | Normalisations |
|------|--------------------|----------------|
| `Plane` | `mplane_t` | signbits computed at load; `type` byte kept for the axial fast path |
| `ClipNode32` | `mclipnode16/32_t` | **always 32-bit** — the `world.version` global and dual-width kernels are gone (Known Deviation; a Chunk 6 ABI shim may narrow) |
| `Node` | `mnode_t` | children keep DISK semantics (`< 0` → leaf `-1-child`); no parent links, no 32-bit bitfield packing |
| `Leaf` | `mleaf_t` | `cluster` derived (index−1, clamp); `visofs` raw/unclamped (legacy parity) |
| `SubModel` | `dmodel_t` + `"*N"` model_t | bounds spread ±1; `hulls[4]` wired descriptors; `k_model_*` flag bits (ref_api values) |
| `HullDescriptor` | `hull_t` span | `present == false` ≙ legacy `planes == NULL` (missing hull) |
| `Surface`/`TexInfo` | `msurface_t`/`mtexinfo_t` | flag/miptex subset only — extents/lightmaps are content-side (Chunk 7) |

## Accessors

All `[[nodiscard]] const noexcept`: `planes/nodes/leafs/marksurfaces/
submodels/clipnodes/hull0_nodes/surfaces/texinfos/texture_names` spans;
`visdata/visclusters/visbytes`; `entities/wadlist/message` string views;
`version/flags/checksum/name` scalars. Storage is `std::vector` members
(QL — BSP arrays exceed the 64 KB array cap); cold-path load allocation is
Q-13 exempt.

## Lifecycle / ownership

Move-only (QJ: copy deleted in the header, moves declared there and
defaulted in `world.cpp`). Produced by `load_world_data`; typically owned by
`MapLoader::Impl` inside a `std::optional` and borrowed via
`MapLoader::world()` — the pointer is valid until the next
load/clear/shutdown ([fsm.md](./fsm.md)). Queries never retain it.

The clipnode arrays split by hull index: `hull0_nodes()` is the MakeHull0
duplicate of the draw nodes (hull 0), `clipnodes()` backs hulls 1–3 (shared
widened array on classic maps; concatenated per-hull remaps on BSP30ext —
see [hulls.md](./hulls.md)).

## Threading model

Immutable after load; every accessor and every free-function query is safe
from any thread once the object is activated. The activation swap itself is
main-thread-owned — see
[threading analysis](../../threading-analysis/map_loader-threading.md).

## Edge cases and invariants

- `submodels()[0]` is always the world; `"*N"` inline models are indices
  1..N with per-model hull descriptors and origin/flag detection.
- `leafs()[0]` is the shared CONTENTS_SOLID leaf (enforced for worlds).
- `visofs` may legitimately point anywhere ≥ `visdata.size()` on corrupt
  maps; consumers go through `leaf_compressed_pvs`, which maps out-of-range
  to the legacy NULL/full-visibility convention.
- `checksum()` is the SP constant unless `WorldLoadOptions::multiplayer_crc`
  was set at load.

## See also

- [bsp-loading.md](./bsp-loading.md) — how it gets filled
- [pvs.md](./pvs.md), [trace.md](./trace.md) — its consumers
