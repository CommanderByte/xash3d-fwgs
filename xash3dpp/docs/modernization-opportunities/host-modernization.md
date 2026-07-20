# Host Modernization Opportunities

> C++ standard in use: C++**23** (from `xash3dpp/CMakeLists.txt`,
> `CMAKE_CXX_STANDARD 23`) — but `xash3dpp_host`'s own
> `target_compile_features` still pins **`cxx_std_20`** (`src/host/CMakeLists.txt:11`);
> harmless today because the root setting wins, tracked as L-3 below.
> Boundary spec: `docs/boundaries/host-boundary.md`
> Threading analysis: `docs/threading-analysis/host-threading.md`
> ABI-frozen symbols reachable through this subsystem: **`Host_Error`**
> (`engine/eiface.h` `pfnHostError`; a `GAME_EXPORT` direct-symbol export). The
> C signature `void Host_Error(const char *fmt, ...)` is frozen and lives in the
> `xash3dpp_abi` shim, not here — host exposes only the typed
> `signal_frame_abort(core::ErrorCode, std::string_view)` sink it routes to.
> The launcher↔engine `Host_Main`/`Host_Shutdown` symbols are an internal
> contract the rewrite folds away (boundary spec §External ABI) — not frozen.
>
> **Refreshed 2026-07-20** (tree-wide modernization audit, `CORRECTIONS.md` +
> corpus digest `## host` + lenses L3/L5/L11): the 2026-07-06 pass's one
> High-tier candidate (the missing `assert_thread_role` on
> `RunFrame`/`RequestShutdown`/`signal_frame_abort`) **shipped 2026-07-19**
> (HB-3, see host-threading.md) — marked resolved in place below. Two new
> High items replace it: a **CONFIRMED live defect** (the shipped launcher
> never constructs `EngineContext`, so host's real frame dispatch is dead code
> in production) and a **subtraction-lens deletion** (`Host::Impl::fs()` is
> unreferenced). M-1 is retiered to Low with a corrected, non-speculative
> proposal (its ID is kept for `abi-modernization.md`'s cross-reference).
> L-1's original "leave as-is" verdict is superseded by the fs() deletion.
> New: M-2 (CMake link-visibility mistag), M-3 (a false doc claim about
> `std::expected` short-circuiting that does not exist as-built), M-4 (delete
> `HostStats`, subsuming the old F100 `[[nodiscard]]` nit), M-5 (the
> diagnostics-aggregator assembly role host would play), L-3 (cxx_std_20
> residue), L-5 (a shape-constraint-only note on `RequestShutdown`).

*Authored 2026-07-06 (as-built pass); refreshed 2026-07-20. Scope: the three
host TUs (`host.cpp`, `engine_context.cpp`, `engine_context_accessor.cpp`) +
the two public headers. **Verdict, reaffirmed:** host is idiomatic modern
C++23 in its own code — pimpl, injected non-owning deps, `std::array`/
`std::string_view`, `enum class` lifecycle states, `noexcept` teardown, zero
raw owning pointers, zero `malloc`/`free`, zero naked `new`. What the refresh
adds is not new C-isms found in host's own code — it is (a) one confirmed
production-wiring defect that makes host's real dispatch path unreachable in
the shipped binary, (b) one dead accessor the tree-wide subtraction lens
named, and (c) a cluster of doc-vs-code mismatches in `host-boundary.md`
itself. The string_view→C-string `strnicmp`/`strncmp` over-read pattern
tracked in utilities (M-4) / filesystem (M-7) / cmd_cvar (M-5) remains
**ABSENT** here (0 sites) — still a negative data point for that sweep.*

## Summary

The host subsystem is the engine's lifecycle orchestrator: it is deliberately
**free-functions/orchestrator-shaped over an aggregate** (`EngineContext`), which
Q-22 explicitly sanctions for frame loops and lifecycle sequencing — so the
usual "promote to RAII class" modernization does not apply to `EngineContext`
itself (it is already the canonical flat-owner). `Host` is a pimpl class with an
RAII `Impl`, matching the P-7 idiom for the one piece of host that carries
invariants (the pool + frame-abort state).

The refresh's headline is not a C-idiom finding: it is that **the code
inside host is more idiomatic than the path that reaches it.**
`launcher/main.cpp:90-91` — the only entry point the shipped binary calls —
builds an all-null `HostInitParams` and never constructs an `EngineContext`,
so every `if (s.X) ...`-guarded dispatch line in `Host::RunFrame` is a no-op
in production today (H-1). Two of host's introspection surfaces also fail
their own documented contract: `Host::stats()` returns a live `const&` into
`Impl` state, which `host-boundary.md`'s own P-2 row says a snapshot accessor
must never do (M-4), and `Host::Impl::fs()` — previously assessed as a
legitimate but slightly-spread accessor — turns out to have zero callers
anywhere in the tree (H-2).

Non-obvious constraints found (do **not** "modernize" these):

- **No `setjmp`/`longjmp`** — already resolved (OQ-1). The legacy
  `Host_AbortCurrentFrame` stack-jump is intentionally replaced by a
  flag-polled-at-frame-top design; do not reintroduce any non-local jump.
- **The frame-abort escalation uses `XASH_FATAL` + `platform::crash::print_trace()`**
  — this is a terminal, signal-safe-by-delegation path (legacy Q-4 `Sys_Error`
  semantics). Not a modernization target.
- **Injected deps are non-owning raw pointers by design** (Q-4 DI-params,
  nullable = standalone/test mode). These are *not* owning-pointer smells;
  `@lifetime: caller` annotations already mark them.
- **`current_engine_context()` returning a mutable `EngineContext*`** — this
  looked like a P-4 typed-surface violation to one corpus finding this pass;
  it is the deliberate, ratified Q-2/OQ-10 exception, documented at the exact
  lines the finding cited against it and covered by an inline
  `compliance-allow(di-global-ref)`. See "Investigated and refuted" below —
  do not re-raise it.

______________________________________________________________________

## High-priority opportunities

### RESOLVED — `assert_thread_role` gap on `RunFrame`/`RequestShutdown`/`signal_frame_abort`

The 2026-07-06 pass's one would-be-High item (P-8 annotation gap; the three
mutating entry points other than `init`/`shutdown` did not assert
`ThreadRole::Main`) **shipped 2026-07-19** in the consolidation audit (HB-3).
Verified directly: `host.cpp:208` (`RunFrame`), `:262` (`RequestShutdown`),
`:283` (`signal_frame_abort`) each open with
`::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );`.
`host-boundary.md:606-613` records the same closure and
`host-threading.md:157-163` marks its own Recommendation 1 done. No further
action; kept here only so this refresh does not silently drop a resolved
item.

### H-1: Shipped launcher never constructs `EngineContext` — host's real per-frame dispatch is dead code in production `[EXT:G-1,G-2,G-3]`

- **File(s)**: `xash3dpp/src/launcher/main.cpp:90-91`;
  `xash3dpp/src/host/host.cpp:309-330` (`Host::Main`), `:237,240,250`
  (the guarded dispatch lines in `RunFrame`); the only construction site of
  `EngineContext` anywhere in the tree is
  `xash3dpp/tests/host/test_engine_context_networking.cpp:99,119,136`.

- **Current pattern**:

  ```cpp
  // launcher/main.cpp:90-91 — the entire production entry point
  xash::Host host;
  return host.Main(args);
  ```

  `Host::Main` (`host.cpp:309-330`) builds a `HostInitParams` with only the
  string/flag fields; the comment at `:321` says outright "dep pointers
  intentionally left null (standalone path)". Every subsystem dispatch in
  `RunFrame` is then guarded — `if (s.cmd_cvar) ...` (`:237`),
  `if (s.map_loader) ...` (`:240`), `if (s.server) ...` (`:250`) — and all
  four pointers (`cmd_cvar`, `clock`, `map_loader`, `server`) are null in the
  shipped binary, so the frame loop the whole rewrite exists to run is a
  no-op every tick. `EngineContext` (`engine_context.hpp:79`), the one type
  that wires host to `filesystem`/`cmd_cvar`/`clock`/`networking`/
  `map_loader`/`server`, is constructed nowhere in `src/` — only in one test
  file.

- **Suggested replacement**: Not a call to build anything new inside host —
  a wiring/doc correction. `host-boundary.md` and `launcher-boundary.md`
  should stop implying the shipped launcher exercises the wired engine, and
  the Chunk 12 (client-wiring) work item in `implementation-plan.md` should
  explicitly include switching `launcher/main.cpp` from the standalone
  `Host::Main` path to constructing an `EngineContext` and driving it — or,
  if the standalone path is deliberately staying the production shape for
  longer, that must be stated, not left implicit. This is a precondition for
  Chunk 12/13: those chunks plan to CMake-link `xash3dpp_sound`/
  `xash3dpp_input`/`xash3dpp_imagelib` into `xash3dpp_host`, which will make
  them `dep_scan`-reachable while remaining runtime-unreachable in the
  shipped binary unless this is fixed first (see L5-5 in the corpus lenses;
  cross-reference `launcher-modernization.md`, which owns the actual code
  change).

- **Boundary-safe**: Yes — no frozen-ABI symbol is touched; this is an
  internal wiring/doc defect.

- **Rationale**: Survived four independent refutation attempts in the Phase-2
  audit ("the code is worse than the finding claims"). Every extension goal
  this campaign targets (G-1 MCP service, G-2 game ABI v2, G-3 debug thread)
  is described in `host-boundary.md`'s own Extension-axes table as consuming
  `EngineContext`-wired state (`HostStats`/`ClockStats`, the P-1 drain slot at
  the frame edge) — none of that exists in the running process today. This is
  the single highest-leverage correction in this report: every other host
  finding in this file describes code that is unreachable in production until
  this is fixed.

### H-2: Delete `Host::Impl::fs()` — unreferenced dead code `[subtraction: L11-SUB-1]`

- **File(s)**: `xash3dpp/src/host/host.cpp:71-74` (declaration),
  confirmed zero call sites tree-wide (`init()` at `:149-171` and
  `Impl::shutdown()` at `:87-93` both access `own_fs`/`ext_fs` directly).

- **Current pattern**:

  ```cpp
  // Convenience: returns the active filesystem (owned or injected).
  filesystem::Filesystem &fs() noexcept
  {
      return ext_fs ? *ext_fs : own_fs;
  }
  ```

  A grep for `\.fs\(\)` / `->fs\(\)` across `xash3dpp/` returns nothing —
  re-verified for this refresh. The 2026-07-06 pass analysed this as a
  legitimate-but-slightly-spread own-or-injected accessor and recommended
  leaving it as-is (see the superseded L-1 below); that recommendation was
  wrong on the facts, not on the design intent.

- **Suggested replacement**: Delete the four lines. No behaviour change — no
  caller exists to break. If a genuine owned-vs-injected read accessor is
  needed later (e.g. a Chunk-12 client that needs the active FS), re-add it
  at that point with a real call site; re-adding costs the same four lines.

- **Boundary-safe**: Yes.

- **Rationale**: This is the L11 subtraction lens's own SUB-1 decision-free
  deletion batch item (i) — "`Host::Impl::fs()` (`host.cpp:71-74`), 4 lines,
  zero callers" — one of ~14 items across the tree with zero call sites and
  zero design questions attached. Per this campaign's instruction to give
  subtraction-lens-named deletions a High slot: promoted here from the prior
  pass's Low/no-action verdict. Effort is trivial; the value is removing a
  dual-ownership accessor that looks load-bearing but is not, so a future
  reader does not build on top of it.

______________________________________________________________________

## Medium-priority opportunities

### M-1: `frame_abort_detail` manual `std::array` + `memcpy` + null-terminate → do not build a standalone helper `[EXT:P-3]` — **retiered to Low-priority, ID kept for cross-reference stability (see L-4)**

Content moved to L-4 below (`abi-modernization.md` cites this ID directly,
so the heading stays but the entry itself lives at its corrected tier).

### M-2: `EngineContext`'s public header depends on `cmd_cvar`, but `xash3dpp_host` links it `PRIVATE`

- **File(s)**: `xash3dpp/src/host/CMakeLists.txt:13-27` (the `PRIVATE` block
  is `:21-23`, `xash3dpp_cmd_cvar` at `:23`);
  `xash3dpp/include/xash3dpp/host/engine_context.hpp:16` (`#include
  <xash3dpp/cmd_cvar/context.hpp>`) and `:89` (`cmd_cvar::CmdCvarContext
  cmd_cvar;` embedded by value in the public `EngineContext` struct); also
  `EngineContextInitParams::trust_oracle`/`compat_policy`
  (`engine_context.hpp:46-47`) are `cmd_cvar::ITrustOracle`/`ICompatPolicy`
  pointers in the same public header.

- **Current pattern**: `engine_context.hpp` is a **public** header
  (`xash3dpp/host/engine_context.hpp`) with a real, non-optional dependency
  on `cmd_cvar`'s types — a downstream consumer that only includes
  `<xash3dpp/host/engine_context.hpp>` gets a hard compile error unless
  `xash3dpp_cmd_cvar`'s headers are also on its include path, which today
  happens to work only because CMake's `PRIVATE` propagation still puts the
  include dir on `xash3dpp_host`'s own compile line, not because the
  dependency is declared correctly. Every other public-header dependency
  host has (`filesystem`, `core`, `map_loader`, `networking`, `server`) is
  correctly `PUBLIC`; `cmd_cvar` is the one mistagged case.

- **Suggested replacement**: Move `xash3dpp_cmd_cvar` from `PRIVATE` to
  `PUBLIC` in `xash3dpp/src/host/CMakeLists.txt`. One-line change; no new
  consumer needed since `cmd_cvar` is already a real dependency, just
  mis-tagged.

- **Boundary-safe**: Yes — CMake-only change, no source edit.

- **Rationale**: This is not host-specific sloppiness — the same
  `PUBLIC`-should-be-`PRIVATE`-or-vice-versa link-visibility mistake was
  independently hand-found by six-plus other per-subsystem packs this pass
  (server, cmd_cvar, filesystem, input, imagelib, plus a sound mention). The
  cross-cutting lens recommends a `dep_scan.py` check that flags this
  mechanically (diff `PUBLIC` link edges against what a target's public
  headers actually `#include`) rather than relying on the next tree-wide
  audit to re-find each instance by hand — see the tooling recommendation in
  the lens digest; that check is out of scope for this file (it belongs to
  whichever report owns `xash3dpp/tools/`), but the underlying CMake fix
  belongs here.

### M-3: `host-boundary.md`'s OQ-1 doc entry claims an `std::expected` short-circuit that does not exist as-built

- **File(s)**: `xash3dpp/docs/boundaries/host-boundary.md:339-344` and
  `:355-357`; `xash3dpp/include/xash3dpp/map_loader/map_loader.hpp:139`;
  `xash3dpp/include/xash3dpp/server/server.hpp:137`;
  `xash3dpp/src/host/host.cpp:229,237,240,250`.

- **Current pattern**: `host-boundary.md:339-344` states "Each per-frame
  entry point (`MapLoader::run_frame_step`, `Server::frame`, `Client::frame`,
  `Networking::http_run`) returns `expected<void, ErrorCode>`;
  `Host::RunFrame` short-circuits on first error", and `:355-357` further
  claims "the legacy kill-the-frame-mid-tick semantic is preserved: once the
  flag is set, every subsequent per-frame entry point sees it and bails
  early". As-built, `MapLoader::run_frame_step()` is declared
  `void ... noexcept` (`map_loader.hpp:139`) and `Server::frame(double)` is
  declared `void ... noexcept` (`server.hpp:137`); `host.cpp` calls both as
  bare statements (`:240`, `:250`) with nothing to short-circuit on. A grep
  for `frame_abort` across `src/`/`include/` returns only
  `host.cpp:51-53,215-222,290-302` and `abi/engine_funcs.cpp:73` — no
  subsystem holds a reference to `frame_abort_pending`, so nothing mid-frame
  actually polls it; the flag is only observed at the *top* of the next
  `RunFrame` (`:215-222`), not mid-tick as the doc claims.

- **Suggested replacement**: Correct three doc sites in `host-boundary.md`,
  not code: (a) `:339-344` — mark the engine-internal `std::expected` half
  DEFERRED-with-owner (Chunk 12, when `Client::frame`/`Networking::http_run`
  actually exist as `expected`-returning signatures), noting the as-built
  `void` signatures explicitly; (b) `:355-357` — strike or qualify the
  "kill the frame mid-tick" sentence: the flag is observed only at the top
  of the *next* frame, never mid-tick, today; (c) note that only the
  game-DLL half of OQ-1's "hybrid abort" (the `frame_abort_pending`
  flag-poll-at-frame-top) is implemented — the engine-internal half is not
  yet built.

- **Boundary-safe**: Yes — documentation-only.

- **Rationale**: A future implementer reading OQ-1 as written would believe
  `Host::RunFrame` already has an engine-internal error short-circuit and
  would either skip building it or build it inconsistently with what the doc
  describes. Blast radius 2 (two false claims in the same paragraph pair);
  effort S (doc edit only).

### M-4: Delete `HostStats`, `Host::stats()`, and its four mirror writes — the accessor contradicts `host-boundary.md`'s own P-2 row `[subtraction]`

- **File(s)**: `xash3dpp/include/xash3dpp/host/host.hpp:109-112` (`HostStats`
  struct, one `HostStatus status` field), `:171` (`const HostStats&
  stats() const noexcept`, the only public observer on `Host` **without**
  `[[nodiscard]]` — subsumes the prior pass's F100); `xash3dpp/src/host/host.cpp:345`
  (accessor returns `impl_->stats_` directly — a live reference, not a
  snapshot); the four mirror writes at `host.cpp:98-99` (`Impl::shutdown`),
  `:135-136` and `:197-198` (`init`), `:263-264` (`RequestShutdown`).

- **Current pattern**:

  ```cpp
  // host.hpp:109-112
  struct HostStats { HostStatus status = HostStatus::Init; };
  // host.hpp:171
  const HostStats& stats() const noexcept;
  // host.cpp:345
  const HostStats& Host::stats() const noexcept { return impl_->stats_; }
  // host.cpp — four sites, e.g. :263-264
  impl_->status        = HostStatus::Shutdown;
  impl_->stats_.status = impl_->status;
  ```

  `HostStats` has zero callers anywhere in the tree; its one field exactly
  duplicates `Host::status()`, which stays the real accessor. Every write to
  `status` is separately mirrored into `stats_.status` — four extra lines
  that only exist to keep a struct nobody reads in sync.

- **Suggested replacement**: Remove `HostStats` (`host.hpp:109-112`),
  `Host::stats()` (`host.hpp:171` + the accessor at `host.cpp:345`), and the
  four `stats_.status = status` mirror writes. `Host::status()` is unaffected
  and remains the query surface. Update `host-boundary.md`'s P-2 row
  (`:594`, currently "`HostStats` (today one `status` field) is the seed of
  the host-side snapshot surface") and P-4 row (`:595`, currently cites
  `Host::stats()` as part of the typed query surface) to point at
  `Host::status()` instead.

- **Boundary-safe**: Yes — `HostStats` is not part of any frozen ABI.

- **Rationale**: `host-boundary.md`'s own P-2 row states off-main readers
  "consume the snapshot at the frame edge, **never live Impl state**" — but
  `stats()` hands back a live `const&` **into** `Impl::stats_`, contradicting
  the door rule the same doc asserts one paragraph away. The conforming
  shape already exists in-tree as a sibling precedent —
  `MapLoader::stats()` (`map_loader.cpp:271`) returns `MapLoaderStats` **by
  value**. Rather than flip `Host::stats()` to by-value (the generic
  cross-tree rule for the other five class-D "plain-storage-returned-as-live-`const&`"
  stats structs — see the diagnostics-lens digest), deleting is strictly
  better here specifically because the struct has zero callers and its only
  field is a redundant mirror: deletion removes the non-atomic-mirror
  question rather than answering it, and drops one of the tree's 14 stats
  structs entirely rather than converting it. Also removes the missing
  `[[nodiscard]]` nit (prior pass's F100) by removing the function it was
  attached to.

### M-5: If the tree-wide diagnostics aggregator (HB-6) is built, host is the composition root that assembles and registers it — record the shape now, land it with its own consumer `[EXT:G-1,G-3]`

- **File(s)**: `xash3dpp/src/host/CMakeLists.txt:13-19` (host's actual link
  set: `cmd_cvar`, `core`, `filesystem`, `map_loader`, `memory`,
  `networking`, `platform`, `server` — and *not* `sound`, `input`,
  `content`, `imagelib`); `xash3dpp/src/cmd_cvar/context_init.cpp:191,210,228`
  (`cmdlist`/`cvarlist`/`hashstats` — the existing registry-dumping
  built-in precedent host already links).

- **Current pattern**: `docs/design/debug-stats-design.md:359` sketches a
  positional `diagnostics_dump(CmdCvarContext&, MemorySubsystem&, ...)
  noexcept` aggregator. Host is the natural place to *assemble* such an
  aggregator (it is the composition root that owns the subsystem objects and
  already links `cmd_cvar`), but the positional shape would force host to
  gain four **new** link edges (`sound`, `input`, `content`, `imagelib`)
  purely to hold a debug command — measured directly from host's own
  `CMakeLists.txt`, not estimated.

- **Suggested replacement**: This is a shape constraint on `core` (which
  would own `DiagSink`/`DiagChannel`/`diagnostics_dump` as the only common
  ancestor of all 17 build targets), not new work owed by host today. **When**
  that primitive lands, host's role is: assemble a fixed
  `std::array<DiagChannel, N>` from the subsystems it already links, and
  register one `stats` console built-in via the existing
  `CmdCvarContext::cmd_add` mechanism (the same call host would need for any
  new command), with the sink writing to console. This costs zero new link
  edges — the composition root registers exactly what it owns.

- **Boundary-safe**: NeedsVerification — the `DiagSink`/`DiagChannel` types
  themselves are `core`'s to define (out of scope for this file); host's
  slice is CMake-neutral and touches no frozen ABI.

- **Rationale**: `[EXT:G-1,G-3]` because both the MCP service and a debug
  thread are named consumers of a future introspection layer in
  `host-boundary.md`'s own Extension-axes table, but per the anti-gold-plating
  rule this is recorded as a **shape constraint**, not buildable work, until
  `core` lands the primitive — the day-one consumer for the primitive itself
  is the `stats` console command, which would ship in the same change (not
  ahead of it), matching the `cmdlist`/`cvarlist`/`hashstats` precedent
  already in production. This also discharges the "`diagnostics_dump`
  overdue since ≥3 stats structs" backlog trigger, which has been overdue
  since the tree had three (it now has 14). Not assigned an ID beyond this
  entry because the buildable half belongs to `core`'s report; recorded here
  only because host is named as the assembly point.

______________________________________________________________________

## Low-priority / cosmetic opportunities

### L-1: `Host::Impl::fs()` owned-vs-injected filesystem selector — **SUPERSEDED, see H-2**

The 2026-07-06 verdict here ("noting it as a candidate ... left as-is;
documented so a future reader does not mistake the dual members for a bug")
was wrong on the facts: re-checking call sites this pass found `fs()` has
**zero callers anywhere** — both `init()` and `Impl::shutdown()` already
access `own_fs`/`ext_fs` directly. Promoted to H-2 (delete it) per the
subtraction lens. Left here, marked superseded, so the ID is not silently
dropped.

### L-2: stale/aspirational doc-comment cvar rosters `[doc]`

- **File(s)**: `xash3dpp/src/host/host.cpp:173-177` (unchanged line range
  from the prior pass) — the `TODO Chunk 3` block listing `host_developer`,
  `host_gameloaded`, `host_clientloaded`, … and the
  `quit`/`exit`/`memlist`/`host_error` commands.

- **Current pattern**: host registers **no** lifecycle cvars or commands
  yet; the boundary-spec cvar tables and these inline TODOs are aspirational.
  Reconfirmed this pass: `host-boundary.md`'s own §Divergences item makes the
  same disclaimer, so the doc is internally consistent about this gap.

- **Suggested replacement**: not a code change — flagged so the modernization
  reader knows the "Cvars owned by host" tables in the boundary spec are
  **not-yet-implemented** targets, not as-built state.

- **Boundary-safe**: Yes.

- **Rationale**: Missing code, not C-style code — kept here only as a status
  pointer, confirmed still accurate 2026-07-20.

### L-3: `xash3dpp_host` (and `xash3dpp_abi`) still declare `cxx_std_20`

- **File(s)**: `xash3dpp/src/host/CMakeLists.txt:11`
  (`target_compile_features(xash3dpp_host PUBLIC cxx_std_20)`); for
  awareness only, `xash3dpp/src/abi/CMakeLists.txt:14` has the identical
  residue but is out of scope for this file.

- **Current pattern**: 18 of the tree's 20 `target_compile_features` lines
  declare `cxx_std_23`; `xash3dpp_host` and `xash3dpp_abi` are the two
  exceptions. `layer-model.md:63` claims the "6B wave" normalized every
  target to `cxx_std_23` — that claim is false for these two, re-verified by
  direct grep this pass (the corpus digest independently confirms it via a
  fresh `dep_scan.py` run).

- **Suggested replacement**: `target_compile_features(xash3dpp_host PUBLIC
  cxx_std_23)` — one line.

- **Boundary-safe**: Yes.

- **Rationale**: Harmless today — the root `CMAKE_CXX_STANDARD 23` setting
  wins regardless — so this is pure documentation-hygiene / drift-prevention,
  not a live bug. Zero risk, mechanical; land it opportunistically next time
  `CMakeLists.txt` is touched for another reason.

### L-4 (was M-1): `frame_abort_detail` manual `std::array` + `memcpy` + null-terminate `[EXT:P-3]` — retiered from Medium, corrected proposal

- **File(s)**: `xash3dpp/src/host/host.cpp:298-302` (line numbers shifted
  from the prior pass's ~271-279 as `RunFrame` grew a `Clock::tick()`/
  `Server::frame()` body in the interim; same code).

- **Current pattern**:

  ```cpp
  const std::size_t n   = detail.size() < s.frame_abort_detail.size() - 1
                          ? detail.size()
                          : s.frame_abort_detail.size() - 1;
  std::memcpy( s.frame_abort_detail.data(), detail.data(), n );
  s.frame_abort_detail[n] = '\0';
  ```

  Bounded and correct (no over-read, no over-write); `detail` is a
  `std::string_view` (`host.hpp:162-163`) so `.data()`/`.size()` are used
  consistently. The only defect is that the clamp/terminate logic is spelled
  out inline at its one call site.

- **Suggested replacement**: **Corrected from the prior pass — do not build
  a standalone `copy_truncated` header.** A single, correct, behaviour-neutral
  4-line call site does not justify a new header-only primitive in
  `xash3dpp_utilities` (that would add a link edge from `xash3dpp_host` to
  `xash3dpp_utilities` for one call site — a net loss). Recorded instead as a
  shape constraint: **if** a `string_view`-in / bounded-`char`-buffer-out
  helper is ever built in `utilities/string.hpp` for a different reason —
  e.g. the `ut::strncpy(char *dst, std::string_view src, std::size_t size)
  noexcept` overload utilities' own report proposes for its five server
  callers — `host.cpp:298-302` becomes its sixth call site at that time, not
  before. Note: `abi-modernization.md` currently cites this entry (as "M-1")
  proposing a `copy_truncated(std::span<char>, std::string_view)` helper for
  its own `Host_Error` formatting path; that citation should be read against
  this corrected proposal (fold into the utilities helper, not a standalone
  one) when that report is next refreshed.

- **Boundary-safe**: Yes — engine-internal, single call site.

- **Rationale**: Re-assessed this pass: Medium was the wrong tier for a
  single correct call site whose only defect is that it is spelled out
  inline, and the original proposal (`consumer_status: speculative`, "no
  consumer in-tree today") was written as buildable work rather than as the
  shape constraint the anti-gold-plating rule requires for a
  no-named-consumer primitive. Blast radius 1; no behaviour change either
  way.

### L-5: `RequestShutdown`'s Main-thread assert has no stated structural rationale — shape constraint, no action owed

- **File(s)**: `xash3dpp/src/host/host.cpp:260-265`;
  `xash3dpp/docs/boundaries/host-boundary.md:606-613`.

- **Current pattern**: `host-boundary.md` gives named structural reasons for
  the `RunFrame` (frame-edge/marshal-back destination) and
  `signal_frame_abort` (GoldSrc `Host_Error` DLL-contract: game DLLs call it
  synchronously from the Main call stack) asserts. `RequestShutdown` just
  writes two plain fields (`status`, `stats_.status`) and was asserted in the
  same 2026-07-19 blanket closure with no operation-specific reason recorded.

- **Suggested replacement**: Not a code change — a register note for when
  the P-1 main-thread inbox (HB-4) lands: `RequestShutdown` is the natural
  first candidate to route through it for a future off-main caller (a G-1
  MCP "stop server" command, a G-3 debug-thread emergency stop) rather than
  requiring that caller to already be on Main. No action needed until HB-4
  exists.

- **Boundary-safe**: Yes.

- **Rationale**: Purely a register/documentation gap the assert closure left
  behind — the assert itself is correct and required under the current
  Main-only contract; only its stated rationale is thin compared to its two
  siblings.

______________________________________________________________________

## Out of scope / ABI-frozen

Patterns found that *look* modernizable but must not change:

- **`EngineContext` as a flat struct of public members** — Q-22 reserves the
  free-functions-over-aggregate / flat-owner shape for orchestrators;
  `EngineContext` is the canonical example. Do **not** wrap its members
  behind getters or promote it to a pimpl.
- **Non-owning raw dependency pointers in `HostInitParams`/`EngineContextInitParams`** —
  the Q-4 DI-params model; nullable = standalone/test. Not owning-pointer
  smell.
- **The `Host_Main` / `Host_Shutdown` / `pfnChangeGame` launcher symbols** — an
  internal launcher↔engine contract the rewrite folds into one executable
  (boundary spec §External ABI); not a public ABI to preserve, and not
  present in the current host TUs (the launcher owns them).
- **`realtime()` falling back to `platform::get_time()` when `clock ==
  nullptr`** — the standalone/test path has no `Clock`; the fallback is
  intentional, not a missing-injection bug.
- **`current_engine_context()`'s mutable `EngineContext*` return** — the
  documented Q-2/OQ-10 exception (`extern "C"` callers have no other way to
  reach the live context). See "Investigated and refuted" below; not a
  modernization target and already correctly recorded in `host-boundary.md`'s
  P-4/P-6 rows.
- **`Host_Error(const char *fmt, ...)`'s `va_list`/variadic boundary** — the
  frozen `GAME_EXPORT` signature forces it; only the *forwarded* detail
  string is a modernization surface (see L-4, and `abi-modernization.md`'s
  own entry against the shim side).

## Investigated and refuted

Recorded so nobody re-derives these:

- **`current_engine_context()` hands any C-ABI caller thread an unrestricted
  mutable pointer to the whole live `EngineContext`, bypassing typed
  introspection** — REFUTED. `engine_context_accessor.hpp:6-10` documents
  this as "the single documented exception to the 'no global accessor'
  rule", carries decision refs to `decisions-architecture.md` §3 Q-2 and
  `host-boundary.md` OQ-10, and the definition
  (`engine_context_accessor.cpp:25`) carries
  `compliance-allow(di-global-ref): Q-2 documented singleton exception
  (OQ-10)`. Exactly one production caller exists
  (`abi/engine_funcs.cpp:69`, the `Host_Error` path) — blast radius 1,
  verified not estimated. The "typed surface" remedy the finding proposed
  is already recorded, verbatim, in `host-boundary.md`'s P-4/P-6 rows. One
  narrow nuance survives from the amendment round: `engine_context_accessor.hpp`
  physically lives under `xash3dpp/include/xash3dpp/abi/`, which the
  campaign's frozen-ABI fence names by directory — but this specific header
  is `xash3dpp`'s own C-ABI shim glue (Q-2/OQ-10), not a vendored legacy
  shape, so `touches_frozen_abi:false` is defensible in spirit. Any *future*
  change to the accessor function's own signature (not what it points at)
  should still get an explicit deviation record.

______________________________________________________________________

## Open questions

- **When Chunk 12 (client) wires `EngineContext` in the shipped launcher
  (resolving H-1), does `Host::Main`'s standalone all-null path stay as a
  supported embedding mode (Android JNI / test harness, per the header's own
  documented usage example) or does it become test-only?** Affects whether
  the `if (s.X) ...` guards in `RunFrame` stay permanent or become
  debug-assertable once the launcher always wires a full context. No action
  owed by host today; flagged for whoever picks up H-1 / L5-5.
- **Does the `core`-owned diagnostics aggregator (M-5) land in Chunk 11-13's
  window, or does it wait for an actual G-1/G-3 consumer beyond the `stats`
  console command?** The shape constraint is recorded; the scheduling
  decision belongs to whoever owns the HB-6 backlog item, not this report.

______________________________________________________________________

## Implementation gaps (missing code, not modernization)

Recorded for completeness; these are Chunk-6/12 work, not C→C++ refactors:

- Host lifecycle cvar + command registration (L-2).
- `RunFrame` frame-abort recovery body (`SV_Shutdown`/`CL_Drop`/
  `CL_ClearEdicts`/`Mod_FreeAll` are `TODO Chunk 6/12`).
- Dedicated-server stdin pump (OQ-9 `platform::console::poll_line` wrapper) —
  `platform::console::read_line()` itself has zero production callers tree-wide
  (confirmed by the platform pack this pass), so this gap is two-sided: host
  has an unwired TODO and platform has an unwired provider.
- Server/client per-frame dispatch (`TODO Chunk 6/12`).
- Wiring `EngineContext` into the shipped launcher (H-1) — the highest-priority
  item in this list; everything else in this section is dead code until it
  lands.
- `Filesystem::init` → `FilesystemInitParams` ergonomics — **already tracked**
  as DEFER-with-owner in the boundary spec (ripples into ~24 filesystem test
  call sites; owned by a future filesystem-ergonomics slice).
