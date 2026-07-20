# Save Modernization Opportunities

> **First pass, 2026-07-20** (tree-wide modernization audit). No prior
> `save-modernization.md` existed — `save` shipped as Chunk 8 (S8.7 milestone,
> retail `hl.dll` save→load witness on `c0a0`) without a modernization report
> ever being written for it. This pass is a from-scratch scan of all 13
> `src/save/*.cpp` TUs plus the `include/xash3dpp/{save,private/save}/`
> headers, cross-checked against the Phase-2 adversarial digest (F130-F135)
> and the cross-cutting lenses pass. IDs start at H-1/M-1/L-1 — there is no
> prior tier sequence to continue.

> C++ standard in use: C++**23** (from `xash3dpp/CMakeLists.txt`,
> `CMAKE_CXX_STANDARD 23`)\
> Boundary spec: [`docs/boundaries/save-boundary.md`](../boundaries/save-boundary.md)\
> ABI-frozen symbols in this subsystem:
> `LEVELLIST` / `ENTITYTABLE` / `SAVERESTOREDATA` / `FIELDTYPE` /
> `TYPEDESCRIPTION` (vendored at `abi/eiface.hpp:131-215`); the seven
> `DLL_FUNCTIONS` save-callback slots `pfnSave`/`pfnRestore`/
> `pfnSaveWriteFields`/`pfnSaveReadFields`/`pfnSaveGlobalState`/
> `pfnRestoreGlobalState`/`pfnResetGlobalState` plus `pfnChangeLevel`
> (`eiface.h:429-437,462-468`). The on-disk GoldSrc save-file layout
> (`GameHeader`/`SaveHeader`/`SaveLightStyle`/`SaveClient`/`SaveDecalEntry`/
> `SaveSoundEntry`, `private/save/format.hpp:88-368`) is **not** a Fence-1
> struct (it lives under `private/save/`, not `abi/`) and `save` is **not**
> one of the five HB-2 byte-exact-kernel subsystems — but it is still a
> save-owned compat surface pinned by `offsetof`/`sizeof` `static_assert`s and
> must not change field order, width, or endianness. See **Out of scope**
> below for the full list and the distinction between the two fences.

## Summary

`save` is already clean, idiomatic C++23: `Result<T>` (`std::expected`)
error returns throughout, `std::span`/`std::byte` for wire buffers, no
`malloc`/`free`, no raw owning `new`, no `char *` ownership drift, and
`offsetof`/`sizeof` `static_assert`s pinning every wire struct. There is
essentially no C-idiom debt to modernize. The one substantive, well-evidenced
finding is **internal duplication**, not staleness: the little-endian
int32/uint16 codec, a fixed-width zero-fill string copy, and an
edict-validity check are each hand-reimplemented in an anonymous namespace
in 10 of the subsystem's 13 translation units — several bodies are
character-for-character identical — when `SaveBuffer` already ships a
canonical, tested version of the numeric codec. That is this report's one
High item, and it must land as a pure relocation (byte-for-byte, no
reformatting), because these bytes are the GoldSrc on-disk save-file
compat surface.

Below that, nothing rises above Medium. The Medium item is not a code
change at all — it is a boundary-spec row (`SaveStats`) that commits to a
stats struct no code implements, with no code-eligible consumer yet; the
correct move is to walk the doc back to a shape constraint, not to build
speculative instrumentation. The Low items are two documentation-accuracy
notes and one genuinely cosmetic `char[N]` → `std::array<char,N>` swap that
the evidence itself shows is marginal. **No deletion candidate was found in
save's own code** — the subsystem's zero-production-implementation
interfaces (`IDecalListProvider`, `IDynamicSoundsProvider`,
`IMusicStateProvider`, `IMapValidityChecker`) are correctly-scheduled
Chunk-11/12 doors, not over-abstraction, per the tree-wide lens pass; they
are recorded under **Out of scope**, not proposed for removal.

Max tier observed this pass: **H-1, M-1, L-3**.

______________________________________________________________________

## High-priority opportunities

### H-1: Little-endian int32/uint16 codec + fixed-width string copy + edict-validity check are independently reimplemented in 10 of 13 save TUs

- **File(s)**:
  `xash3dpp/src/save/entity_table.cpp:22-41`,
  `xash3dpp/src/save/entity_patch.cpp:18-34`,
  `xash3dpp/src/save/level_state_writer.cpp:27-51`,
  `xash3dpp/src/save/container_codec.cpp:20-47`
  (plus `descriptor_codec.cpp`, `field_sink.cpp`, `save_comment.cpp`,
  `client_state.cpp`, `adjacent_transfer.cpp`, `save_directory.cpp` — 10 of
  the 13 `src/save/*.cpp` TUs carry at least one copy).

- **Current pattern**: each TU defines its own anonymous-namespace
  `append_i32_le`/`read_i32_le` pair, and/or its own
  `copy_fixed(char *, n, string_view)` zero-fill-then-truncate helper,
  and/or its own `is_valid_edict(edict_t *)` check. `append_i32_le` is
  verified **character-for-character identical** across
  `entity_patch.cpp:18`, `level_state_writer.cpp:35`,
  `container_codec.cpp:20`, and `client_state.cpp:19`; `copy_fixed` is
  likewise identical across `level_state_writer.cpp:27`,
  `container_codec.cpp:41`, and `client_state.cpp`. Counted exactly: the
  little-endian codec appears 73 times across 8 files, the
  `copy_fixed`/`pad2`/`is_valid_edict` family 22 times across 6 files — 95
  duplicate sites total.

- **Suggested replacement**: extract one shared
  `private/save/byte_codec.hpp` of small, header-only `constexpr`/`inline`
  free functions — `encode_i32_le`/`decode_i32_le`/`read_u16_le`/
  `read_f32_le` over `std::span<const std::byte>`, plus `copy_fixed` and
  `is_valid_edict(const abi::edict_t *)` — and replace the 8+6
  anonymous-namespace copies with `#include`s. `SaveBuffer`'s own
  `write_i16`/`write_i32`/`write_f32`/`read_i16`/`read_i32`/`read_f32`
  (`save_buffer.cpp:73-153`) are the canonical, already-tested byte-for-byte
  reference the new header should match bit-for-bit; the buffer-less call
  sites (`parse_hl1_preamble`, `parse_five_int_preamble`,
  `save_comment`'s standalone hand-parse, the container/client/entity-patch
  assembly paths) are exactly where a `SaveBuffer`-free codec is needed.

- **Boundary-safe**: Yes — purely internal relocation of byte-identical
  logic; no wire-format change. Must land as a **verbatim** move (no
  epsilon/rounding/`std::ranges` "improvement" of the bit-shift arithmetic)
  since these bytes are the GoldSrc on-disk save compat surface (see
  **Out of scope**).

- **Rationale**: 95 duplicate call sites across 10 of 13 TUs is the largest
  concrete duplication footprint found in `save`. The risk is not just
  code size — it is that the copies can silently drift (one already differs
  in whether it null-pads or truncates at the boundary byte, per the
  digest's own file-by-file breakdown), and a decode-side divergence in a
  byte-exact save/load format is a parity bug, not a style nit. Named
  day-one consumer: the 8+6 existing call sites in the tree today
  (`consumer_status: exists-in-tree`) — this is deduplication of code that
  already runs, not new infrastructure.

______________________________________________________________________

## Medium-priority opportunities

### M-1: `SaveStats` — boundary spec commits to an always-on stats struct that was never built and has no code-ready consumer

- **File(s)**: `xash3dpp/docs/boundaries/save-boundary.md:277-279` (doc
  only — no source file exists to cite: `SaveStats` matches zero files
  under `xash3dpp/src/save/` or `xash3dpp/include/xash3dpp/{save,private/save}/`).

- **Current pattern**: `save-boundary.md` phrases the row as a delivered
  Chunk-8 fact ("Yes", "as always-on Tier-1 atomics"), not as a door-keep
  entry. `save` is the only one of the tree's 15 stats-bearing-or-committed
  subsystems whose own boundary spec names a stats struct that does not
  exist in code (`facts.json`'s stats-struct inventory has no `save`
  entry at all).

- **Suggested replacement**: **do not build the struct now.** Correct the
  doc row to a shape constraint: "when save stats are built — driven by an
  actual reader landing (the tree-wide `diagnostics_dump` aggregator, or a
  real G-3 debug thread) — `SaveStats` must be always-on Tier-1
  `std::atomic` fields with a by-value snapshot accessor, matching
  `include/xash3dpp/networking/stats.hpp`'s `NetworkingStats` shape, not
  one of the 9 existing plain (non-atomic) stats structs." If instrumentation
  is wanted anyway, the natural increment points are the existing
  `LevelStateWriter::write` / `LevelStateLoader::load` / `AdjacentTransfer::run`
  call sites — but that is a decision for whoever owns the consumer, not
  this report.

- **Boundary-safe**: N/A — documentation-only change; no source file is
  touched by this finding.

- **Rationale**: fails the anti-gold-plating rule as currently written.
  The tree records exactly 2 production thread spawns (both in
  `src/sound/topology.cpp`), and `diagnostics_dump` is itself an open,
  overdue, tree-wide backlog item — not a scheduled `save` deliverable — so
  `consumer_status: scheduled-chunk-N` is unearned; the honest value is
  `speculative`, which the brief says may only be recorded as a shape
  constraint, never as work. Building a 10th stats struct with zero readers
  would repeat the exact anti-pattern the 9 existing plain stats structs
  already represent.

______________________________________________________________________

## Low-priority / cosmetic opportunities

### L-1: `SV_GetSaveComment` read path has zero production callers — boundary doc overstates it as shipped

- **File(s)**: `xash3dpp/include/xash3dpp/private/save/save_comment.hpp:139-150`
  (`save_comment`/`save_comment_file` declarations);
  `xash3dpp/src/server/lifecycle/save_bridge.cpp:819` (the only production
  call site, and it is `build_save_comment` — the **write** side, not the
  read side).

- **Current pattern**: `save-boundary.md`'s interface table lists
  `SV_GetSaveComment` alongside `SaveGameState`/`LoadGameState`/
  `LoadAdjacentEnts`/`ClearSaveDir` as one of "the four primitives ... +
  `SV_GetSaveComment` ship ... exactly as scoped", and its own OQ status
  table marks the read-side hand-parse as landed. In the shipped tree only
  `build_save_comment()` (write side) has a production caller;
  `save_comment()`/`save_comment_file()` (read side, and
  `IMapValidityChecker`'s consumer) are exercised only by tests.

- **Suggested replacement**: no code change — a status-accuracy note. The
  boundary doc's as-built banner should distinguish "the parse logic is
  implemented and tested" from "the primitive is wired into a live call
  path", as it already does elsewhere for other deferred bodies. This
  resolves itself when Chunk 12 wires `pfnGetSaveComment` through a
  production `IMapValidityChecker` (a named, scheduled consumer —
  `OBL-12-9` in the tree-wide lens pass, not a speculative one).

- **Boundary-safe**: N/A — doc note only.

- **Rationale**: keeps the as-built banner honest and prevents a future
  auditor re-discovering "the read side is dead code" as if it were a bug
  rather than a scheduled Chunk-12 wiring gap.

### L-2: `format.hpp` fixed-width text fields use raw `char[N]` where `std::array<char, N>` is layout-identical

- **File(s)**: `xash3dpp/include/xash3dpp/private/save/format.hpp:91-92`
  (`GameHeader::map_name`/`comment`), `:108-109`
  (`SaveHeader::map_name`/`sky_name`), `:133`
  (`SaveLightStyle::style`), `:237-238` (a further wire struct) — 9
  `char[N]` fields total across `GameHeader`/`SaveHeader`/
  `SaveLightStyle`/`SaveClient`/`SaveDecalEntry`/`SaveSoundEntry`.

- **Current pattern**: `char map_name[32]`, `char comment[80]`, etc. These
  structs live under `private/save/`, not `abi/`, so they are save-owned
  wire-format mirrors rather than Fence-1 frozen-ABI structs — but they
  are still byte-pinned by adjacent `offsetof`/`sizeof` `static_assert`s.

- **Suggested replacement**: `std::array<char, N>` is bit-identical in
  size, alignment, and layout (the existing `offsetof`/`sizeof`
  `static_assert`s are unaffected) and gains `.size()`/`.begin()`/`.end()`.

- **Boundary-safe**: Yes — layout-identical swap, verified against the
  adjacent `static_assert`s.

- **Rationale**: genuinely cosmetic. Every call site already treats the
  field as a raw `memcpy`/`copy_fixed` destination (see H-1), so the
  practical benefit is marginal — recorded so it is not re-derived by a
  future pass, not pushed as a priority.

### L-3: Phase-0 future-chunk-marker scan misses save's hyphenated "Chunk-12" spelling

- **File(s)**: `xash3dpp/include/xash3dpp/private/save/save_directory.hpp:82`;
  `xash3dpp/include/xash3dpp/private/save/adjacent_transfer.hpp:20,128`.

- **Current pattern**: `facts.json`'s `future_chunk_markers.by_subsystem`
  has zero entries for `save` (the 69-marker inventory spans
  `abi`/`core`/`host`/`imagelib`/`input`/`platform`/`server`/`sound` only)
  because the mechanical scan's regex matches `chunk12`/`Chunk 12` but not
  the hyphenated `Chunk-12` spelling `save`'s own authors consistently used
  at these three sites.

- **Suggested replacement**: not a `save` code change — a correction to
  the tree-wide fact-base tooling (`xash3dpp/tools/`, the `q21_scan`-style
  marker regex) to also match the hyphenated `Chunk-N` spelling, so future
  obligations-register passes do not silently drop subsystems that use the
  hyphenated house style.

- **Boundary-safe**: N/A — tooling fix, not a `save` source change.

- **Rationale**: these are genuine unresolved future-chunk obligations
  (renderer image-cache eviction ownership, `RestoreDecal`/
  `LoadClientState` wiring) that a tree-wide obligations register would
  otherwise report as zero for `save`, undercounting real deferred work.

______________________________________________________________________

## Out of scope / ABI-frozen

- **Fence 1 (frozen ABI shape)**: `LEVELLIST`, `ENTITYTABLE`,
  `SAVERESTOREDATA`, `FIELDTYPE`, `TYPEDESCRIPTION` (vendored verbatim at
  `abi/eiface.hpp:131-215`); the seven `DLL_FUNCTIONS` save-callback
  slots and `pfnChangeLevel` (`eiface.h:429-437,462-468`). None of these
  may change shape, calling convention, or name.
- **Save-owned compat surface, not Fence-1 but still byte-pinned**: the
  on-disk GoldSrc save/level-transfer format —
  `GameHeader`/`SaveHeader`/`SaveLightStyle`/`SaveClient`/
  `SaveDecalEntry`/`SaveSoundEntry` (`format.hpp:88-368`) and the little
  -endian codec H-1 proposes deduplicating. `save` is **not** one of the
  five HB-2 byte-exact-kernel subsystems (map_loader/content/networking/
  server/utilities), so this is not a kernel-fence item in the HB-2 sense —
  but the on-disk format is still a real-world GoldSrc interop contract:
  field order, width, and endianness must not change, and H-1 must land as
  a verbatim relocation, never a "modernized" reformat.
- **`IDecalListProvider`/`IDynamicSoundsProvider`/`IMusicStateProvider`/
  `IMapValidityChecker`** (`private/save/client_state.hpp:66-101`,
  `private/save/save_comment.hpp:108`) — all zero production
  implementations today. These are **correctly-scheduled Chunk-12 doors**
  (`OBL-12-9` in the tree-wide lens pass names Chunk 12 as the supplier),
  not over-abstraction; do not delete them, and do not build production
  implementations ahead of their named Chunk-12 consumer.
- **`ISaveGlobalState`** (`container_codec.hpp`, wired to `nullptr`) — the
  physint global-state provider seam (`pfnSaveGlobalState`/
  `pfnRestoreGlobalState`). Scheduled to Chunk 11 (`OBL-11-3`); same rule
  as above.

## Open questions

- **Does the SaveStats door (M-1) get built as part of a future
  `diagnostics_dump` aggregator wave, or does the boundary-spec row get
  walked back to "Deferred, chunk-tagged" instead of "Yes"?** Either
  answer is fine; leaving the doc asserting a struct `grep` cannot find is
  not.
- **`save::IDynamicSoundsProvider` must not be retrofitted into a future
  HB-5 published-snapshot primitive if/when one lands.** The tree-wide
  lens pass flags it as the only in-tree *named* candidate Tier-A consumer
  for HB-5 (the tree's still-open "one shared published-snapshot idiom"
  backlog item), but its actual contract
  (`std::span<const SaveSoundEntry> dynamic_sounds()`,
  `client_state.hpp:81-93`) wants a borrow valid for one call —
  materialize-into-caller-storage — not a `Published<T>` snapshot read.
  Recorded here so a future HB-5 implementer does not reshape this
  interface to fit a primitive it was never designed against.
- **`docs/architecture/save/` does not exist.** `save` is one of four
  shipped subsystems (with `content`/`sound`/`input`) with no
  `/document-architecture` output yet, despite Chunk 8 being complete.
  This is outside a modernization report's scope to fix, but is flagged
  since a future architecture pass will need the H-1 dedup landed first to
  avoid documenting the duplicated codec sites as if they were the design.
