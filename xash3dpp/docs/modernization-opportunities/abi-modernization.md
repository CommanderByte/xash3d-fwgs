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
`include/xash3dpp/abi/` (`abi_types.hpp`, `edict.hpp`, `eiface.hpp`,
`engine_context_accessor.hpp`, `entity_state.hpp`, `event_args.hpp`,
`event_state.hpp`, `pm_defs.hpp`, `pm_movevars.hpp`, `server_consts.hpp`,
`sound_api.hpp`, `usercmd.hpp`, `weaponinfo.hpp`). **Verdict: `abi` is
deliberately, load-bearingly C-shaped — most "C-isms" here are the frozen ABI
and MUST NOT be "modernised".** The findings below are a very short tail: they
are constrained to the shim body and the build metadata, never the vendored
declarations. The `strnicmp`/`strncmp` `string_view` over-read pattern tracked
in utilities (M-4) / filesystem (M-7) / cmd_cvar (M-5) is **ABSENT** here (0
sites — the shim does only `va_list` formatting, no string comparison) — a
negative data point for that sweep.*
>
> **Refresh (2026-07-20, tree-wide modernization audit):** verified every
> claim below against HEAD `cc73c054`; nothing in the shim body changed.
> Three updates from the Phase-2/Phase-3 adversarial passes: (1) the scope
> list above now names `sound_api.hpp` (added by commit `72293dc7`, S9.1,
> after this report's authoring date) — reviewed and carries the same
> "frozen vendored mirror, zero production consumers" verdict as the rest,
> so the summary's verdict is unchanged, only its file list was stale
> (digest F170, CONFIRMED). (2) M-1 and L-1 were independently re-verified
> byte-for-byte unchanged (digest F167 CONFIRMED, F168 UNVERIFIED-but-intact).
> (3) O-1's disposition changed from "left as-is, optional" to "recommended,
> zero-risk" — a fresh `dep_scan.py` run in the L5-topology-compat lens
> confirms the `cxx_std_20` residue is harmless *and* mechanically safe to
> clear (digest F169; lens recommendation L5-4). No new High/Medium/Low
> findings were added; the tier ceiling stays M-1 / L-1.

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

- **File(s)**: `xash3dpp/src/abi/engine_funcs.cpp:64-67` (the `va_start`/
  `va_end` block feeding `core::log_va`) and `:73-74` (the fixed-literal
  detail string forwarded to `signal_frame_abort`).

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

- **Status (2026-07-20)**: Re-verified byte-for-byte unchanged since
  2026-07-06 (digest F167, CONFIRMED against an adversarial refutation
  attempt). Still no second direct-export shim exists in this TU, so the
  factoring trigger has not fired — remains deferred, not actioned.

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

- **Status (2026-07-20)**: Re-verified — both comments still read exactly as
  quoted (digest F168). No new shim has been added to this TU, so the watch
  item has not yet been triggered; kept open.

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
  `CMAKE_CXX_STANDARD 23`. It is harmless in practice (the actual compile
  still uses the project standard).

- **Suggested replacement (2026-07-20, upgraded from "left as-is")**: bump
  `xash3dpp_abi` to `cxx_std_23`, matching the other 18 of 20 CMake targets.
  `layer-model.md:63` already claims the 6B wave normalized every target to
  `cxx_std_23`; a fresh `dep_scan.py` run (L5-topology-compat lens,
  2026-07-20) shows that claim is off by two — `xash3dpp_abi`
  (`src/abi/CMakeLists.txt:14`) and `xash3dpp_host`
  (`src/host/CMakeLists.txt:11`) are the only two targets still on
  `cxx_std_20`. This is a one-line, behaviour-neutral edit (the root
  `CMAKE_CXX_STANDARD 23` already wins at compile time — digest F169), so it
  is recorded here as recommended rather than optional. **Coordinated
  finding**: the matching `xash3dpp_host` line is out of this report's scope
  (see `host-modernization.md`) — both lines should land together so
  `layer-model.md:63`'s normalization claim becomes true rather than merely
  less false.

- **Boundary-safe**: Yes.

- **Rationale**: Consistency note that graduated to a trivial, zero-risk fix
  once independently re-verified this pass (digest F169; lens
  recommendation L5-4, effort S). Still cosmetic — no behavioural or ABI
  effect — so it stays in this section rather than being promoted a tier.

______________________________________________________________________

## Out of scope / ABI-frozen

Per the header blockquote: everything under `include/xash3dpp/abi/` (all 13
vendored headers listed there, including `sound_api.hpp` as of this refresh)
plus `engine/eiface.h` / `engine/edict.h` / `engine/cdll_int.h` /
`engine/cdll_exp.h` / `pm_shared/**` at the repository root. See "Deliberately
NOT opportunities" above for the specific per-pattern reasoning (LLP64
widths, raw cross-link pointers, missing `[[nodiscard]]` on fn-ptr table
slots, `std::FILE*`, the `Host_Error` `va_list`, `std::abort()` on the
no-context path, and the `k_*` ABI/wire constants). Nothing new was added to
this list in the 2026-07-20 pass.

______________________________________________________________________

## Open questions

- **Q-20 compliance-scan rule, when it lands, must keep `src/abi/` as a
  named-exempt raw-access owner.** The L5-topology-compat lens (2026-07-20)
  confirms Q-20's promised compliance-scan rule for raw `entvars_t`/`edict_t`
  field access (`decisions-architecture.md:898`) still does not exist in
  `xtools/rules.py` or `compliance_scan.py`, and recommends building it
  (L5-2b) scoped to flag access **outside** three named owners: `src/abi/`,
  the pmove bridge, and the Chunk 8 save serializer. This report has no
  action item today (`engine_funcs.cpp` does no raw `entvars_t`/`edict_t`
  field access — it is a `va_list` shim, not a compat-layer consumer) but
  whoever implements L5-2b must confirm `src/abi/`'s exemption is scoped
  correctly rather than accidentally flagging this TU. Owner: server/Q-20,
  not abi — tracked here only as a downstream watch item.
- **Should `M-1`'s bounded-formatter factoring be pulled forward, or does it
  stay gated on "a second direct-export shim lands"?** Unchanged question
  from the 2026-07-06 authoring; still no second shim exists (2026-07-20
  re-check), so the gate has not fired. No action needed until it does.
