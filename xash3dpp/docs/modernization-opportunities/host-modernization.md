# Host Modernization Opportunities

> C++ standard in use: C++**23** (from `xash3dpp/CMakeLists.txt`, `CMAKE_CXX_STANDARD 23`)
> Boundary spec: `docs/boundaries/host-boundary.md`
> Threading analysis: `docs/threading-analysis/host-threading.md`
> ABI-frozen symbols reachable through this subsystem: **`Host_Error`**
> (`engine/eiface.h` `pfnHostError`; a `GAME_EXPORT` direct-symbol export). The
> C signature `void Host_Error(const char *fmt, ...)` is frozen and lives in the
> `xash3dpp_abi` shim, not here — host exposes only the typed
> `signal_frame_abort(core::ErrorCode, std::string_view)` sink it routes to.
> The launcher↔engine `Host_Main`/`Host_Shutdown` symbols are an internal
> contract the rewrite folds away (boundary spec §External ABI) — not frozen.

*Authored 2026-07-06 (as-built pass). Scope: the three host TUs
(`host.cpp`, `engine_context.cpp`, `engine_context_accessor.cpp`) + the two
public headers. **Verdict: host is already idiomatic modern C++23** — pimpl,
injected non-owning deps, `std::array`/`std::string_view`, `enum class`
lifecycle states, `std::atomic` accessor, `noexcept` teardown, zero raw owning
pointers. The findings below are a short tail: one small fixed-buffer helper,
one already-tracked ergonomics deferral, and a cluster of items that are
**missing code** (cvar/command registration, recovery bodies) rather than
C-style code to modernize. The string_view→C-string `strnicmp`/`strncmp`
over-read pattern tracked in utilities (M-4) / filesystem (M-7) / cmd_cvar (M-5)
is **ABSENT** here (0 sites) — a negative data point for that sweep.*

## Summary

The host subsystem is the engine's lifecycle orchestrator: it is deliberately
**free-functions/orchestrator-shaped over an aggregate** (`EngineContext`), which
Q-22 explicitly sanctions for frame loops and lifecycle sequencing — so the
usual "promote to RAII class" modernization does not apply to `EngineContext`
itself (it is already the canonical flat-owner). `Host` is a pimpl class with an
RAII `Impl`, matching the P-7 idiom for the one piece of host that carries
invariants (the pool + frame-abort state).

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

______________________________________________________________________

## High-priority opportunities

*(none)* — no High-tier C-ism or door-closing pattern was found. The one item
that would normally rank High under the extension-goals promotion rule (the
missing `assert_thread_role` on `RunFrame`/`RequestShutdown`/`signal_frame_abort`)
is a **threading/annotation-conformance** fix, tracked in
`host-threading.md` Recommendation 1 and P-8, not a modernization refactor —
it is recorded there to avoid double-counting.

______________________________________________________________________

## Medium-priority opportunities

### M-1: `frame_abort_detail` manual `std::array` + `memcpy` + null-terminate → a bounded fixed-string copy `[EXT:P-3]`

- **File(s)**: `xash3dpp/src/host/host.cpp` — `Host::Impl::frame_abort_detail`
  (`std::array<char, limits::host_frame_abort_detail_buf>`) and the copy in
  `signal_frame_abort` (lines ~271–279).

- **Current pattern**:

  ```cpp
  std::array<char, ::xash::limits::host_frame_abort_detail_buf> frame_abort_detail {};
  // ...
  const std::size_t n = detail.size() < s.frame_abort_detail.size() - 1
                        ? detail.size()
                        : s.frame_abort_detail.size() - 1;
  std::memcpy( s.frame_abort_detail.data(), detail.data(), n );
  s.frame_abort_detail[n] = '\0';
  ```

  The copy is **correct and bounded** (no over-read — this is the negative data
  point vs the utilities/fs/cmd_cvar sweep), but the min-clamp + `memcpy` +
  explicit terminator is hand-rolled at the one call site.

- **Suggested replacement**: a tiny header-only `copy_truncated(std::span<char>,
  std::string_view)` helper (or reuse `utilities::string` `strncpy`-mirror if one
  already covers `std::span`) so the clamp/terminate logic is named and tested
  once. No behaviour change; removes the only raw `memcpy` in the subsystem.

- **Boundary-safe**: Yes — engine-internal, single call site.

- **Rationale**: Cosmetic; the value is one fewer hand-rolled bounded copy to
  audit. Low urgency because the current code is already safe. Tagged `[EXT:P-3]`
  only in the weak sense that keeping fixed-buffer copies uniform helps the
  eventual no-heap crash/abort path stay auditable.

______________________________________________________________________

## Low-priority opportunities

### L-1: `Host::Impl::fs()` owned-vs-injected filesystem selector `[cosmetic]`

- **File(s)**: `xash3dpp/src/host/host.cpp` — `Impl::ext_fs` + `own_fs` +
  `fs()` returns `ext_fs ? *ext_fs : own_fs`.

- **Current pattern**: two members (`Filesystem *ext_fs` and
  `Filesystem own_fs`) with a ternary accessor encode the standalone-vs-injected
  duality. This is clear and correct, but the "own it only when not injected"
  invariant is spread across `init` (conditional `own_fs.init`), `shutdown`
  (`if (!ext_fs) own_fs.shutdown()`), and `fs()`.

- **Suggested replacement**: none required — noting it as a candidate for a
  small `MaybeOwned<Filesystem>` wrapper *iff* the same own-or-injected pattern
  recurs in another subsystem (it does not today). Left as-is; documented so a
  future reader does not mistake the dual members for a bug.

- **Boundary-safe**: Yes.

- **Rationale**: Deliberately *not* an action item — recorded to prevent a
  well-meaning "simplify" that would break the standalone `Host::Main` path
  (which has no `EngineContext` and must own its FS).

### L-2: stale/aspirational doc-comment cvar rosters `[doc]`

- **File(s)**: `xash3dpp/src/host/host.cpp` — the `TODO Chunk 3` block listing
  `host_developer`, `host_gameloaded`, `host_clientloaded`, … and the
  `quit`/`exit`/`memlist`/`host_error` commands.

- **Current pattern**: host registers **no** lifecycle cvars or commands yet;
  the boundary-spec cvar tables and these inline TODOs are aspirational.

- **Suggested replacement**: not a code change — flagged so the modernization
  reader knows the "Cvars owned by host" tables in the boundary spec are
  **not-yet-implemented** targets, not as-built state. (Cross-referenced in the
  refreshed boundary spec.)

- **Boundary-safe**: Yes.

- **Rationale**: Missing code, not C-style code — kept here only as a status
  pointer.

______________________________________________________________________

## Deliberately-not-opportunities

These look modernizable but must stay as they are:

- **`EngineContext` as a flat struct of public members** — Q-22 reserves the
  free-functions-over-aggregate / flat-owner shape for orchestrators;
  `EngineContext` is the canonical example. Do **not** wrap its members behind
  getters or promote it to a pimpl.
- **Non-owning raw dependency pointers in `HostInitParams`/`EngineContextInitParams`** —
  the Q-4 DI-params model; nullable = standalone/test. Not owning-pointer smell.
- **The `Host_Main` / `Host_Shutdown` / `pfnChangeGame` launcher symbols** — an
  internal launcher↔engine contract the rewrite folds into one executable
  (boundary spec §External ABI); not a public ABI to preserve, and not present
  in the current host TUs (the launcher owns them).
- **`realtime()` falling back to `platform::get_time()` when `clock == nullptr`** —
  the standalone/test path has no Clock; the fallback is intentional, not a
  missing-injection bug.

______________________________________________________________________

## Implementation gaps (missing code, not modernization)

Recorded for completeness; these are Chunk-6/12 work, not C→C++ refactors:

- Host lifecycle cvar + command registration (L-2).
- `RunFrame` frame-abort recovery body (`SV_Shutdown`/`CL_Drop`/
  `CL_ClearEdicts`/`Mod_FreeAll` are `TODO Chunk 6/12`).
- Dedicated-server stdin pump (OQ-9 `platform::console::poll_line` wrapper).
- Server/client per-frame dispatch (`TODO Chunk 6/12`).
- `Filesystem::init` → `FilesystemInitParams` ergonomics — **already tracked**
  as DEFER-with-owner in the boundary spec (ripples into ~24 filesystem test
  call sites; owned by a future filesystem-ergonomics slice).
