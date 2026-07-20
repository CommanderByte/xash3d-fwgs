# Core Modernization Opportunities

> Authored 2026-07-06 (as-built pass).
> Refreshed 2026-07-20 (tree-wide modernization audit, HEAD `cc73c054`,
> read-only Phase-2/3 pass). Changes this pass: promoted two confirmed
> findings to High (H-1 unify `assert_main_thread()` onto
> `assert_thread_role(Main)` and delete `private/core/assert_main.hpp` —
> supersedes the former L-4 comment-only fix; H-2 add the `core/diagnostics.hpp`
> channel-span aggregator, the buildable slice of HB-6); added three Medium
> findings (M-1 delete `ClockStats`/`Clock::stats()` — AMENDED down from the
> original finder's High, see rationale; M-2 assert Main on
> `log_set_callback()`; M-3 guard `Clock::tick()` against a second caller
> thread); added L-5 (`std::bit_ceil` replaces two hand-rolled
> round-up-to-power-of-two helpers). L-1/L-2/L-3 unchanged. Recorded a new
> **Open questions** section carrying the HB-5 shape constraint that touches
> `core/` and the settled diagnostics-aggregator placement decision (types in
> `core`, assembly in `host`).
> C++ standard in use: C++**23** (from `xash3dpp/src/core/CMakeLists.txt`,
> `target_compile_features(xash3dpp_core PUBLIC cxx_std_23)`, and the tree-wide
> `CMAKE_CXX_STANDARD 23`).
> Boundary spec: `docs/boundaries/core-boundary.md`
> Threading: inline in the boundary spec (`## Threading`) — core keeps its
> thread-role analysis in the boundary doc, not a separate file.
> ABI-frozen symbols in this subsystem: **None** — core is fully internal
> (no Game/Client DLL header exposes `xash::core`). The only external-facing
> contracts are informal: the `[tag][LEVEL]:` log prefix matches the legacy
> `Con_Printf` shape users recognise, and committed `ErrorCode` integer values
> appear in diagnostic logs.

## Summary

The core subsystem is four small TUs — `clock.cpp` (frame timing over
`host.c`), `log.cpp` (diagnostic sinks), `thread_role.cpp` (the thread-role
registry), `error.cpp` (`ErrorCode` → string) — plus two header-only
cross-thread primitives (`mpsc_queue.hpp`, `spsc_ring.hpp`). It remains
**mostly modern and clean**: `compliance_scan.py core` reports 0 blocker / 0
warning / 0 note, `stub_scan.py core` reports 0 TODO markers, and the code
uses `std::atomic`, `std::string_view`, `enum class`, `thread_local`, a
magic-static-free RAII pimpl (`Clock`), and `[[nodiscard]]` throughout. The
2026-07-20 pass did not find new idiom-level debt; it found **two doc-vs-code
contradictions**, **one dead-mechanism deletion**, and **one buildable slice
of an overdue tree-wide backlog item (HB-6)** that belongs in `core` because
`core` is the only common ancestor of all 17 lib targets.

Facts that frame this report:

1. **The C-style surface in `log.cpp` is deliberate, not debt.** The fixed
   stack buffer + `std::vsnprintf` / `std::snprintf` / `std::memcpy` and the
   `va_list` variadic API are **constraint-driven**: `log()` / `logf()` are
   `noexcept` and must do **zero heap allocation** on the hot path (callable
   from frame zero, from any thread, from a crash path). `std::format` /
   `std::print` allocate and can throw, so they are disqualified for the sink
   itself; the printf-compatible signature also preserves the legacy prefix the
   game DLLs and users expect. These are recorded below as **doors** (a
   type-safe *wrapper* is possible, the *sink* stays C-shaped), not as
   correctness bugs.

2. **The `strnicmp` / `strncmp` `string_view` over-read pattern is absent.**
   The utilities **M-4** / filesystem **M-7** latent over-read does **not**
   occur in core: a scan of all four TUs plus the public headers finds **zero**
   `strnicmp` / `strncmp` / `strcmp` / `stricmp` call sites. Core does its one
   string comparison need (`level_tag` / `error_code_name` / `thread_role_name`)
   with `switch` over enums, and formats through length-bounded `snprintf`.
   Reported clean — a *negative* data point for the tree-wide sweep, matching
   platform.

3. **`core/thread_role.hpp` and `docs/design/threading-model.md` both
   mischaracterize `assert_main_thread()` as "a thin wrapper" over
   `assert_thread_role(Main)` — it is an independent, lazily-capturing
   mechanism that silently no-ops until first capture (H-1).** The mechanism's
   own header (`private/core/assert_main.hpp:44-55`) documents this
   accurately; three *other* documents assert a rewrite that never happened.
   The live call site this protects (`crash::install_handler`) is silently
   permissive during exactly the startup window it exists to guard.

4. **`core-boundary.md` contradicts itself about `Clock::stats()` in the same
   file** (line 208 correct, lines 224/230 false — M-1). A future G-3 debug
   thread author reading only the wrong rows would write a race into a struct
   that turns out to have zero callers and no reason to exist.

What remains beyond H-1/H-2/M-1..M-3/L-5 is the short, low-urgency tail this
report already carried: two genuinely-deferred "doors" (type-safe logging,
typed frame-gate seam) that are correctly *not* built yet because no consumer
needs them, and two cosmetic tidy-ups.

______________________________________________________________________

## Implementation-status table

| Design element | Status | Notes |
|----------------|--------|-------|
| `Clock` RAII pimpl + atomic observers + `ClockStats` | **Implemented, `ClockStats` half is dead** | Six `std::atomic` timing fields (kept); `ClockStats` mirror + `stats()` accessor have zero callers — **M-1 deletes them** |
| `ThreadRole` registry (`thread_local`, `assert_thread_role`) | **Implemented** | The enforcement primitive; additive enum; `ThreadRole::Render` reserved for Chunk 13, no code changes owed by core |
| `log` / `logf` / `log_va` + `log_set_callback` | **Implemented, one gap** | `noexcept`, zero-heap, atomic callback slot (G-1 hook); `log_set_callback` is undocumented-as-unasserted Main-only — **M-2** |
| `assert_main_thread()` / `capture_main_thread()` (`private/core/assert_main.hpp`) | **Implemented, superseded mechanism** | Independent lazily-capturing check, not a wrapper over `assert_thread_role` despite 3 docs saying so — **H-1 deletes the header** |
| `XASH_PRINTF_FORMAT` portable `[[gnu::format]]` | **Implemented** | D-1 hardening; compiles away on MSVC |
| `ErrorCode` + `error_code_name` | **Implemented** | Expanded past Chunk-2 vocab (generic + Host + Map/BSP) |
| `XASH_ASSERT` / `XASH_FATAL` two-tier macros | **Implemented** | Replaces `<cassert>`; `XASH_FATAL` logs via `core::logf` |
| `core::diagnostics` channel-span aggregator (`DiagSink`/`DiagChannel`/`diagnostics_dump`) | **Not implemented** | **H-2** — the buildable slice of HB-6; tree has 14 stats structs and no aggregator |
| `mpsc_round_up_pow2` / `spsc_round_up_pow2` hand-rolled helpers | **Implemented, redundant** | Two copies of the same loop; `<bit>` `std::bit_ceil` supersedes both — **L-5** |
| Type-safe logging wrapper over the C sink | **Not implemented** | **Door** — L-1; blocked-by-design on the no-heap sink |
| Typed frame-gate seam (vs raw `bool(*)() noexcept`) | **Not implemented** | **Door** — L-2; single cold injection, deferred |
| `emit()` comma-operator newline fixup | **Not implemented** | Cosmetic — L-3 |
| `thread_role.hpp` stale `assert_main.hpp` path comment | **Not implemented** | Doc/comment drift — **superseded by H-1** (was L-4; the header this comment cites is being deleted, not repointed) |
| `strnicmp` / `strncmp` `string_view` over-read | **N/A — absent** | 0 sites; utilities M-4 / filesystem M-7 pattern does not occur here |

______________________________________________________________________

## High-priority opportunities

### H-1: Unify `assert_main_thread()` onto `assert_thread_role(Main)`; delete `private/core/assert_main.hpp`

- **File(s)**: `xash3dpp/include/xash3dpp/private/core/assert_main.hpp`
  (whole file, `capture_main_thread()` / `assert_main_thread()`, lines 31-55);
  `xash3dpp/include/xash3dpp/core/thread_role.hpp:21-23` (comment). The four
  *call sites* live in `platform`, not `core`:
  `xash3dpp/src/platform/{win32,posix}/console.cpp:44/45` (dead —
  `console::read_line` has zero production callers) and
  `xash3dpp/src/platform/{win32,posix}/crash.cpp:56/74` (live —
  `crash::install_handler`); the two `capture_main_thread()` calls sit in
  `xash3dpp/src/platform/{win32,posix}/sys.cpp:53/57`. Those platform-side
  edits belong to `platform-modernization.md`; this entry covers what `core`
  owns — the header, the doc-drift comment, and the deletion decision.

- **Current pattern**: `core` ships two independent "main-thread only" checks.
  `assert_thread_role(ThreadRole::Main)` is the documented enforcement
  primitive (`thread_role.cpp`, `thread_local` role set by an explicit
  `register_thread_role()` call — the first statement of `main()` at
  `launcher/main.cpp:64`). `detail::assert_main_thread()` is a second,
  independent mechanism keyed on a function-local `static std::thread::id`
  populated lazily by `capture_main_thread()` from inside `get_time()`'s magic
  static (`{win32,posix}/sys.cpp:53/57`). Three documents —
  `thread_role.hpp:21-23` ("a thin wrapper kept for existing platform code",
  and citing the wrong path, `private/platform/assert_main.hpp`, for a file
  that actually lives at `private/core/assert_main.hpp`),
  `threading-model.md:11`, and `threading-model.md:81-83` — all assert this
  rewrite already happened. It did not: `assert_main_thread()` calls
  `assert_thread_role()` nowhere, and its own header documents the divergent
  behaviour accurately (`assert_main.hpp:44-55`, "No-op until
  `capture_main_thread()` has been called").

- **Suggested replacement**: delete `include/xash3dpp/private/core/assert_main.hpp`
  outright. Replace the two live call sites (`crash::install_handler` in
  `win32/crash.cpp:56` and `posix/crash.cpp:74`) with
  `::xash::core::assert_thread_role(::xash::core::ThreadRole::Main)`; drop the
  two dead `console::read_line` call sites along with the
  `capture_main_thread()` calls in both `sys.cpp` files. Correct
  `thread_role.hpp:21-23` to stop describing a wrapper that does not exist,
  and fix `threading-model.md:11` / `:81-83` to match.

- **Boundary-safe**: Yes. `core` is not an HB-2 subsystem; no frozen ABI is
  touched; the deletion is confined to a `private/` implementation header with
  four known call sites, all already enumerated.

- **Rationale**: this is a **confirmed defect that removes a real safety
  hazard**, not a style preference — it survived adjudication in the L8
  threading lens. The lazy-capture semantics are strictly worse for the one
  live site: `crash::install_handler` installs a process-wide POSIX
  `sigaction` handler during startup, exactly the window in which
  `capture_main_thread()` may not have fired yet, so the guard silently
  no-ops precisely when it exists to protect. `register_thread_role(Main)`
  runs as the first statement of `main()`, so `assert_thread_role` is armed
  strictly earlier than the lazy capture ever could be. The doc-drift
  liability has already fired in three separate files (`thread_role.hpp`,
  `threading-model.md` ×2), and `thread_role.hpp` additionally cites a
  nonexistent path. This is a **net deletion** — one header, one mechanism,
  and the stale comment it needed — which the audit brief's anti-gold-plating
  and prefer-deletions rules both favour over a comment-only patch. The
  residual cost, stated honestly: after unification `crash::install_handler`
  aborts in any host that never calls `register_thread_role`; that is the
  intended behaviour, and the shipped launcher always registers.
  `[EXT:P-8]` — closes an annotation-discipline gap between three docs and
  the code they describe.

### H-2: Add a channel-span diagnostics aggregator (`core/diagnostics.hpp`) — the buildable slice of HB-6

- **File(s)**: new `xash3dpp/include/xash3dpp/core/diagnostics.hpp` (and a
  matching `.cpp` if `diagnostics_dump`'s loop body warrants one — it can also
  be header-only). Cross-references (not touched by this entry):
  `xash3dpp/docs/design/debug-stats-design.md:359` (the positional sketch this
  proposal replaces) and the host-side assembly, which belongs in
  `host-modernization.md` / the HB-6 backlog item, not here.

- **Current pattern**: 14 stats structs exist tree-wide
  (`cmd_cvar/stats.hpp:22`, `content/content.hpp:63`, `core/clock.hpp:31` —
  see M-1, `filesystem/filesystem.hpp:42`, `host/host.hpp:109`,
  `imagelib/imagelib.hpp:30`, `input/stats.hpp:19`,
  `map_loader/map_loader.hpp:101`, `memory/stats.hpp:14`,
  `networking/delta.hpp:149`, `networking/stats.hpp:17`,
  `private/server/string_pool.hpp:33`, `server/server.hpp:35`,
  `sound/sound.hpp:54`) and **nothing reads any of them**. The
  `debug-stats-design.md:359` sketch —
  `void diagnostics_dump(CmdCvarContext&, MemorySubsystem&, ...) noexcept` —
  is a positional parameter list that grows once per subsystem and, measured
  against `dep_scan.json`, is **unbuildable as written**: `xash3dpp_host`
  links `cmd_cvar`, `core`, `filesystem`, `map_loader`, `memory`,
  `networking`, `platform`, `server` and does **not** link `sound`, `input`,
  `content`, or `imagelib`, so a positional signature naming every stats owner
  would force four new link edges into the composition root purely for a
  debug command — the exact layer inversion the brief forbids, from the
  opposite direction.

- **Suggested replacement**: a POD, fn-pointer channel-span shape that costs
  zero new link edges because `core` is the only common ancestor of all 17 lib
  targets:

  ```cpp
  // core/diagnostics.hpp
  struct DiagSink {
      void (*write)( std::string_view line, void *ud ) noexcept;
      void *ud;
  };
  struct DiagChannel {
      const char *name;
      void (*emit)( const void *self, const DiagSink & ) noexcept;
      const void *self;
  };
  void diagnostics_dump( std::span<const DiagChannel>, const DiagSink & ) noexcept;
  ```

  One parameter that never grows; `alignof <= 8`, no `std::function` (the tree
  has zero), no RTTI, no exceptions, identical on x86 and x64. The composition
  root (`host`) assembles the span from whatever it already links and
  registers a `stats` console built-in through the existing `cmd_add`
  mechanism — the same registration idiom `cmdlist` / `cvarlist` / `hashstats`
  already use (`cmd_cvar/context_init.cpp:191/210/228`). That host-side
  assembly is out of scope for this file (see `host-modernization.md`); this
  entry is only the `core::` primitive it depends on.

- **Boundary-safe**: Yes. `core` is not an HB-2 subsystem, adds no dependency
  edges (every subsystem already links `core`), and the fn-ptr + `void*`
  convention is not new — it is already the tree's callback idiom at 40+
  `cmd_add` sites and at `memory::for_each_pool(void (*)(PoolStats, void*),
  void*)` (`memory/memory.hpp:108`).

- **Rationale**: `debug-stats-design.md:389`'s own trigger ("diagnostics_dump
  overdue at >= 3 stats structs") fired when the tree had 3 structs; it now
  has 14. **Consumer**: `consumer.named_consumer` = "a `stats` console
  built-in registered via `CmdCvarContext::cmd_add`, landed in the same change
  as the aggregator"; `consumer_status` = **exists-in-tree** — the
  registration mechanism and its three siblings (`cmdlist`/`cvarlist`/
  `hashstats`) already ship, `host` already links `cmd_cvar`, so this is not
  speculative infrastructure ahead of a consumer, it is the primitive and its
  first consumer landing together. `[EXT:P-4]` — this is the headline P-4
  typed-introspection-substrate item `core-boundary.md` already claims core
  provides; today it provides three channels informally (logs, `Clock`
  observers, `error_code_name`) with no unifying aggregator.
  `[EXT:G-1]` `[EXT:G-3]` — the sink indirection (`emit` formats into a
  caller-supplied `DiagSink`) means the same channels later serve an MCP text
  frame (G-1) or a G-3 debug thread dump with zero signature change,
  formatting confined to the sink per the no-format-on-hot-path rule.

______________________________________________________________________

## Medium-priority opportunities

### M-1: Delete `ClockStats`, `Clock::stats()`, and the six mirror writes in `tick()`

- **File(s)**: `xash3dpp/include/xash3dpp/core/clock.hpp:30-38` (`ClockStats`
  struct), `:101` (`stats()` declaration); `xash3dpp/src/core/clock.cpp:242-247`
  (field-by-field mirror writes in `tick()`), `:258`
  (`const ClockStats& Clock::stats() const noexcept`); doc:
  `xash3dpp/docs/boundaries/core-boundary.md:208` (correct row), `:224`
  ("the door is already open" — false), `:230` ("returns the by-value
  `ClockStats` snapshot" — false).

- **Current pattern**: `Clock::stats()` hands back `const ClockStats&`
  aliasing `Impl::stats_`, a plain 6-field, zero-atomic struct that `tick()`
  rewrites field-by-field every accepted frame (`clock.cpp:242-247`) — even
  though the same six values are *already* published torn-free by the atomic
  observers two lines below (`clock.cpp:251-256`). `core-boundary.md`
  contradicts itself about this in one file: line 208 says correctly "no
  atomic, so off-main readers observe a possibly-stale-but-consistent-enough
  snapshot"; line 230 says the accessor "returns the by-value `ClockStats`
  snapshot" (false — it is a live reference, `clock.hpp:101`); line 224 tells
  a future G-3 debug-thread author "the door is already open" for `stats()`
  (also false — reading it races the concurrent mirror writes).
  `Clock::stats()` has **zero callers** in `src/`, `tests/`, or `include/`.

- **Suggested replacement**: delete `ClockStats`, the `stats()` declaration
  and accessor, and the six mirror-write lines in `tick()`. Correct
  `core-boundary.md:208/224/230` to point at the six atomic observers
  (`realtime()`, `frametime()`, etc.) as the P-2/P-4 perf channel — the
  surviving surface, which already has callers and is already torn-free.

- **Boundary-safe**: Yes — pure subtraction, zero blast radius (confirmed:
  no callers anywhere in the tree), no ABI, no HB-2 kernel.

- **Rationale**: the original finder proposed this as a High-tier
  signature-only fix ("return `ClockStats` by value"). Adversarial review
  **amended** both the proposal and the tier: a by-value copy would still not
  make cross-thread reads torn-free (the 6-field copy itself is not atomic),
  and every field the struct carries is *already* published torn-free by the
  atomic observers — so fixing the signature preserves a redundant, provably
  inferior mirror instead of removing it. **High is not defensible** for a
  zero-caller accessor whose correct fix is deletion, not repair; this is
  recorded as Medium, bumped one tier from what a pure dead-code deletion
  would otherwise warrant because it also closes a live doc-vs-code
  contradiction that misdirects a future off-main reader.
  `[EXT:G-3]` `[EXT:P-2]` `[EXT:P-4]` — `core-boundary.md:224` explicitly
  frames this as a G-3 (debug thread) door; leaving the contradiction in
  place would have that door open onto a race.

### M-2: Assert `Main` in `log_set_callback()`

- **File(s)**: `xash3dpp/src/core/log.cpp:215-218`;
  `xash3dpp/docs/boundaries/core-boundary.md:189`;
  `xash3dpp/docs/architecture/core/logging.md:138`.

- **Current pattern**: every other documented Main-only mutator in core
  (`Clock::init` / `Clock::shutdown` / `Clock::set_frame_rate_gate`, at
  `clock.cpp:70/109/133`) enforces its contract with
  `assert_thread_role(ThreadRole::Main)`. `log_set_callback()` is documented
  with the identical Main-only contract in two places, including an
  explicitly acknowledged data race if violated, but calls no assert at all:

  ```cpp
  void log_set_callback( LogCallback callback ) noexcept
  {
      g_log_callback.store( callback, std::memory_order_relaxed );
  }
  ```

- **Suggested replacement**: add `assert_thread_role( ThreadRole::Main );` as
  the first line of the function, matching the pattern already established
  two files away in the same subsystem.

- **Boundary-safe**: Yes. Zero behaviour change for every existing caller —
  all 8 non-definition call sites in the tree already run on the main/test
  thread.

- **Rationale**: **UNVERIFIED** in the Phase-2 adversarial pass (flagged, not
  yet counter-checked) — included because it is well-evidenced (a direct
  pattern comparison against three sibling mutators in the same subsystem)
  and cheap to verify before landing. Closes an inconsistency where the
  identical documented contract is enforced on three call sites and not on a
  fourth in the same TU family.

### M-3: Guard `Clock::tick()` against a second caller thread (debug-only)

- **File(s)**: `xash3dpp/src/core/clock.cpp:171-174` (`tick()`, the plain
  `oldtime` / `last_frame_realtime` scalars it mutates); precedent:
  `xash3dpp/include/xash3dpp/core/mpsc_queue.hpp:288-297`
  (`assert_single_consumer()`).

- **Current pattern**: `tick()` carries no `assert_thread_role` call at all —
  deliberately, so tests can drive it from a role-less thread. The mechanism
  used to achieve that (no assert whatsoever) also removes any protection
  against two *different* threads calling `tick()` at different times, which
  would race on the plain, non-atomic `oldtime` / `last_frame_realtime`
  fields (`clock.cpp:50/53`). `ThreadRole::Render` is a reserved,
  currently-unused enum slot (`thread_role.hpp:59`) for a future Chunk 13
  render thread; if a second thread ever calls `tick()`, this is where it
  would first go wrong.

- **Suggested replacement**: reuse the debug-only single-caller-thread
  pattern this same subsystem already ships in `mpsc_queue.hpp` /
  `spsc_ring.hpp` — `compare_exchange` a captured `std::thread::id` on first
  call, `XASH_ASSERT` on a mismatch from a second thread, no-op in `NDEBUG`.
  This preserves the role-less test-stub allowance (no
  `register_thread_role` requirement) while catching an accidental
  second-thread caller in debug builds.

- **Boundary-safe**: Yes — debug-only, `NDEBUG`-gated, matches an existing
  in-tree idiom exactly.

- **Rationale**: **UNVERIFIED** in the Phase-2 pass. Recorded because the
  fix is a copy of a pattern the same subsystem already trusts elsewhere
  (`mpsc_queue.hpp`/`spsc_ring.hpp`), so the risk of introducing it is low
  relative to the gap it closes (silent UB on a second-thread `tick()`
  caller, currently undetectable in any build configuration).

______________________________________________________________________

## Low-priority opportunities

### L-1: Type-safe logging wrapper over the C-style sink (door, not debt)

- **File(s)**: `xash3dpp/include/xash3dpp/core/log.hpp` (`logf` / `log_va`);
  `xash3dpp/src/core/log.cpp`.

- **Current pattern**: the printf-style entry points are classic C varargs:

  ```cpp
  void logf( LogLevel level, std::string_view tag, const char *fmt, ... ) noexcept;
  void log_va( LogLevel level, std::string_view tag, const char *fmt, va_list args ) noexcept;
  ```

  Type-safety today is a compile-time `[[gnu::format]]` check (GCC/Clang only,
  compiled away on MSVC via `XASH_PRINTF_FORMAT`), so MSVC builds get no
  format/argument mismatch diagnostics.

- **Suggested replacement**: add a thin `std::format`-style variadic-template
  **front** (`template<class... A> void logf_fmt(LogLevel, std::string_view,
  std::format_string<A...>, A&&...)`) that formats into a caller-provided or
  stack `std::span<char>` via `std::format_to_n` (bounded, no heap) and then
  hands the finished `string_view` to the existing `log()` sink. The C sink
  stays exactly as-is for the crash / frame-zero path.

- **Boundary-safe**: Yes — additive; existing `logf` remains for callers that
  need raw `va_list` forwarding.

- **Rationale + why deferred**: gives MSVC compile-time format checking and
  removes the `const char*`/`va_list` footgun for *new* call sites. It is
  **not** urgent because `std::format_to_n` must be measured against the
  zero-heap / `noexcept` contract (it can throw on a bad format string, which
  the wrapper must swallow), and no current caller is bitten. Recorded as a
  door: the sink stays C-shaped; only the front becomes type-safe.

### L-2: Typed frame-rate-gate seam instead of a raw function pointer

- **File(s)**: `xash3dpp/include/xash3dpp/core/clock.hpp`
  (`set_frame_rate_gate`); `xash3dpp/src/core/clock.cpp` (`gate_fn`).

- **Current pattern**: the OQ-11 singleplayer-no-demo gate is a bare C function
  pointer stored on `Impl`:

  ```cpp
  void set_frame_rate_gate( bool (*fn)() noexcept ) noexcept;
  // ...
  bool (*gate_fn)() noexcept = nullptr;   // Impl
  ```

- **Suggested replacement**: leave it. If a second injected policy ever appears
  it could become a tiny `IFrameGate` interface (parallel to
  `content::IModelPostProcess`), but with exactly one cold, single-call-site
  consumer a function pointer is the *correct* minimal shape — promoting it now
  would be gold-plating.

- **Boundary-safe**: Yes (if ever taken).

- **Rationale**: documented so the synthesis pass does not mistake the raw
  function pointer for an oversight — it is a deliberate minimal seam. **Door,
  deferred.**

### L-3: Replace the comma-operator newline fixup in `emit()`

- **File(s)**: `xash3dpp/src/core/log.cpp` (`emit`).

- **Current pattern**: the buffer-full branch uses the comma operator:

  ```cpp
  if( end < k_buf - 1 )
      buf[end++] = '\n';
  else
      buf[k_buf - 2] = '\n', end = k_buf - 1;
  ```

- **Suggested replacement**: split into two statements in a braced `else`
  block. Pure readability.

- **Boundary-safe**: Yes.

- **Rationale**: cosmetic; the comma-operator sequence point is correct but
  reads as a typo. Trivial.

### L-4: Correct the stale `assert_main.hpp` path in `thread_role.hpp` — **SUPERSEDED by H-1**

- **Status (2026-07-20): superseded, not resolved.** Still present in code
  (`xash3dpp/include/xash3dpp/core/thread_role.hpp:21-23` still cites
  `<xash3dpp/private/platform/assert_main.hpp>`, the wrong path). Kept here
  for traceability rather than deleted, per this audit's do-not-silently-drop
  rule. **Do not apply the comment-only fix below** — H-1 deletes the whole
  `assert_main.hpp` header this comment points at, which makes a path
  correction moot; H-1's doc fix rewords the comment instead of repointing
  it. Landing this L-4 fix and then H-1 in sequence would touch the same
  three lines twice for no reason — do H-1 directly.

- **File(s)**: `xash3dpp/include/xash3dpp/core/thread_role.hpp:21-23` (header
  comment).

- **Current pattern**: the comment cites the legacy wrapper as
  `<xash3dpp/private/platform/assert_main.hpp>`, but the file actually lives at
  `include/xash3dpp/private/core/assert_main.hpp`.

- **Suggested replacement (original, now superseded)**: fix the path in the
  comment (`platform` → `core`). Superseded because the file it points at is
  being deleted (H-1), not repointed.

- **Boundary-safe**: Yes — comment only, either way.

- **Rationale**: prevents a reader chasing a non-existent path — subsumed by
  H-1's larger fix, which additionally corrects the "thin wrapper"
  mischaracterization this comment also carries.

### L-5: `MpscQueue` / `SpscRing` duplicate a hand-rolled round-up-to-power-of-two helper that `std::bit_ceil` supersedes

- **File(s)**: `xash3dpp/include/xash3dpp/core/mpsc_queue.hpp:57-63`
  (`detail::mpsc_round_up_pow2`, used at `:212`);
  `xash3dpp/include/xash3dpp/core/spsc_ring.hpp:68-74`
  (`detail::spsc_round_up_pow2`, used at `:205`).

- **Current pattern**: two files carry the same hand-rolled bit-twiddling
  loop under two different names, added in the same commit (`bc4de703`,
  S9.7a):

  ```cpp
  [[nodiscard]] inline constexpr std::size_t mpsc_round_up_pow2( std::size_t n ) noexcept
  { /* ... shift-and-or loop ... */ }
  ```

  each used at exactly one call site. This is the classic "no stdlib
  support" pattern — except the stdlib support (C++20 `<bit>`
  `std::bit_ceil`) already exists and the tree is on C++23.

- **Suggested replacement**: delete both private helpers; replace both call
  sites with `std::bit_ceil(Capacity [+ ReserveCapacity])` from `<bit>`.
  `std::bit_ceil(0)` returns `1`, matching the documented "`n == 0` yields
  `1`" contract of both existing helpers, so the swap is behaviour-preserving.

- **Boundary-safe**: Yes — both call sites are `constexpr` capacity
  computations, not runtime hot-path code; no ABI, no HB-2 kernel.

- **Rationale**: removes 14 lines of duplicated logic and the drift risk of
  two copies of the same loop silently diverging the next time either is
  edited. Low tier: purely a currency swap with two call sites, no
  behaviour or shape change, no extension-axis relevance.

______________________________________________________________________

## Out of scope / ABI-frozen

Core exposes **no ABI-frozen symbols** (see header blockquote) — nothing here
is out of scope for that reason. The following are out of scope for
*constraint*, not ABI, reasons and are recorded to prevent churn:

- **Fixed stack buffer + `memcpy` / `vsnprintf` in `log.cpp`.** This is the
  zero-heap, `noexcept`, any-thread, crash-safe sink. `std::string` /
  `std::format` would allocate and can throw — disqualified for the sink. **Do
  not "modernize" the buffer.** (A type-safe *front* is L-1; the *sink* stays.)
- **`va_list` in `log_va`.** Required to forward from C-vararg wrappers
  (`XASH_FATAL`, subsystem `*_printf` shims). Keep.
- **Three parallel `switch`→`const char*` functions** (`level_tag`,
  `error_code_name`, `thread_role_name`). Each maps a *different* enum; a shared
  "enum name" abstraction would be more code than it saves and would obstruct
  the committed integer values. Keep separate.
- **`compliance-allow` on `g_log_callback`.** The single mutable global in core
  is the documented P-3 exception (no context exists at log time). It is an
  atomic, main-write-once. **Correct as-is** (boundary *Extension axes* P-3
  row).
- **Lifting `pool_new<T>`'s `alignof(T) <= 8` limit (HB-7).** Refuted
  tree-wide in this audit — self-declared speculative consumer, no chunk
  number. Recorded here only because core's `mpsc_queue.hpp` /
  `spsc_ring.hpp` family is the house style any future HB-5 primitive would
  have to match, and that primitive is one of HB-7's two named requirers (see
  Open questions below). **Do not lift the limit as part of any core work.**

## Open questions

- **If a shared published-snapshot primitive (HB-5) is ever built, it must
  join the `core::` primitive family `mpsc_queue.hpp` / `spsc_ring.hpp`
  already establish** — this is a **shape constraint, not work**: no
  day-one consumer exists (the tree's one real cross-thread structured
  publisher, `AudioTopology::channel_snapshot` in `sound`, has zero
  production callers; see the tree-wide `L1-publish-primitive` lens).
  Constraints such a primitive must satisfy, for whoever eventually briefs
  it: template on a `static_assert(std::is_trivially_copyable_v<T>)` payload,
  fixed compile-time capacity, zero allocation after construction, explicit
  64-bit cursors (x86 ships), `static_assert(std::atomic<pos_t>::is_always_lock_free)`,
  no atomic wider than 64 bits (no portable DWCAS), demand-driven
  request/ack pull rather than per-frame push, and a POD/string
  two-tier split (string-bearing payloads copy out under a lock instead of
  riding the lock-free path). This is not core's work item today — recorded
  so a future author does not reinvent the house style.
- **Where should the diagnostics-aggregator types live — all in `core`, or
  types in `core` with assembly in `host`?** This report's H-2 already
  answers it for the piece core owns: **types in `core`** (`DiagSink`,
  `DiagChannel`, `diagnostics_dump`), because `core` is the only common
  ancestor of all 17 lib targets and putting them anywhere else costs new
  link edges. The remaining half of the question — where the `stats` console
  command and the per-subsystem `emit` adapters are registered — is `host`'s
  decision and belongs in `host-modernization.md`, not here.
- **Does `Clock::tick()`'s missing thread-caller guard (M-3) matter before
  Chunk 13?** Today core has exactly zero production off-main callers of
  `tick()` — the tree's only two thread spawns are both in `sound`. The
  finding is real (nothing today prevents a second-thread call from racing
  the plain scalars) but its urgency is coupled to the still-open Chunk-12/13
  thread-model decision (does rendering ever move off Main?); if the answer
  stays "no" through Chunk 13, `ThreadRole::Render`'s reservation
  (`thread_role.hpp:59`) stays unused and M-3 stays a hardening item rather
  than a live bug waiting to happen.

## Cross-cutting flags (for the Phase 14 synthesis)

- **`strnicmp` / `strncmp` over-read is ABSENT in core** (0 sites). Second
  negative data point after platform: the utilities-M-4 / filesystem-M-7
  `string_view` → C-string over-read is real but **not universal**. Core and
  platform are the two clean subsystems so far.
- **Core is the P-4 substrate, not a P-4 consumer.** Every synthesis note about
  introspection frontends (MCP tool, overlay, debug thread) should terminate
  at core's *surviving* channels post-M-1/H-2: the six `Clock` atomic
  observers (not `ClockStats`/`stats()` — deleted), the log sinks, and
  `error_code_name`, plus the new `core::diagnostics` channel-span aggregator
  (H-2) once it lands. Flag: the harmonized "one introspection layer" rule
  (extension-goals G-4) should name these explicitly so no frontend grows a
  private backdoor into `Clock::Impl`.
- **`ClockStats` is dead, not merely non-atomic — see M-1.** The prior
  report's note here ("off-main readers get a consistent-enough but not
  lock-free view") is superseded: `Clock::stats()` has zero callers tree-wide,
  so there are no off-main readers to protect, and the correct fix is
  deletion, not a future double-buffer/seqlock. If a debug thread ever needs
  a torn-free multi-field time snapshot, that is new work sized against the
  HB-5 shape constraints above, not a repair of `ClockStats`.
- **Type-safe logging (L-1) is a tree-wide ergonomics door.** Every subsystem's
  `*_printf` shim funnels through `core::logf`; a single type-safe front would
  benefit all of them at once. Worth one line in the synthesis as a shared,
  low-priority ergonomics item (blocked-by-design on the no-heap sink, so it is
  a *front*, never a *sink* rewrite).
- **`assert_main_thread()` / `capture_main_thread()` (H-1) is a tree-wide
  doc-trust flag, not just a core finding.** Three documents in two different
  subsystems' territory (`core/thread_role.hpp`, `docs/design/threading-model.md`
  ×2) independently assert a rewrite that never happened; the mechanism's own
  header is the only accurate account. Worth a line in the synthesis pass as
  a pattern — "the code is the honest party" — not just a one-off fix.
