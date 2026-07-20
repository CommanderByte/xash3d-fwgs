# C++ Modernization Opportunities — `xash3dpp/src/filesystem`

**Standard**: C++23 (`xash3dpp/CMakeLists.txt` `CMAKE_CXX_STANDARD 23`;
`xash3dpp_filesystem` declares `target_compile_features … PUBLIC cxx_std_23`)\
**Exceptions**: disabled (`/EHs-c-` / `-fno-exceptions`)\
**RTTI**: disabled\
**ABI boundary**: None frozen today — filesystem is fully internal; only
`GetFSAPI` is exported. The stubbed `IFileSystem009` compat shim
(`src/filesystem/vfs009/`) will be ABI-frozen to the legacy `VFileSystem009`
vtable **if and when it is implemented** — see "Out of scope / ABI-frozen".\
See `xash3dpp/docs/boundaries/filesystem-boundary.md`.

> Refreshed 2026-07-06 (as-built pass) — re-scanned the current
> `xash3dpp_filesystem` sources. **Most of the original backlog is now
> implemented**: H-1, M-1, M-2, M-3, M-4, M-5 and M-6 all landed (see the
> per-item status lines). H-2 / L-4 referenced `src/filesystem/platform/{win32,
> posix}.cpp`, which **no longer live in this subsystem** — OS file I/O was
> extracted into `xash3dpp_platform`, so those items are **relocated** to the
> platform modernization backlog, not filesystem's. One **new** finding was
> added: **M-7** (the `strnicmp`/`string_view` over-read in `archive_helpers.hpp`
> — the same class as utilities M-4). Prior dated analysis is preserved below;
> resolved items are annotated in place rather than deleted.

> **Refreshed 2026-07-20** (tree-wide modernization audit, Phase 2/3) —
> cross-checked the audit's per-subsystem digest and cross-cutting lenses
> against current HEAD, one item at a time, rather than trusting either
> verbatim. Six digest findings verified still present and folded in as
> **H-3, H-5, M-8, M-9, L-6..L-10** (continuing this report's own H/M/L
> sequence — the digest's own `F`-numbers are cited per item for
> cross-reference, they are not this report's IDs). Two extension bumps
> applied per the campaign brief: **F39** (Medium→High, `[EXT:G-3]`) and
> **F37** (Low→Medium, `[EXT:G-2]`). One item (**F32**, the dead `#if 0`
> block already in this report as a Low candidate) is promoted to **H-4**
> because the tree-wide subtraction lens (L11) independently named it as
> item (c) of a zero-risk, zero-design-question deletion batch — the
> campaign brief instructs promoting anything a subtraction lens names as
> deletable to a High slot. One **new** finding was added from the stats/
> introspection lens (L3): **H-3**, `Filesystem::stats()` hands back a live
> `const&` into a plain (non-atomic) struct the owner rewrites in place. One
> lens claim (L2-R4, a supposed case-sensitive archive-extension parity bug
> at `filesystem.cpp:68`/`:336`) was **independently re-verified against
> current source and found already fixed** — both cited call sites already
> call `utilities::ci_equal` (landed in `b1a7b4de`, which predates this
> audit's own fact-base HEAD `cc73c054`) — recorded under "Investigated and
> refuted", not added as a finding. Prior 2026-07-06 analysis (H-1/H-2,
> M-1..M-7, L-1..L-5) is unchanged and preserved below.

______________________________________________________________________

## Summary

> **Superseded 2026-07-20:** the table below reflects the *cumulative*
> state across both refresh passes. High now carries three new,
> currently-actionable items (H-3, H-4, H-5) alongside the two
> already-closed 2026-07-06 items (H-1 done, H-2 relocated); Medium carries
> two new actionable items (M-8, M-9) alongside the seven already-closed
> 2026-07-06 items (M-1..M-6 done, M-7 resolved); Low gains five new items
> (L-6..L-10) alongside the five original (mostly no-action/optional).

| Tier | Total | Closed (done/relocated) | Actionable now |
|--------|-------|--------------------------|-----------------|
| High | 5 | 2 (H-1, H-2) | 3 (H-3, H-4, H-5) |
| Medium | 9 | 7 (M-1..M-7) | 2 (M-8, M-9) |
| Low | 10 | 0 | 5 new (L-6..L-10) + 5 original (mostly no-action) |

______________________________________________________________________

## High Priority

### H-1 — 64 KB stack allocation in `OsFile::Seek` backward path

> **✅ Implemented (verified 2026-07-06, re-verified 2026-07-20).** The
> 64 KB `sink[65536]` is gone. `OsFile::Seek` drains the discard loop into
> the existing `buf_` member (`k_buf_size` chunks, `file.cpp:79-80,239`).

**File**: `src/filesystem/file.cpp` ~L233\
**Category**: 2-C (raw array), safety hazard

No further action. See the 2026-07-06 entry above for the original finding.

______________________________________________________________________

### H-2 — Fixed-size `wchar_t` and `char` stack buffers in `platform/win32.cpp`

> **↪ Relocated 2026-07-06.** `src/filesystem/platform/` no longer exists —
> all OS file I/O moved to `xash3dpp_platform`. Out of filesystem scope; see
> the platform modernization backlog.

______________________________________________________________________

### H-3 — Dead `#if 0`'d legacy `GameInfo` struct definition in `gameinfo.hpp`

*(promoted from Low — tree-wide subtraction lens named it a zero-risk deletion; digest ID F32)*

**File(s)**: `include/xash3dpp/filesystem/gameinfo.hpp:18-63`\
**Category**: 2-J (deletion candidate)

**Current pattern**: The header already does the real work in one `using`
alias (`gameinfo.hpp:14`, `using GameInfo = ::xash::GameInfo;`) after
`GameInfo` moved to `xash3dpp/include/xash3dpp/gameinfo.hpp`. The entire
original 46-line struct definition is preserved immediately below it,
uncompiled, behind `#if 0` (verified still present at HEAD).

**Suggested replacement**: Delete lines 18-63 (the `#if 0` block and its
closing comment). The file's own comment already documents the move; the
disabled block adds nothing and cannot even bit-rot usefully since it never
compiles.

**Boundary-safe**: Yes

**Rationale**: Pure subtraction — zero call sites, zero design questions,
zero behavioural change either way. This is not a High item on its own
technical merits (it is genuinely cosmetic); it is promoted because it is
item (c) of the tree-wide "decision-free deletion batch" the L11
subtraction lens identified across 8 subsystems (`lenses.md`
L11-SUB-1), and this campaign's own brief instructs giving anything a
subtraction lens names as deletable a High slot — every other lens is
biased toward adding structure, so the one lens that only ever removes
lines should not get buried in a Low section.

______________________________________________________________________

### H-4 — `Filesystem::stats()` returns a live `const&` into a plain, non-atomic struct the owner rewrites in place (new)

*(cross-cutting stats/introspection lens L3, class D — "the concrete G-3 blocker"; no prior digest ID, `[EXT:G-3]`)*

**File(s)**: `include/xash3dpp/filesystem/filesystem.hpp:42-45` (struct
`FilesystemStats`), `:162` (accessor declaration); `src/filesystem/filesystem.cpp:633-635`
(accessor definition, unsynchronized read); write sites at `filesystem.cpp:172,
178, 201, 265, 306, 315, 324, 353` (all under `paths_mutex`, none atomic).

**Current pattern**:

```cpp
// filesystem.hpp
struct FilesystemStats {
    bool        game_loaded       = false;
    std::size_t search_path_count = 0;
};
...
[[nodiscard]] const FilesystemStats& stats() const noexcept;

// filesystem.cpp
const FilesystemStats& Filesystem::stats() const noexcept {
    return impl_->stats_;   // no lock, no atomics — a live alias
}
```

Eight sites write `impl_->stats_.*` under `paths_mutex`, but `stats()`
itself takes no lock and the fields are plain `bool`/`size_t`, not
`std::atomic`. A caller holding the returned reference across any of those
writes observes a live, potentially torn read of a two-field struct whose
fields are correlated (`game_loaded` and `search_path_count` change
together on `activate_game`/`clear_paths`).

**Suggested replacement**: Flip the accessor to return by value:
`[[nodiscard]] FilesystemStats stats() const noexcept { std::shared_lock
lock{impl_->paths_mutex}; return impl_->stats_; }` — one line at the
declaration, one at the definition. `MapLoader::stats()`
(`map_loader.cpp:271`) is the in-tree by-value precedent this should copy.
By-value does not make a cross-thread read torn-free by itself (be honest
about that), but it removes the worse property — handing the caller an
alias into memory the owner will rewrite next — and, taken under the
existing `paths_mutex` (which every write site already holds), it also
removes the torn-read risk for this specific pair of correlated fields
without introducing new synchronization.

**Boundary-safe**: Yes

**Rationale**: This is the concrete instance of a tree-wide pattern the
audit's stats/introspection lens (L3) singled out: of the tree's 14 stats
structs, exactly 6 combine plain (non-atomic) storage with a `const&`
accessor — "class D — worst … unsafe and it hands the caller an alias
into future writes" — and `FilesystemStats` is one of the four survivors
after the other two (`ClockStats`, `HostStats`) are recommended for
deletion elsewhere in the same lens. The lens's own accessor rule — "atomic
storage may return `const&`; plain storage MUST return by value" — is
recorded once and should be applied here rather than re-derived. Promoted
to High under this report's own tier criteria (removes a safety hazard),
independent of any extension bump; it is additionally the concrete blocker
the lens names for a future G-3 debug-thread stats reader, so it also
carries `[EXT:G-3]`. Do **not** additionally atomicize `game_loaded`/
`search_path_count` field-by-field as a substitute fix — they are a
correlated pair, not independent counters, and per-field atomics would
give race-freedom without a coherent snapshot (the same failure mode the
lens documents for the now-deleted `ClockStats`). Effort: S.

______________________________________________________________________

### H-5 — Main-thread pinning for `init()`/`shutdown()` is real design intent but currently unenforced (waiver, not a real assert)

*(UNVERIFIED in Phase 2, `[EXT:G-3]` — extension bump promotes Medium → High; digest ID F39)*

**File(s)**: `src/filesystem/filesystem.cpp:137-141` (`init()`), `:159-163`
(`shutdown()`); `xash3dpp/docs/boundaries/filesystem-boundary.md` Threading
section (`:368-`).

**Current pattern**:

```cpp
// compliance-allow(thread-assert): main-thread-only lifecycle by contract
// (README key invariants — "before worker threads start"); writes pool_/rootdir
// unguarded. A runtime assert_thread_role(Main) would XASH_FATAL the fs test
// harness (which registers no ThreadRole) — deferred with the shutdown() pair;
// see filesystem-boundary.md Threading.
bool Filesystem::init(std::string_view rootdir, ...) {
    impl_->rootdir = rootdir;   // no lock, no assert
    ...
}
```

`init()`/`shutdown()` write `impl_->pool_`/`rootdir`/`basedir`/`gamedir`/
`rodir` with zero synchronization and zero runtime thread check — the only
guard today is the `compliance-allow(thread-assert)` comment. The comment
itself explains the deferral is deliberate: turning it into a real
`assert_thread_role(Main)` today would `XASH_FATAL` the four
`tests/filesystem/*.cpp` harnesses, none of which register a `ThreadRole`.

**Suggested replacement**: Register `ThreadRole::Main` in the four
filesystem test harnesses (matching the pattern other subsystems' test
mains already use), then replace both `compliance-allow(thread-assert)`
comments with a real `assert_thread_role(ThreadRole::Main)` call at the top
of `init()`/`shutdown()`.

**Boundary-safe**: Yes

**Rationale**: This is UNVERIFIED — a Phase-2 finding that was not put
through adversarial refutation — but it is well-evidenced (the waiver
comment and the file:line evidence are unambiguous) and independently
corroborated by the audit's Phase-3 thread-model lens (L8), which
classifies `filesystem.cpp:137,160` as "Bucket C — design-pinned by a
named permanent constraint … G-3 explicitly depends on it". That
corroboration cuts in the finding's favour (the constraint is real and
worth enforcing) but also means the fix is enforcement of an already-agreed
design decision, not a design change — hence it does not need a new
Open Question, only the mechanical follow-through. `[EXT:G-3]` because a
future dedicated-debug-thread reader needs this lifecycle boundary to be a
real invariant, not a comment, before it can safely assume filesystem
state is stable outside `init`/`shutdown`. The extension bump promotes
this from the digest's original Medium to High. Effort: S; blast radius 6
(2 call sites + 4 test harnesses).

______________________________________________________________________

## Medium Priority

### M-1 — `::strnlen` POSIX extension in `pak_backend.cpp`

> **✅ Implemented (verified 2026-07-06, re-verified 2026-07-20).**

**File**: `src/filesystem/backends/pak_backend.cpp` L80\
**Category**: 2-C (non-standard C function), portability

No further action. See the 2026-07-06 entry above for the original finding.

______________________________________________________________________

### M-2 — `File::Seek` takes a raw `int` whence parameter

> **✅ Implemented (verified 2026-07-06, re-verified 2026-07-20).**
> `SeekOrigin` is defined at `file.hpp:22-26` and used by every `Seek`
> override.

______________________________________________________________________

### M-3 — `inflate_read(void* out, size_t n)` raw void pointer

> **✅ Implemented (verified 2026-07-06).**

______________________________________________________________________

### M-4 — `reinterpret_cast<const char*>(buf.data())` byte-to-string conversion in `filesystem.cpp`

> **✅ Implemented (verified 2026-07-06).**

______________________________________________________________________

### M-5 — Remaining manual 3-segment path joins in `FindLibrary`

> **✅ Implemented (verified 2026-07-06).**

______________________________________________________________________

### M-6 — WAD / PAK magic constants use verbose bit-shift form

> **✅ Implemented (verified 2026-07-06, re-verified 2026-07-20).**
> `std::bit_cast` confirmed at `wad_backend.cpp:38,41` and
> `pak_backend.cpp:34`.

______________________________________________________________________

### M-7 — `ci_find_by_name` over-reads past `string_view` bounds via `strnicmp`

> **RESOLVED 2026-07-19 (HB-1, consolidation audit).** Both `strnicmp` call
> sites in `ci_find_by_name` now use the bounded `utilities::ci_compare`.
> See `utilities-modernization.md` M-4 (resolved the same pass).

______________________________________________________________________

### M-8 — `File`/`OsFile`/`MemFile` hierarchy uses PascalCase methods, violating the mandatory snake_case member-function convention (new)

*(CONFIRMED — survived three refutation attempts in Phase 2; digest ID F31)*

**File(s)**: `include/xash3dpp/filesystem/file.hpp:45-56`;
`src/filesystem/file.cpp:139,184,201,253,257,259,261,267,302,304`;
`include/xash3dpp/private/filesystem/mem_file.hpp:27-80`

**Current pattern**: `File` (the internal streaming-I/O abstract base — not
an `I`-prefixed vtable interface) and both of its production
implementations (`OsFile`, `MemFile`) name every member function in
PascalCase: `Read`/`Write`/`Seek`/`Tell`/`Length`/`Eof`/`Flush`/`Gets`/
`Getc`/`UnGetc`. The rest of the subsystem (`Filesystem::open`/`load_file`/
`file_exists`/…, `ISearchBackend::open_file`/`find_file`/…) and the
tree-wide binding convention use snake_case throughout.

**Suggested replacement**: Rename `File`'s 10 virtual methods (and both
sets of overrides) to snake_case (`read`/`write`/`seek`/`tell`/`length`/
`eof`/`flush`/`gets`/`getc`/`ungetc`), matching `Filesystem` and
`ISearchBackend`. Pure rename — no signature, ABI, or behavioural change.

**Boundary-safe**: Yes

**Rationale**: `.github/instructions/xash3dpp.instructions.md` exempts only
`I<X>` vtable method names dictated by a legacy ABI (e.g.
`IFilesystem::Open` matching `VFileSystem009`). `class File` is not
`I`-prefixed and has no ABI counterpart to mirror — legacy exposes
`file_t` as an opaque C struct manipulated by free functions
(`FS_Read`/`FS_Seek`/`FS_Gets`), not a C++ vtable whose spelling must be
matched, and no naming waiver exists in `filesystem-boundary.md`. Blast
radius 93 call sites (Phase-2 measured). Effort: M — mechanical rename,
but touches every call site across the subsystem's tests and backends, so
it is sized Medium rather than Small.

______________________________________________________________________

### M-9 — `VFileSystem009` shim is exported, built by default, and has zero production implementation or callers

*(UNVERIFIED, `[EXT:G-2]` — extension bump promotes Low → Medium; digest ID F37)*

**File(s)**: `src/filesystem/vfs009/vfs009.cpp:29-35`;
`include/xash3dpp/private/filesystem/vfs009/vfs009.hpp:20`;
`src/filesystem/CMakeLists.txt:6-13`

**Current pattern**: `option(XASH_VFS009_SHIM ... ON)` (CMakeLists.txt:7)
compiles `vfs009.cpp` into every build and exports
`create_vfs009_interface(Filesystem&)` from a private header. The function
is a full stub — `Vfs009Adapter` is commented out and
`create_vfs009_interface` always returns `nullptr`:

```cpp
void* create_vfs009_interface(Filesystem& fs) {
    // TODO: return new Vfs009Adapter{fs};
    (void)fs;
    return nullptr;
}
```

A tree-wide grep finds zero callers of `create_vfs009_interface` anywhere
in `src/` or `host/`.

**Suggested replacement**: `filesystem-boundary.md:271-273` already records
the design decision to keep the shim ("exact placement deferred") — that
decision stands, and this is not a proposal to delete it. What should
change is honesty about status: flip `XASH_VFS009_SHIM`'s CMake default to
`OFF` (or gate it behind a comment stating it is inert) so the
always-`nullptr` exported factory is not silently shipped enabled in every
build until a chunk names a real `VFileSystem009` consumer.

**Boundary-safe**: Yes

**Rationale**: UNVERIFIED but well-evidenced (grep-confirmed still true at
HEAD). `[EXT:G-2]` — this is exactly the shim game ABI v2 will need, so its
current always-nullptr/default-ON state is a latent trap for whoever wires
G-2, not surplus abstraction to delete. The extension bump promotes it
from the digest's original Low to Medium. Pairs with L-6 (F33), which
records the shape the eventual `Vfs009Adapter` implementation must follow.
Effort: S (one CMake default flip).

______________________________________________________________________

## Low Priority

### L-1 — `#pragma pack` in `zip_backend.cpp`

**No action needed.** See original 2026-07-06 entry.

______________________________________________________________________

### L-2 — Unnamed padding bytes `pad0/pad1` in `DiskLump` (wad_backend.cpp)

**Optional only.** See original 2026-07-06 entry.

______________________________________________________________________

### L-3 — `any(SearchPathFlags)` free function could become `operator bool`

**No change needed** — `operator bool` is not valid for a scoped enum. See
original 2026-07-06 entry.

______________________________________________________________________

### L-4 — `SEEK_SET` / `SEEK_CUR` / `SEEK_END` macros at internal call sites

> **↪ Partially superseded 2026-07-06.** See original entry — the
> remaining `<cstdio>` include is cosmetic and now a **platform** decision.

______________________________________________________________________

### L-5 — Arithmetic `static_cast` noise in binary-format readers

**Acceptable as-is.** See original 2026-07-06 entry.

______________________________________________________________________

### L-6 — `Vfs009Adapter`'s own architecture doc prescribes raw `new`/`delete`, violating the subsystem's own P-7 pool-owned RAII idiom (new)

*(UNVERIFIED, shape constraint only — no consumer today; digest ID F33)*

**File(s)**: `src/filesystem/vfs009/vfs009.cpp:29-34`;
`xash3dpp/docs/architecture/filesystem/vfs009-shim.md:34-35`

**Current pattern**: Every other polymorphic, pool-owned type in this
subsystem (`File`, `ISearchBackend` and its 6 implementations) follows
`create_<thing>` + `pool_new<T>` + both `operator delete` overloads
(P-7/Q-22). The not-yet-implemented `Vfs009Adapter` is documented and
stubbed to use a raw heap `new` returned as `void*`, deleted by a raw
`delete` cast at the call site — the exact pattern P-7 exists to eliminate.

**Suggested replacement**: Not work to schedule now — `consumer_status` is
`speculative` (zero in-tree callers of `create_vfs009_interface`, see
M-9). Record as a shape constraint: when `Vfs009Adapter` is implemented it
must follow the same `create_<thing>` + `pool_new<T>` + dual
`operator delete` idiom as `File`/`ISearchBackend`, and
`vfs009-shim.md`'s "cast + delete" ownership note must be corrected before
anyone implements against it.

**Boundary-safe**: Yes (documentation-only until a consumer exists)

**Rationale**: Cheap to get right at the moment of construction, expensive
to unwind afterward if the raw-`new` shape ships first and gains callers.
Recording it now costs one doc correction; not building it now respects
the anti-gold-plating rule (no named day-one consumer).

______________________________________________________________________

### L-7 — `AndroidBackend` is the only one of six `ISearchBackend` implementations missing `[[nodiscard]]` on its interface overrides (new)

*(UNVERIFIED; digest ID F34)*

**File(s)**: `include/xash3dpp/private/filesystem/backends/android_backend.hpp:24-41`
(verified: none of its 7 overrides carry `[[nodiscard]]`)

**Current pattern**: `DirBackend`, `PakBackend`, `WadBackend`,
`ZipBackend`, and `Pk3DirBackend` all mark every `ISearchBackend`-
implementing declaration `[[nodiscard]]`, matching the base class's own
`[[nodiscard]] virtual` declarations. `AndroidBackend` (compiled only under
`XASH_ANDROID`) has zero `[[nodiscard]]` on any of its 7 declarations.

**Suggested replacement**: Add `[[nodiscard]]` to all 7 `AndroidBackend`
method declarations to match the other 5 backends and the base interface.

**Boundary-safe**: Yes

**Rationale**: Discarding the return value of e.g. `find_file`/`open_file`
silently drops a resource or a found-path result exactly as it would for
the other five backends — the risk this convention guards against is
identical here. Low priority because the platform (`XASH_ANDROID`) is not
built by any CI configuration this repo currently exercises. Effort: S;
blast radius 7.

______________________________________________________________________

### L-8 — Six near-identical hand-rolled reverse-iteration loops over `search_paths` in `Filesystem`'s read methods (new)

*(UNVERIFIED; also named in the tree-wide subtraction lens as a
second-wave consolidation candidate (`lenses.md` L11-SUB-8 item d);
digest ID F35)*

**File(s)**: `src/filesystem/filesystem.cpp:362-374` (`open`), `:379-388`
(`load_file`), `:437-445` (`file_exists`), `:447-458` (`file_size`),
`:460-470` (`file_time`), `:472-486` (`disk_path`) — `write_file`
(`:408-435`) is a close seventh with extra `NoWrite`-flag and
cache-invalidation logic layered on the same scaffold.

**Current pattern**: Each of the six methods independently takes
`std::shared_lock(paths_mutex)`, reverse-iterates
`impl_->search_paths`, applies the identical
`gamedironly && !any(it->flags & SearchPathFlags::GameDir)` skip, and calls
one per-backend method — six copies of the same 5-6 line scaffold differing
only in the one line that calls into the backend and the return handling.

**Suggested replacement**: Factor the lock+reverse-iterate+`gamedironly`-
filter scaffold into one private helper — e.g. a template
`find_in_backends(gamedironly, fn)` taking the `shared_lock` once inside
and returning the first non-empty/truthy result of `fn(backend)`. Each of
the six call sites becomes 1-2 lines.

**Boundary-safe**: Yes

**Rationale**: Net effect is fewer total lines and one place to fix if the
priority order or `gamedironly` semantics ever change. Low priority (not
promoted like H-3) because, unlike the pure-deletion L11-SUB-1 batch, this
is a consolidation requiring a small design decision about the helper's
shape (SUB-8, not SUB-1) — the subtraction lens's own High-slot rule is
reserved for the zero-decision deletion batch, not every item a subtraction
lens happens to mention. Effort: M; blast radius 6.

______________________________________________________________________

### L-9 — Tree-wide zero `std::ranges` usage is inertia, not a documented choice, at concrete filesystem call sites (new)

*(UNVERIFIED, shape constraint; digest ID F36)*

**File(s)**: `src/filesystem/filesystem.cpp:63` (`std::sort`), `:255-257`
(`std::remove_if`, duplicated at `clear_paths():316`);
`include/xash3dpp/private/filesystem/archive_helpers.hpp:75-76`
(`std::find`/`std::lower_bound`)

**Current pattern**: Classic iterator-pair standard algorithms throughout.
No `concept` or `std::ranges` call exists anywhere in the 15-subsystem
tree (confirmed zero tree-wide), and no decision doc records this as a
deliberate choice for these specific call sites.

**Suggested replacement**: Shape constraint, not urgent work — these
specific sites (`std::sort`→`std::ranges::sort`,
`std::remove_if`→`std::ranges::remove_if`, `std::find`→`std::ranges::find`)
are container-argument-eliding, low-risk, one-token swaps with identical
behaviour and no ABI/parity exposure. Legitimate low-priority modernization
candidates precisely because nothing here is fenced — not worth a
dedicated pass on its own, but a natural inclusion the next time any of
these three lines is touched for another reason.

**Boundary-safe**: Yes

______________________________________________________________________

### L-10 — CMake `PUBLIC` link of `xash3dpp_utilities`/`xash3dpp_memory` is not justified by any public header (new)

*(UNVERIFIED; independently hand-found by 6+ per-subsystem packs across the
tree with the identical shape — see rationale; digest ID F38)*

**File(s)**: `src/filesystem/CMakeLists.txt:43`;
`include/xash3dpp/filesystem/filesystem.hpp:16-28`, `file.hpp:12-17`,
`gameinfo.hpp:11-13` (verified: none include `xash3dpp/utilities/**` or
`xash3dpp/memory/**` — `PoolHandle` and every `utilities::` symbol are
confined to private headers).

**Current pattern**:

```cmake
target_link_libraries(xash3dpp_filesystem PUBLIC  xash3dpp_utilities xash3dpp_memory)
target_link_libraries(xash3dpp_filesystem PRIVATE xash3dpp_platform)
```

`PUBLIC` visibility means every one of the downstream targets that link
`xash3dpp_filesystem` (map_loader, server, networking, …) transitively
picks up `xash3dpp_utilities`/`xash3dpp_memory` on its own link line even
where it never uses filesystem's public headers' worth of those
dependencies — they should almost all already link both directly (every
subsystem uses memory pools), so the blast radius of a correct fix is
expected to be near-zero, but each of the 9 consuming targets needs
re-verifying for a direct (non-transitive) dependency before flipping to
`PRIVATE`.

**Suggested replacement**: Re-verify each of the 9 consuming
`CMakeLists.txt` files, then flip both to `PRIVATE`.

**Boundary-safe**: Yes

**Rationale**: This exact defect shape — a target declaring `PUBLIC` on a
dependency only its private surface uses — was independently hand-derived
by at least six other per-subsystem packs in this campaign (server,
cmd_cvar, host, input, imagelib, and a sound seam mention), always via the
same manual "grep the target's public headers against its CMakeLists"
check. The audit's cross-cutting synthesis recommends this become a
mechanical check in `xash3dpp/tools/dep_scan.py` (which already parses
both `link_edges` and `include_edges`) rather than being re-derived by
hand in every future audit; see Open Questions. Low priority for this
report specifically because filesystem's own instance is unremarkable
compared to the systemic pattern — the durable fix belongs in tooling, not
in six independent one-line CMake edits.

______________________________________________________________________

## Already Modern — No Action Needed

| Pattern | Location | Status |
|---------|----------|--------|
| `OsFd` RAII wrapper | `os_fd.hpp` | ✅ Move-only RAII, no leaks |
| `ZlibState` RAII | `file.cpp` | ✅ `mz_inflateEnd` in destructor |
| `std::array<std::byte, limits::filesystem_file_buffer_size> buf_{}` | `file.cpp` | ✅ Value-initialised fixed buffer |
| `std::shared_mutex` for path list | `filesystem.cpp` | ✅ Reader/writer lock |
| `ISearchBackend` virtual interface | `i_search_backend.hpp` | ✅ Replaces legacy fn-ptr vtable |
| `SearchPathFlags` scoped enum | `search_path_flags.hpp` | ✅ Typed bitmask, no raw `int` flags |
| `std::optional` return values | throughout | ✅ No out-parameters for file lookups |
| `std::unique_ptr<File>` | throughout | ✅ No raw owning pointers |
| `std::vector<std::byte>` for buffers | throughout | ✅ No raw `malloc`/`free` |
| `std::string_view` parameters | throughout | ✅ No raw `const char*` in new APIs |
| `k_archive_types` extension compare | `filesystem.cpp:69,342` | ✅ Both call sites already use `utilities::ci_equal` — see "Investigated and refuted" below |

**Cited tree-wide as a reference idiom (2026-07-20):** the audit's
published-primitive lens (L1) singles out `get_game_info()`'s
`shared_mutex` + deep-copy read (`filesystem.cpp:213`, guarded by
`filesystem.cpp:109,116`'s two `shared_mutex`es) as "the empirically
successful idiom in this tree" for string-bearing cross-thread reads,
alongside sound's mutex-guarded borrow design — and notes that HB-5 (the
still-open "one shared published-snapshot idiom" backlog item) does not
currently mention it. No action needed here; recorded so a future HB-5
brief author looks at this subsystem's existing pattern rather than
inventing a fourth one.

______________________________________________________________________

## Investigated and refuted

- **Case-sensitive archive-extension compare at `filesystem.cpp:68`/`:336`
  (a supposed live parity divergence against legacy's `Q_stricmp`).** The
  audit's registry-unification lens (L2-R4) cited this as an outstanding
  bug: `ext.substr(1) != at.extension` / `ext != at.extension`, silently
  skipping mixed-case archive names like `FOO.PAK` that legacy would mount.
  **Directly re-verified against current source and found already fixed**:
  both cited comparisons (now at `filesystem.cpp:69` in
  `collect_paths_for_dir` and `:342` in `mount_archive`) call
  `xash::utilities::ci_equal(...)`, with an in-code comment citing the same
  legacy `Q_stricmp` behaviour the lens was checking against. `git
  merge-base --is-ancestor` confirms the fix (`b1a7b4de`, "filesystem:
  adopt bounded ci_compare (HB-1/M-7)") is an ancestor of `cc73c054`, the
  HEAD this campaign's own fact base was built against — so the lens's
  source read was stale, not the fact base's HEAD. Recorded here so it is
  not re-found and re-proposed as work.

______________________________________________________________________

## Out of scope / ABI-frozen

- **`GetFSAPI`** is the one exported entry point today; its shape is
  determined by the launcher/host composition root, not by this
  subsystem's internals, and is unaffected by every finding above.
- **`IFileSystem009` (the `vfs009` compat shim), if and when
  `Vfs009Adapter` is implemented**, must match the legacy `VFileSystem009`
  vtable method names and signatures exactly — this is the one place the
  binding convention's `I<X>` ABI-naming exception applies in this
  subsystem. See M-9 and L-6 for its current (inert) state and the shape
  constraint on its eventual implementation. No `File`/`ISearchBackend`
  finding above (including M-8's PascalCase rename) touches this shim —
  `File` is not `I`-prefixed and has no legacy vtable to mirror.
- **This subsystem is not one of the five HB-2 float/byte-exact-kernel
  subsystems** (map_loader, content, networking, server, utilities) — no
  finding in this report is fence-adjacent, and none was excluded on that
  basis.

______________________________________________________________________

## Open questions

1. **H-5 follow-through vs. permanent-waiver classification.** The audit's
   Phase-3 thread-model lens (L8) independently classifies
   `filesystem.cpp:137,160` as "Bucket C — design-pinned by a named
   permanent constraint", i.e. correctly waived today, not open debt — yet
   also cites this subsystem's own F39 finding as the reason G-3 depends
   on it. Both readings agree the constraint is real; they differ on
   whether the *comment* should stay a waiver or become a live assert
   before G-3 needs it. Recommend doing H-5's mechanical fix (register
   `ThreadRole::Main` in the 4 test harnesses, promote the waiver to a real
   assert) now, since it is Small effort and removes the ambiguity for
   whoever reads the two classifications side by side later — but this is
   a judgement call, not a blocking decision.
2. **M-9's CMake default flip.** Is `XASH_VFS009_SHIM` flipped to `OFF` by
   default now (cheap, no consumer loses anything since there is no
   consumer), or left `ON` until a chunk names a `VFileSystem009` caller
   and the flip happens alongside the real implementation? No design work
   is gated on this either way; it is a one-line default with no
   behavioural stakes today.
3. **Cross-subsystem extension-matching contract.** filesystem's own
   `k_archive_types` table already matches legacy's case-insensitive
   `Q_stricmp` contract (see "Investigated and refuted"), but the audit's
   L2 lens notes `tests/sound/test_sound_codec.cpp:142` pins the *opposite*
   behaviour for sound's codec table, and recommends ratifying
   case-insensitive dispatch as the tree-wide contract in
   `decisions-architecture.md` so the two subsystems stop disagreeing.
   That decision is owned jointly by sound and filesystem, not by this
   report alone — flagging it here since filesystem's own code is already
   on the side the lens recommends.
4. **L-10's durable fix.** Should the `PUBLIC`-vs-`PRIVATE` CMake
   visibility check land as a mechanical `dep_scan.py` rule (recommended by
   the audit's cross-cutting synthesis, given 6+ subsystems independently
   hit the identical shape) before or instead of hand-fixing filesystem's
   own instance? Fixing filesystem alone does not prevent the pattern
   recurring in a 7th subsystem.

______________________________________________________________________

## Application Order

> **Superseded 2026-07-20.** Adds H-3, H-4, H-5, M-8, M-9, L-6..L-10 to the
> already-closed 2026-07-06 backlog below. Suggested order for the newly
> actionable items, cheapest/lowest-risk first:

1. **H-3** — delete the `#if 0`'d `GameInfo` block (pure deletion, 0 risk)
1. **H-4** — flip `Filesystem::stats()` to by-value under `paths_mutex`
1. **M-9** — flip `XASH_VFS009_SHIM` CMake default to `OFF`
1. **H-5** — register `ThreadRole::Main` in the 4 test harnesses; promote
   the two waivers to real `assert_thread_role(Main)` calls
1. **L-7** — add `[[nodiscard]]` to `AndroidBackend`'s 7 overrides
1. **M-8** — snake_case rename of `File`'s 10 virtual methods (blast 93 —
   land on its own commit, not bundled with anything else)
1. **L-8** — factor the six reverse-iteration scaffolds into one helper
1. **L-6** — correct `vfs009-shim.md`'s ownership note (doc-only)
1. **L-9**, **L-10** — opportunistic, land alongside unrelated touches to
   the same lines

When implementing the original 2026-07-06 backlog (all now closed), the
order applied was:

1. **H-1** — replace `sink[65536]` in `OsFile::Seek`
1. **H-2** — dynamic `to_wide` in win32.cpp
1. **M-1** — `strnlen` → range algorithm
1. **M-3** — `inflate_read` void\* → span
1. **M-2** — `SeekOrigin` enum in `file.hpp`; update L-4 alongside
1. **M-5** — variadic `path_join`; remove remaining manual joins in `FindLibrary`
1. **M-4** — `bytes_as_string` helper
1. **M-6** — bit_cast magic constants
1. **L-2** — unnamed padding (cosmetic, batch with other wad edits)
