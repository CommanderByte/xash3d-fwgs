# Content Modernization Opportunities

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
- **OQ-E — where the byte-swap helper lives.** `read_le<T>`/`std::byteswap`
  wrapper (H-3) is useful to map_loader and networking too — promote to
  `utilities`/`swap` rather than duplicating in content? (`utilities` already
  has a `swap` module.)

______________________________________________________________________

*Analysis only — no source files modified. Findings H-1/H-2/H-4 are gated on
boundary OQ-1 (`rgbdata_t`) and are prerequisites for the Chunk-7 worker-pool
hook; the OOP shapes (O-1…O-5) are the structural target the High/Medium idiom
swaps assemble into. Sequence both into `/plan-implementation content` after the
boundary OQs resolve — `ModelCache` (O-1) and the `IImageCodec` registry (O-2)
are the two anchor classes the scaffold should stand up first.*
