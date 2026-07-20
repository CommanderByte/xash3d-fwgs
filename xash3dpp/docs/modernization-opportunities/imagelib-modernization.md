# imagelib Modernization Opportunities

> C++ standard in use: C++**23** (from `xash3dpp/CMakeLists.txt`)
> Boundary spec: `docs/boundaries/content-boundary.md` — imagelib is one of
> content's two Q-11 satellite targets (`xash3dpp_imagelib`, separate CMake
> target from `xash3dpp_content`); it has no dedicated boundary doc of its
> own.
> ABI-frozen symbols in this subsystem: the on-disk WAD3/mip byte formats
> (`dwadinfo_t`/`dlumpinfo_t`/`mip_t`, `common/wadfile.h`) that
> `codec_wad.cpp`/`codec_mip.cpp` read and write byte-for-byte. **Not**
> frozen for imagelib itself: `rgbdata_t`/`pixformat_t`/`imgFlags_t`
> (`common/com_image.h`) — `content-boundary.md` OQ-1 already decided
> imagelib returns a fresh internal `Image` value type; adapting that to
> `rgbdata_t` is the Chunk-13 renderer seam's job, not imagelib's.
>
> Refreshed 2026-07-20 (tree-wide modernization audit). **This is the first
> report for this subsystem** — no prior `imagelib-modernization.md`
> existed. Most of imagelib's idiom- and structural-level findings were
> already captured in this same audit pass under
> `content-modernization.md` (imagelib ships inside content's Chunk 7/13
> boundary spec and CMake area): **H-7, M-7..M-13, O-2**. This report does
> **not** re-derive them — each is cross-referenced by its content-report ID
> below — and instead adds the two residual findings that report's own
> changelog promised but never wrote down (L-1, L-2), plus the framing a
> per-finding digest does not carry: imagelib is the one subsystem in the
> entire audit with **zero production callers of any kind** (decode *and*
> all four save paths), so every item here is a forward obligation to
> settle *before* Chunk 13 wires the link in, not a live-code fix.

## Summary

imagelib is functionally complete (7 decoders, 4 encoders, a WAD3 pack/
unpack path, ~13 TUs) and is exercised only by its own test suite —
`grep xash3dpp_imagelib` across every `CMakeLists.txt` in the tree returns
exactly one production line (its own target definition) and one test
line (`tests/content/CMakeLists.txt:6`); `xash3dpp_content` still links it
under a stale `TODO(Chunk 7)` marker instead of a real
`target_link_libraries`. That is not dead code: `implementation-plan.md:205`
assigns the `content → imagelib` wiring to Chunk 13 (the renderer), and
`codec_ktx2.cpp:13`'s own comment already names `ref_vk` as the intended
reader of its compressed-format output. It does mean every finding in this
subsystem carries a `speculative`-leaning consumer story unless it is
phrased as "fix before the second caller (Chunk 13) arrives" — which is
exactly how the codec-registry finding below is framed, and exactly why the
anti-gold-plating rule keeps the mobile-codec and passthrough-validation
items in Open Questions rather than High/Medium.

The subsystem's single biggest structural issue is also the one the
Phase-0 regex scan was structurally blind to: the codec dispatch at
`imagelib.cpp:94` is a function-local raw pointer array with **no key
column at all**, the third shape variant among the tree's three
extension-keyed registries (`k_archive_types`, `k_audio_codecs`,
imagelib's array) and the only one where "is `.png` an image format?"
cannot be answered without instantiating all seven codec singletons and
running a decode probe. That finding is filed in full, with its exact
replacement code, as `content-modernization.md` M-7 (cross-cutting lens
`L2-R3`); this report's High section restates it at imagelib's own
priority rather than content's, because the blast radius (9 production
sites, 0 external callers today) only grows once Chunk 13 becomes a
second caller — the same urgency the lens itself names.

Everything else worth reporting is either already tracked in
`content-modernization.md` (table below) or one of two small, genuinely
new Low-tier duplications this pass found in the save (encode) paths,
which nobody had written down yet.

### Already tracked in `content-modernization.md` — not duplicated here

| ID | Title | imagelib file(s) | Status |
|----|-------|-------------------|--------|
| H-7 | Delete `ImageDecoder::Impl::pool_` — created, destroyed, never allocated from | `imagelib.cpp:39-45,52-67` | Open |
| M-7 | Codec registry has no key column (full replacement spec) | `imagelib.cpp:80-90,94-115`; `codec.hpp:26,36-44`; 7 `handles()` overrides | Open — see H-1 below |
| M-8 | Stale `TODO(Chunk 7)` tags on the `content → imagelib` link contradict `implementation-plan.md`'s Chunk-13 assignment | `content/CMakeLists.txt:62`; `content.hpp:55` | Open |
| M-10 | `ImageStats`'s two plain counters are the only reason `decode()` asserts Main | `imagelib.hpp:30-34`; `imagelib.cpp:73,77,110,112,117` | Open — `[EXT:G-3,P-1]` |
| M-11 | `xash3dpp_imagelib`'s CMake PUBLIC links (utilities/memory/core) are overbroad for its 5 public headers | `content/CMakeLists.txt:33-37` | Open — UNVERIFIED |
| M-12 | `byte_at()` bounds-checked accessor redefined identically in all 7 codec TUs | `codec_bmp.cpp:41-44` + 6 more | Open |
| M-13 | Image dimension cap (8192) duplicated 5x, 2 name spellings, 3 types; no `limits.hpp` section | `codec_bmp.cpp:39` + 4 more | Open |
| O-2 | `IImageCodec` polymorphic codec registry (the OOP framing M-7's table sits inside) | `codec.hpp` | Shipped design; M-7 refines it |

## High-priority opportunities

### H-1: Codec dispatch has no key column — the tree's third, and only keyless, extension-to-codec shape

- **File(s)**: `xash3dpp/src/content/imagelib/imagelib.cpp:80-90` (hand-rolled
  `extension_of` + 16-char lowercase buffer), `:94-115` (function-local
  `static const IImageCodec *const registry[]`, linear-scanned, one virtual
  call per candidate); `xash3dpp/include/xash3dpp/private/imagelib/codec.hpp:26`
  (pure virtual `IImageCodec::handles`) and `:36-44` (the 7 accessor
  declarations, two files away from the array they must stay in sync
  with); the 7 one-line `handles()` overrides (`codec_bmp.cpp:306`,
  `codec_dds.cpp:394`, `codec_ktx2.cpp:240`, `codec_mip.cpp:288`,
  `codec_png.cpp:418`, `codec_tga.cpp:228`, `codec_wad.cpp:141`).

- **Current pattern**: `k_archive_types` (`archive_registry.hpp:23-44`) and
  sound's `k_audio_codecs` (`private/sound/codec.hpp:87-99`, whose own
  comment says it "mirrors archive_registry.hpp's `k_archive_types` shape")
  both use a `{extension, accessor}` `constexpr` table read at the
  dispatch site. imagelib instead asks each of its 7 codec singletons
  `handles(ext)`, where every override is a single equality against one
  string literal — a virtual predicate carrying exactly one bit a table
  column would carry more cheaply and more safely, because a codec added
  to `codec.hpp`'s accessor list but omitted from the function-local
  `registry[]` two files away **compiles, links, and silently never
  dispatches**. The hand-rolled extension/lowercase step has a live
  defect on top of the shape problem: `imagelib.cpp:85`,
  `n = ext.size() < lo.size() ? ext.size() : 0`, yields the **empty**
  extension (not a truncated one) for any extension of 16 characters or
  more, so such a file is misclassified `UnknownFormat` instead of merely
  unmatched.

- **Suggested replacement**: add
  `struct ImageCodecEntry { std::string_view key; const IImageCodec &(*codec)() noexcept; };`
  plus `inline constexpr std::array<ImageCodecEntry, 7> k_image_codecs` to
  `private/imagelib/codec.hpp` directly below the 7 accessor declarations
  (declaration and registration one screen apart, not two files apart),
  mirroring `k_audio_codecs` field-for-field. Replace the `registry[]`
  scan in `imagelib.cpp:94-115` with a `xash::utilities::ci_equal` scan of
  that table. Delete `IImageCodec::handles` and all 7 overrides. Delete
  the local `extension_of` and its 16-char buffer in favour of
  `xash::utilities::file_extension` (`utilities/path.hpp:19`) — imagelib
  already links `xash3dpp_utilities` PUBLIC
  (`content/CMakeLists.txt:34`). The full replacement code and a
  cross-subsystem rationale (this is one of three sibling registries the
  audit's L2 lens found in three different shapes) are written out in
  `content-modernization.md` M-7; this entry does not repeat that code,
  only the imagelib-scoped priority call.

- **Boundary-safe**: Yes — imagelib-private dispatch table, not one of the
  frozen DLL vtables or the frozen WAD3 on-disk format.

- **Rationale**: Ranked High here (content-modernization.md files the same
  fact as Medium, at content's tree-wide priority ordering) because scoped
  to imagelib alone this is both the subsystem's only real correctness
  hazard (silent no-dispatch on a typo'd registration) and the one item
  with a hard *before* deadline: `codec.hpp:34`'s comment already invites
  "one line per codec as they land," and every codec added between now and
  Chunk 13 grows the blast radius (currently 9 production sites, 0
  external callers of `handles()` — grep over `src/content`,
  `private/imagelib`, `tests/content`) with no compiler or test signal
  that anything is wrong. Land it while imagelib's only caller is its own
  test suite, not after Chunk 13 becomes a second one. **Consumer**:
  `ImageDecoder::decode` (`imagelib.cpp:104`) itself — the function being
  restructured, already in-tree `[exists-in-tree]`; the Chunk-13 renderer
  is the downstream asker per `implementation-plan.md:205`
  `[scheduled-chunk-N]`.

## Medium-priority opportunities

No new Medium-tier findings beyond the six already tracked in
`content-modernization.md` (M-8, M-10, M-11, M-12, M-13, and M-7's table
above H-1). See that report for full detail; nothing here duplicates them.

## Low-priority / cosmetic opportunities

### L-1: Little-endian byte-append lambdas reinvented in the 3 non-PNG save paths

- **File(s)**: `xash3dpp/src/content/imagelib/codec_bmp.cpp:363-371`
  (`put_u16`/`put_u32`/`put_byte`); `codec_tga.cpp:278-282`
  (`put_byte`/`put_u16`); `codec_wad.cpp:178-190`
  (`put_u32`/`put_u16`/`put_name`/`put_byte`).

- **Current pattern**: `save_bmp`, `save_tga`, and `save_wad` each declare
  their own local lambdas that call `xash::utilities::write_le<T>()` into a
  fixed-size stack buffer and then `push_back` the bytes onto the output
  `std::vector<std::byte>`. This is the natural next step after
  `utilities/swap.hpp` already promoted the raw `read_le`/`write_le`
  primitives to a shared header (`content-modernization.md` OQ-E,
  resolved) — the append-to-vector wrapper around them was never itself
  hoisted, so it is reinvented three times with the same body.

- **Suggested replacement**: add
  `template<typename T> void append_le(std::vector<std::byte>&, T)` (and an
  `append_byte` convenience) to `xash::utilities` (`swap.hpp`), matching
  the existing `read_le`/`write_le` naming, and replace the three sets of
  local lambdas with calls to it. `put_name` in `codec_wad.cpp:186-189`
  (fixed-width, NUL-padded string field) is WAD3-specific and stays local.

- **Boundary-safe**: Yes — pure helper consolidation in an existing
  imagelib-linked utility header; no format-byte change (verified by the
  existing round-trip tests in `tests/content/test_imagelib.cpp:135,
  386-403, 567`).

- **Rationale**: Small (blast radius 8: 3 duplicate lambda sets plus their
  call sites), and matches the `byte_at()` consolidation
  (`content-modernization.md` M-12) the same read-side asymmetry already
  called out — the write side has the identical shape. **Consumer**: the
  3 existing save paths (`save_bmp`/`save_tga`/`save_wad`), compiled and
  covered by `tests/content/test_imagelib.cpp` today `[exists-in-tree]`;
  like `decode()`, none of the four save entry points has a production
  caller yet — the same forward-obligation status applies.

### L-2: PNG 8-byte signature literal duplicated within the same translation unit

- **File(s)**: `xash3dpp/src/content/imagelib/codec_png.cpp:119-120`
  (`decode_png`'s copy) and `:503-504` (`save_png`'s copy).

- **Current pattern**: `decode_png()` and `save_png()` each declare their
  own local `static constexpr std::array<std::uint8_t, 8> sig` holding the
  identical PNG magic bytes (`0x89 P N G \r \n \x1a \n`), inside the same
  `.cpp` file.

- **Suggested replacement**: hoist one
  `constexpr std::array<std::uint8_t, 8> k_png_signature` to
  anonymous-namespace scope in `codec_png.cpp` and have both functions
  reference it.

- **Boundary-safe**: Yes — the PNG signature is a published, stable
  8-byte constant (RFC 2083 / the PNG spec), not a frozen internal struct;
  hoisting it changes no bytes.

- **Rationale**: Smallest possible finding (blast radius 2, one file, one
  literal) but zero-risk and zero-design-decision, in the same spirit as
  the `byte_at()` and dimension-cap consolidations
  (`content-modernization.md` M-12/M-13) that already apply the same fix
  to imagelib's other codecs.

## Out of scope / ABI-frozen

- **WAD3/mip on-disk byte format** — `dwadinfo_t`/`dlumpinfo_t`/`mip_t`
  (`common/wadfile.h`), a shared SDK struct under the campaign's frozen-ABI
  fence. `codec_wad.cpp:5,26-31` and `codec_mip.cpp:34-36` already mark the
  header/field-width constants "frozen"; the `k_mip_header = 40` /
  `k_mip_name = 16` duplication between those two files is tracked as part
  of `content-modernization.md` M-13's "optionally fold in" note, not a
  separate finding here.
- **`rgbdata_t`/`pixformat_t`/`imgFlags_t`** (`common/com_image.h`) —
  crosses the renderer ABI (`ref_api.h`) and the mainui SDK, but
  `content-boundary.md` OQ-1 already decided imagelib itself does **not**
  freeze on this struct; the adapter lives at the Chunk-13 renderer seam
  (`content-modernization.md` notes this is "unlocks modernization H-4 /
  M-1 / M-2 / O-2" for content, and the tree-wide corpus digest records it
  as obligation OBL-13-3). Nothing to do in imagelib itself.
- **Legacy `engine/common/imagelib/*.c`** — reference-only per repo rules;
  read for behaviour parity (byte-for-byte round-trip against
  `Image_LoadXXX`/`Image_SaveXXX`), never modified.

## Open questions

- **Compressed-format passthrough contract has never been exercised by a
  real consumer.** imagelib's `PixelFormat` set (`pixel_format.hpp:29-43`)
  passes DXT1/3/5, ATI2, BC4-7 (signed/unsigned), BC7 UNORM/sRGB, and raw
  KTX2 straight through for GPU-side decode; `codec_ktx2.cpp:13`'s own
  comment names `ref_vk` as the intended reader, but nothing in the tree
  has ever validated the contract against an actual texture-format
  capability query. This needs the Chunk-13 backend decision (which API,
  desktop vs. mobile) before it can be checked — recorded as a Chunk-13
  precondition (corpus digest obligation OBL-13-4), not work to schedule
  now.
- **No ETC2/ASTC codecs — a conditional gap, not a missing feature today.**
  If the Chunk-13 backend decision names a mobile-primary target (GL ES or
  mobile Vulkan), imagelib's `PixelFormat` set and codec roster need an
  ETC2/ASTC addition before mobile-native compressed assets can be
  decoded; desktop GL/Vulkan commonly expose BC6H/BC7 today and are
  already covered. Per the anti-gold-plating rule this is a **shape
  constraint only** (`consumer_status: speculative` until the backend
  decision names mobile in scope) — do not add codecs speculatively.
  Record the trigger in `content-boundary.md` / the eventual renderer
  boundary spec, not here as an actionable item.
- **Does H-1 (registry key column) need to land before or can it land
  alongside `content-modernization.md` H-7 (dead-pool deletion)?** Both
  touch `imagelib.cpp`'s `Impl` struct and `init()`/`decode()` in
  adjacent-but-disjoint regions (`pool_` lifecycle vs. the registry scan
  inside `decode()`); no ordering dependency was found, but whoever picks
  up H-1 should rebase past H-7 (or vice versa) rather than resolve a
  merge conflict blind, since both are pure-deletion/pure-restructure
  changes with no shared state between them.
