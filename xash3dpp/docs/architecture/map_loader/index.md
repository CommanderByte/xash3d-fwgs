# map_loader — Index

## Public API headers

| Header | Namespace | Key symbols |
|--------|-----------|-------------|
| `map_loader/map_loader.hpp` | `xash` | `MapLoader`, `MapLoaderInitParams`, `MapLoadState`, `IMapLoaderObserver` |
| `map_loader/world.hpp` | `xash::map_loader` | `WorldData`, `load_world_data()`, `WorldLoadOptions`, `BspVersion`, `Plane`, `ClipNode32`, `Node`, `Leaf`, `SubModel`, `HullDescriptor`, `Surface`, `TexInfo`, `HullBounds`/`k_default_hull_bounds`, `k_model_*`, `k_surf_*`, `k_fworld_wateralpha` |
| `map_loader/contents.hpp` | `xash::map_loader` | `k_contents_*` (ABI values) |
| `map_loader/pvs.hpp` | `xash::map_loader` | `decompress_pvs`, `point_leaf`, `leaf_compressed_pvs`, `pvs_for_point`, `box_leafnums`, `box_visible`, `fat_pvs`, `check_vis_bit`, `k_max_box_leafs`, `k_fatpvs_radius`, `k_fatphs_radius` |
| `map_loader/trace.hpp` | `xash::map_loader` | `TraceHull`, `TraceResult`, `TracePlane`, `hull_point_contents`, `recursive_hull_check`, `trace_hull`, `finalize_trace`, `world_hull`, `hull_for_bsp`, `HullSelection`, `BoxHull`, `k_dist_epsilon` |

## Private / internal headers

| Header | Purpose |
|--------|---------|
| `private/map_loader/bsp/disk_format.hpp` | On-disk BSP records (legacy `d*_t` names, size/offset static_asserts), lump ids, version fourccs, format caps, TEX_* flags, plane types; `read_record`/`read_record_at` memcpy readers |
| `private/map_loader/bsp/bsp_loader.hpp` | `HeaderInfo`/`parse_header`, `LumpView`/`resolve_lump` (srclumps validation table), `LoadContext`, `LoadScratch`, `WorldDataFill` stage declarations |
| `private/map_loader/bsp/map_crc.hpp` | `map_checksum_multiplayer`, `k_map_crc_singleplayer` |
| `private/map_loader/trace_math.hpp` | `plane_diff` (axial fast path — ULP-critical), `box_on_plane_side` (signbits corner tables), `vec3_component` |

## Source files

| File | Responsibility |
|------|---------------|
| `src/map_loader/map_loader.cpp` | FSM (OQ-2), observer table, world ownership (`load_world`/`clear_world`/`world()`) |
| `src/map_loader/world.cpp` | `WorldData` special members + accessors; `load_world_data` orchestration (span + Filesystem overloads incl. the `.ent` patch probe) |
| `src/map_loader/pvs.cpp` | PVS queries (RLE decompress, leaf/box/fat walks) |
| `src/map_loader/trace.cpp` | Trace kernel, hull selection, `BoxHull` |
| `src/map_loader/bsp/bsp_loader.cpp` | Header/version/quirk detection, per-lump validation ladder |
| `src/map_loader/bsp/bsp_lumps.cpp` | Entities/planes/submodels/visibility/marksurfaces/leafs/nodes stages; worldspawn scan; underwater marking; water-alpha probe |
| `src/map_loader/bsp/bsp_hulls.cpp` | Clipnode widening (+aguirRe fix), `MakeHull0`, `SetupSubmodels`/`SetupHull` (BSP30ext remap, ZHLT skips), `"*N"` origin detection |
| `src/map_loader/bsp/bsp_flags.cpp` | Miptex names, texinfo, SURF_* flag derivation, checksum stage |
| `src/map_loader/bsp/map_crc.cpp` | Wire-frozen map CRC (utilities::crc32, no final invert) |

## Key types

| Type | Kind | Defined in | Role |
|------|------|-----------|------|
| `WorldData` | class (move-only) | `world.hpp` | Immutable loaded world; const span accessors |
| `MapLoader` | class (pimpl) | `map_loader.hpp` | Map-load FSM + active-world owner |
| `TraceHull` | struct (view) | `trace.hpp` | Non-owning hull_t equivalent for the kernel |
| `TraceResult` | struct | `trace.hpp` | pmtrace_t minus ent/hitgroup |
| `BoxHull` | class (non-copyable) | `trace.hpp` | PM_InitBoxHull/PM_HullForBox as a value type |
| `WorldLoadOptions` | aggregate | `world.hpp` | is_world / multiplayer_crc / hull_bounds / entity_patch |
| `HullDescriptor` | struct | `world.hpp` | Per-(submodel, hull) clipnode span + padding + `present` |
| `WorldDataFill` | struct (friend) | `bsp_loader.hpp` | Loader stages — the only writer of WorldData |

## CMake targets

| Target | Type | Public deps | Private deps |
|--------|------|-------------|--------------|
| `xash3dpp_map_loader` | STATIC | `xash3dpp_memory`, `xash3dpp_filesystem`, `xash3dpp_utilities` | `xash3dpp_core` |

## Tests (12 executables, one `tests/map_loader/CMakeLists.txt`)

| Suite | Pins |
|-------|------|
| `test_map_loader` | FSM contract (one-step LoadLevel, observers, truncation) |
| `test_map_loader_world` | Filesystem integration, `.ent` patch, FSM load flows |
| `bsp/test_disk_format` | Record sizes/offsets, fourccs, LE byte images |
| `bsp/test_bsp_header` | Version dispatch, BSP30ext id-only probe, Blue-Shift, validation ladder |
| `bsp/test_bsp_lumps` | Core lump stages, clusters, leaf-0, node validation, BSP2 |
| `bsp/test_bsp_hulls` | Widening/aguirRe, hull wiring, ZHLT, BSP30ext remap, origins |
| `bsp/test_bsp_flags` | Texture names, SURF_* rules, MODEL_* flags, underwater, water-alpha |
| `bsp/test_map_crc` | CRC goldens (0x340BC6D9), entities exclusion, SP constant |
| `pvs/test_pvs_decompress` | Zero-RLE goldens incl. clamps |
| `pvs/test_pvs_queries` | point_leaf tie-break, box/fat queries |
| `trace/test_point_contents` | Contents walk, front tie-break, CONTENTS_NONE |
| `trace/test_hull_trace` + `trace/test_box_hull` | **Q-18 golden vectors** (bit-exact), BoxHull layout, hull_for_bsp offsets |
