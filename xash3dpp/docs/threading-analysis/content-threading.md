# content — Threading Analysis (Chunk 7)

> Boundary spec: `docs/boundaries/content-boundary.md`

*2026-07-06 (as-built pass). Decision refs: Q-18 (studio bone math is bit-exact,
do-not-modernize), Q-21 / extension-goals §4 (content is where the worker pool +
`JobToken` land), P-1/P-2/P-3. `core::assert_thread_role` / `ThreadRole::Main`
is the enforcement primitive. Companion docs: `map_loader-threading.md` (the
sibling immutable-world reader), `server-threading.md` (the studio-hitbox
consumer), `memory-threading.md` (the pool backend).*

## Why content is the crux

Content is the **per-chunk hook for the worker pool + `JobToken`**
(extension-goals §4): it is the first subsystem where a real parallel workload
is scheduled, and the legacy code it replaces was singleton-hostile (a global
`imglib_t image` decode scratch, a `mod_studiohdr` current-model pointer, the
`mod_known[]` array, `g_poseverts`). This doc's job is to prove the as-built
code kept the door open and to name the one piece of machinery still owed (the
P-2 published-snapshot swap).

## Ownership model

**Main-thread-only for mutation; reentrant for pure work — enforced, not merely
assumed.** The design cleanly splits into two halves:

1. **The mutable registry half** (`ModelCache`, `ImageDecoder`) — every
   state-changing entry point opens with
   `assert_thread_role(ThreadRole::Main)`. **12 assertions across 2 files**:
   - `model_cache.cpp` (9): `init`, `shutdown`, `find_or_alloc`,
     `register_world`, `free_model`, `purge_for_level_change`, `free_unused`,
     `load_from_bytes`, `need_crc`.
   - `imagelib.cpp` (3): `init`, `shutdown`, `decode`.
2. **The pure-work half** (parsers, codecs, the bone/hull kernel) — **zero**
   `assert_thread_role` sites, **by design**: these functions hold no shared
   state, so they are unconditionally reentrant and a worker may own one off
   the Main thread. `compliance_scan content` is clean (0 blocker/warning/note);
   the pure kernels carry no thread-assert because there is nothing to protect.

There is **no file-scope mutable state anywhere in the 13 TUs** — the headline
P-3 result. Every legacy singleton was dissolved into instance state or a
`constexpr`:

| Legacy singleton | As-built replacement | Shape |
|------------------|----------------------|-------|
| `mod_known[]` / `mod_numknown` / `com_studiocache` | `ModelCache::Impl::slots_` (pimpl member) | instance |
| `mod_studiohdr` (current-model scratch) | `StudioView` (per-call value over caller bytes) | value |
| `imglib_t image` (global decode scratch) | codecs return an owned `Image` value | value |
| `g_poseverts` / `g_posenum` | local `setup_bones` / `calc_rotations` scratch | stack |
| `palette_q1[768]` / `palette_hl[768]` | `constexpr std::array<std::uint8_t, 768>` | compile-time |
| `load_game[]` codec table | `static const IImageCodec* const registry[]` | read-only |

No `longjmp`, no signal-handler reachability (content is never registered from a
signal handler; the host owns those) — **Step 5 signal-handler safety is N/A**.

## Safe items

All **Safe-by-contract** or **Safe-RO / Safe-immutable**:

- **`ModelCache::Impl` / `ImageDecoder::Impl`** — heap-owned pimpl instances,
  one per `EngineContext`, reached only through Main-thread entry points for
  mutation. Not static, not shared across threads. Safe by the Main-only
  contract.
- **The 7 codecs** (`wad/tga/bmp/dds/ktx2/mip/png`) — stateless free functions
  behind `IImageCodec`; each `decode()` reads the input `std::span`, allocates
  its own output `Image`, and touches no shared state. **Reentrant** — the P-1
  door for off-Main texture decode.
- **`parse_studio` / `parse_sprite` / `parse_alias`** — pure `std::span →
  Result<T>`; own scratch only. Reentrant.
- **The bone/hull kernel** (`calc_bone_adj`, `calc_bones`, `calc_rotations`,
  `setup_bones`, `bone_world_position`, `attachment_world_position`,
  `studio_hitbox_hulls`) and `BuiltinBoneSolver` — pure functions over a
  caller-owned `StudioView` + `BoneSetupInput`; per-call stack scratch (`adj[]`,
  `v1[6]`/`v2[6]`, `pos`/`q`). **Stateless: safe to share one `BuiltinBoneSolver`
  and each call owns its scratch** (its own header comment). Reentrant. *These
  are also the Q-18 bit-exact kernels — reentrant AND float-frozen.*
- **`constexpr` palette tables + the texgamma LUT** (`palette.cpp`) —
  compile-time immutable; no lazy-init guard, no first-call data race (the
  legacy `q1palette_init`/`hlpalette_init` bools are gone).
- **`registry[]`** (`imagelib.cpp`) — `static const` array of codec pointers;
  read-only, initialised at first `decode()` with C++11 thread-safe static
  init semantics. Safe-RO.

## Hazards

None **under the current Main-only contract.** The table records what *would*
race if an off-main consumer were wired **before** the P-2 snapshot machinery
lands — i.e. the caller contracts the module relies on but does not itself
synchronize. **The model cache is the crux.**

| Symbol | File | Class | Notes |
|--------|------|-------|-------|
| `ModelCache::resolve()` / `find()` (live `Model*`) | `model_cache.cpp` | Race-shared (contained by Main-only) | Hand back a **live** pointer into the mutable `slots_` vector. Safe today (all callers on Main); an off-main G-3 render/trace reader holding a `Model*` across a concurrent `free_model` / `load_from_bytes` / `free_unused` would dangle. **This is the P-2 door**: off-main readers must consume a published snapshot, not a live `resolve()`. |
| `ModelCache::slots_` (vector growth) | `model_cache.cpp` | Race-realloc (contained by Main-only) | `find_or_alloc` may `emplace_back`, reallocating the vector and invalidating every outstanding `Model*`/`Slot&`. A concurrent reader mid-reallocation is UB. Contained because mutation is Main-only. |
| `stats_` counters (`ModelCache`, `ImageDecoder`) | both | Race-counter (contained by Main-only / decode-Main) | `++models_loaded` / `++images_decoded` are plain, non-atomic. `decode()` asserts Main today, so even a "reentrant codec" call increments the shared `ImageDecoder::stats_` — the **one** shared write on the otherwise-pure decode path. If `decode()` is relaxed off-Main, the stat increment must move to the caller or become atomic. |

**Non-hazards worth recording** (negative data points):

- **No RNG** anywhere in content — clean determinism (unlike server's
  `s_rng_state`).
- **No static return buffers** — the ABI edge (`studio_extradata`) returns a
  pointer *into the owned model bytes*, not a per-call static scratch, so the
  server/abi *Race-static-buf* class does **not** appear here.
- **strnicmp/strncmp string_view→C-string over-read: ABSENT.** Name matching
  (`find`/`find_or_alloc`/`validate_crc`) uses `std::string_view ==
  std::string_view` over an owned `std::string`; the codec prefix tests
  (`codec_mip.cpp` `istarts_with`, `imagelib.cpp` `extension_of` lowercasing)
  are length-bounded. Texture/bone/bodygroup name matching — the strong
  candidate flagged for this sweep — was implemented safely. (Pattern present in
  utilities/filesystem/cmd_cvar; absent here, matching platform/core/host/abi/
  launcher/map_loader/networking.)

## Required caller contracts

1. **Mutate `ModelCache` / `ImageDecoder` from the Main thread only.** Enforced
   by the 12 asserts; unenforced entry off-Main is UB (vector realloc + live
   refs).
2. **Do not hold a `Model*` from `resolve()`/`find()` across any cache
   mutation.** The pointer is valid only until the next `find_or_alloc` /
   `free_model` / `free_unused` / `load_from_bytes` / `purge_for_level_change`.
   Off-main readers must take a `model_infos()` snapshot (owned copy) instead.
3. **One `BuiltinBoneSolver` may be shared across callers**, but each call must
   supply its own `out_bones` span — the solver keeps no state between calls.
4. **The pure parsers/codecs/kernels may run off-Main** provided the *result*
   is published back through a Main-thread `load_from_bytes` (or the caller owns
   the returned value outright).

## Recommendations

- **P-1 (worker pool) is a near-mechanical door.** The parse-off-Main /
  publish-on-Main split already exists: schedule `parse_studio` / a codec
  `decode()` on a worker, marshal the owned `Result<T>` / `Image` back to a
  Main-thread `load_from_bytes` / cache install. No loader change owed. Build it
  when a consumer schedules the first job (no gold-plating).
- **P-2 (published snapshot) is the real work.** Before any off-main render/
  trace reader, add a published, generation-swapped read view (double-buffer or
  copy-on-publish) so `resolve()` is never called across a thread boundary.
  `model_infos()` is the seed of this — extend it from a debug snapshot to the
  render-consumable registry view.
- **Relaxing `decode()` off-Main** requires moving the `stats_` increment out of
  the codec path (return the count to the Main caller, or make the two counters
  atomic). Trivial, but do it *with* the P-1 wiring, not before.
- **Do not "fix" the missing asserts on the pure kernels** — their absence is
  the P-1 door, not a P-8 conformance gap (contrast host `RunFrame` /
  map_loader `new_game`, where the missing assert *is* a gap). The pure kernels
  are reentrant on purpose.
- **Preserve the Q-18 bit-exactness of the bone math when threading it.** The
  kernel is reentrant *and* float-frozen; a worker-pool port must not reassociate
  or FMA-fuse the `calc_bones` float ops (see `content-modernization.md`).
