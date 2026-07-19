# map_loader Modernization Opportunities

> Authored 2026-07-06 (as-built pass).
> C++ standard in use: C++**23** (from `xash3dpp/src/map_loader/CMakeLists.txt`,
> `target_compile_features … cxx_std_23`; the tree-wide `CMAKE_CXX_STANDARD 23`).
> Boundary spec: `docs/boundaries/map_loader-boundary.md`
> Threading: `docs/threading-analysis/map_loader-threading.md`
> Deep dives: `docs/legacy-survey/deep-dive-bsp-loader.md`,
> `docs/legacy-survey/deep-dive-trace-pvs.md`
> ABI-frozen symbols in this subsystem: `k_contents_*` (BSP leaf/clipnode +
> game-DLL ABI, `int` per QG); the `k_surf_*` / `k_model_*` / `k_fworld_*` flag
> bits (game DLLs read them through the SDK `msurface_t`/`model_t`/`world`
> fields); the BSP disk-format struct layouts (`d*_t` in `disk_format.hpp`,
> `static_assert`-pinned); and — as a **behavioural** ABI — the Q-18
> float-/bit-exact trace, PVS and map-CRC results. None of the value types the
> queries take (`WorldData`, `TraceHull`, `TraceResult`, `PhsTable`) are
> ABI-frozen; they are `xash3dpp`-internal.

## Summary

map_loader is a large subsystem (10 TUs, six public headers) but was written
directly in modern C++23 and is already **highly modern** — there is no
legacy-C residue to convert. The BSP parser reads on-disk records through
`memcpy`-based `read_record<T>(std::span<const std::byte>, index)` /
`read_record_at<T>` helpers (no `reinterpret_cast` over lump arrays — the
alignment-safe pattern), validates with `static_assert`-pinned struct sizes and
a `std::endian::native == little` guard, propagates load failures with
`std::expected<WorldData, core::ErrorCode>` (no `Host_Error` process-kill), and
exposes the whole immutable world through `std::span` / `std::string_view` const
accessors. Queries take `const WorldData&` / non-owning `TraceHull` views and
caller-supplied `std::span` buffers; the legacy `g_visdata` / `pm_boxhull` /
`world.version` globals are gone (function-locals + a `BoxHull` value type +
width-normalized-at-load).

The result is that the usual modernization headline — raw casts over lump data,
C string handling, sentinel error returns — **does not apply**: those idioms
were never introduced. `compliance_scan.py map_loader` and `stub_scan.py
map_loader` are the mechanical checks; what remains after them is a short tail
of **cosmetic** cleanups plus one **load-bearing non-opportunity**: the Q-18
determinism constraint that forbids "modernizing" the trace/PVS/CRC math in any
way that changes the produced bits (the exact analog of the studio-math caution
in the utilities report).

The `string_view`→C-string `strnicmp`/`strncmp` over-read pattern that headlines
the utilities / filesystem / cmd_cvar reports is **absent** here — the two
candidate spots (texture-name matching, entity-key parsing) feed
NUL-terminated `std::string::c_str()` to the C-string comparators. See the
boundary §9 note.

______________________________________________________________________

## The Q-18 determinism constraint (do NOT "modernize" the math)

This is the single most important item and it is a **prohibition**, not an
opportunity. The trace kernel (`trace.cpp`), the PVS/PHS codecs and walks
(`pvs.cpp`, `phs.cpp`, `private/map_loader/fat_vis.hpp`) and the map CRC
(`bsp/map_crc.cpp`) are pinned float-/bit-exact to the legacy engine (Q-18
`PM_FP_MODEL`; `docs/design/pm-determinism-decision.md`; golden vectors +
verbatim-kernel cross-check gate them, 18,156 traces / 0 mismatches). The
following "clean-ups" would silently change results and must **not** be made:

- **No reassociation / FMA / `-ffp-contract` in the kernel.** `PlaneDiff`'s
  single-precision `DotProduct` (`n.x*p.x + n.y*p.y + n.z*p.z`), the axial fast
  path branch (`type < 3`), the `frac = (t1 ± DIST_EPSILON)/(t1 - t2)`
  crosspoint, the `frac -= 0.1f` back-up loop and the `VectorLerp` expansion are
  bit-load-bearing. Do not rewrite them with `std::transform_reduce`,
  `std::fma`, `std::inner_product`, or a "tidier" fused expression — any of
  those can reorder the adds or contract a multiply-add and move the last ULP.
- **No changing the on-plane tie-break.** `point_leaf` sends an on-plane point
  to the BACK child (`PlaneDiff <= 0`); the hull walkers send it FRONT
  (`< 0`). The asymmetry is deliberate and tested — do not "unify" it.
- **No touching the CRC fold.** The MP checksum is an **un-inverted** CRC-32
  over lumps 1..14 as stored on disk; the SP constant is `0x58415348`. These
  are wire-frozen (`sv.worldmapCRC`). Leave the polynomial and the
  no-final-XOR exactly as is.
- **No reordering the zero-RLE codec.** `decompress_pvs` / `compress_pvs`
  reproduce the legacy run-length stream byte-for-byte (incl. the 255-run cap
  and the exhausted-stream zero-fill hardening). A `std::ranges` rewrite is
  allowed only if it is proven to emit identical bytes.

Any future refactor that touches these files should re-run the golden fixtures
as the acceptance gate, exactly as the utilities studio-math kernels do.

______________________________________________________________________

## Implementation-status table

| Design element | Status | Notes |
|----------------|--------|-------|
| `std::expected` load-error propagation | **Implemented** | `load_world_data` → `std::expected<WorldData, ErrorCode>`; no `Host_Error` kill |
| `memcpy` record readers over `std::span<const std::byte>` | **Implemented** | `read_record<T>` / `read_record_at<T>`; alignment-safe, no array `reinterpret_cast` |
| `static_assert`-pinned disk layouts + LE guard | **Implemented** | every `d*_t` size-asserted; `std::endian::native` checked |
| Immutable `WorldData` with `std::span` / `std::string_view` accessors | **Implemented** | Safe-RO after load (Q-6) |
| Edict-free trace API (`TraceHull` / `TraceResult` value types) | **Implemented** | globals `pm_boxhull` / `world.version` eliminated |
| `enum class` flavours + typed flag constants | **Implemented** | `BspVersion`, `MapLoadState`; `k_surf_*` / `k_model_*` etc. |
| PHS as `map_loader` module (Q-19) | **Implemented** | `PhsTable` + `build_phs` / `fat_phs` / `headnode_visible` |
| `bit_cast` record reader variant | **Not implemented** | L-1 — marginal over the memcpy readers; source is a sub-span, not a value |
| `as_chars` byte→text view helper | **Not implemented** | L-2 — two documented `reinterpret_cast<const char*>` sites |
| `std::ranges` needle scan / fixed-name copy | **Not implemented** | L-3 — cosmetic |
| Node-tree traversal cycle-guard | **Not implemented** | Tracked as a Chunk 6 hardening follow-up in boundary §5, not a modernization item |

______________________________________________________________________

## Low-priority opportunities

All three are cosmetic — the current code is correct, `noexcept` and
alignment-safe. They are recorded for completeness, not because anything is
wrong.

### L-1: `std::bit_cast` record-reader variant (marginal)

- **File(s)**: `xash3dpp/include/xash3dpp/private/map_loader/bsp/bsp_loader.hpp`
  (`read_record<T>` / `read_record_at<T>`).
- **Current pattern**: `T out; std::memcpy(&out, bytes.data() + …, sizeof(T)); return out;`
- **Modernization**: `std::bit_cast<T>` is the C++23 idiom for byte-reinterpret,
  but it needs a value of a *same-sized source type*, whereas here the source is
  a slice of a larger `std::span<const std::byte>`. A `bit_cast` variant would
  only help at the fixed-size call sites that first copy into a
  `std::array<std::byte, sizeof(T)>` — which is exactly what `memcpy` already
  does in one step. **Verdict: leave as memcpy.** The helpers are the correct,
  documented, alignment-safe form; recording only so a future reader does not
  "discover" bit_cast and regress the ergonomics.

### L-2: `as_chars(std::span<const std::byte>)` view helper

- **File(s)**: `xash3dpp/src/map_loader/bsp/bsp_lumps.cpp` (entity lump +
  `.ent` patch assign, two `reinterpret_cast<const char*>` with SAFETY
  comments).
- **Modernization**: a one-line `[[nodiscard]] inline std::string_view
  as_chars(std::span<const std::byte> b) noexcept { return { reinterpret_cast<
  const char*>(b.data()), b.size() }; }` (or `std::bit_cast` of the pointer)
  would centralise the byte→char aliasing behind one audited seam and let the
  call sites read `w.entities_.assign(as_chars(lv->bytes))`. Pure readability;
  the aliasing itself is well-formed (`std::byte`/`char` may alias any object).

### L-3: `std::ranges` for the classname scan and the miptex-name copy

- **File(s)**: `xash3dpp/src/map_loader/bsp/bsp_loader.cpp` (the
  `reinterpret_cast<const std::byte*>` classname-needle search) and
  `bsp/bsp_flags.cpp:108` (`std::memcpy(name, mip.name, 16)`).
- **Modernization**: the needle search could be `std::ranges::search` over two
  byte spans; the fixed 16-byte miptex-name copy could be `std::ranges::copy`
  into a `std::array<char, 16>`. Both are cosmetic and neither is on a hot path
  (load-time only).

______________________________________________________________________

## Deliberately NOT opportunities

- **The `d*_t` disk structs keep the legacy spelling and raw field types.**
  They are format-frozen POD mirrors of `bspfile.h` (same rule as
  `include/xash3dpp/abi/`); the 1:1 name mapping is what keeps the parser
  greppable against the legacy source. Do not `enum class` the lump indices or
  rename the fields.
- **The trace/PVS/CRC math is C-shaped on purpose.** See the determinism
  section — the "C-looking" float expressions are the parity contract.
- **`k_contents_*` stay `int`.** They are the BSP + game-DLL ABI values (QG);
  an `enum class` here would fight the frozen SDK. Named `inline constexpr int`
  is the agreed form.
- **No stats tier.** Map loads are cold-path; the subsystem exposes no always-on
  counters by design (boundary §7, threading notes). Not a gap.
- **`std::expected` is already the error model.** There is no
  sentinel-return or `Host_Error` residue left to modernize.

______________________________________________________________________

## Testing note

Any change under this subsystem — even a cosmetic L-item that touches
`trace.cpp` / `pvs.cpp` / `phs.cpp` / `bsp/map_crc.cpp` — must be gated by the
existing golden fixtures (trace goldens + verbatim-kernel cross-check, map-CRC
vectors, the per-quirk BSP regression fixtures) before and after. Parser-only
cleanups (L-2 / L-3) are covered by the quirk-fixture suite. The pure
`as_chars` helper (L-2), if added, is trivially unit-testable in isolation.
