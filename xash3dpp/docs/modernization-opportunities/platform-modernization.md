# Platform Modernization Opportunities

> Authored 2026-07-06 (as-built pass).
> **Refreshed 2026-07-20** (tree-wide modernization audit, Phase 2/14): the
> `open_library` truncation framing in **H-1** was corrected (it fails
> closed with no diagnostic, it does not silently truncate the path — the
> consolidation target and rationale survive, the wording did not); a new
> **H-2** was added and promoted to High for the confirmed
> `assert_main_thread()` doc/code contradiction (three docs claim a
> "thin wrapper" that does not exist, one of the two live call sites
> silently no-ops during the exact startup window it exists to guard); the
> Q-21 "missing thread-spawn primitive" cross-cutting flag is **retired** —
> `spawn_thread`/`JoinHandle` (Q-24) shipped and is consumed in production by
> `src/sound/topology.cpp`, so the implementation-status table below is
> corrected; **M-2** is downgraded in place (folded, not re-derived, with a
> corrected 8-site blast radius and a doc-cleanup addendum) rather than
> re-filed as a new Medium; three new findings were added from the
> tree-wide corpus (**M-4**, **M-5**, **L-4**, **L-5**) that a single-
> subsystem pass would not have surfaced on its own (a missing-assert gap
> shared with the audit's thread-role lens, a boundary-doc classification
> gap, an interface `[[nodiscard]]` gap, and a small duplicated-helper
> cluster). Nothing in this subsystem was refuted this pass.
> C++ standard in use: C++**23** (from `xash3dpp/src/platform/CMakeLists.txt`,
> `target_compile_features(xash3dpp_platform PUBLIC cxx_std_23)` — required for
> `std::expected<T, NetError>` in the sockets layer; the tree-wide
> `CMAKE_CXX_STANDARD 23`).
> Boundary spec: `docs/boundaries/platform-boundary.md`
> Threading: inline in the boundary spec (`## Threading`) — platform keeps its
> thread-role analysis in the boundary doc, not a separate file.
> ABI-frozen symbols in this subsystem: **None** — platform is fully internal
> (boundary spec §External ABI). No Game/Client DLL header exposes
> `xash::platform` symbols; the engine host will eventually fill legacy
> `enginefuncs_t` slots (e.g. `pfnLoadLibrary`) from here via a thin shim.

## Summary

The platform subsystem is small-per-OS but wide (14 TUs across
`win32` / `posix` / `android`) and already modern: `std::string_view` inputs,
`std::optional` returns, `enum class OpenMode` bit-flags, RAII handle types
(`OsFd`, `OsSocket`, `LibHandle`), `std::expected<T, NetError>` on the socket
surface, magic-static clock init, `std::atomic` for the WSA refcount and
crash-installed flag, `call_once`-guarded Android JNI glue, and (since the
Q-24 thread-spawn primitive landed) a `JoinHandle` RAII join wrapper around
`std::thread`. `compliance_scan.py platform` is **clean** and
`status_table.py` reports it **Complete**.

Three facts drive this report:

1. **The OS file I/O backend lives here.** During the filesystem refresh,
   `src/filesystem/platform/{win32,posix}.cpp` were extracted into
   `xash3dpp_platform` (`os_io.hpp` + per-OS `os_io.cpp`). The filesystem
   **H-2** (fixed-size `wchar_t`/`char` path buffers) and **L-4**
   (`SEEK_SET`/`SEEK_CUR`/`SEEK_END` at the OS-call boundary) findings
   relocated to this backlog. **H-2 is largely already done in the
   extracted code** — `os_io.cpp`'s `to_wide` uses the robust two-call
   `MultiByteToWideChar` pattern returning `std::optional<std::wstring>`,
   and `list_directory` sizes a `std::string` dynamically — so the
   relocated finding survives only as an **inconsistency** (below, **H-1**)
   and a typed-origin question (**M-1**).

2. **The `strnicmp`/`strncmp` over-read pattern is absent.** The utilities
   **M-4** / filesystem **M-7** latent `string_view` → C-string over-read does
   **not** occur in platform: a scan of all 14 TUs finds **zero** `strnicmp` /
   `strncmp` call sites. Platform's `string_view` inputs are either copied into
   a bounded buffer with an explicit terminator (`open_library`,
   `message_box`, `shell_execute`) or handed to a length-taking OS API
   (`MultiByteToWideChar(..., s.size(), ...)`). Reported clean.

3. **The `assert_main_thread()` mechanism is not the wrapper three docs say
   it is.** This was surfaced by the 2026-07-20 audit's cross-cutting
   thread-model lens, not by a platform-only re-read: `thread_role.hpp`,
   `threading-model.md:11`, and `:81-83` all describe
   `xash::core::detail::assert_main_thread()` as "a thin wrapper" over
   `assert_thread_role(ThreadRole::Main)`. It is an independent
   implementation with its own lazily-captured `std::thread::id` that
   silently no-ops until first capture — and one of its two live call
   sites (`crash::install_handler`) is invoked during exactly the startup
   window in which that capture may not yet have happened. See **H-2**.

What remains is a short tail: one latent diagnostic-quality inconsistency in
the dynamic-library loader (High, corrected framing below), the
`assert_main_thread` unification (High, new), a typed-origin promotion at
the `platform::seek` OS boundary (Medium, the relocated L-4), a missing
thread-role assert and a boundary-doc classification gap (Medium, both new),
a couple of enum / interface-annotation tidy-ups, and three
feature-completeness gaps that are already tracked as boundary-spec open
questions, `stub_scan` TODOs, or a chunk-numbered obligation (**OBL-12-7**,
`ThreadPriority::Realtime`).

______________________________________________________________________

## Implementation-status table

| Design element | Status | Notes |
|----------------|--------|-------|
| OS file I/O backend (`os_io.hpp` + per-OS `os_io.cpp`) | **Implemented** | Absorbed from filesystem; carries the relocated H-2 / L-4 |
| `to_wide` two-call `MultiByteToWideChar` (`os_io.cpp`) | **Implemented** | The filesystem-H-2 fix **landed here** — returns `std::optional<std::wstring>`, no truncation |
| Socket layer + `IPlatformSockets` seam | **Implemented** | Q-7 test seam; `std::expected` result type |
| OS thread-spawn + `ThreadRole` registration primitive (`platform::spawn_thread`/`JoinHandle`) | **Implemented** (corrected 2026-07-20) | Q-24; `include/xash3dpp/platform/thread.hpp`, `{win32,posix}/thread.cpp`; production consumer is `src/sound/topology.cpp` (the tree's only 2 production thread spawns). Was previously listed here as "Not implemented" — that was stale by several chunks |
| Diagnostics tier hosted in target (`core/log.cpp`, `core/thread_role.cpp`) | **Implemented** | D-1 dependency hardening; breaks the core ⇄ platform cycle |
| Consolidated UTF-8 → UTF-16 conversion (win32) | **Not implemented** | Three separate converters; `open_library`'s is the odd one out — **H-1** (framing corrected 2026-07-20; it fails closed, not truncates) |
| `assert_main_thread()` as a genuine wrapper over `assert_thread_role(Main)` | **Not implemented — and the docs claiming it is are wrong** | **H-2** (new 2026-07-20) |
| Typed origin at `platform::seek` (vs raw `int whence`) | **Not implemented** | Relocated filesystem **L-4** — **M-1** |
| `resolve_blocking()` Worker/NetIO-only contract enforcement | **Not implemented** | Documented-only; zero runtime assert, zero callers anywhere — **M-4** (new 2026-07-20) |
| `OpenMode::create` casing / boundary-spec `Create` mismatch | **Not implemented** | Naming drift — tracked as **M-2**, tier reassessed to Low 2026-07-20 (see M-2) |
| `is_debugger_present` on macOS / BSD | **Not implemented** | `stub_scan` TODO (`posix/sys.cpp`) — **L-1** |
| `shell_execute` POSIX double-fork (zombie reaping) | **Not implemented** | `stub_scan` TODO (`posix/sys.cpp`) — **L-2** |
| High-resolution sleep (`Win32_NanoSleep` / `SDLash_NanoSleep`) | **Not implemented** | Boundary-spec open question — **L-3** |
| `ThreadPriority::Realtime` (real scheduling, not just the enum value) | **Stubbed by design** | `XASH3DPP-STUB(chunk12)` — logs a Warning, runs at Normal. Tracked as **OBL-12-7** (audit obligations register), awaiting the SDL audio device's T_AudioCallback. Not a modernization finding — see Out-of-scope |
| Clipboard / SIGTERM / Android extras | **Not implemented** | Boundary-spec open questions; deferred to host / Android target |
| `strnicmp` / `strncmp` `string_view` over-read | **N/A — absent** | 0 sites; utilities M-4 / filesystem M-7 pattern does not occur here |

______________________________________________________________________

## High-priority opportunities

### H-1: Consolidate the win32 UTF-8 → UTF-16 converters and give `open_library` a diagnosable long-path failure (relocated filesystem H-2)

- **File(s)**: `xash3dpp/src/platform/win32/sys.cpp` (`open_library`,
  `get_executable_dir`, `get_working_directory`);
  `xash3dpp/src/platform/win32/os_io.cpp` (`to_wide`);
  `xash3dpp/src/platform/win32/os_socket.cpp` (`to_wide`).

- **Current pattern**: three independent UTF-8 → UTF-16 conversions coexist in
  the win32 platform TUs, and they do **not** agree on shape.
  `os_io.cpp` / `os_socket.cpp` use the robust two-call pattern (query length,
  then fill a right-sized `std::wstring`):

  ```cpp
  static std::optional<std::wstring> to_wide( std::string_view s ) noexcept; // os_io.cpp:45-55, os_socket.cpp:115-127 — byte-identical bodies
  ```

  but `open_library` (`sys.cpp:73-86`) still converts into a **fixed**
  1024-wchar stack buffer:

  ```cpp
  wchar_t wbuf[::xash::limits::platform_path_buf_wchars];   // 1024 wchars
  int len = MultiByteToWideChar( CP_UTF8, 0, path.data(),
      static_cast<int>( path.size() ), wbuf,
      static_cast<int>( std::size( wbuf ) ) - 1 );
  if( len <= 0 ) return {};                                 // long path → {}
  ```

  **2026-07-20 correction — the original framing was wrong.** `sys.cpp:77-79`
  passes an explicit `cchWideChar` of `size(wbuf) - 1`. When the UTF-16 form
  does not fit, `MultiByteToWideChar` does **not** truncate and use a partial
  path — it writes nothing, sets `ERROR_INSUFFICIENT_BUFFER`, and returns 0,
  which line 80's `if (len <= 0) return {};` already catches. There is no
  truncated-path-gets-loaded bug here; the bug (such as it is) is purely
  diagnostic: a >~1023-wchar DLL path fails closed with an empty `{}` and no
  distinguishable reason ("bad path" and "path too long" look identical to
  the caller). The duplication is still real and still worth deleting: three
  independent `to_wide`-shaped implementations for one conversion, one of
  them (the fixed-buffer form) with a narrower failure mode than the other
  two for no reason tied to `open_library`'s own contract.

- **Suggested replacement**: promote the two-call `to_wide` (already
  duplicated verbatim at `os_io.cpp:45-55` and `os_socket.cpp:115-127`) to
  one shared internal win32-only helper — there is no private
  `src/platform/win32/` header today, so this would be the first, staying
  out of `include/` since it is TU-local to the win32 backend — and route
  `open_library` (`sys.cpp:77`), plus the two `get_*_directory` paths
  (which already size their output dynamically), through it. Net effect is
  a deletion: two duplicate `to_wide` bodies and the last fixed-size
  conversion buffer disappear, and `open_library`'s failure mode for a
  too-long path becomes the same `std::nullopt`-with-cause shape the other
  two conversions already use instead of a bare `{}`.

- **Boundary-safe**: Yes — engine-internal; the observable contract
  (`{}` on failure) is unchanged in shape; only the internal conversion path
  and the failure-cause visibility improve.

- **Rationale**: de-duplication of three copies of the same conversion (the
  consolidation target is sound and unchanged by the correction) plus a
  small diagnostics improvement on the one path that must never quietly
  fail for a valid mod. Consumer is `exists-in-tree`: three call sites today
  (`os_io.cpp:74`, `os_socket.cpp:179`, `sys.cpp:77`) all doing the identical
  job.

### H-2: Unify `assert_main_thread()` into `assert_thread_role(Main)` — three docs describe a rewrite that never happened, and the divergence is live-dangerous at its one production call site *(new 2026-07-20)*

- **File(s)**: `xash3dpp/include/xash3dpp/private/core/assert_main.hpp:31-55`
  (the independent implementation); `xash3dpp/include/xash3dpp/core/thread_role.hpp:21-23`
  (the false "thin wrapper" claim, with a wrong file path); call sites at
  `xash3dpp/src/platform/win32/console.cpp:44`,
  `xash3dpp/src/platform/win32/crash.cpp:56`,
  `xash3dpp/src/platform/posix/console.cpp:45`,
  `xash3dpp/src/platform/posix/crash.cpp:74`; capture sites at
  `{win32,posix}/sys.cpp:53` (inside `get_time()`'s magic static);
  `xash3dpp/docs/design/threading-model.md:11,81-83` (doc claims).

- **Current pattern**: `thread_role.hpp:21-23` states — of
  `assert_main.hpp`'s helper — "The legacy `assert_main_thread()` helper in
  `<xash3dpp/private/platform/assert_main.hpp>` is a thin wrapper kept for
  existing platform code" (the path is also wrong: the file lives at
  `private/core/assert_main.hpp`, not `private/platform/`).
  `threading-model.md:11` and `:81-83` make the same claim ("becomes a thin
  wrapper over `assert_thread_role(ThreadRole::Main)`" / "is **kept** and
  rewritten as a thin wrapper"). None of that happened. The two mechanisms
  share nothing:

  ```cpp
  // assert_main.hpp — completely independent, function-local static, lazy capture
  inline std::thread::id &main_thread_id_ref() noexcept
  { static std::thread::id id; return id; }
  inline void assert_main_thread( const char *location ) noexcept
  {
      const std::thread::id &id = main_thread_id_ref();
      XASH_FATAL( id == std::thread::id{} || id == std::this_thread::get_id(), ... );
      //          ^^^^^^^^^^^^^^^^^^^^^^^^^^^ explicit no-op if never captured
  }
  ```

  `main_thread_id_ref()` is populated lazily by `capture_main_thread()`,
  called from inside the `get_time()` magic static — i.e. whenever anything
  first asks for the time — while `thread_role`'s `thread_local ThreadRole`
  is set explicitly by `register_thread_role()`, called as the first
  statement of `main()`. There are exactly **4** call sites, all in
  platform: `console::read_line` (win32:44, posix:45) and
  `crash::install_handler` (win32:56, posix:74).

- **Suggested replacement**: replace all 4 call sites with
  `::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main )`;
  delete `capture_main_thread()`'s call from both `{win32,posix}/sys.cpp`
  magic statics; delete `include/xash3dpp/private/core/assert_main.hpp`
  entirely; correct `thread_role.hpp:21-23` (drop the "thin wrapper" claim
  and the wrong path) and `threading-model.md:11,81-83`. Add
  `xash::core::register_thread_role(xash::core::ThreadRole::Main)` as the
  first line of `main()` in `tests/platform/test_crash.cpp` and
  `tests/platform/test_console.cpp` — without it the swap aborts both
  tests, since the production launcher already registers Main at
  `launcher/main.cpp:64` but the test binaries do not.

- **Boundary-safe**: Yes — engine-internal, no ABI surface. The
  behavioural delta is deliberate and documented below.

- **Rationale**: this is a **deletion** (whole header, one static, one
  lazily-capturing mechanism) that also fixes a live-dangerous asymmetry,
  not a cosmetic rename. `console::read_line` has zero production callers
  (`host.cpp:244` carries an unwired OQ-9 TODO), so half the call sites are
  dead code either way. The other half is not: `crash::install_handler` is
  called during process startup — exactly the window in which
  `capture_main_thread()` may not have fired yet (it only fires on first
  `get_time()` call) — so the assert guarding the process-wide POSIX
  `sigaction` install can silently no-op precisely when it matters most.
  `assert_thread_role(Main)` has no such window: `register_thread_role`
  runs as the literal first statement of `main()`, strictly before
  `install_handler` can run. Three separate docs (`thread_role.hpp`,
  `threading-model.md` twice) independently assert a migration that never
  happened, which is a standing doc-drift liability distinct from and
  additional to `CORRECTIONS.md`'s already-recorded observation of the
  same contradiction — this entry gives it the fix, not just the note.
  `ext_tags`: **G-3** (a future debug thread is exactly the kind of caller
  this assert exists to stop).

______________________________________________________________________

## Medium-priority opportunities

### M-1: Typed seek origin at the `platform::seek` OS boundary (relocated filesystem L-4)

- **File(s)**: `xash3dpp/include/xash3dpp/platform/os_io.hpp`
  (`seek( OsFd&, std::int64_t, int whence )`); per-OS `os_io.cpp` (forwards to
  `_lseeki64` / `lseek`); filesystem callers in `src/filesystem/file.cpp` /
  backends.

- **Current pattern**: the platform seek takes a raw POSIX `int whence`, so
  every caller still writes `platform::seek( fd, off, SEEK_SET )` and drags in
  `#include <cstdio>` purely for the macros:

  ```cpp
  [[nodiscard]] std::int64_t seek( OsFd &fd, std::int64_t offset, int whence ) noexcept;
  ```

  The filesystem layer already has a typed `enum class SeekOrigin { Begin,
  Current, End }` (its M-2, done) at the `File` API, but it degrades back to the
  raw `int` at this OS-call boundary. Blast radius: **17** call sites tree-wide
  (unverified against the Phase-2 pack scope, but consistent with the
  filesystem/platform grep and not contested by any refutation).

- **Suggested replacement**: give the platform seek its own scoped origin enum
  (e.g. `enum class SeekWhence : int { Begin = SEEK_SET, Current = SEEK_CUR,
  End = SEEK_END };` in `os_io.hpp`) and take it by value. Callers pass
  `SeekWhence::Begin`; the `<cstdio>` include at the call sites disappears.
  Ownership of this decision moved to **platform** when the OS I/O extracted
  out of filesystem (the filesystem L-4 note explicitly hands it over).

- **Boundary-safe**: Yes — additive within `xash3dpp`; the enum values pin to
  the same OS constants, so no behavioural change.

- **Rationale**: closes the relocated L-4; makes the OS boundary self-describing
  and stops leaking libc macros into higher layers.

### M-2: `OpenMode::create` casing — align with the enum and the boundary spec *(tier reassessed 2026-07-20 — see note)*

- **File(s)**: `xash3dpp/include/xash3dpp/platform/os_io.hpp` (`enum class
  OpenMode`); every `any( mode & M::create )` use in the per-OS `os_io.cpp`;
  `xash3dpp/private/filesystem/os_file_factory.hpp:42,46`;
  `xash3dpp/src/filesystem/filesystem.cpp:411`;
  `xash3dpp/tests/platform/test_os_io.cpp:42,126`;
  `xash3dpp/docs/architecture/platform/os-io.md:200-201`.

- **Current pattern**: the flag is spelled lowercase `create` while its siblings
  are PascalCase (`ReadOnly`, `WriteOnly`, `ReadWrite`, `Append`, `Truncate`,
  `Memory`), and the boundary-spec Interface table documents it as `Create`:

  ```cpp
  enum class OpenMode : std::uint32_t {
      ReadOnly = 0, WriteOnly = 1, ReadWrite = 2, Append = 4,
      create = 8,           // <-- lowercase outlier
      Truncate = 16, Memory = 32,
  };
  ```

  `docs/architecture/platform/os-io.md:200-201` already records this
  explicitly as a known inconsistency ("Future values will follow
  PascalCase").

- **Suggested replacement**: rename `create` → `Create` (the paired
  `make_directory` comment "create the directory" is unrelated prose — leave
  it). Mechanical rename across **8** edit sites: the declaration
  (`os_io.hpp:48`) plus the 7 uses listed above. Delete
  `os-io.md:200-201`'s now-stale inconsistency note in the same commit —
  once the casing is fixed the sentence describing the inconsistency
  becomes the only remaining inconsistency.

- **Boundary-safe**: Yes — enumerator is `xash3dpp`-internal; no ABI, no wire
  value change (the numeric value 8 stays).

- **Tier reassessment (2026-07-20)**: the 2026-07-20 audit re-derived this
  finding independently against the tree, confirmed the fact and the
  8-site blast radius exactly, and adjudicated the tier down to **Low** —
  this is a pure mechanical casing fix with a stale-doc-sentence cleanup
  riding along, not a Medium-weight change. Left titled **M-2** here
  (existing findings are not renumbered) with its priority now tracking
  Low; treat it as the lowest item in the backlog rather than moving it
  into the Low section and breaking the ID.

- **Rationale**: removes the one casing outlier so the enum reads consistently
  and matches its own boundary-spec documentation; the doc cleanup prevents
  the inconsistency note from itself going stale once the code no longer
  needs it.

### M-3: POSIX `open_library` path copy → bounded helper (parallel to H-1)

- **File(s)**: `xash3dpp/src/platform/posix/sys.cpp:83-89` (`open_library`).

- **Current pattern**: the POSIX loader copies the `string_view` into a
  fixed `char buf[PATH_MAX]` with a manual clamp + terminator before `dlopen`:

  ```cpp
  char buf[PATH_MAX];
  std::size_t n = path.size() < sizeof( buf ) - 1 ? path.size() : sizeof( buf ) - 1;
  std::memcpy( buf, path.data(), n );
  buf[n] = '\0';
  void *h = dlopen( buf, RTLD_NOW | RTLD_LOCAL );
  ```

  Same silent-truncation class as H-1's *original* framing (unlike win32's
  `MultiByteToWideChar`, `memcpy` genuinely does clamp-and-continue with no
  failure signal at all), though `PATH_MAX` (4096 on Linux) makes it far
  less likely to matter in practice than the win32 case ever was. Blast
  radius: 1 call site.

- **Suggested replacement**: since `dlopen` needs a C string, the cleanest fix
  is a tiny `std::string tmp{ path }` (heap, cold path — dynlib load is not hot)
  passed as `tmp.c_str()`, which is always null-terminated and never truncates.
  Alternatively a shared `copy_bounded(std::span<char>, std::string_view)`
  helper if the stack copy is preferred (see **L-5**, which would host it).

- **Boundary-safe**: Yes.

- **Rationale**: unlike H-1 (which turned out to fail closed, not truncate),
  this POSIX path genuinely clamps and loads whatever fits — the correctness
  argument here is the one H-1's title originally claimed for win32. Worth
  pairing with H-1 so both loaders lose their respective footguns together.

### M-4: `resolve_blocking()`'s Worker/NetIO-only contract has zero runtime enforcement *(new 2026-07-20)*

- **File(s)**: `xash3dpp/include/xash3dpp/platform/os_socket.hpp:229-233`;
  `xash3dpp/src/platform/posix/os_socket.cpp:363-378`;
  `xash3dpp/src/platform/win32/os_socket.cpp:396-411`.

- **Current pattern**: `resolve_blocking()` is documented — both in the
  header comment ("MUST NOT be called from `ThreadRole::Main`") and in the
  boundary spec's Threading section — as forbidden on Main, but neither
  implementation performs an `assert_thread_role()` / `XASH_ASSERT` check.
  This is the one function in `os_socket.hpp` that stands out: the other
  14 declarations in the same header are annotated `@thread-safety:
  T_NetIO-ready` (no constraint yet, since NetIO does not exist as a
  separate thread today); `resolve_blocking` alone claims `@thread-safety:
  Worker / NetIO ONLY` and is the one with nothing checking it. It is also
  currently unreachable in production: grepping `xash3dpp/src` and
  `xash3dpp/tests` for `resolve_blocking` turns up only its own two
  definitions — no caller anywhere, production or test.

- **Suggested replacement**: when the first consumer (a `DnsResolver` or
  equivalent) is wired up, add
  `XASH_ASSERT( ::xash::core::current_thread_role() != ::xash::core::ThreadRole::Main, "resolve_blocking: must not run on Main" )`
  as the first line of `resolve_blocking()` on both backends, matching the
  assert-on-entry idiom already used at the tree's other
  `assert_thread_role`-family call sites, so a future accidental Main-thread
  call (which would stall a frame on synchronous `getaddrinfo`) fails
  loudly at the call instead of silently degrading frame pacing.

- **Boundary-safe**: Yes — additive assert, no signature change.

- **Rationale**: this is a shape constraint, not work to do today —
  `consumer_status` is `speculative` (no DNS/HTTP consumer exists in the
  tree yet, matching `CORRECTIONS.md`'s note that neither of the boundary
  spec's own two triggers for this has fired). Recorded here so the assert
  lands with the first caller instead of being forgotten because the
  function currently compiles clean with nothing watching it.

### M-5: `console::read_line`'s Main-pinning is an implementation artifact, not the same kind of constraint as `crash::install_handler`'s — the boundary doc groups them as one class *(new 2026-07-20)*

- **File(s)**: `xash3dpp/src/platform/posix/console.cpp:43-48`;
  `xash3dpp/src/platform/win32/console.cpp:42-47`;
  `xash3dpp/src/platform/posix/crash.cpp:72-88`;
  `xash3dpp/src/host/host.cpp:242-244`; `docs/boundaries/platform-boundary.md`
  (the "Main-thread-asserting" class row).

- **Current pattern**: both `console::read_line()` and
  `crash::install_handler()` open with the same `assert_main_thread()` call
  (see **H-2**) and are grouped together under the boundary doc's single
  "Main-thread-asserting" class row, which implies they share one kind of
  constraint. They do not: `crash::install_handler`'s pin is a real OS
  invariant (POSIX signal disposition is process-wide and must be set
  before other threads exist that could race the installation);
  `console::read_line`'s pin is an implementation artifact of its
  `static char[]` accumulator buffer (documented in
  `docs/architecture/platform/index.md:95` as "overwritten each call") —
  nothing about reading a console line is inherently Main-only. The
  function is also currently dead code: `host.cpp:244` carries an unwired
  OQ-9 TODO, so `read_line`'s constraint has never actually been exercised
  by a caller.

- **Suggested replacement**: once **H-2** unifies both onto
  `assert_thread_role(Main)`, split the boundary doc's single
  "Main-thread-asserting" row into two: a by-design row
  (`crash::install_handler`, permanent OS-invariant constraint) and an
  incidental row (`console::read_line`, implementation-artifact
  constraint). When OQ-9 wires `read_line()` into `host::run_frame`, either
  keep it Main-only deliberately (simplest — the dedicated-server console
  has no reason to move threads) or, if a future debug/console-on-a-thread
  design needs it off-Main, replace the static accumulator with a
  thread-confined buffer first and drop the assert rather than widening
  it.

- **Boundary-safe**: Yes — doc-only until OQ-9 is scheduled; no code change
  required today.

- **Rationale**: a boundary doc that reads "these two functions have the
  same constraint" tells a future reader the wrong thing about how portable
  `read_line()`'s Main-pin is. Cheap to fix (one table row split), and it
  removes a plausible source of a future incorrect assumption when OQ-9
  is finally picked up.

______________________________________________________________________

## Low-priority opportunities

### L-1: Implement `is_debugger_present` on macOS / BSD (stub TODO)

- **File(s)**: `xash3dpp/src/platform/posix/sys.cpp` (`is_debugger_present`).

- **Current pattern**: Linux reads `TracerPid` from `/proc/self/status`; every
  other POSIX target returns `false` with a `// TODO: implement for macOS
  (PT_ATTACHEXC) and BSD (ptrace).` marker (surfaced by `stub_scan.py platform`).

- **Suggested replacement**: macOS `sysctl(KERN_PROC, KERN_PROC_PID)` +
  `P_TRACED`; BSD `ptrace`/`kinfo_proc`. Boundary-spec quirk already documents
  the `false` fallback.

- **Boundary-safe**: Yes — behaviour-additive on non-Linux only.

- **Rationale**: developer-experience only (auto-break on crash under a
  debugger); genuinely low priority.

### L-2: POSIX `shell_execute` double-fork to reap zombies (stub TODO)

- **File(s)**: `xash3dpp/src/platform/posix/sys.cpp` (`shell_execute`).

- **Current pattern**: `fork` + `execvp` fire-and-forget with a `// TODO:
  replace with double-fork to avoid zombie accumulation on long runs.` marker.
  A never-`wait`ed child becomes a zombie until the engine exits.

- **Suggested replacement**: double-fork (fork → child forks the exec target and
  `_exit`s, parent `waitpid`s the middle child) so the grandchild is reparented
  to init and reaped by the OS.

- **Boundary-safe**: Yes.

- **Rationale**: matters only for very long dedicated-server uptimes that open
  many external URLs; rare in practice.

### L-3: High-resolution sleep primitive

- **File(s)**: `os_io.hpp` neighbours in `platform.hpp` (`sleep(ms)`); per-OS
  `sys.cpp`.

- **Current pattern**: `platform::sleep` is millisecond-granular (`Sleep(ms)` /
  `nanosleep` on a ms value). The legacy engine exposes sub-millisecond timing
  via `Win32_NanoSleep` (a `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION` waitable
  timer) and `SDLash_NanoSleep` for the frame-timing loop.

- **Suggested replacement**: add a `sleep_precise(nanoseconds)` when the host
  frame-timing loop is built (boundary-spec open question). Defer until the host
  subsystem schedules it — no consumer today.

- **Boundary-safe**: Yes — additive.

- **Rationale**: frame-pacing accuracy; explicitly deferred to the host bring-up
  (Chunk 12). `ext_tags`: **G-4** (in-game debugging's frame-accurate replay
  tooling would also want this).

### L-4: `IPlatformSockets`'s 7 pure-virtual methods lack the `[[nodiscard]]` their free-function twins all carry *(new 2026-07-20, UNVERIFIED — well-evidenced but outside Phase-2 verification scope)*

- **File(s)**: `xash3dpp/include/xash3dpp/platform/platform_sockets.hpp:31-70`;
  `xash3dpp/include/xash3dpp/platform/os_socket.hpp:135-232`.

- **Current pattern**: every free function in `os_socket.hpp` that
  `IPlatformSockets` wraps (`open_udp_socket`, `open_tcp_socket`, `sendto`,
  `recvfrom`, `send_stream`, `recv_stream`, `connect_stream` — confirmed
  `[[nodiscard]]` at `os_socket.hpp:135,141,181,187,199,205,212`) is
  `[[nodiscard]]`. None of the 7 matching pure-virtual declarations on
  `IPlatformSockets` (`platform_sockets.hpp:36-69`) are — an idiom
  deviation from this subsystem's own established convention.

- **Suggested replacement**: add `[[nodiscard]]` to all 7 pure-virtual
  declarations in `platform_sockets.hpp` to match the free-function surface
  it mirrors. Mechanical, single-file, no override-site changes required
  (an override with a covariant/matching signature does not need to repeat
  `[[nodiscard]]`, but the base declaration should carry it for callers
  going through the interface).

- **Boundary-safe**: Yes.

- **Rationale**: closes an interface/free-function annotation mismatch at
  the one seam (`IPlatformSockets`) where callers can reach this API two
  ways with two different discard-checking guarantees.

### L-5: The clamp-copy-null-terminate bounded-string idiom is hand-duplicated 6x across 3 functions instead of one shared helper *(new 2026-07-20, UNVERIFIED — well-evidenced but outside Phase-2 verification scope)*

- **File(s)**: `xash3dpp/src/platform/win32/sys.cpp:314-317` (`message_box`
  title/message), `:324-327` (`shell_execute` path/params);
  `xash3dpp/src/platform/posix/sys.cpp:248-252` (`shell_execute`
  path/params).

- **Current pattern**: the identical 3-line "clamp length, memcpy,
  null-terminate" idiom appears 6 times across 3 functions (win32
  `message_box`'s 2 buffers, win32 `shell_execute`'s 2 buffers, posix
  `shell_execute`'s 2 buffers) with no shared helper:

  ```cpp
  std::size_t n = s.size() < sizeof( buf ) - 1 ? s.size() : sizeof( buf ) - 1;
  std::memcpy( buf, s.data(), n ); buf[n] = '\0';
  ```

  **M-3**'s own suggested replacement already floats a
  `copy_bounded(std::span<char>, std::string_view)` helper as an
  alternative for exactly this reason — this finding generalizes that note
  to all 6 sites rather than just POSIX `open_library`'s.

- **Suggested replacement**: extract a single, `constexpr`-friendly bounded
  copy-and-terminate helper (e.g.
  `std::size_t copy_bounded_cstr(char *dst, std::size_t dst_cap,
  std::string_view src) noexcept`) into an anonymous-namespace or shared
  platform-internal header, and route all 3 functions (6 call sites, plus
  **M-3**'s POSIX `open_library` site as a 7th if that finding is taken)
  through it — deletes 5 of the 6 duplicate copies of the same 3-line
  idiom.

- **Boundary-safe**: Yes.

- **Rationale**: small, but it is exactly the "second copy should already
  go into a private header" pattern this audit's subtraction lens flags
  tree-wide as a standing rule (`decisions-architecture.md` §4.3) — this
  cluster is at 6 copies, well past the 2-copy threshold the rule sets.

______________________________________________________________________

## Out of scope / ABI-frozen

- **`ThreadPriority::Realtime`** (`include/xash3dpp/platform/thread.hpp:75`;
  `src/platform/win32/thread.cpp:92`; `src/platform/posix/thread.cpp:98`) —
  a deliberate `XASH3DPP-STUB(chunk12)` that logs a Warning and runs at
  Normal priority, awaiting the SDL audio device's real-time scheduling
  need (`T_AudioCallback`). Tracked in the 2026-07-20 audit's obligations
  register as **OBL-12-7**, alongside macOS/BSD thread-naming
  (`pthread_setname_np` is Linux/Android-only in the current POSIX
  backend). This is a chunk-numbered, already-marked stub — not a
  modernization finding — and is recorded here only so it is not
  re-discovered as one.
- No ABI-frozen symbols in this subsystem (see header blockquote).

______________________________________________________________________

## Open questions (carried from the boundary spec)

- **Clipboard** (`Sys_GetClipboardData`) — in-game console paste. Own here or in
  a future `xash3dpp_window` subsystem?
- **SIGTERM handling** (`Posix_SetupSigtermHandling`) — catch here → a
  `host::request_quit()`, or own it in the host?
- **Android extras** (`Android_GetKeyboardHeight`, `Android_GetAndroidID`) —
  deferred until the Android build target is added.
- **High-resolution sleep** — see L-3; add with the frame-timing loop.

______________________________________________________________________

## Cross-cutting flags (for the Phase 14 synthesis)

- **Relocated filesystem H-2 / L-4 land here.** H-2 is *substantially already
  implemented* in the extracted `os_io.cpp` (two-call `to_wide`, dynamic
  `list_directory`); its residue is the win32 `open_library` diagnostic-
  quality inconsistency (**H-1** — corrected 2026-07-20: it fails closed,
  it does not silently truncate a path into use) — flag that the "fix"
  existed in one TU but not the sibling dynlib loader, a classic
  extract-and-diverge. L-4 becomes platform's **M-1** (typed seek origin at
  the OS boundary).
- **`strnicmp` / `strncmp` over-read is ABSENT in platform** (0 sites). The
  utilities-M-4 / filesystem-M-7 `string_view` → C-string over-read does not
  recur here — platform either copies into a bounded terminated buffer or uses
  length-taking OS APIs. This is a *negative* data point for the sweep: the
  pattern is real but not universal.
- **The Q-21 "missing thread-spawn primitive" flag from the 2026-07-06 pass
  is RETIRED (2026-07-20).** `platform::spawn_thread`/`JoinHandle` (Q-24)
  shipped — `include/xash3dpp/platform/thread.hpp`, both per-OS
  `thread.cpp` backends — with a real production consumer
  (`src/sound/topology.cpp`, the tree's only 2 production thread spawns).
  Platform was correctly identified as the natural owner in the earlier
  pass; the primitive itself is simply done. What remains open on this axis
  is not a platform gap: **OBL-12-7** (`ThreadPriority::Realtime`, see
  Out-of-scope) and the **P-1 main-thread inbox drain** (HB-4, the host
  `RunFrame` side, tracked in `host`/synthesis, not here).
- **The `assert_main_thread()` doc/code contradiction is a platform-owned
  fix (H-2), even though it was surfaced by a tree-wide thread-model lens,
  not a platform-only re-read.** All 4 call sites and both docs asserting
  the false "thin wrapper" claim live in or reference platform code. Worth
  flagging to the synthesis pass because the same contradiction is
  independently recorded in `CORRECTIONS.md`'s "Doc claims found FALSE"
  list — this report is where the actual fix (delete the header, unify the
  4 sites) is proposed; `CORRECTIONS.md` only records that the claim is
  false.
- **Three UTF-8 → UTF-16 converters** in the win32 TUs remain a small
  instance of a likely tree-wide theme (per-TU re-implementation of the
  same OS glue) — see **H-1**. The 2026-07-20 audit's registry-unification
  lens (L2) found the same shape recurring at a larger scale in filesystem
  archive dispatch, sound codec dispatch, and imagelib codec dispatch (three
  incompatible extension-keyed dispatch shapes for one legacy idiom); this
  entry's win32 converter triplication is the platform-scoped instance of
  that same "copy the shape, then diverge" failure mode, not a duplicate
  of the registry-dispatch finding itself.
