# Content Modernization Opportunities

> Refreshed 2026-07-06 (as-built pass). The original forward-looking audit is
> preserved below (its `H-*`/`M-*`/`L-*`/`O-*` items are unchanged as the
> *rationale* record); an `As-built status refresh` section was inserted after
> the Summary reconciling each item against the shipped 13-TU code, adding the
> Q-18 bone-math bit-exactness prohibition, and recording the `strnicmp`
> over-read verdict.

> Refreshed 2026-07-20 (tree-wide modernization audit, adversarially
> verified). Added six new findings (H-6, H-7, M-7..M-13, L-7..L-12) sourced
> from the audit's `content` and `imagelib` packs plus the L2 (registry
> unification), L9 (renderer-packet prep) and L11 (subtraction) cross-cutting
> lenses; **H-7 is a pure-deletion finding promoted a tier per the
> anti-accretion/subtraction rule** (two never-allocated memory pools whose
> own doc contradicts itself about them). One prior finding's tier
> interpretation is corrected in place (F29 → M-9: the 12 Main-thread pinning
> sites are **permanent under P-2**, not removable by it — the opposite of
> how a published-snapshot door is usually read). One candidate finding
> (`IBoneSolver` builtin-singleton accessor) was investigated and refuted —
> see "Investigated and refuted" before Open questions. No prior item was
> renumbered or deleted.

> C++ standard in use: C++**23** (from `xash3dpp/CMakeLists.txt`)
> Boundary spec: `docs/boundaries/content-boundary.md`
> ABI-frozen symbols in this subsystem: `model_t`, `studiohdr_t` (+ all
> `mstudio*`), `msprite_t`/`mspriteframe_t`, `aliashdr_t`/`trivertex_t`,
> `cache_user_t` (`common/com_model.h`, `engine/studio.h`); the DLL
> function-pointer tables (`ref.dllFuncs`, `svgame.physFuncs`,
> `gStudioAPI`/`sv_blending_interface_t`); the `GAME_EXPORT` studio-cache
> callbacks. **Needs verification**: `rgbdata_t`/`pixformat_t`/`imgFlags_t`
> (`common/com_image.h`) — crosses `ref_api.h` and the mainui SDK (boundary
> OQ-1).

## Summary

Content has **no `xash3dpp/` code yet** (Chunk 7 is unscaffolded), so this is a
*forward-looking* audit: it maps the C idioms in the legacy reference
(`engine/common/model.c`, `mod_studio.c`, `mod_sprite.c`, `mod_alias.c`,
`imagelib/*`) to the C++23 shapes the rewrite should adopt, and flags the
extension-posture doors the rewrite must not close. The legacy code is heavily
C: a single global `imglib_t image` scratch struct makes the entire codec layer
non-reentrant, a `mod_studiohdr` singleton does the same for studio queries,
every loader reports success through a `qboolean *loaded` out-parameter while
writing results into globals, and the ~182 hand-rolled byte-swaps in the studio
and sprite parsers plus the `le_struct_*` swap tables are pure boilerplate under
C++23's `std::byteswap`. The three biggest wins are structural and reinforce
each other: **(1)** delete the global singletons in favour of injected context
(P-3 — the prerequisite for the Chunk-7 worker pool this chunk is hooked to),
**(2)** give loaders a `std::expected<T, LoadError>` contract, and **(3)** parse
untrusted file bytes through `std::span`/`std::mdspan` instead of raw `byte*` +
manual size arithmetic (the classic image-decoder overread surface). The
dominant constraint is that the *in-memory result structs* are frozen ABI — the
rewrite modernises the **process** (scratch, dispatch, error handling, pixel
access) around unchanged **data** shapes, and `rgbdata_t` itself needs an ABI
decision (OQ-1) before its internal representation can be modernised. Above the
idiom swaps sits a **design-paradigm (OOP)** layer — the `ModelCache` /
`IImageCodec` / `StudioView` shapes below — which is the project-sanctioned form
here (Q-22 / P-7 / Q-11) rather than gratuitous class-ification, subject to the
engine's `/GR-` (no RTTI) and `/EHs-c-` (no exceptions) constraints.

______________________________________________________________________

## As-built status refresh (2026-07-06)

**Superseded 2026-07-06:** the Summary above opens "Content has *no `xash3dpp/`
code yet*". That is no longer true — content is **Complete (13 TUs)** per
`status_table.py`. The forward-looking audit was **substantially realised**: the
structural wins it prioritised are all shipped. Statuses below; the H/M/L/O item
bodies are kept as the rationale record.

### High tier — all Implemented

| Item | Status | As-built evidence |
|------|--------|-------------------|
| **H-1** delete global scratch singletons | ✅ **Implemented** | **Zero file-scope mutable state** across all 13 TUs (content-threading.md). `mod_known`→`ModelCache::Impl::slots_`; `mod_studiohdr`→`StudioView` value; `image`→codecs return owned `Image`; `g_poseverts`→local scratch |
| **H-2** `qboolean *loaded` → `std::expected` | ✅ **Implemented** | Every loader/decoder returns `Result<T>` (`Result<StudioModel>`, `Result<Image>`, `ModelCache::load_from_bytes → Result<void>`); the `crash`/`*loaded` channels are gone |
| **H-3** hand-rolled byte-swap → `std::byteswap` | ✅ **Implemented** | Parsers read through `utilities::read_le<T>` (the `std::byteswap`-backed helper); no bespoke `le_struct_*` swap tables in the rewrite |
| **H-4** raw `byte*` + size math → `std::span` | ✅ **Implemented** | All codecs + parsers take `std::span<const std::byte>`; every source read is bounds-checked (e.g. `codec_tga.cpp`, `codec_mip.cpp` mip-run guard). `std::mdspan` pixel views are a Chunk-13 renderer refinement |
| **H-5** manual `Mem_*` → pool RAII | ✅ **Implemented** | `ModelCache`/`ImageDecoder` own a `memory::PoolHandle` (create/destroy in init/shutdown); variable-length buffers are `std::vector<std::byte>` |

### Medium tier

| Item | Status | Note |
|------|--------|------|
| **M-1** flag `#define`s → `enum class` | ✅ **Implemented** | `enum class NeedLoad`, `enum class CrcFlags` (+ bitwise ops), `enum class ModelType` |
| **M-2** plain `enum`/`typedef struct` → scoped | ✅ **Implemented** (content); imagelib enums land with the format types | |
| **M-3** fn-ptr dispatch table → typed registry | ✅ **Implemented** as the O-2 hierarchy — `IImageCodec* const registry[]` walked by `handles(ext)` (chose the hierarchy over the flat table) | |
| **M-4** static tables → `constexpr std::array` | ✅ **Implemented** | `palette.cpp` `k_palette_q1`/`k_palette_hl` are `constexpr std::array<…,768>` |
| **M-5** lazy-init guards → magic static | ✅ **Implemented** (moot) | palettes are `constexpr` — no runtime build, no guard bool |
| **M-6** `const char*` → `std::string_view` | ✅ **Implemented** | loader/lookup entry points take `std::string_view` |

### Low tier — Implemented where the code exists

`L-1` (`nullptr`), `L-2` (`std::min/max/clamp` — `clampf` in `bone_solver.cpp`
is the *deliberate* exception, a bit-exact `bound()` transcription, see below),
`L-3`/`L-5`/`L-6` all follow rewrite house style. `L-4` (`std::bit_cast` pixel
puns) lands with the renderer image-lump path (Chunk 13).

### OOP / design — all shipped

| Item | Status | As-built |
|------|--------|----------|
| **O-1** `ModelCache` class | ✅ | pimpl class; `needload` FSM + slot-0-world + purge as invariants; `model_infos()` = P-4 surface |
| **O-2** `IImageCodec` registry | ✅ | stateless codecs + owned `Image` result; 7 codecs registered |
| **O-3** format dispatch: hierarchy or `variant` | ✅ | `std::variant<monostate, StudioModel, SpriteModel, AliasModel>` payload + magic `switch` in `load_from_bytes` (chose `variant`, per the recommendation) |
| **O-4** `StudioView` typed accessor | ✅ | `StudioView` + 6 sub-views over the frozen `studiohdr_t`; the G-2 confinement |
| **O-5** injected DLL seams | ◑ Partial | `IModelPostProcess` (OQ-4) + `IBoneSolver` (OQ-5) shipped; `content::ICompatPolicy` (Q-12) is a remaining door |

### Remaining / deferred (renderer Chunk 13)

The `img_utils.c` MDL/SPR/LMP/FNT/PAL **image-lump** codecs, `Image_Process`
(resample / flip / quantise / NeuQuant), the internal `Image`↔`rgbdata_t`
adapter (OQ-1), and the `content → imagelib` + `content → map_loader` CMake
links are the renderer's chunk, not content debt. One live stub:
`imagelib.cpp:42` (tag `o-2`) is a **stale scratch-comment** — the stateless
codecs need no shared decode scratch, so the TODO describes a design the code
already obviated; recommend deleting the comment.

### NEW — Q-18 bone-math bit-exactness PROHIBITION (do-not-modernize)

The studio bone kernel (`bone_solver.cpp` `calc_bones` / `calc_bone_adj` /
`calc_rotations`, and the `utilities` quaternion/matrix primitives it drives) is
**bit-exact against the Q-18 verbatim-legacy goldens**
(`tests/goldens/studio_math_goldens.inc`). This is a **hard no-touch class**,
the same class as `map_loader`'s trace/PVS/CRC math and `networking`'s wire
codecs:

- **Do NOT** apply `std::min/max/clamp` to the float ops — `clampf` and the
  hand-written RLE `bound()`/lerp gate are deliberate transcriptions; a library
  `std::clamp` can differ on NaN/`-0.0` edge cases.
- **Do NOT** reassociate, FMA-fuse, or `-ffast-math` the `AngleQuaternion` /
  `QuaternionSlerp` / matrix-concat float arithmetic. The exact-float
  `VectorCompare` gate (lerp vs copy) depends on identical rounding.
- **Do NOT** replace the merged position+rotation RLE decompressor with a
  "cleaner" `std::ranges` walk — the span-walk order is observable through the
  goldens.
- The hardening guards added (zero/out-of-range RLE span, controller-byte
  bounds) never fire on well-formed data, so parity holds — **do not remove them
  either** (they are the untrusted-file safety net).

Any future worker-pool port (P-1) must preserve this: reentrant is fine,
float-reordered is not.

### `strnicmp` string_view→C-string over-read — ABSENT

The cross-subsystem over-read pattern (a non-NUL-terminated `string_view` fed to
a C-string `strnicmp`/`strncmp`, present in utilities `M-4` / filesystem `M-7` /
cmd_cvar `M-5`) is **absent in content**, despite texture/bone/bodygroup name
matching being the flagged candidate. Name comparison uses `std::string_view ==
std::string_view` over an owned `std::string` (`ModelCache::find` /
`find_or_alloc` / `validate_crc`); the codec prefix tests
(`codec_mip.cpp` `istarts_with`, `imagelib.cpp` `extension_of`) are
length-bounded loops. No new item — recorded as a negative data point for the
Phase-14 sweep (content joins platform/core/host/abi/launcher/map_loader/
networking on the absent side).

______________________________________________________________________

## High-priority opportunities

### H-1: Delete the global scratch singletons — inject context `[EXT:P-3]`

- **File(s)**: `engine/common/imagelib/img_main.c:23` (`imglib_t image`);
  `engine/common/imagelib/imagelib.h:59-98` (its type); `mod_studio.c:46`
  (`mod_studiohdr`), `:45` (`pBlendAPI`), `:41-59` (trace-cache statics);
  `model.c:27-30` (`mod_known[]`, `mod_numknown`, `com_studiocache`);
  `mod_alias.c:25-26` (`g_poseverts`, `g_posenum`); `mod_local.h:134` (`world`).

- **Current pattern**: every codec writes its result into the one file-scope
  `image` struct, which `ImagePack` then snapshots; every studio query rewrites
  the `mod_studiohdr` pointer and file-static bone/hull scratch.

  ```c
  imglib_t image;                       // img_main.c:23 — global decode scratch
  static studiohdr_t *mod_studiohdr;    // mod_studio.c:46 — current-model singleton
  ```

- **Suggested replacement**: an injected `ImageDecoder` (owns the former
  `image` fields) and a per-call `StudioQuery`/model-handle carrying the
  `studiohdr` and bone scratch. No file-scope mutable state beyond the
  documented ABI exceptions.

  ```cpp
  auto rgba = decoder.load( name, bytes );          // decoder owns scratch
  StudioQuery q{ model };  auto hull = q.hull_for( frame, seq, … );
  ```

- **Boundary-safe**: Yes (the singletons are engine-internal; the frozen result
  structs are unchanged).

- **Rationale**: This is the boundary spec's headline P-3 debt. The globals make
  the whole subsystem non-reentrant, which directly blocks the worker pool +
  `JobToken` that Chunk 7 is hooked to land (`extension-goals.md §4`, §P-1). A
  door, promoted to the top of the High tier by Q-21. Every other structural
  finding (H-2, H-4) depends on this landing first.

### H-2: Loader contract — `qboolean *loaded` out-param → `std::expected` `[EXT:P-3]`

- **File(s)**: `mod_local.h:168,173,188,205` (`Mod_LoadAliasModel`/`Brush`/
  `Studio`/`Sprite` all `(model_t*, void*, size, qboolean *loaded)`);
  `imagelib.h:152-171` (every `qboolean Image_LoadXXX(name, buffer, filesize)`
  returning success while filling the global `image`); `model.c:299`
  (`Mod_LoadModel(mod, qboolean crash)`).

- **Current pattern**: success via out-parameter / return-code, result via
  global, and a `crash` bool deciding whether failure aborts.

  ```c
  void Mod_LoadStudioModel( model_t *mod, void *buffer, size_t size, qboolean *loaded );
  qboolean Image_LoadDDS( const char *name, const byte *buffer, fs_offset_t filesize );
  ```

- **Suggested replacement**: loaders return `std::expected<T, LoadError>`
  (C++23); the caller branches on `.has_value()`, so the `crash`/`*loaded`
  channels disappear (category 2-H).

  ```cpp
  std::expected<StudioModel, LoadError> load_studio( std::span<const std::byte> file );
  std::expected<RgbData,      ImageError> load_dds  ( std::string_view name,
                                                      std::span<const std::byte> file );
  ```

- **Boundary-safe**: Yes (internal signatures; `loadpixformat_t` in
  `imagelib.h:46` is imagelib-private, not a shared header).

- **Rationale**: Removes three parallel error channels, makes "no result" a
  type-system fact, and is the context-first entry shape (P-3) a worker job
  needs. Pairs with H-1: an `expected`-returning loader has nowhere to hide a
  global write. This return type is also the no-throw factory contract the
  OOP shapes rely on (O-1, O-2, P-7's `create_<thing>`).

### H-3: Hand-rolled byte-swap boilerplate → `std::byteswap`

- **File(s)**: `mod_studio.c` (~149 swap/`le_struct_*` sites incl. the
  `Mod_SwapStudioModel` tables `:62-205`); `mod_sprite.c` (~33 sites,
  `Mod_SwapSprite`/`…Frame`/`…Group`); the `Little*/Big*` macros in every
  imagelib codec header parse (`img_bmp/tga/dds/ktx2.c`).

- **Current pattern**: per-field manual endian swaps and bespoke struct
  swap-table machinery (validation interleaved with swapping).

  ```c
  phdr->numbones = LittleLong( phdr->numbones );   // ×hundreds, mod_studio.c
  ```

- **Suggested replacement**: a typed `read_le<T>(span, offset)` helper built on
  `std::byteswap` (C++23) — the swap tables collapse into a header-walk, and on
  LE targets the swap is a compile-time no-op (category 2-J deletable helper +
  2-G).

  ```cpp
  template <std::integral T>
  T read_le( std::span<const std::byte> s, size_t off ) {
      T v; std::memcpy( &v, s.data()+off, sizeof v );
      if constexpr( std::endian::native == std::endian::big ) v = std::byteswap( v );
      return v;
  }
  ```

- **Boundary-safe**: Yes.

- **Rationale**: Deletes hundreds of error-prone lines in the hot load path,
  and folds the swap+validation into one bounds-checked read. `std::byteswap`
  is exactly the missing-feature this boilerplate papered over.

### H-4: Raw `byte*` + manual size math in codecs → `std::span` / `std::mdspan`

- **File(s)**: `imagelib.h:73` (`uint ptr; // safe image pointer` + `size_t
  size; // for bounds checking` — the hand-rolled bounds guard); the pixel
  loops in `img_utils.c` (`Image_Copy8bitRGBA:573`, `Image_Decompress:1217`,
  the resample kernels `:708-1050`, `Image_GenerateMipmaps:1478`) and every
  codec's decode loop.

- **Current pattern**: pointer + separate width/height/bpp/size, with a
  manually-maintained "safe image pointer" and open-coded bounds checks while
  parsing **untrusted file data**.

  ```c
  byte *rgba; size_t size; uint ptr;   // imglib_t — manual bounds tracking
  ```

- **Suggested replacement**: `std::span<const std::byte>` for the input file
  and output buffer (bounds carried with the pointer); a 2D
  `std::mdspan<byte, extents<…,dyn,dyn>>` view for width×height pixel access
  where the toolchain ships `<mdspan>` (category 2-C).

  ```cpp
  std::span<const std::byte> file = ...;         // input, bounds-checked
  std::mdspan pixels( out.data(), height, width * bpp );
  ```

- **Boundary-safe**: Yes for the internal decode buffers; the `rgbdata_t`
  *result* type is **Needs verification** (OQ-1).

- **Rationale**: Image/model parsers consume attacker-controllable file bytes —
  overreads here are the canonical decoder-CVE class. `span`/`mdspan` make the
  bounds part of the type instead of a hand-maintained `size` field.

### H-5: Manual `Mem_Malloc`/`Mem_Free` lifetime → pool-backed RAII (Q-22/P-7)

- **File(s)**: `imagelib/*.c` (~40 `Mem_Malloc`/`Mem_Calloc`/`Mem_Realloc`
  sites); `model.c:146` (per-model `mempool` freed by hand in `Mod_FreeModel`);
  `mod_studio.c` texture-merge copies; the `FS_LoadFile`→`Mem_Free` file
  buffers in every loader entry.

- **Current pattern**: pooled `Mem_*` allocation with manual `Mem_Free` on each
  early-return error path.

  ```c
  byte *buf = Mem_Malloc( host.imagepool, size );
  ... if( bad ) { Mem_Free( buf ); return false; }   // repeated per error path
  ```

- **Suggested replacement**: pool-backed `std::vector<std::byte>` for
  variable-length buffers and the pool-owned-class idiom
  (`create_pool`/`pool_new` + `unique_ptr` with the `operator delete` →
  `mem_free` override) for owning types — per **Q-22/P-7**, *not* raw
  `std::make_unique`/global `new`. The `FS_LoadFile` buffer becomes the
  filesystem `File` RAII wrapper that already exists (category 2-B/2-D).

- **Boundary-safe**: Yes.

- **Rationale**: Removes leak-on-error-path bugs from file parsers that have
  many early returns, while keeping every allocation accounted to a pool
  (Q-2). Constraint to verify: `pool_new<T>` requires `alignof(T) ≤ 8` — check
  any SIMD-aligned pixel scratch before mandating pool ownership for it.

### H-6: Frozen studiohdr offset table hand-transcribed in three unlinked copies `[EXT:G-2,P-4]`

- **File(s)**: `xash3dpp/src/content/model/studio.cpp:19-76` (the 30-constant
  `kOff*`/`kBone*`/`kBc*`/`kSeq*`/`kAtt*`/`kHb*` table, anonymous-namespace,
  what `StudioView` and all six sub-views actually parse with);
  `xash3dpp/tests/content/test_studio_layout.cpp:57-62` (26 `HDR_OFF` +
  12 `offsetof` static_asserts, a second hand-typed copy of the same
  integers); `xash3dpp/tests/content/studio_builder.hpp:48-54` and
  `xash3dpp/tests/content/test_studio_bones.cpp:206-209` (a third,
  test-fixture copy).

- **Current pattern**: the `HDR_OFF(ident, 0)` macro expands to
  `static_assert(offsetof(legacy::studiohdr_t, ident) == 0)` — it never
  references `studio.cpp`'s `kOff*` table, because that table sits in an
  anonymous namespace nothing outside the TU can name. The tripwire's own
  comment claims the expected values are "an INDEPENDENT transcription that
  must equal both the legacy `offsetof` AND the file-local `kOff*`
  constants," but that second half is enforced only by human discipline: a
  typo in `studio.cpp`'s `kOffNumSeq` produces zero test failures.

- **Suggested replacement**: move the `kOff*`/`kBone*`/`kBc*`/`kSeq*`/
  `kAtt*`/`kHb*` block out of `studio.cpp`'s anonymous namespace into
  `xash3dpp/include/xash3dpp/content/studio.hpp`, directly beside the
  `k_studio_*_stride` constants that are already public there and already
  shared with the test builder (`studio_builder.hpp:48` uses
  `k_studio_bone_stride`). Rewrite the tripwire to assert
  `offsetof(legacy::studiohdr_t, numbones) == xash::content::kOffNumSeq`
  (etc.) directly, so a mismatch between the parser's constant and the
  legacy struct fails to compile instead of silently diverging.

- **Boundary-safe**: NeedsVerification. `studiohdr_t`/`mstudio*_t` are
  defined in `engine/studio.h`, not literally one of the brief's named
  frozen-ABI paths, so `touches_frozen_abi:true` is defensible-but-imprecise
  — more precisely this offset table **is** the byte-layout substrate the
  content "studio bone math" HB-2 kernel reads through, so treat it as
  kernel-adjacent. That reclassification does not change the outcome:
  moving and deduplicating identical integer literals is zero arithmetic
  reordering and closes a silent-divergence hole rather than opening one —
  exactly the kind of change the byte-exact rule wants (it protects the
  kernel instead of touching it), but land it with a full studio-goldens
  run before/after as the verification step.

- **Rationale**: Three unlinked transcriptions of the same 30+ offsets is
  the largest single duplication cluster in content (blast radius 88 per
  the campaign's grep), and it is the one place in the subsystem where a
  typo produces a green test suite. Consolidating to one source of truth
  the tripwire actually references converts "reviewer must notice a
  hand-typed mismatch" into "compiler rejects it."

### H-7: Delete the two content-side memory pools that are created, destroyed, and never allocated from (promoted from Medium by the subtraction lens)

- **File(s)**: `xash3dpp/src/content/model/model_cache.cpp:6,84-90`
  (`PoolHandle pool_` created in `init()`, destroyed in `shutdown()`);
  `xash3dpp/src/content/imagelib/imagelib.cpp:39-45,52-67` (same shape for
  `ImageDecoder::Impl::pool_`); `xash3dpp/docs/boundaries/content-boundary.md:148`
  (asserts "all allocation routes through" the pools).

- **Current pattern**: both pimpls hold a `PoolHandle pool_` member that is
  created via `xash::memory::create_pool(...)` in `init()` and destroyed via
  `destroy_pool()` in `shutdown()`, and referenced nowhere else — grepping
  `pool_` returns exactly 6 hits in each file, all lifecycle. Every actual
  allocation (the whole-file `std::vector<std::byte>` model/image buffers,
  the 4096-entry slot vector, per-slot `std::string` names) goes through the
  CRT-heap default allocator, not the pool. Two file-header comments and
  `content-boundary.md:148` state the opposite.

- **Suggested replacement**: delete the `pool_` member and its
  create/destroy calls from both pimpls; make `init()` infallible (or
  fallible on something real) now that it does no allocation; correct the
  two file-header comments and `content-boundary.md:148` to say allocation
  is CRT-heap `std::vector` today. Re-introduce a pool only together with
  the first allocation that actually uses it.

- **Boundary-safe**: Yes. No bone-math kernel, no `eiface`/`edict`/`cdll`
  ABI struct, no `xash3dpp/include/xash3dpp/abi/**` file is involved — pure
  dead-state deletion plus a doc correction.

- **Rationale**: This is a documented case of the tree's "inert
  declarations" pattern (the audit's L11 subtraction lens; the identical
  shape recurs at `map_loader.cpp:92`, tracked in that subsystem's own
  report) — allocation infrastructure that has never allocated a byte, next
  to a doc that asserts the opposite. `content-boundary.md:148` is the more
  serious half: a reader trusting it believes content's memory is
  pool-accounted, and it is not. Promoted to High because it is a pure
  deletion with zero design decision required (per the campaign brief's
  "prefer deletions" rule) and because it corrects a false claim in the
  subsystem's own boundary spec.

______________________________________________________________________

## Medium-priority opportunities

### M-1: Internal flag `#define`s → `enum class` + operators

- **File(s)**: `mod_local.h:57-60` (`NL_*` needload states), `:47-48`
  (`FCRC_*`); `imagelib.h:109-126` (`LUMP_*`, `PAL_*` anonymous enums); the
  imagelib-internal use of `IL_*`/`IMAGE_*` process flags.

- **Current pattern**: `#define NL_PRESENT 2` etc., stored in the frozen
  `model_t.needload` (a `qboolean`) and combined as `int`.

- **Suggested replacement**: `enum class NeedLoad : int`, `enum class CrcFlags`,
  `enum class LumpRender`; add bitwise operators for the flag sets. Convert at
  the frozen field with `std::to_underlying` (C++23) so the ABI storage is
  unchanged (category 2-E).

- **Boundary-safe**: Yes for the internal enums; **Needs verification** for any
  `imgFlags_t` value that also crosses `ref_api.h` (OQ-1) — wrap the internal
  use, keep the shared enum.

- **Rationale**: Type-safe states/flags; makes the `needload` FSM (a boundary
  quirk) explicit instead of magic integers in a `qboolean`.

### M-2: Plain `enum` / `typedef struct` → `enum class` / plain `struct`

- **File(s)**: `imagelib.h:25-44` (`side_hint_t`, `image_hint_t` plain enums;
  `loadpixformat_t`/`savepixformat_t` `typedef struct`); `com_model.h:42`
  `modtype_t` (internal use only).

- **Current pattern**: `typedef enum {…} image_hint_t;` and `typedef struct
  loadformat_s {…} loadpixformat_t;`.

- **Suggested replacement**: `enum class ImageHint`, `enum class CubeSide`;
  drop the `typedef struct` in favour of plain `struct LoaderDesc` (categories
  2-E, 2-I).

- **Boundary-safe**: Yes (imagelib-internal).

- **Rationale**: Scoped enums prevent the hint/rendermode int-mixing the codecs
  currently rely on.

### M-3: Function-pointer dispatch tables → typed registry

- **File(s)**: `imagelib.h:46-62` (`loadpixformat_t`/`savepixformat_t` + the
  `loadformats`/`saveformats` table pointers); `img_utils.c:96-134`
  (`load_game[]`/`save_game[]`).

- **Current pattern**: `{ const char *ext; qboolean (*loadfunc)(…); hint }`
  arrays iterated by string compare.

- **Suggested replacement**: `struct LoaderDesc { std::string_view ext;
  LoadFn fn; ImageHint hint; };` in a `std::array`/`std::span`; the ext lookup
  becomes a ranges find (categories 2-F, 2-I). Keep it a plain table — do **not**
  reach for `std::function` (cold path, but no need for type-erasure here).

- **Boundary-safe**: Yes (this table is imagelib-private, unlike the DLL vtables
  which are frozen — see Out of scope).

- **Rationale**: `string_view` ext + typed descriptor removes the C-string
  compares and the parallel null-table shims (`load_null`/`save_null`). **O-2**
  is the polymorphic-hierarchy (`IImageCodec`) form of this same dispatch — pick
  one; the table suffices if codecs never need to register dynamically.

### M-4: Static palette / lookup / quantizer tables → `constexpr std::array`

- **File(s)**: `img_utils.c:30` (`palette_q1[768]`), `:57` (`palette_hl[768]`),
  `:23-25` (`d_8to24table` etc.); `img_quant.c:63-64` (`network[netsize][4]`,
  `netindex[256]`); `img_main.c:76` (`PFDesc[]`).

- **Current pattern**: `static byte palette_q1[768] = { … };` and runtime-built
  lookup tables guarded by init bools.

- **Suggested replacement**: `constexpr std::array<...>` where the data is
  compile-time constant (the stock palettes, `PFDesc`); `std::array` members for
  the quantizer's mutable network (category 2-C).

- **Boundary-safe**: Yes.

- **Rationale**: Bounds-checked, no separate size constant, and the stock
  palettes become compile-time data.

### M-5: `bool …init` lazy-init guards → magic static

- **File(s)**: `img_utils.c` (`q1palette_init`, `hlpalette_init` guarding
  `Image_GetPaletteQ1`/`…HL`).

- **Current pattern**: `if( !q1palette_init ) { build(); q1palette_init = true; }`.

- **Suggested replacement**: a function-local `static` (C++11 thread-safe magic
  static) or, better, a `constexpr` table that needs no runtime build
  (category 2-D).

- **Boundary-safe**: Yes.

- **Rationale**: Removes the mutable global flags (small P-3 win) and the
  first-call data race.

### M-6: View-only `const char *` params → `std::string_view`

- **File(s)**: loader entry points (`Mod_FindName`, `Mod_ForName`,
  `FS_LoadImage`, every `Image_LoadXXX name`) and `Mod_StudioTexName`.

- **Current pattern**: `const char *name` used only for compare/lookup.

- **Suggested replacement**: `std::string_view` at the API (category 2-A).

- **Boundary-safe**: **Needs verification** where the value is forwarded to a
  C API needing NUL-termination (FS/format calls) — the xash3dpp filesystem
  already takes `string_view`, so most convert cleanly.

- **Rationale**: Non-owning intent in the type; avoids re-`strlen`.

### M-7 (new 2026-07-20): imagelib's codec dispatch has no key column — it is the tree's third, and only keyless, extension→codec shape

- **File(s)**: `xash3dpp/include/xash3dpp/private/imagelib/codec.hpp:26` (pure
  virtual `IImageCodec::handles`) and `:36-44` (the 7 accessor
  declarations); `xash3dpp/src/content/imagelib/imagelib.cpp:25-31,80-90`
  (`extension_of` + 16-char lowercase buffer), `:94-115` (function-local
  `static const IImageCodec *const registry[]`, linear-scanned with one
  virtual call per candidate); the 7 one-line `handles()` overrides
  (`codec_bmp.cpp:306`, `codec_dds.cpp:394`, `codec_ktx2.cpp:240`,
  `codec_mip.cpp:288`, `codec_png.cpp:418`, `codec_tga.cpp:228`,
  `codec_wad.cpp:141`).

- **Current pattern**: every codec's `handles(ext)` is a single equality
  against one string literal, so the pure-virtual predicate carries exactly
  one bit of information a table column would carry better; a codec
  declared in `codec.hpp` but omitted from the function-local `registry[]`
  compiles, links, and silently never dispatches. The hand-rolled
  `extension_of`/lowercase step also has a real defect: `n = ext.size() <
  lo.size() ? ext.size() : 0` yields the **empty** extension (not a
  truncated one) for any extension ≥16 characters, so such a file is
  misclassified as `UnknownFormat` rather than merely unmatched.
  `xash3dpp_private_filesystem`'s `k_archive_types`
  (`archive_registry.hpp:23-44`) and sound's `k_audio_codecs`
  (`private/sound/codec.hpp:87-99`, whose own comment says it "mirrors
  archive_registry.hpp's `k_archive_types` shape") both already use a
  `{extension, accessor}` constexpr table; imagelib is the odd one out
  among three sibling registries.

- **Suggested replacement**: add `struct ImageCodecEntry { std::string_view
  key; const IImageCodec &(*codec)() noexcept; };` plus `inline constexpr
  std::array<ImageCodecEntry, 7> k_image_codecs` to
  `private/imagelib/codec.hpp` directly below the 7 accessor declarations
  (so the table and the declarations cannot drift apart unseen), mirroring
  `k_audio_codecs` field-for-field. Replace the `registry[]` scan in
  `imagelib.cpp:94-115` with a `xash::utilities::ci_equal` scan of that
  table. Delete `IImageCodec::handles` and all 7 overrides. Delete the
  local `extension_of` and its 16-char buffer in favour of
  `xash::utilities::file_extension` (`path.hpp:19`) — imagelib already
  links `xash3dpp_utilities` PUBLIC (`content/CMakeLists.txt:34`).

- **Boundary-safe**: Yes — this dispatch table is imagelib-private, not one
  of the frozen DLL vtables (see Out of scope).

- **Rationale**: `ImageDecoder::decode` (`imagelib.cpp:104`) is the same
  function this restructuring touches, and the Chunk-13 renderer is the
  downstream asker — the consumer already exists in-tree even though
  `xash3dpp_imagelib` today only has test linkage. This is also the one
  dispatch site in the tree with no key at all, so nothing outside can ask
  "is `.png` an image format?" without instantiating all 7 codec
  singletons and running a decode probe. Land it before Chunk 13 links
  `xash3dpp_imagelib` — the blast radius (9 production sites today) only
  grows once the renderer is a second caller. Best sequenced together with
  the parallel `HB-13` conversions in filesystem/sound so the tree settles
  on one extension-keyed dispatch idiom instead of three.

### M-8 (new 2026-07-20): content-boundary.md self-contradicts on the content→imagelib CMake link status, and the code/comment cite a chunk that already shipped

- **File(s)**: `xash3dpp/docs/boundaries/content-boundary.md:341-345`
  ("deferred to Chunk 13") vs `:424-433` ("✅ RESOLVED (implemented)" /
  "activated"); `xash3dpp/src/content/CMakeLists.txt:62-64` and
  `xash3dpp/include/xash3dpp/content/content.hpp:55-56` (both still say
  `TODO(Chunk 7)`); `xash3dpp/docs/implementation-plan.md:205` (the
  authoritative, frozen-numbering plan, assigns this exact link to
  Chunk 13, and Chunk 7 already shipped per the same plan).

- **Current pattern**: the boundary doc's own OQ-3 entry says the link is
  resolved and activated in one section and deferred to Chunk 13 (not yet
  wired) 80 lines later, while the actual `CMakeLists.txt`/`content.hpp`
  TODO comments cite a third answer — a chunk number that already shipped,
  which makes the obligation ungreppable by chunk. Grep confirms zero
  content `.cpp` currently includes an imagelib header, so the "resolved"
  half is the one that is wrong.

- **Suggested replacement**: doc-and-comment correction only, no behaviour
  change — reword the OQ-3 entry in `content-boundary.md` to "decided, not
  yet built — deferred to Chunk 13" (matching the already-correct :341-345
  wording), and change the TODO tag in `CMakeLists.txt:62` and
  `content.hpp:55` from `(Chunk 7)` to `(Chunk 13)` so all three sources
  agree with `implementation-plan.md:205`.

- **Boundary-safe**: Yes — wording-only changes to a doc and two comments,
  no fence contact.

- **Rationale**: A chunk tag naming an already-shipped chunk is worse than
  an untagged TODO: it hides a live Chunk-13 obligation behind a number
  every marker-based scan treats as closed. `content-boundary.md`'s own
  header already lists Chunk 7 as done, so the self-contradiction plus the
  stale tag would otherwise cost the Chunk-13 implementer a re-derivation
  this report can settle in one sentence.

### M-9 (corrects a threading claim implicit in the As-built section above): content.hpp's thread-safety comment overclaims Main-thread enforcement on reads, and the 12 Main asserts are permanent under P-2, not removed by it

- **File(s)**: `xash3dpp/include/xash3dpp/content/content.hpp:9-12` (the
  one-line `@thread-safety` contract); `xash3dpp/src/content/model/model_cache.cpp:81,95,123,158,191,212,224,239,312`
  and `xash3dpp/src/content/imagelib/imagelib.cpp:54,61,73` (the 12
  `assert_thread_role(Main)` sites); `model_cache.cpp:111-119,179-187,323-332`
  (five `const` read accessors — `find`/`resolve` ×2/`model_infos`/
  `studio_extradata`/`validate_crc` — with no assert at all).

- **Current pattern**: the header states "every load/lookup entry...assert
  Main," read alone by a downstream implementer who has not also opened
  `content-threading.md`. In fact all 12 asserted sites are mutators or
  lifecycle entry points (`init`/`shutdown`/`find_or_alloc`/
  `register_world`/`free_model`/`purge_for_level_change`/`free_unused`/
  `load_from_bytes`/`need_crc`, plus `ImageDecoder::init`/`shutdown`/
  `decode`); the five `const` read accessors that hand back live state are
  safe only by an unenforced Main-only calling convention. A prior
  characterization of this split (documented in `content-threading.md:130-139`)
  described the 12 sites as "incidental, removable by P-2" — that gets the
  mechanism backwards: P-2 (published-snapshot reads) relaxes the **read**
  side only; single-publisher discipline is the entire mechanism, so every
  one of the 12 writer/lifecycle asserts must **survive** a P-2 adoption,
  not be removed by it.

- **Suggested replacement**: tighten the header comment to name the split
  precisely — "`ModelCache` mutators (`init`/`shutdown`/`find_or_alloc`/
  `register_world`/`free_model`/`purge`/`free_unused`/`load_from_bytes`/
  `need_crc`) assert `ThreadRole::Main`; read accessors (`find`/`resolve`/
  `model_infos`/`studio_extradata`/`validate_crc`) are Main-only by caller
  convention today, not enforcement — see `content-threading.md` for the
  hazard this contains." Record as a shape constraint only (no work to
  schedule today): when HB-5 (one shared published-snapshot idiom) gets its
  brief, content's generation-swapped read view should be a thin wrapper
  over the already-shipped `model_infos()`/`ModelInfo` shape, adding a
  read-side assert rather than removing the 12 write-side ones.

- **Boundary-safe**: Yes — wording-only header-comment change; the shape
  constraint proposes nothing to build today (`consumer_status: speculative`,
  recorded per the anti-gold-plating rule, not scheduled work).

- **Rationale**: An overclaiming public contract is worse than an absent
  one — it tells a caller the compiler enforces something it does not.
  Getting the P-2 mechanism direction right here matters beyond content:
  it is the cleanest tree example the audit found of the by-design/
  incidental Main-pinning split, and the correct reading (writers pinned
  permanently, readers unenforced today) is the one HB-5's eventual brief
  needs to inherit.

### M-10 (new 2026-07-20): ImageStats's two plain counters are the only reason `ImageDecoder::decode()` needs Main — make them atomic ahead of the Chunk-13 texture-decode path `[EXT:G-3,P-1]`

- **File(s)**: `xash3dpp/include/xash3dpp/imagelib/imagelib.hpp:30-34`
  (`ImageStats`, two plain `std::uint64_t` counters); `xash3dpp/src/content/imagelib/imagelib.cpp:71-79`
  (`decode()`'s `assert_thread_role(Main)`), `:77,110,112,117` (the 4
  increment sites); `xash3dpp/docs/design/threading-model.md:358-363`
  (§6.4, the async texture-decode worker path this unblocks).

- **Current pattern**: `init()`/`shutdown()`/`decode()` each assert
  `ThreadRole::Main`. `decode()` does not touch `pool_` at all (it appears
  only in `init()`/`shutdown()`); the codec registry is a magic-static,
  immutable, already-safe-from-any-thread array; the only shared mutable
  state `decode()` touches is the two plain `ImageStats` counters. The
  Main-pin on `decode()` is therefore entirely incidental to two counters,
  not to pool lifecycle or codec state.

- **Suggested replacement**: convert `images_decoded`/`decode_failures` to
  `std::atomic<std::uint64_t>` (relaxed ordering), mirroring the 6 other
  atomic stats structs already in the tree (`SoundStats`, `NetworkingStats`,
  etc.); update the 4 increment sites. Keep `init()`/`shutdown()` Main-
  pinned for pool-lifecycle discipline; drop the Main assert from
  `decode()` only, and update `content-boundary.md`'s P-1 row to state that
  the parse-off-Main door is open for model parsing but was **not**
  previously open for image decode until this lands.

- **Boundary-safe**: Yes — imagelib is not one of the five HB-2 kernel
  subsystems, and codecs are already stateless/const.

- **Rationale**: This finding was first raised with a `speculative`
  consumer classification, correctly demoted to a shape-constraint-only
  verdict because `decode()` has zero production callers today. That
  classification flips the moment Chunk 13 exists: `threading-model.md`
  §6.4 already specifies the async texture-decode worker path (a worker
  decodes pixels off-Main, Main appends a texture-upload command to the
  next `RenderFrame`) that needs `decode()` callable off-Main, identically
  under a GL or Vulkan backend — this fix is backend-agnostic prep, not
  speculative infrastructure. Two fields, four call sites, lock-free on
  both x86 and x64; doing it now means Chunk 13 does not rediscover it as
  day-one debt. **Consumer**: Chunk 13's async texture-decode worker path
  per `threading-model.md` §6.4 (`consumer_status: scheduled-chunk-N`).

### M-11 (new 2026-07-20, UNVERIFIED — flagged, not adversarially confirmed): `xash3dpp_imagelib`'s CMake PUBLIC links are overbroad for its public headers

- **File(s)**: `xash3dpp/src/content/CMakeLists.txt:33-37` (PUBLIC links to
  utilities/memory/core for the imagelib target); `xash3dpp/include/xash3dpp/imagelib/*.hpp`
  (`image.hpp`, `imagelib.hpp`, `errors.hpp`, `save.hpp`,
  `pixel_format.hpp` — all 5 public headers include only other imagelib
  public headers and the standard library); `xash3dpp/src/content/imagelib/imagelib.cpp:14-15`
  (where `core/thread_role.hpp` and `memory/memory.hpp` are actually used).

- **Current pattern**: `utilities::swap.hpp` is used only inside
  `codec_*.cpp`; `core/thread_role.hpp` and `memory/memory.hpp` are used
  only inside `imagelib.cpp` — all three are implementation-file-only
  dependencies, yet the CMakeLists declares all three `target_link_libraries`
  as PUBLIC, so anything linking `xash3dpp_imagelib` transitively pulls in
  all three even though no public header requires them.

- **Suggested replacement**: change the three `target_link_libraries` lines
  to PRIVATE. Verify by rebuilding — a PRIVATE link that still compiles
  proves no public header depended on it.

- **Boundary-safe**: Yes — a pure CMake-file edit, no source change.

- **Rationale**: A PUBLIC link a consumer does not need is the kind of
  drift that goes unnoticed until a downstream target inherits a
  dependency it never asked for (relevant the moment Chunk 13 links
  `xash3dpp_imagelib` for real). Flagged here as unverified because it was
  not independently rebuilt-and-confirmed during this campaign; treat as a
  cheap thing to check, not an established defect.

### M-12 (new 2026-07-20): `byte_at()` bounds-checked accessor redefined identically in all 7 imagelib codec TUs

- **File(s)**: `xash3dpp/src/content/imagelib/codec_bmp.cpp:41-44`,
  `codec_tga.cpp:32-35`, `codec_dds.cpp:107-110`, `codec_ktx2.cpp:81-84`
  (and 3 more codec TUs) — 75 total occurrences of the identifier tree-
  wide, ~68 of them call sites.

- **Current pattern**: the same anonymous-namespace helper
  `byte_at(std::span<const std::byte>, std::size_t) -> std::uint8_t` is
  independently declared in all 7 codec `.cpp` files. Any future change to
  its semantics (an added assert, a different truncation policy) must be
  applied 7 times by hand.

- **Suggested replacement**: move one definition into a shared imagelib-
  private header (`private/imagelib/codec.hpp`, already shared by all 7
  TUs) and delete the 6 duplicate copies; call sites are unchanged.

- **Boundary-safe**: Yes — an anonymous-namespace helper, not a frozen
  struct or ABI signature.

- **Rationale**: The audit's L11 (subtraction) lens independently names
  this exact hoist as part of a decision-free deletion batch — zero call
  sites change, zero design decision required, and it removes the single
  largest per-file duplication count in imagelib.

### M-13 (new 2026-07-20): Image dimension cap (8192) duplicated 5× with drifting name and type; imagelib has no `limits.hpp` section

- **File(s)**: `xash3dpp/src/content/imagelib/codec_bmp.cpp:39`
  (`constexpr std::int64_t k_max_dim = 8192`), `codec_tga.cpp:30`
  (`std::size_t k_max_dim`), `codec_png.cpp:38` (`std::size_t k_max_dim`),
  `codec_mip.cpp:37` (`std::size_t k_max_dim`), `codec_dds.cpp:105`
  (`u32 k_image_max_dim`); `xash3dpp/limits.hpp` (has a dedicated section
  per subsystem — cmd_cvar, memory, host, map_loader, utilities, gameinfo,
  filesystem, clock, platform, networking, server, content, save, sound,
  input — but no `imagelib` section).

- **Current pattern**: the legacy `IMAGE_MAXWIDTH`/`IMAGE_MAXHEIGHT` (8192,
  `engine/common/imagelib/imagelib.h:101-102`, reference-only) is
  re-declared as a local `constexpr` in 5 codecs under 2 different name
  spellings and 3 different types.

- **Suggested replacement**: add an `imagelib` section to
  `xash3dpp/limits.hpp` (`XASH_LIMIT_IMAGE_MAX_DIM`, matching the pattern
  every other subsystem already follows) and have the 5 codecs reference
  `xash::limits::image_max_dim` instead of their own local copies.
  Optionally fold in `k_mip_header = 40` (the frozen `mip_t` size), also
  duplicated verbatim in `codec_wad.cpp:30` and `codec_mip.cpp:35`.

- **Boundary-safe**: Yes — the cap is a validation bound checked against
  untrusted file data, not a computed or ABI value.

- **Rationale**: One value, 2 spellings, 3 types is exactly the drift the
  rest of the tree's per-subsystem `limits.hpp` sections exist to prevent;
  imagelib is the one subsystem missing one despite already having 5
  candidate call sites.

______________________________________________________________________

## Low-priority / cosmetic opportunities

| ID | Pattern | Files (representative) | Replacement |
|----|---------|------------------------|-------------|
| L-1 | `NULL` / `0`-as-pointer | pervasive across `model.c`, `mod_*.c`, `imagelib/*` | `nullptr` (2-I) |
| L-2 | `Q_min`/`Q_max`/`bound()` macros | imagelib codecs, studio bounds | `std::min`/`std::max`/`std::clamp` (2-J) |
| L-3 | `ARRAYSIZE`-style macros | dispatch tables, swap tables | `std::size` / `std::ssize` (2-J) |
| L-4 | C-style pixel type-puns `(uint*)`/`(word*)`; `HostFourCC` bit-pack | `img_utils.c`, `img_bmp/dds.c` | `std::bit_cast` (value puns) / `reinterpret_cast` (byte views) (2-G) |
| L-5 | `typedef struct {…} X;` in headers | `mod_local.h`, `imagelib.h` | plain `struct X {…};` (2-I) |
| L-6 | `PFDesc[type].bpp` int-indexed enum lookups | `img_utils.c`, `img_quant.c` | `std::to_underlying` at the index (2-I) |

### L-7 (new 2026-07-20): `StudioView` carries private duplicate byte/float readers next to the file's existing `rd_i32`/`rd_f32` helpers

- **File(s)**: `xash3dpp/src/content/model/studio.cpp:80-104`
  (anonymous-namespace `rd_i32`/`rd_u16`/`rd_i16`/`rd_u8`/`rd_f32`),
  `:158-173` (`StudioView::i32`/`StudioView::vec3`, private members);
  `xash3dpp/include/xash3dpp/content/studio.hpp:249-250` (their public-
  header declarations).

- **Current pattern**: six sub-views read through the file-local `rd_*`
  helpers; `StudioView` alone reads through two private member functions
  that are byte-for-byte the same logic (bounds check, `read_le`, zero on
  overflow), with `vec3` additionally re-inlining `rd_f32` as a lambda. The
  duplication also leaks two private member declarations into the public
  header.

- **Suggested replacement**: delete `StudioView::i32`/`StudioView::vec3`
  (declarations in `studio.hpp:249-250`, definitions in `studio.cpp:158-173`),
  add a `rd_vec3(span, off)` next to the other `rd_*` helpers, and rewrite
  the 15 `i32(kOff*)` and 5 `vec3(kOff*)` call sites as `rd_i32(data_,
  kOff*)`/`rd_vec3(data_, kOff*)`.

- **Boundary-safe**: Yes. Both the duplicate and the original are
  bounds-checked `memcpy`+`bit_cast` field decoders with zero
  floating-point computation — the actual bit-exact kernel is
  `bone_solver.cpp`'s bone math, not this ABI-offset parse layer.
  Consolidating changes zero bits read.

- **Rationale**: Removes 20 call sites' worth of duplicate logic and two
  stray private declarations from the public header. Also independently
  named by the audit's L11 (subtraction) lens as part of a decision-free
  deletion batch.

### L-8 (new 2026-07-20): `Model::set_type` is a public setter that can desync the type tag from the `variant` payload it exists to keep in sync

- **File(s)**: `xash3dpp/include/xash3dpp/content/model.hpp:98`
  (`set_type`), `:109-123` (`set_studio`/`set_sprite`/`set_alias`, which
  each set both tag and payload), `:150-157` (`type_` member);
  `xash3dpp/src/content/model/model_cache.cpp:164` (the one external
  caller, `register_world`, tagging the world `Brush`).

- **Current pattern**: `Model` is a class specifically so the type tag and
  the payload cannot disagree, but `set_type` is public and unconditional,
  so `type_` is a mutable duplicate of `payload_.index()` for three of the
  five enumerators — nothing stops a future caller from setting a tag that
  the `variant` payload contradicts.

- **Suggested replacement**: delete `set_type` and the `type_` member;
  derive `type()` from `payload_` (monostate → a `brush_` flag decides
  Brush vs. Bad; the three `variant` alternatives map to Studio/Sprite/
  Alias), and give `register_world` a `set_brush()` that sets only the one
  bit it needs.

- **Boundary-safe**: Yes — `Model`'s internal tag/payload representation is
  xash3dpp's own class invariant, not a frozen-ABI struct field.

- **Rationale**: Q-22 says state-with-invariants belongs in a class; a
  public setter that can violate the class's own invariant is exactly the
  gap that pattern exists to close. One field and one setter removed makes
  the tag/payload disagreement unrepresentable instead of merely unlikely.

### L-9 (new 2026-07-20): content's sole thread-assert waiver is a compliance-scanner false positive (verb-name collision on "start"), not a genuine conditional-role case

- **File(s)**: `xash3dpp/src/content/model/studio.cpp:123-126`
  (`BoneControllerView::start()`/`end()`, two structurally identical
  `const noexcept` accessors two lines apart); `:125` carries a
  `compliance-allow(thread-assert)` waiver, `:126` (`end()`) carries none;
  `xash3dpp/tools/xtools/rules.py:367-373` (`MUTATOR_NAMES`).

- **Current pattern**: `studio.cpp` defines 24 one-line `const` readers
  over a caller-owned span; `start()` is flagged by the compliance scanner
  solely because the bare verb "start" is one of `MUTATOR_NAMES`' 16
  unsuffixed entries, then suppressed with a waiver comment ("read-only
  value query over caller-owned bytes") that applies verbatim to `end()`
  and to the other 22 identical readers, none of which carry any waiver at
  all — the scanner never flagged them because "end" is not in the list.

- **Suggested replacement**: delete the waiver comment on `:125`. If the
  scanner then re-flags the line, that confirms the scanner's heuristic
  needs fixing (a `const noexcept` member returning a value with no member
  mutation is never a genuine thread-assert site) — report it to the
  compliance-scan owner rather than re-suppressing per-line. Either way
  content's own waiver count goes to 0.

- **Boundary-safe**: Yes — deleting a stray annotation comment; the tooling
  fix (if pursued) is out of content's own tree.

- **Rationale**: `xtools/rules.py`'s bare-verb `MUTATOR_NAMES` list (which
  the audit found has a broader, tree-wide blind spot — see the campaign's
  `CORRECTIONS.md`) produces this exact false-positive shape anywhere a
  struct-field accessor happens to share a name with an unsuffixed mutator
  verb; content is simply where it happened to land first.

### L-10 (new 2026-07-20, UNVERIFIED): little-endian byte-append lambdas (`put_u16`/`put_u32`/`put_byte`) reinvented in 3 imagelib save paths

- **File(s)**: `xash3dpp/src/content/imagelib/codec_bmp.cpp:363-371`,
  `codec_tga.cpp:278-282`, `codec_wad.cpp:178-190`.

- **Current pattern**: `save_bmp`, `save_tga`, and `save_wad` each
  independently define local lambdas that push the bytes of
  `xash::utilities::write_le<T>()` onto the output `std::vector<std::byte>`
  — the append-to-vector wrapper around the already-shared `write_le`
  primitive was never itself promoted to `utilities`.

- **Suggested replacement**: add `template<typename T> void
  append_le(std::vector<std::byte>&, T)` (and an `append_byte`
  convenience) to `xash::utilities` (`swap.hpp`), matching the existing
  `read_le`/`write_le` naming; replace the 3 sets of local lambdas with
  calls to it.

- **Boundary-safe**: Yes — an internal serialization helper, not ABI-facing.

- **Rationale**: The natural next step after this report's already-
  resolved OQ-E (`read_le`/`write_le` promoted to `utilities/swap.hpp`);
  flagged UNVERIFIED because the exact call-site count was not
  independently re-derived during adversarial review.

### L-11 (new 2026-07-20, UNVERIFIED): PNG 8-byte signature literal duplicated within the same TU

- **File(s)**: `xash3dpp/src/content/imagelib/codec_png.cpp:119-120` and
  `:503-504`.

- **Current pattern**: `decode_png()` and `save_png()` each declare their
  own local `static constexpr` copy of the identical 8-byte PNG signature,
  inside the same translation unit.

- **Suggested replacement**: hoist one `constexpr std::array<std::uint8_t,
  8> k_png_signature` to file (anonymous-namespace) scope in
  `codec_png.cpp` and have both functions reference it.

- **Boundary-safe**: Yes.

- **Rationale**: Also named by the audit's L11 (subtraction) lens as part
  of the decision-free deletion batch — same TU, same value, zero reason
  for two copies.

### L-12 (new 2026-07-20, doc-only): record the model-format magic-number dispatch as a deliberate "leave as switch" verdict, with its growth trigger, in content-boundary.md

- **File(s)**: `xash3dpp/src/content/model/model_cache.cpp:257-289` (the
  3-arm heterogeneous `switch` over model magic numbers, plus a 3-magic
  rejection group).

- **Current pattern**: the switch is a closed, 3-format dispatch whose arms
  produce three structurally different payload types (`StudioModel`/
  `SpriteModel`/`AliasModel`) feeding one `variant`; nothing in
  `content-boundary.md` records that this was evaluated against a table
  form and deliberately left as a `switch`.

- **Suggested replacement**: add a row to `content-boundary.md` stating the
  verdict and its trigger — "model magic dispatch (`model_cache.cpp:257`):
  leave as `switch`; 3 arms with heterogeneous payload types do not table-
  ify without type-erasing them. Growth trigger: a 4th non-brush model
  container format. If that lands, convert to a `{std::int32_t magic,
  parse fn}` table only if the parse results can be unified behind one
  setter; otherwise it stays a switch — do not type-erase three
  heterogeneous payloads to save three case labels."

- **Boundary-safe**: Yes — documentation only, no code change.

- **Rationale**: The audit's L2 (registry-unification) lens evaluated this
  switch alongside three other dispatch tables in the tree and explicitly
  left it as-is; recording the verdict and its trigger here stops the next
  auditor from re-deriving (or re-proposing) the same conversion against a
  format count that has not changed.

______________________________________________________________________

## OOP / design-paradigm opportunities

The idiom-level findings above (2-A…2-K) leave a design-level question the
rewrite must answer up front: which of content's C dispatch-`switch`es and
stateful globals become **classes with invariants** or **polymorphic
hierarchies**. The register makes this the *sanctioned* shape where it fits —
Q-22 (state-with-invariants lives in a class), P-7 (pool-owned RAII classes),
Q-11 (open-set features as registered implementations) — with the seam
precedents already in the tree: `ISearchBackend` (filesystem), `IProtocolDriver`
(networking), `ITrustOracle` / `ICvarObserver` (cmd_cvar), `EntityView`
(server). Three hard constraints shape *how much* OOP:

- **`/GR-` (no RTTI)** — virtual dispatch is fine; `dynamic_cast` is not. A
  closed, frozen set may prefer `std::variant` + visitor over a virtual
  hierarchy.
- **`/EHs-c-` (no exceptions)** — constructors must not fail; use the P-7
  `create_<thing>` factory returning `std::expected` (pairs with H-2), never a
  throwing ctor.
- **Orchestrators stay procedural (Q-22)** — the top-level load flow and the
  level-transition sequence remain free functions that *drive* these classes;
  the classes own the *state and invariants*, not the orchestration. OOP here is
  for the stateful aggregates and the open dispatch sets, not everything.

### O-1: `ModelCache` class — encapsulate the cache + `needload` FSM `[EXT:P-4]`

- **Current pattern**: `mod_known[]` / `mod_numknown` / `mod_crcinfo[]`
  (`model.c:27-30`) mutated by free functions, with the 4-state `needload` FSM
  (`mod_local.h:57-60`), the slot-0-is-world invariant, and the `MAX_MODELS` cap
  enforced ad hoc.
- **Suggested**: one `ModelCache` class (Q-22) holding those arrays as private
  state; public `find_or_load` / `extradata` / `purge_for_level_change` /
  `free_unused` / `validate_crc`. The purge and `needload` quirks (boundary
  "Quirks and invariants") become documented class invariants instead of
  scattered global writes, and a typed `models()` query is the P-4 introspection
  surface (no `extern mod_known`). This is the class realisation of H-1's
  model-cache half.
- **Tier**: High. **Boundary-safe**: Yes — the per-side `sv.models[]` /
  `cl.models[]` precache maps stay in server/client.

### O-2: `IImageCodec` polymorphic codec registry

- **Current pattern**: `{ ext, fnptr, hint }` tables (`imagelib.h:46-62`,
  `img_utils.c:96-134`) iterated by string compare, all writing the global
  `image`.
- **Suggested**: an `IImageCodec` interface — `can_decode(bytes)`, `decode()
  -> std::expected<Image, ImageError>`, `encode()` — one implementation per
  format in a registry (open-closed; 12 formats today, and archive/codec plugins
  are an explicit extension goal). Keep codecs **stateless** (scratch moves into
  an `ImageDecoder` and the returned `Image` value type), so a single shared
  instance is reentrant — a direct enabler for the Chunk-7 worker pool. The
  `Image` result is a value class (dimensions / format / mips as methods) and a
  `Palette` value type absorbs the install / compare / translate / hue-replace
  free functions. Mirrors `ISearchBackend`; this is the hierarchy form of M-3.
- **Tier**: High — but drops to Medium (dispatch only, codecs return the ABI
  struct) if OQ-1 keeps `rgbdata_t` frozen. **Boundary-safe**: the dispatch is;
  the `Image` result type is **Needs verification** (OQ-1).

### O-3: Model-format dispatch — hierarchy or `variant`, not `switch`

- **Current pattern**: the magic `switch` in `Mod_LoadModel` (`model.c:347-368`)
  over a closed, frozen set of four formats (studio / sprite / alias / brush).
- **Suggested**: either an `IModelLoader` virtual hierarchy (matches the `I*`
  house style, trivially extensible) **or** — since the set is closed and RTTI
  is off — a `std::variant<StudioModel, SpriteModel, AliasModel, BrushModel>` +
  visitor (no heap, no vtable, exhaustiveness-checked). Recommend `variant` for
  the *result* model and a thin `IModelLoader` only if loaders must register
  dynamically. Note the brush arm still delegates to `map_loader`.
- **Tier**: Medium. **Boundary-safe**: Yes.

### O-4: `StudioView` typed accessor over frozen `studiohdr_t` `[EXT:P-4]` `[EXT:G-2]`

- **Current pattern**: every studio query re-derives the `mod_studiohdr`
  singleton (`mod_studio.c:46`) and walks `boneindex` / `hitboxindex` /
  `seqindex` by raw offset.
- **Suggested**: a non-owning `StudioView` class over the frozen `studiohdr_t`
  with typed accessors — `bone(i)`, `hitboxes()`, `sequence(i)`,
  `attachment(i)`. The struct stays the ABI data; the view is the typed surface
  over it — the exact `EntityView`-over-`entvars_t` precedent (Q-20 / P-4) — and
  it confines the raw `void*` `pfnGetModelPtr` hands out (G-2 door: a v2 ABI
  could hand the view instead of the pointer).
- **Tier**: Medium. **Boundary-safe**: Yes (view over unchanged data).

### O-5: Injected DLL seams as interfaces `[EXT:P-6]`

- **Current pattern**: three engine↔DLL C vtables content drives directly —
  `ref.dllFuncs.Mod_ProcessRenderData` / `svgame.physFuncs.Mod_ProcessUserData`,
  the swappable `Server_GetBlendingInterface` bone solver, and the WAD/palette
  quirk branches.
- **Suggested**: inject them as C++ interfaces so content depends on the
  abstraction and never links toward the renderer — `IModelPostProcess`
  (boundary OQ-4), `IBoneSolver` (boundary OQ-5), and `content::ICompatPolicy`
  (Q-12, link-selected by `XASH_GOLDSRC_COMPAT`). Each is the internal consumer
  of an unchanged frozen C vtable, matching the `IProtocolDriver` /
  `ICompatPolicy` precedent.
- **Tier**: Medium. **Boundary-safe**: Yes (the C ABI vtables are untouched).

______________________________________________________________________

## Out of scope / ABI-frozen

- **`model_t`** (`com_model.h:327`) — layout frozen (`STATIC_CHECK_SIZEOF`);
  `char name[64]`, the `needload` `qboolean` storage, the `T* + int count`
  geometry pairs, and the `edges16/32`/`clipnodes16/32` unions stay as-is.
  Internal code may *view* them through `span`/`enum class`, but the struct does
  not change.
- **`studiohdr_t` + all `mstudio*`** (`studio.h`) — the game DLL walks these by
  offset via `pfnGetModelPtr`'s raw `void*`; every field, including the
  `unused[]` former-sound slots and `studiohdr2index`, is frozen.
- **`msprite_t`/`mspriteframe_t`, `aliashdr_t`/`trivertex_t`, `cache_user_t`** —
  frozen (client draws / studio DLL caches them).
- **DLL function-pointer tables** — `ref.dllFuncs.Mod_ProcessRenderData`/
  `Mod_StudioLoadTextures`, `svgame.physFuncs.Mod_ProcessUserData`, and
  `gStudioAPI`/`sv_blending_interface_t`. These are the frozen engine↔DLL ABI;
  **do not** convert to `std::function`/virtual interfaces. (Content's *own*
  internal dispatch — M-3 — is a different, private table.)
- **`GAME_EXPORT` callbacks** — `Mod_Calloc`, `Mod_CacheCheck`,
  `Mod_LoadCacheFile`, `Mod_StudioExtradata` keep their C signatures.

______________________________________________________________________

## Investigated and refuted (2026-07-20)

Recorded so a future audit does not re-derive these at full cost.

- **A `builtin_bone_solver()` function-local-singleton accessor with
  defaulted `IBoneSolver &` parameters, to remove the 3 stack
  `BuiltinBoneSolver solver;` declarations at `engine_table.cpp:1426,1510`
  and `model_resolver.cpp:169`.** The facts are real (one production impl,
  a 4-line forwarder to free `setup_bones`; all 3 call sites do
  stack-construct the object immediately before use) but the proposal does
  not survive its own cited precedent: imagelib's `tga_codec()`-style
  singleton accessors work because every `IImageCodec` method is `const`
  (`codec_tga.cpp:237-239`), while `IBoneSolver::setup_bones` is a
  **non-const** virtual (`bone_solver.hpp:73-74,82-83`), so a
  `builtin_bone_solver()` would have to return a mutable reference to a
  mutable function-local static — the inverse of the cited shape, and it
  would land in the `mutable-global`/`di-global-ref` compliance-waiver
  class the tree otherwise avoids. Applying the anti-gold-plating
  consumer test to what is actually being *built* (a new singleton
  accessor plus default arguments) rather than to the code being touched:
  nobody needs it — the 3 call sites compile and work today, and the
  `IBoneSolver &` parameter is the explicit substitution point the OQ-5
  door exists for. **Recorded as door-debt, not work**: if server OQ-2
  (the studio-hull provider) lands a second solver, the parameter stays an
  explicit `IBoneSolver &` on `bone_world_position`/
  `attachment_world_position`/`studio_hitbox_hulls` — do not introduce a
  singleton accessor or a defaulted solver argument, because a defaulted
  global-backed default silently reintroduces the same non-reentrant
  pattern H-1 removed. (`bone_solver.hpp:79-87`.)

______________________________________________________________________

## Open questions

- **OQ-A (gated H-4, M-1, M-3) — `rgbdata_t` ABI status. ✅ RESOLVED
  2026-07-06 (boundary OQ-1): internal `Image` type + `rgbdata_t` adapter at
  the renderer seam.** H-4 (`span`/`mdspan`), M-1/M-2 (`enum class` formats),
  and O-2 (`Image` value type) are unblocked for the imagelib internals;
  `rgbdata_t` is materialised only at the future (Chunk 13) renderer boundary,
  not frozen by content.
- **OQ-B — `std::expected` error model.** One shared `LoadError`/`ImageError`
  enum across all formats, or per-format error types? Affects every loader
  signature (H-2).
- **OQ-C — worker-pool prerequisite ordering.** H-1 (kill singletons) and H-2
  (`expected` loaders) are *prerequisites* for the Chunk-7 worker pool, not
  optional cleanups — the pool cannot own a load while a global `image`/
  `mod_studiohdr` exists. Confirm they land before the first parallel load.
- **OQ-D — toolchain feature availability.** Verify MSVC (x64 + x86) ships
  `<mdspan>` under `/std:c++latest`; if not, H-4 falls back to `std::span` +
  explicit stride. Verify `pool_new<T>` `alignof(T) ≤ 8` holds for any
  SIMD-aligned pixel scratch (H-5) before mandating pool ownership.
- **OQ-E — where the byte-swap helper lives. ✅ RESOLVED**: `read_le<T>`
  (H-3) shipped in `xash::utilities`/`swap.hpp`, not duplicated in content —
  confirmed by the As-built refresh's H-3 row and by L-10's `append_le<T>`
  proposal, which is written as the natural next promotion beside it.
- **OQ-F (new 2026-07-20) — imagelib's compressed pixel-format set has no
  ETC2/ASTC entries** (`xash3dpp/include/xash3dpp/imagelib/pixel_format.hpp:29-43`
  lists Dxt1/3/5, Ati2, Bc4-7, Ktx2Raw only). Not content's decision to
  make: the audit's L9 (renderer-packet) lens ties this to the still-open
  Chunk-13 GL/Vulkan/multi-backend choice — if that decision names a
  mobile-primary target, imagelib needs an ETC2/ASTC codec family it has
  never carried before. Recorded here because the gap lives in this
  subsystem's header; owner is the Chunk-13 renderer decision, not content.

______________________________________________________________________

*Analysis only — no source files modified. Findings H-1/H-2/H-4 are gated on
boundary OQ-1 (`rgbdata_t`) and are prerequisites for the Chunk-7 worker-pool
hook; the OOP shapes (O-1…O-5) are the structural target the High/Medium idiom
swaps assemble into. Sequence both into `/plan-implementation content` after the
boundary OQs resolve — `ModelCache` (O-1) and the `IImageCodec` registry (O-2)
are the two anchor classes the scaffold should stand up first.*

*2026-07-20 addendum: H-6 (offset-table dedup) and H-7 (dead-pool deletion)
are independent of the OQ-1 gate above and can land immediately — neither
touches `rgbdata_t` or the worker-pool prerequisite chain. M-7/M-12 (imagelib
registry + `byte_at` dedup) are best sequenced before Chunk 13 links
`xash3dpp_imagelib`, per their own rationale. M-10 (`ImageStats` atomic) is
the one item this pass promotes from a recorded shape constraint to real,
schedulable work, specifically because Chunk 13's threading-model §6.4
async-decode path now names it as a consumer. Max IDs after this refresh:
H-7, M-13, L-12, O-5.*
