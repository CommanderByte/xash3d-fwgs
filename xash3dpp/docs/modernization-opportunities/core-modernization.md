# Core Modernization Opportunities

> Authored 2026-07-06 (as-built pass).
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
registry), `error.cpp` (`ErrorCode` → string) — and it is **already modern and
clean**. `compliance_scan.py core` reports **0 blocker / 0 warning / 0 note**,
`stub_scan.py core` reports **0 TODO markers**, and `status_table.py` lists
core **Complete**. The code uses `std::atomic`, `std::string_view`,
`enum class`, `thread_local`, a magic-static-free RAII pimpl (`Clock`), and
`[[nodiscard]]` throughout.

Two facts frame this report:

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

What remains is a short, low-urgency tail: a couple of cosmetic tidy-ups, one
stale header comment, and two genuinely-deferred "doors" (type-safe logging,
typed frame-gate seam) that are correctly *not* built yet because no consumer
needs them.

______________________________________________________________________

## Implementation-status table

| Design element | Status | Notes |
|----------------|--------|-------|
| `Clock` RAII pimpl + atomic observers + `ClockStats` | **Implemented** | Six `std::atomic` timing fields; always-on `stats()` snapshot (P-4) |
| `ThreadRole` registry (`thread_local`, `assert_thread_role`) | **Implemented** | The enforcement primitive; additive enum |
| `log` / `logf` / `log_va` + `log_set_callback` | **Implemented** | `noexcept`, zero-heap, atomic callback slot (G-1 hook) |
| `XASH_PRINTF_FORMAT` portable `[[gnu::format]]` | **Implemented** | D-1 hardening; compiles away on MSVC |
| `ErrorCode` + `error_code_name` | **Implemented** | Expanded past Chunk-2 vocab (generic + Host + Map/BSP) |
| `XASH_ASSERT` / `XASH_FATAL` two-tier macros | **Implemented** | Replaces `<cassert>`; `XASH_FATAL` logs via `core::logf` |
| Type-safe logging wrapper over the C sink | **Not implemented** | **Door** — L-1; blocked-by-design on the no-heap sink |
| Typed frame-gate seam (vs raw `bool(*)() noexcept`) | **Not implemented** | **Door** — L-2; single cold injection, deferred |
| `emit()` comma-operator newline fixup | **Not implemented** | Cosmetic — L-3 |
| `thread_role.hpp` stale `assert_main.hpp` path comment | **Not implemented** | Doc/comment drift — L-4 |
| `strnicmp` / `strncmp` `string_view` over-read | **N/A — absent** | 0 sites; utilities M-4 / filesystem M-7 pattern does not occur here |

______________________________________________________________________

## High-priority opportunities

None. Core has no High or Medium correctness findings — the subsystem is
already at the conformance bar the 6B wave targets.

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

### L-4: Correct the stale `assert_main.hpp` path in `thread_role.hpp`

- **File(s)**: `xash3dpp/include/xash3dpp/core/thread_role.hpp` (header
  comment).

- **Current pattern**: the comment cites the legacy wrapper as
  `<xash3dpp/private/platform/assert_main.hpp>`, but the file actually lives at
  `include/xash3dpp/private/core/assert_main.hpp` (confirmed by `file_search`;
  matches the boundary-spec *Interface* section).

- **Suggested replacement**: fix the path in the comment (`platform` → `core`).

- **Boundary-safe**: Yes — comment only.

- **Rationale**: prevents a reader chasing a non-existent path. Doc-drift only.

______________________________________________________________________

## Deliberately-not-opportunities (constraint-driven, recorded to prevent churn)

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

______________________________________________________________________

## Cross-cutting flags (for the Phase 14 synthesis)

- **`strnicmp` / `strncmp` over-read is ABSENT in core** (0 sites). Second
  negative data point after platform: the utilities-M-4 / filesystem-M-7
  `string_view` → C-string over-read is real but **not universal**. Core and
  platform are the two clean subsystems so far.
- **Core is the P-4 substrate, not a P-4 consumer.** Every synthesis note about
  introspection frontends (MCP tool, overlay, debug thread) terminates at
  `Clock::stats()`, the log sinks, and `error_code_name`. Flag: the harmonized
  "one introspection layer" rule (extension-goals G-4) should name core's three
  channels explicitly so no frontend grows a private backdoor into `Clock::Impl`.
- **`ClockStats` is a plain (non-atomic) snapshot.** Off-main P-2/P-4 readers of
  `stats()` get a consistent-enough but not lock-free view (individual fields
  are copied from atomics on Main, but the struct read is not atomic as a
  whole). If a debug thread ever needs a torn-free snapshot, that is a
  double-buffer / seqlock addition — recorded so the G-3 bring-up sizes it.
- **Type-safe logging (L-1) is a tree-wide ergonomics door.** Every subsystem's
  `*_printf` shim funnels through `core::logf`; a single type-safe front would
  benefit all of them at once. Worth one line in the synthesis as a shared,
  low-priority ergonomics item (blocked-by-design on the no-heap sink, so it is
  a *front*, never a *sink* rewrite).
