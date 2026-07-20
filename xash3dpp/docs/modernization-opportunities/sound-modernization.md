# Sound Modernization Opportunities

> **First pass, 2026-07-20** (tree-wide modernization audit). No prior
> `sound-modernization.md` existed — `sound` shipped as Chunk 9 (S9.0-S9.8,
> the tree's first and only multi-threaded subsystem: MPSC → decoder thread →
> SPSC ring → callback, closed by a byte-identical-PCM console-scripted
> witness) without a modernization report ever being written for it. This
> pass is a from-scratch scan of `src/sound/*.cpp` and
> `include/xash3dpp/{sound,private/sound}/` cross-checked against the
> Phase-2 adversarial digest (F64-F81) and the cross-cutting lenses pass
> (L1 publish-primitive, L2 registry-unification, L3 introspection-stats, L6
> allocation-seam, L7 determinism-rng, L8 thread-model, L11 subtraction).
> IDs start at H-1/M-1/L-1 — there is no prior tier sequence to continue.
> Max tier observed this pass: **H-3, M-5, L-6**.

> C++ standard in use: C++**23** (`xash3dpp/CMakeLists.txt`,
> `CMAKE_CXX_STANDARD 23`; `target_compile_features(xash3dpp_sound PUBLIC
> cxx_std_23)`)\
> Boundary spec: [`docs/boundaries/sound-boundary.md`](../boundaries/sound-boundary.md)\
> ABI-frozen symbols in this subsystem: the SoundAPI override surface
> (`snd_globals_t`/`sound_api_t`/`sound_interface_t`, `CL_SOUND_INTERFACE_VERSION
> = 1`) vendored verbatim at `xash3dpp/include/xash3dpp/abi/sound_api.hpp`,
> pinned by `tests/.../test_sound_api_layout.cpp` field-offset asserts. Sound
> is **not** one of the BRIEF's five HB-2 byte-exact-kernel subsystems
> (map_loader/content/networking/server/utilities) — but see **Out of
> scope** for the S9.8 PCM-determinism witness, which is a project-level
> parity contract outside the audit's two fences and still binds every item
> below touching the mix/DSP arithmetic path.

## Summary

`sound` is the tree's one real multi-threaded subsystem (both of the tree's
two production `thread::spawn` calls live in `src/sound/topology.cpp`) and
is already idiomatic C++23 throughout: `std::expected`-shaped errors,
`std::atomic` stats, RAII device/loader ownership, and a pool created via
`create_pool`/`destroy_pool` (contrary to a stale boundary-doc row — see
L-5). There is no legacy-C residue to convert. What this pass found instead
is internal **shape drift accumulated across eight S9.x slices**: the same
four-line `bound()` clamp reimplemented seven times, a codec dispatch table
whose declared key column is dead code, four write-never-read placeholder
structs still carried as `Impl` members, and a 25-cvar DSP surface
maintained as six independently hand-written lists. None of it is a
correctness bug in shipped behaviour — the S9.8 byte-identical-PCM witness
already guards the mix kernel — and none of it requires a design decision;
every High and most Medium items here are same-behaviour deletions or
relocations with an existing, named, in-tree consumer.

Two items are elevated one tier under the brief's "prefer deletions" rule
because the tree-wide L11 subtraction lens independently named them as
zero-risk, fence-clear deletions (H-2, H-3). One item (H-1) is elevated
because it is sound's slice of a tree-wide registry-unification effort
(L2/HB-13) with a concrete, evidenced fix and a live parity-adjacent defect
(the two codec tables can silently disagree with no diagnostic). Sound is
also the richest in-tree evidence base for two open tree-wide backlog items
— HB-5 (published-snapshot idiom, four bespoke shapes live here) and HB-12
(deterministic RNG, a third untracked stand-in lives here) — recorded under
**Open questions** rather than as sound-local work, per the anti-gold-plating
rule.

Fact-base corrections carried forward from CORRECTIONS.md / the digest and
verified against source in this pass: `IAudioDevice` and `IAudioLoader` are
**not** zero-production-impl interfaces (Phase-0's regex missed header-only
implementations — `NullDevice`/`SinkDevice`/`FilesystemAudioLoader` are all
real, `device.hpp:141-199`, `registry.hpp:137-146`); the Phase-0
`malloc_free:2` and `c_cast_suspect:2` heuristic hits for sound are both
comment-substring false positives (zero real sites of either); `SoundStats`
has 5 always-on atomic Tier-1 fields, not 1 (`sound.hpp:57-82`) — it is
Class-B ("atomic storage, `const&` accessor") per L3's four-way stats
taxonomy, already G-3-safe, no action needed.

______________________________________________________________________

## High-priority opportunities

### H-1: The codec registry's declared key column is dead code, and the shadow extension table can silently disagree with it

- **File(s)**: `xash3dpp/include/xash3dpp/private/sound/codec.hpp:87-99`
  (`AudioCodecEntry`/`k_audio_codecs`), `:106-119` (`k_audio_extensions`,
  `is_supported_audio_extension`), `:124-133` (`find_audio_codec`), `:49`
  (`IAudioCodec::handles` pure virtual); `xash3dpp/src/sound/codec_wav.cpp:408`
  (`WavCodec::handles` override); `xash3dpp/src/sound/registry.cpp:53-71`
  (the only production caller, plus the heap-allocating `to_lower` at :58);
  `xash3dpp/tests/sound/test_sound_codec.cpp:137-161` (7 asserts pinning
  the two-table shape).

- **Current pattern**: `AudioCodecEntry::extension` is declared, commented
  as "Mirrors `archive_registry.hpp`'s `k_archive_types` shape"
  (`codec.hpp:84`), and never read: `find_audio_codec`'s loop calls
  `codec.handles(ext)` instead of comparing `entry.extension`, and
  `WavCodec::handles` re-hardcodes the literal `"wav"` a second time. A
  **separate**, key-only `k_audio_extensions` array (4 rows) answers the
  "is this a known format?" query, so the two tables can disagree in the
  undesigned direction — a codec row present in `k_audio_codecs` but
  omitted from `k_audio_extensions` makes `is_supported_audio_extension`
  say no while `find_audio_codec` still returns a codec, and nothing
  (compiler, runtime, or test) would catch it.

- **Suggested replacement**: make `k_audio_codecs` the single 4-row
  nullable-accessor table `{{"wav",&wav_codec},{"mp3",nullptr},
  {"ogg",nullptr},{"opus",nullptr}}`; `find_audio_codec` becomes
  `if (ci_equal(e.key, ext) && e.codec) return &e.codec();` using the
  existing `xash::utilities::ci_equal`; `is_supported_audio_extension`
  becomes the same scan without the null check. Delete
  `k_audio_extensions`, delete the pure virtual `IAudioCodec::handles`
  and `WavCodec`'s override plus its duplicated `"wav"` literal, and
  delete the heap-allocating `to_lower` at `registry.cpp:58` (`ci_equal`
  removes the need to pre-lowercase). Update the 7 pinning asserts in
  `test_sound_codec.cpp:137-161`. This is sound's item under the
  tree-wide `HB-13` registry-unification convention (L2-R1/L2-R2), which
  also converts `k_wad_types`/imagelib/the filesystem archive table to
  the same shape — land in the same style, not independently.

- **Boundary-safe**: Yes. Sound's HB-2-adjacent exposure is the mix
  kernel and the DSP float chain; neither is on the codec-selection
  path, and the S9.8 determinism witness compares PCM for a fixed
  console script, unaffected by which table row selects the WAV codec.

- **Rationale**: this is a pure deletion (−1 table, −1 pure virtual, −1
  override, −1 duplicated literal, −1 allocation) that also removes the
  only silent-disagreement hazard in the subsystem's dispatch surface.
  Named day-one consumer: `FilesystemAudioLoader::load`
  (`registry.cpp:61,71`), the sole production caller, unchanged in shape
  — `consumer_status: exists-in-tree`. **Open decision, not settled by
  this report**: switching to `ci_equal` is a case-insensitivity
  behaviour change matching legacy's `Q_stricmp` contract at
  `snd_utils.c:454`, and `test_sound_codec.cpp:142`
  (`CHECK(!wav_codec().handles("WAV"))`) currently pins the opposite —
  see **Open questions**.

______________________________________________________________________

### H-2: Four `owned_state.hpp` structs are write-never-read `Impl` members — delete them

- **File(s)**: `xash3dpp/include/xash3dpp/private/sound/owned_state.hpp:45-76`
  (`MixClock`, `ChannelState`, `RawChannelState`, `AmbientState`);
  `xash3dpp/src/sound/sound.cpp:195-198` (the four `Sound::Impl` member
  declarations); `xash3dpp/include/xash3dpp/private/sound/vox.hpp:492`
  (the `ChannelState` name collision this deletion also clears).

- **Current pattern**: `owned_state.hpp` declares six typed placeholder
  structs ported from the legacy `snd` global's fields as boundary-doc
  scaffolding. Only two are alive: `DmaState` (read at `sound.cpp:1092`
  for `s_info`) and `FadeState` (written by `cmd_s_fade_f`/
  `cmd_soundfade_f`, read by a named Chunk-12 consumer). `MixClock`,
  `ChannelState`, `RawChannelState`, and `AmbientState` are
  default-constructed as `Sound::Impl` members and never read anywhere
  in `src/` or `tests/`; `ChannelState` additionally collides in name
  (not in type) with an unrelated `ChannelState` inside `vox.hpp:492`.

- **Suggested replacement**: delete `MixClock`, `ChannelState`,
  `RawChannelState`, `AmbientState` from `owned_state.hpp` and their four
  member declarations from `Sound::Impl` (`sound.cpp:195-198`). Keep
  `DmaState` and `FadeState`, both of which have a live or named-Chunk-12
  reader.

- **Boundary-safe**: Yes — pure deletion of dead, default-constructed,
  never-read members; no ABI, no wire format, no HB-2 kernel involved.

- **Rationale**: independently confirmed by the tree-wide L11 subtraction
  lens as part of its decision-free deletion batch (zero call sites,
  zero design questions, fence clear). Net effect: −4 structs, ~32
  lines, 4 dead `Impl` members, and one fewer same-name-different-type
  collision in the header namespace. Named day-one consumer: n/a — pure
  deletion. `[EXT: none — this narrows P-3/P-7 surface, it does not open
  a door]`

______________________________________________________________________

### H-3: Seven byte-identical `bound_int`/`bound_float` bodies across five sound TUs — hoist one

- **File(s)**: `xash3dpp/src/sound/channel_alloc.cpp:23-31`;
  `xash3dpp/src/sound/mixer.cpp:37-40`;
  `xash3dpp/src/sound/vox.cpp:40-43`;
  `xash3dpp/src/sound/sound.cpp:53-56`;
  `xash3dpp/include/xash3dpp/private/sound/dsp.hpp:210` (inlined into
  `select_room`'s clamp); `xash3dpp/include/xash3dpp/private/sound/mixer.hpp:227`
  (inlined into `clip16`).

- **Current pattern**: the legacy `bound(min,num,max)` macro
  (`public/xash3d_mathlib.h:141`) is reimplemented independently in five
  translation units plus hand-inlined twice more — all seven bodies are
  the identical `v >= lo ? ( v < hi ? v : hi ) : lo` expression, each
  carrying its own "duplicated locally per the `mixer.cpp`/`vox.cpp`/
  `dsp.cpp` precedent" comment rather than a shared definition.

- **Suggested replacement**: hoist one `[[nodiscard]] constexpr`
  `bound_int`/`bound_float` pair into a header every TU already includes
  transitively — `private/sound/mix_kernels.hpp` if one is created for
  H-1/M-2's arithmetic-law constants, otherwise `owned_state.hpp` or a
  new small `bound.hpp` — and replace the five free-standing copies plus
  the two inlined clamps with calls to it.

- **Boundary-safe**: Yes — provably bit-identical: all seven bodies are
  the same text and `constexpr`-evaluate identically; this is a
  same-arithmetic relocation, not a rewrite, so it carries no risk
  against the S9.8 PCM witness.

- **Rationale**: confirmed by the L11 subtraction lens as fence-clear
  (verified per-item: the bodies are `constexpr`-identical, no
  frozen-ABI signature is touched). The tree-wide shape constraint this
  item exemplifies — "the second copy of any micro-predicate goes into a
  private header on first duplication, not the fifth" — is exactly what
  sound's own history violated seven times over. Named day-one consumer:
  the five existing call TUs plus the two inlined sites, unchanged in
  behaviour — `consumer_status: exists-in-tree`.

______________________________________________________________________

## Medium-priority opportunities

### M-1: Two unwaived candidate-BLOCKER mutable statics; DSP RNG ownership closed

- **File(s)**: `xash3dpp/src/sound/vox.cpp:57-58` (`g_vox_period_word`,
  `g_vox_comma_word`); `xash3dpp/docs/boundaries/sound-boundary.md:372`
  (the doc's P-3 row).

- **Current pattern**: `sound-boundary.md`'s P-3 row asserts flatly that
  "the campaign added none" [no file-scope mutable state]; two were
  added during S9.x. `compliance_scan --subsystem sound` reports
  `vox.cpp:57` and `:58` as candidate-BLOCKERs (`mutable-global`) on
  every run — neither carries a `compliance-allow(mutable-global)`
  marker, even though the file already carries the justification prose
  immediately above them ("no shared/global mutable state beyond what
  the legacy file-scope statics already had").

- **Suggested replacement**: add the missing
  `compliance-allow(mutable-global)` machine-readable markers to
  `vox.cpp:57-58` pointing at the existing rationale — the prose is
  already correct, only the token is missing. Chunk 11 separately removed
  DSP's function-local RNG state and injected the canonical random-long
  callback; a missing callback is inert and creates no fallback stream.
  Correct `sound-boundary.md:372`'s P-3 row to name the remaining two
  additions and their waiver status instead of claiming zero.

- **Boundary-safe**: Yes — annotation and doc changes plus a
  member-relocation of mutable state that is already function-local
  (not file-scope after the move); no behaviour change.

- **Rationale**: two permanent-candidate-BLOCKER compliance-scan hits
  are cheap to clear and currently make every `compliance_scan`
  invocation on sound noisy for a justified, already-documented case.
  The doc self-contradiction (a flat "none" next to three real
  additions) is exactly the class of false boundary-spec claim the
  tree-wide L11 lens flags as a standing hazard (`decisions-architecture.md`
  §3a). Named day-one consumer: `compliance_scan.py sound`, run at every
  commit — `consumer_status: exists-in-tree`.

### M-2: The 25-cvar DSP surface is six hand-written parallel lists — collapse the registration/default half, not the read half

- **File(s)**: `xash3dpp/src/sound/sound.cpp:273-279` (`Cvars` members);
  `xash3dpp/src/sound/sound.cpp:479-494` (registration rows);
  `xash3dpp/src/sound/sound.cpp:291-306` (bare string-literal poll);
  `xash3dpp/include/xash3dpp/private/sound/audio_command.hpp:106-121`
  (`MixConfigSnapshot` fields with defaults repeated a third time).

- **Current pattern**: adding, removing, or re-defaulting one DSP cvar
  requires coordinated edits in six places — a named `Cvars` member, a
  `register_cvar()` row, a `poll_mix_config()` bare-string lookup, a
  `MixConfigSnapshot` field carrying the default a second time, an
  `apply_mix_config()` setter, and a `RoomDsp` member carrying the
  default a **third** time. All three copies of every default agree
  today (spot-checked `room_feedback` 0.2f, `waterroom_type` 14,
  `room_delay` 0.8f across all three sites), so this is structural drift
  risk, not a live bug.

- **Suggested replacement**: **AMENDED from the original two-part
  proposal** — collapse only the registration and default-value copies.
  Add one `constexpr std::array<DspCvarSpec, 14>` of
  `{ name, default_string, default_value, flags, description }` in a
  header the relevant TUs already include, and use it for exactly two
  things: (a) drive the registration loop replacing
  `sound.cpp:477-493`, and (b) supply `MixConfigSnapshot`'s member
  initialisers at `audio_command.hpp:106-121` so the second copy of each
  default becomes a table reference instead of a hand-typed literal. **Do
  not** replace the per-frame `poll_mix_config()` cvar reads
  (`sound.cpp:291-306`, 17 lookups/frame) with a `Cvars::*`
  member-pointer table walk — see Rationale.

- **Boundary-safe**: NeedsVerification — the registration/default half is
  a same-value table-drive with no behaviour change; the amendment below
  is what makes it safe.

- **Rationale**: the duplication half is real and confirmed by reading
  all six lists directly. The headline optimisation in the original
  finding — replacing `ctx.cvar_variable_value("room_type")`-style reads
  with a `(cv_.*row.slot).abi.value` table walk — was **refuted as a
  correctness hazard** during Phase-2 adversarial review:
  `cvar_register_engine` is not unconditional (`cvar_ops.cpp:41-44`), so
  a member-pointer table built at construction time can point at a cvar
  slot that registration later skipped, which the current
  per-name-string lookup does not risk. The amended, narrower proposal
  keeps that lookup path exactly as-is and only removes the two
  default-value copies, which is where the actual drift risk lives.
  Named day-one consumer: the existing `register_cvar`/`MixConfigSnapshot`
  construction call sites, unchanged in observable behaviour —
  `consumer_status: exists-in-tree`. `[EXT: P-4]` — this is also the
  first step toward a typed cvar-spec table, which the tree-wide L3 lens
  names as a missing P-4 introspection surface tree-wide.

### M-3: Five unwaived Q-22 `make_unique`-outside-pimpl sites — split by verdict, two already qualify for `pool_new`

- **File(s)**: `xash3dpp/src/sound/sound.cpp:399` (create_pool, for
  context); `:423-424` (`RoomDsp`); `:435-439` (`FilesystemAudioLoader`,
  `SfxRegistry`); `xash3dpp/src/sound/sound.cpp:539-550` (further
  `make_unique` sites in `create_sound`).

- **Current pattern**: `compliance_scan --subsystem sound` reports five
  unwaived make-unique-outside-pimpl warnings — `RoomDsp` (:423),
  `FilesystemAudioLoader` (:435), `SfxRegistry` (:438-439), the `Sound`
  object itself inside `create_sound`, and one unwaived
  unique-ptr-nonpimpl warning on `create_sound`'s return type
  (`sound.hpp:355`). Sound already created its pool via
  `create_pool("sound")` (:399-403) before any of these five
  allocations, so the pool the P-7 idiom needs already exists at the
  point each object is constructed.

- **Suggested replacement**: split by verdict rather than a blanket
  waiver. `RoomDsp` and `SfxRegistry` both satisfy `alignof(T) <= 8`
  (members are `int`/`float`/`size_t`/pointer/`std::string`/
  `std::vector`/`std::unordered_map`, no over-aligned atomics) and have
  a lifetime strictly inside `Sound::Impl`'s (constructed after the pool,
  destroyed before it) — move both to `pool_new`/`create_room_dsp()`/
  `create_sfx_registry()` factories with the dual-`operator delete` pair
  per the P-7 idiom. `FilesystemAudioLoader`, the `Sound` object itself,
  and `create_sound`'s return type are correctly left as `make_unique`
  (the sanctioned pimpl-`Impl` exception, or a genuine top-level owning
  handle) and should instead gain the `compliance-allow` marker they are
  currently missing.

- **Boundary-safe**: Yes — `RoomDsp`/`SfxRegistry` migration is a pure
  ownership-mechanism swap with identical construction arguments and
  lifetime; no behaviour change.

- **Rationale**: this converts two of the five warnings into a real
  P-7 conformance fix (using a pool sound already pays the cost of
  creating) rather than five waiver comments papering over a
  distinction the code itself already supports. Named day-one consumer:
  `Sound::Impl`'s existing `room_dsp_`/`registry_` construction sites,
  unchanged in shape — `consumer_status: exists-in-tree`.

### M-4: Sound is the richest in-tree evidence for HB-5 (four bespoke cross-thread publish shapes) — no sound-local fix, record the mechanism status

- **File(s)**: `xash3dpp/include/xash3dpp/sound/sound.hpp:54-82`
  (`SoundStats`, whole-struct relaxed atomics); `xash3dpp/include/xash3dpp/private/sound/audio_command.hpp:253-258`
  (`ListenerSnapshot`/`MixGateSnapshot`/`MixConfigSnapshot`, POD-over-MPSC);
  `xash3dpp/include/xash3dpp/private/sound/topology.hpp:146-149`
  (`MouthSlots`, packed atomic slots); `xash3dpp/include/xash3dpp/private/sound/topology.hpp:378-386`
  and `xash3dpp/src/sound/topology.cpp:416-472`
  (`AudioTopology::publish_channels_if_requested`/`channel_snapshot`, the
  tree's one cross-thread structured publisher); `xash3dpp/src/sound/sound.cpp:925`
  (`Sound::channels_snapshot()`, its Main-side consumer); `xash3dpp/src/sound/sound.cpp:463`
  (the `s_show` cvar it exists for).

- **Current pattern**: within one subsystem there are four structurally
  different shapes for moving decoder-owned data to Main or vice versa:
  (1) `SoundStats` — whole-struct relaxed atomics, any-thread read; (2)
  the three `*Snapshot` PODs riding the MPSC `AudioCommand` payload,
  one-way T_Main → T_AudioDecoder; (3) `MouthSlots` — a hand-packed
  `std::atomic<uint64_t>` per slot, decoder → Main, drained against a
  Main-private `seen_` set; (4) `AudioTopology::publish_channels_if_requested`/
  `channel_snapshot` — decoder → Main, pull-based, guarded by an
  acq_rel request flag plus a `publish_mutex_`/`publish_seq_` pair,
  returning an allocating `std::vector<PublishedChannel>`. Mechanism (4)
  is the **tree's only** cross-thread structured-publish handshake, and
  its sole Main-side consumer, `Sound::channels_snapshot()`, has **zero
  production callers** — the `s_show` cvar it was built to serve is
  registered (`sound.cpp:463`) and never read anywhere; only
  `tests/sound/test_sound_entry.cpp` calls it.

- **Suggested replacement**: no sound-local code change. Each of the
  four shapes is individually justified by a real constraint (hot-path
  vs cold-path, tearing avoidance, POD-vs-owning), so unifying them
  inside sound would not remove real drift, it would remove the tree's
  best worked examples. Record in `sound-boundary.md` that mechanism (4)
  is test-exercised only, so a future auditor does not misread it as
  dead code, and that when HB-5's shared published-snapshot idiom is
  briefed, it must be evaluated against all four of sound's shapes
  (plus `LockedSfxResolver`, `registry.hpp:150-172`) as its primary test
  cases, not derived from a blank slate.

- **Boundary-safe**: N/A — documentation-only; no source file is
  changed by this finding.

- **Rationale**: `HB-5` ("one shared published-snapshot idiom") is an
  open, un-briefed tree-wide backlog item. Building a fifth mechanism or
  a generic primitive here now would have no named consumer — the tree
  has exactly two production thread spawns and both are sound's, so
  "future subsystem X will need this too" is speculative by the brief's
  own definition. `consumer_status: speculative` for anything beyond
  documentation; the doc-record itself has an `exists-in-tree` consumer
  (the next HB-5 brief author). `[EXT: P-2]`.

### M-5: Correct the boundary doc's stale P-7 "door-debt, not fixed" row — the pool idiom already landed

- **File(s)**: `xash3dpp/docs/boundaries/sound-boundary.md:381`;
  `xash3dpp/src/sound/sound.cpp:399-403` (pool creation);
  `xash3dpp/src/sound/sound.cpp:684-687` (pool teardown).

- **Current pattern**: the Q-21 Extension-axes table's P-7 row still
  reads "Door-debt, noted not fixed", citing legacy's raw
  `Mem_AllocPool`/`Mem_FreePool` as the current state. `Sound::Impl` now
  owns a `xash::memory::PoolHandle` created via `create_pool("sound")`
  at `init()` and torn down via `destroy_pool()` at `shutdown()` — the
  exact `create_<thing>`/`PoolHandle` idiom the row says is missing.

- **Suggested replacement**: update the P-7 row to reflect that the
  sound pool follows the `create_pool`/`destroy_pool` idiom; keep any
  genuinely open piece (e.g. whether `RoomDsp`'s `PoolIntBuffer` members
  or M-3's two `make_unique` sites want a follow-up review) as a
  narrower note rather than leaving the whole row flagged unfixed.

- **Boundary-safe**: N/A — doc-only change.

- **Rationale**: prevents a future auditor re-deriving "the pool debt is
  unfixed" from a stale doc row when the code already conforms; overlaps
  usefully with M-3, which finishes the P-7 story for the two remaining
  `make_unique` sites the pool now makes eligible for `pool_new`.

______________________________________________________________________

## Low-priority / cosmetic opportunities

### L-1: Two hand-maintained console-command lists (14 added / 13 removed) — table-drive them

- **File(s)**: `xash3dpp/src/sound/sound.cpp:497-518` (14 `cmd_add`
  calls); `:648-660` (13 `cmd_remove` calls);
  `xash3dpp/docs/boundaries/sound-boundary.md:379` (the "13 added, 12
  removed" quirk note — off by one against the actual 14/13 count).

- **Current pattern**: two independent literal lists ~150 lines apart.
  The one intentional asymmetry — `"play2"` is deliberately never
  removed, preserving legacy's leaked registration (`s_main.c:2046-2058`)
  — exists only as the *absence* of a `cmd_remove("play2")` line,
  explained by a comment above the remove block.

- **Suggested replacement**: replace both lists with one
  `constexpr std::array<SoundCommand, 14>` of `{ name, CommandCtxFn fn,
  flags, description, bool remove_at_shutdown }`; init loops adding,
  shutdown loops removing where `remove_at_shutdown` is true; `"play2"`
  becomes the single row with `false`, making the quirk explicit,
  greppable, and unit-testable (a test can assert exactly one row has
  `remove_at_shutdown == false`).

- **Boundary-safe**: Yes — same registration order, same flags, no
  behaviour change; the `"play2"` leak is preserved by construction of
  the table rather than by omission.

- **Rationale**: cosmetic — the current code is correct, just harder to
  audit than a table. Named day-one consumer: the existing `init()`/
  `shutdown()` call sites — `consumer_status: exists-in-tree`.

### L-2: `IAudioCodec::decode` is the only non-`noexcept` virtual across the subsystem's 10 interfaces

- **File(s)**: `xash3dpp/include/xash3dpp/private/sound/codec.hpp:54-55`
  (`decode`, not `noexcept`); `:41` (`handles`, `noexcept`, for
  contrast); `xash3dpp/include/xash3dpp/private/sound/registry.hpp:145`
  (`IAudioLoader::load`, `noexcept`, wraps `decode`);
  `xash3dpp/src/sound/codec_wav.cpp:409` (the one override).

- **Current pattern**: sound declares 15 pure virtuals across 10
  interfaces; 14 are `noexcept`, `IAudioCodec::decode` alone is not. The
  obvious "allocating functions aren't `noexcept`" explanation does not
  hold: `IAudioLoader::load`, which **wraps** `decode` and also returns
  an owning, allocating `std::optional<AudioData>`, **is** `noexcept`.

- **Suggested replacement**: mark `IAudioCodec::decode` `noexcept` to
  match the other 13 virtuals and `IAudioLoader::load`, and update the
  one override in `codec_wav.cpp:409`. (If the tree's actual rule is the
  reverse — allocating virtuals stay non-`noexcept` — then
  `IAudioLoader::load` is the one that's wrong; either resolution is
  acceptable, but the two seams on the same decode path must agree.)

- **Boundary-safe**: Yes — `decode`'s only production body
  (`WavCodec::decode` → `decode_wav`) does not `throw` (the tree builds
  `/EHs-c-`); marking it `noexcept` documents an invariant the code
  already has.

- **Rationale**: cheap consistency fix; recorded so the two-seam
  disagreement is not silently perpetuated the next time a codec is
  added.

### L-3: `xash3dpp_memory` is linked `PUBLIC` from `xash3dpp_sound` but no public header needs it

- **File(s)**: `xash3dpp/src/sound/CMakeLists.txt:41-47`.

- **Current pattern**: `xash3dpp_memory` is `PUBLIC`, but no header
  under `include/xash3dpp/sound/**` includes anything from
  `xash3dpp/memory/**` — the pool usage (`create_pool`/`destroy_pool`,
  `PoolIntBuffer`) is confined to `sound.cpp` and the private
  (non-installed) `private/sound/dsp.hpp`.

- **Suggested replacement**: change `PUBLIC xash3dpp_memory` to
  `PRIVATE xash3dpp_memory`. (`xash3dpp_utilities` stays `PUBLIC`
  correctly — `providers.hpp:13` publicly includes `utilities/math.hpp`
  for `Vec3`.)

- **Boundary-safe**: Yes — one-line, purely mechanical link-visibility
  fix; no code change.

- **Rationale**: a downstream target that only includes
  `sound/sound.hpp` or `sound/device.hpp` currently gets a transitive
  link to `xash3dpp_memory` it never asked for.

### L-4: DSP's third RNG stand-in — ✅ closed Chunk 11

- **As built:** `RoomDsp` receives a narrow random-long callback through
  `SoundInitParams`; its former function-local LCG is gone and a null callback
  returns the lower bound without creating a fallback stream.
- **Proof boundary:** a spy proves `dsp_profile` routes draws through the
  injected function and remains unavailable while the audio topology runs.
  This closes ownership/injection only; legacy-equivalent sound/pmove draw
  scheduling awaits production client integration and a captured schedule.

### L-5: `SoundStats` fact-base correction — 5 atomic fields, not 1 (no code action)

- **File(s)**: `xash3dpp/include/xash3dpp/sound/sound.hpp:57-82`.

- **Current pattern**: `facts.json`'s stats-struct inventory records
  `SoundStats` with `atomic_fields: 1`, undercounting the struct's own
  fields — all 5 are `std::atomic` (`mix_blocks`, `underruns`,
  `active_channels`, `dropped_sounds`, `dsp_room`).

- **Suggested replacement**: no code change. `SoundStats` is fully
  compliant with the tree's Class-B stats shape (atomic storage,
  `const&` accessor is safe because every field is itself atomic) per
  the tree-wide L3 introspection lens, and already exposes
  `[[nodiscard]] const SoundStats &stats() const noexcept` —
  `diagnostics_dump`-ready today, unlike the 6 Class-D structs
  (`ClockStats`, `HostStats`, `ContentStats`, `ImageStats`,
  `FilesystemStats`, `StringPoolStats`) elsewhere in the tree that hand
  back a live reference into non-atomic memory.

- **Boundary-safe**: N/A — fact-base correction only.

- **Rationale**: sizes the future `diagnostics_dump` aggregator work
  (M-4's HB-5 note and the tree-wide L3 recommendation) correctly for
  this subsystem; correcting the undercount here is what the brief calls
  a valuable finding in its own right.

### L-6: Decoder-thread allocation goes through the system heap, not the pool — document the assumption it depends on

- **File(s)**: `xash3dpp/src/sound/topology.cpp:438`
  (`std::vector::push_back` inside `publish_channels_if_requested`, on
  `T_AudioDecoder`).

- **Current pattern**: the one place sound allocates off the main
  thread uses the default `std::vector` allocator (system heap), not
  `xash::memory`'s pool. This is safe today only because nothing else
  in the tree pool-allocates off-main — `xash3dpp/docs/design/threading-model.md:566`
  currently (incorrectly, per the tree-wide L8 lens) claims a pool
  spinlock exists; it does not (`grep -rn 'spinlock|atomic_flag|mutex'`
  over `src/memory/`, `include/xash3dpp/memory/`,
  `include/xash3dpp/private/memory/` returns nothing).

- **Suggested replacement**: no sound code change — the current
  behaviour is correct given the pool is not thread-safe and this path
  does not use it. Record in `sound-boundary.md` that
  `publish_channels_if_requested`'s allocation is a deliberate
  system-heap escape (not an oversight), and that it is sound's own
  load-bearing precedent for the tree-wide assumption "nothing
  pool-allocates off-main" — if that assumption is ever falsified
  elsewhere, this call site's safety argument needs re-examination too.

- **Boundary-safe**: N/A — doc note; no code change.

- **Rationale**: `threading-model.md`'s memory row is flagged tree-wide
  (L8) as the single most dangerous stale claim in that document,
  because it is consulted as ground truth by future off-main work. Sound
  is the concrete example that shows the claim is not exercised
  correctness today, only accidental absence of a competing writer.

______________________________________________________________________

## Out of scope / ABI-frozen

- **Fence 1 (frozen ABI shape)**: the SoundAPI override surface
  (`snd_globals_t`/`sound_api_t`/`sound_interface_t`,
  `xash3dpp/include/xash3dpp/abi/sound_api.hpp`, vendored per the
  BRIEF's `xash3dpp/include/xash3dpp/abi/` rule) — `CL_SOUND_INTERFACE_VERSION
  = 1`, pinned by `test_sound_api_layout.cpp`'s offset asserts. No
  finding in this report touches it.
- **HB-2-adjacent but not one of the five named kernels**: sound is not
  in the BRIEF's five-subsystem HB-2 list (map_loader/content/networking/
  server/utilities). Its own project-level parity contract — the S9.8
  byte-identical-PCM console-scripted witness
  (`tests/sound/test_sound_witness.cpp:623`) — is not one of the audit's
  two fences, but every item in this report that touches the mix/DSP
  arithmetic path (H-3, M-2) was checked against it and found
  arithmetic-preserving (same bodies, same values, no reassociation).
- **`IClipHooks`-style scheduled doors — none apply to sound.** Sound
  carries no zero-production-implementation interfaces of its own after
  the F64/F65 fact-base corrections (`IAudioDevice`: `NullDevice` +
  `SinkDevice`; `IAudioLoader`: `FilesystemAudioLoader`) — both are real,
  header-declared implementations the Phase-0 regex missed.
- **The S9.7b conditional-role thread-assert waivers**
  (`vox.cpp:546-551`, `dsp.cpp:74-78,89,98,107,116,688`,
  `registry.cpp:37-41`) are genuine either/or role cases — the object is
  legitimately owned by `T_AudioDecoder` when the topology runs and by
  `T_Main` when it does not, and no single per-call assert can express
  that. Verified 0 of 13 waivers in sound are unjustified debt; not a
  finding, recorded so a future pass does not re-litigate it.
- **`decoder_step()`'s Main-pinning-by-design is not incidental.** Every
  `S_*`-descended public entry point on `Sound` is Main-pinned by a real
  synchronous client-DLL/SNDDMA contract (no thread existed to race in
  legacy); `decoder_step()` is deliberately the one exception, and the
  actual mixing was moved off Main via the MPSC/SPSC topology while
  preserving that contract. Contrast with subsystems where Main-pinning
  is more likely incidental (nothing yet moves them) — not a finding
  here, recorded for cross-subsystem context.

## Open questions

- **H-1's `ci_equal` question needs an explicit owner sign-off, not a
  cleanup-commit ride-along.** Legacy compares audio-format extensions
  with `Q_stricmp` at `snd_utils.c:454`; the current tree compares with
  `==` and `test_sound_codec.cpp:142` pins the case-sensitive behaviour
  as if it were a requirement. This is a real behaviour change inside a
  closed (Chunk 9) subsystem — the tree-wide L2 lens recommends adopting
  `ci_equal` everywhere (matching legacy's contract, and matching
  sibling table `k_wad_types` in filesystem, which already uses
  `ci_equal` correctly) and treating the test as the thing that's wrong,
  but that is a decision for the sound owner, not something this report
  settles unilaterally.
- **Do not unify sound's three string-bearing snapshot channels
  (`channels_snapshot`, and by extension `Input::bindings_snapshot`/
  `Content::model_infos`) onto the pool's `std::span`-out-param shape.**
  The tree-wide L3 lens records this explicitly as a shape constraint:
  row types owning `std::string` (`ChannelInfo::sentence_name`,
  `sound.hpp:55`) structurally cannot ride a span, and the correct
  sanctioned shape for that class is a cold-path-only `std::vector`
  return, never called on a frame budget or from a debug thread
  expecting no allocation. Recorded here so a future HB-5/HB-6
  implementer does not reshape `channels_snapshot()` to fit a primitive
  it was never designed against.
- **`docs/architecture/sound/` does not exist.** `sound` is one of four
  shipped subsystems (with `content`/`save`/`input`) with no
  `/document-architecture` output yet, despite Chunk 9 being complete
  with a byte-exact witness. Outside a modernization report's scope to
  fix, flagged since a future architecture pass will need H-1/H-2/H-3
  landed first to avoid documenting the dead structs and the
  double-table dispatch as if they were the design.
- **M-4's four publish shapes should be the primary worked examples
  when HB-5 is next briefed**, not a blank-slate design — see M-4 for
  the specific citation set (`sound.hpp:54-82`,
  `audio_command.hpp:253-258`, `topology.hpp:146-149,378-386`,
  `registry.hpp:150-172`).
