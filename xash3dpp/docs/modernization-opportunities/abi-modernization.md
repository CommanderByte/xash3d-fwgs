# ABI Modernization Opportunities

> C++ standard in use: C++**23** (from `xash3dpp/CMakeLists.txt`,
> `CMAKE_CXX_STANDARD 23`; the `xash3dpp_abi` target itself pins
> `cxx_std_20` in `src/abi/CMakeLists.txt` — see O-1 below).
> Boundary spec: `docs/boundaries/abi-boundary.md`
> Threading analysis: `docs/threading-analysis/abi-threading.md`
> ABI-frozen symbols in this subsystem: **everything** — this is the vendored
> frozen-ABI layer. `Host_Error` (`engine/eiface.h` `pfnHostError`, a
> `GAME_EXPORT` direct symbol) plus the full vendored `enginefuncs_t` (159) /
> `DLL_FUNCTIONS` (50) / `NEW_DLL_FUNCTIONS` (5) declarations and their support
> PODs. Their shape, name, calling convention, field order, and slot count are
> **immovable** (eiface.h:286 "INTERFACE VERSION IS FROZEN AT 138").

*Authored 2026-07-06 (as-built pass). Scope: the one shipped TU
(`src/abi/engine_funcs.cpp`, the `Host_Error` shim) + the vendored headers under
`include/xash3dpp/abi/`. **Verdict: `abi` is deliberately, load-bearingly
C-shaped — most "C-isms" here are the frozen ABI and MUST NOT be
"modernised".** The findings below are a very short tail: they are constrained
to the shim body and the build metadata, never the vendored declarations. The
`strnicmp`/`strncmp` `string_view` over-read pattern tracked in utilities (M-4)
/ filesystem (M-7) / cmd_cvar (M-5) is **ABSENT** here (0 sites — the shim does
only `va_list` formatting, no string comparison) — a negative data point for
that sweep.*

## Summary

`abi` has two parts, and each resists modernization for a *different* reason:

1. **The vendored declarations** (`edict.hpp`, `eiface.hpp`, `pm_defs.hpp`,
   `entity_state.hpp`, `usercmd.hpp`, `event_*.hpp`, `abi_types.hpp`, …) are
   **byte-exact layout mirrors** of the frozen GoldSrc/Xash SDK. The LLP64-era
   `unsigned long` / `long cb` widths, the raw cross-link pointers, the
   function-pointer tables, the `CRC32_t = unsigned int` typedef, and the
   `0x80000000u` fenttable flags are **the ABI** — `static_assert`ed against the
   real headers on 32- and 64-bit targets. Every "C-ism" here is load-bearing;
   changing any of it is a silent gameplay/save/wire divergence. These are
   **not** modernization candidates and are explicitly out of scope.
2. **The `extern "C"` bridge** (`engine_funcs.cpp`) is already idiomatic modern
   C++: it uses `<cstdarg>` only where the frozen `...` signature forces it,
   routes diagnostics through typed `core::log*` (never raw stdio), reaches the
   context through the typed `current_engine_context()` accessor, and forwards
   to the typed `signal_frame_abort(core::ErrorCode, std::string_view)`. There
   is essentially nothing to clean up.

So the honest tail is tiny. It is recorded below with an explicit
"deliberately-not-an-opportunity" section so a future reader does not
"modernise" the frozen layer.

______________________________________________________________________

## High-priority opportunities

*(none)* — no High-tier C-ism or door-closing pattern exists in the shippable
surface. Anything that *looks* like one is either the frozen ABI (out of scope)
or is tracked in the threading/boundary docs (the accessor relocation, the
static-return-buffer contract) rather than here, to avoid double-counting.

______________________________________________________________________

## Medium-priority opportunities

### M-1: `Host_Error` varargs → a bounded typed formatter shim `[EXT:P-3]`

- **File(s)**: `xash3dpp/src/abi/engine_funcs.cpp` — the `Host_Error`
  `va_start`/`va_end` block feeding `core::log_va`.

- **Current pattern**:

  ```cpp
  std::va_list ap;
  va_start( ap, fmt );
  xash::core::log_va( xash::core::LogLevel::Fatal, "host", fmt, ap );
  va_end( ap );
  ```

  The frozen `Host_Error(const char *fmt, ...)` signature **forces** the `...`
  and therefore the `va_list` at the boundary — that part is immovable. But the
  *forwarded* detail string handed to `signal_frame_abort` is today a fixed
  literal (`"game DLL invoked Host_Error"`); the human-readable text only
  reaches the log, not the frame-abort record.

- **Suggested replacement**: *iff* it ever becomes useful to carry the formatted
  text into the abort record, format once into a bounded stack buffer (reusing
  the same `copy_truncated(std::span<char>, std::string_view)` helper proposed
  in `host-modernization.md` M-1) and pass a `std::string_view` of it to
  `signal_frame_abort`. This keeps one bounded-copy idiom across the abort path
  and removes the "text lives only in the log" split. **No change to the frozen
  signature** — the `va_list` stays; only the post-format handoff changes.

- **Boundary-safe**: Yes — the `extern "C"` signature is untouched; this is
  purely how the *already-formatted* payload is forwarded internally.

- **Rationale**: Cosmetic + diagnostics fidelity, low urgency. Tagged
  `[EXT:P-3]` only weakly (keeping fixed-buffer copies uniform helps the eventual
  no-heap abort path stay auditable). Explicitly deferred until a second
  direct-export shim lands here and the pattern is worth factoring.

______________________________________________________________________

## Low-priority opportunities

### L-1: Stale ownership comment could over-claim after the OQ-10 relocation `[doc]`

- **File(s)**: `xash3dpp/src/abi/engine_funcs.cpp` header comment;
  `src/abi/CMakeLists.txt` comment block.

- **Current pattern**: The TU + CMake comments already correctly state that the
  accessor *state* lives in `xash3dpp_host` and that `abi` only consumes it
  (D-1, 2026-07-06). This is accurate as-built — recorded here only so the note
  stays in sync if the shim family grows; any new shim added to this TU must
  keep the "reads the host-owned accessor, owns no global" framing.

- **Suggested replacement**: none required today — the comments are correct.
  Flagged as a **watch item** so a future author does not reintroduce
  abi-owned accessor state (which would re-create the `host ⇄ abi` cycle D-1
  removed).

- **Boundary-safe**: Yes.

- **Rationale**: Pure doc-hygiene / cycle-prevention pointer, not a refactor.

______________________________________________________________________

## Deliberately NOT opportunities (recorded so they are not "fixed")

These are frozen-ABI or by-design and must **not** be modernised:

- **LLP64-era widths** (`unsigned long`, `long cb`, `unsigned int` in fn-ptr
  signatures) — the signature *is* the ABI (eiface.h verbatim). `compliance-allow(int-width)`.
- **Raw cross-link pointers** in `edict_t` / `link_t` / `enginefuncs_t` slots —
  the game DLL does byte-offset arithmetic over these (Q-20 EDICT_STORE); they
  are not "owning-pointer smells".
- **Function-pointer *table* members without `[[nodiscard]]`** — `[[nodiscard]]`
  is not applicable to a frozen table slot (`compliance-allow(nodiscard-missing)`).
- **`std::FILE*` in `pfnEngineFprintf`** — frozen export
  (`compliance-allow(abi-file-io)`); not to be replaced with an iostream/typed
  sink.
- **The `va_list` in `Host_Error`** — the `...` is part of the frozen C
  signature; it cannot become a variadic template or `std::format` at the
  boundary (only the post-format handoff can change — see M-1).
- **`std::abort()` in the no-context fall-through** — deliberate: preserves the
  game-DLL `Host_Error` "this call never returns on fatal" contract when there
  is no live context to abort a frame on.
- **The `k_*` ABI/wire constants living beside the ABI (not in `limits.hpp`)** —
  QO: these are frozen, non-tunable, and belong with the declarations they
  describe.

______________________________________________________________________

## Other / build-metadata

### O-1: `xash3dpp_abi` target pins `cxx_std_20` while the project is C++23 `[consistency]`

- **File(s)**: `xash3dpp/src/abi/CMakeLists.txt`
  (`target_compile_features(xash3dpp_abi PUBLIC cxx_std_20)`).

- **Current pattern**: the vendored-ABI target requests `cxx_std_20` as its
  **minimum** public feature level, whereas the top-level project sets
  `CMAKE_CXX_STANDARD 23`. This is deliberate — the vendored declarations are
  intentionally low-requirement so `server`/`client`/tests can include them
  broadly — and it is harmless (the actual compile still uses the project
  standard). Recorded only so the C++20-vs-23 delta is not mistaken for a
  mistake.

- **Suggested replacement**: none required. Optionally align to `cxx_std_23` for
  uniformity *iff* a vendored header ever adopts a C++23-only construct (none
  do today — the headers are deliberately conservative). Left as-is.

- **Boundary-safe**: Yes.

- **Rationale**: Consistency note, not a defect. The lower floor is a feature
  (broad includability), not debt.
