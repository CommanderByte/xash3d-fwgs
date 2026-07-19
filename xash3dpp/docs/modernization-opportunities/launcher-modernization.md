# Launcher Modernization Opportunities

> C++ standard in use: C++**23** (from `xash3dpp/CMakeLists.txt`,
> `CMAKE_CXX_STANDARD 23`; the `xash3dpp` launcher target itself pins
> `cxx_std_23` in `src/launcher/CMakeLists.txt`).
> Boundary spec: `docs/boundaries/launcher-boundary.md`
> Threading analysis: inline in the boundary spec (`## Threading`) — the
> launcher has no dedicated threading doc (pre-thread bootstrap).
> ABI-frozen symbols in this subsystem: **None** of its own. On Windows the
> `.exe` is the required home for the NVIDIA Optimus / AMD PowerXpress export
> symbols (frozen *names*, not yet defined here — see the boundary As-built
> reconciliation); the OS entry signatures (`WinMain` / `main`, `char** argv`)
> are the platform ABI and are immovable.

*Authored 2026-07-06 (as-built pass). Scope: the one shipped TU
(`src/launcher/main.cpp`, ~95 lines — argv→`HostArgs` bootstrap) + its
`CMakeLists.txt`. **Verdict: the launcher is already modern C++23 and mostly
door-neutral; the honest tail is tiny.** Most of the "C-looking" surface here
is load-bearing (the OS entry point deals in `int argc, char** argv` — that IS
the platform ABI, not a smell). The `strnicmp`/`strncmp` `string_view`→C-string
over-read pattern tracked in utilities (M-4) / filesystem (M-7) / cmd_cvar (M-5)
is **ABSENT** here (0 sites — the two argv helpers use `std::strcmp` over the
raw `char** argv`, which are genuine NUL-terminated C strings, never a
`string_view`) — a negative data point for that sweep.*

## Summary

`src/launcher/main.cpp` is a thin bootstrap: register the Main thread role, scan
argv into a stack `HostArgs`, construct `Host`, and return `Host::Main`. It
already uses `noexcept` pure helpers, no heap, no getopt, and `std::string`
only where the platform header path demands it. There is essentially nothing to
"modernise" — the raw `char**` is the OS contract and the `std::strcmp` argv
scan is the correct, allocation-free idiom for it.

What remains is a two-item low-priority tail (one locale-free numeric parse, one
cosmetic helper-signature note) plus a **completeness** gap that is not a
modernization item (the Optimus/PowerXpress exports are documented as living
here but are not yet defined) and a **testing** recommendation (no
`tests/launcher/` unit suite). Both are recorded in their own sections so a
future reader does not mistake them for C-isms to refactor.

______________________________________________________________________

## High-priority opportunities

*(none)* — no High-tier C-ism or door-closing pattern exists. The launcher owns
no file-scope state (P-3 held trivially), exposes no seam, and has no off-main
surface.

______________________________________________________________________

## Medium-priority opportunities

*(none)* — the argv scan, the `HostArgs` fill, and the static link are all
already the intended shape. Anything that *looks* like a Medium item is either
the OS entry ABI (out of scope) or the flavor/mode-select door tracked in the
boundary spec's `## Extension axes (Q-21)` (a preservation door, not a refactor).

______________________________________________________________________

## Low-priority opportunities

### L-1: `-dev` level via `std::atoi` → `std::from_chars` `[robustness]`

- **File(s)**: `xash3dpp/src/launcher/main.cpp` — the `-dev` branch
  (`args.developer = std::atoi(dev)`).

- **Current pattern**:

  ```cpp
  if (const char* dev = get_arg(argc, argv, "-dev", nullptr))
      args.developer = std::atoi(dev);
  ```

  `std::atoi` is locale-influenced, silently returns `0` on a non-numeric
  argument, and has undefined behaviour on overflow.

- **Suggested replacement**: parse with `std::from_chars` (locale-free,
  error-checked, no UB): on failure leave `args.developer` at its default rather
  than silently coercing to `0`. Cosmetic robustness only — the input is a
  developer-supplied flag, so the blast radius is nil.

- **Boundary-safe**: Yes — no signature or ABI change; `<cstdlib>` include for
  `atoi` can drop if `from_chars` replaces the only user.

- **Rationale**: Low urgency. `atoi` here is idiomatic-legacy and harmless in
  practice; flagged only for the locale/UB hygiene it removes.

### L-2: `get_arg` / `has_flag` keys could be `std::string_view` `[cosmetic]`

- **File(s)**: `xash3dpp/src/launcher/main.cpp` — the two `static` argv helpers.

- **Current pattern**: both take `const char* key` / `const char* flag` and
  compare with `std::strcmp` against `argv[i]` (a `char*`).

- **Suggested replacement**: *marginal.* The callers always pass string
  literals, and `argv[i]` is a genuine C string, so `std::strcmp` is already the
  right, allocation-free tool. A `string_view` key would force either a
  bounded compare against a possibly-non-terminated buffer (introducing the very
  over-read class this subsystem is **free of**) or a re-materialisation to a C
  string — no benefit. **Recorded as a deliberate non-change** so a future
  "modernise to `string_view`" sweep does not import the over-read pattern here.

- **Boundary-safe**: Yes.

- **Rationale**: Explicitly **not** worth doing — the C-string compare is
  correct precisely because `argv` is NUL-terminated. Kept as a documented
  no-op.

______________________________________________________________________

## Deliberately NOT opportunities (recorded so they are not "fixed")

- **`int argc, char** argv` / `WinMain(HINSTANCE, …)`** — the OS entry-point
  signatures. `char**` is the platform ABI; it cannot become `std::span` or a
  vector at the boundary.
- **`__argc` / `__argv` on Win32** — the CRT-provided argv; using them (instead
  of re-parsing `GetCommandLineW`) is the intended minimal shape.
- **`std::strcmp` over `argv`** — correct because `argv[i]` is NUL-terminated;
  see L-2. Not a `string_view` candidate.
- **`WIN32_LEAN_AND_MEAN` + `<windows.h>`** — required for the `WinMain` entry;
  not bloat.
- **Static link to `xash3dpp_host`** — the deliberate dissolution of the legacy
  `LoadLibrary("xash.dll")` / `GetProcAddress("Host_Main")` DLL boundary
  (host-boundary.md § External ABI contracts). Not a "missing dynamic load".

______________________________________________________________________

## Completeness gaps (not modernization — port items)

### C-1: NVIDIA Optimus / AMD PowerXpress export symbols not yet defined

- **File(s)**: `xash3dpp/src/launcher/main.cpp` (Win32 path).

- **Gap**: the boundary spec (ABI / Quirks) and `host-boundary.md` both name the
  launcher `.exe` as the required home for the `dllexport`
  `NvOptimusEnablement` / `AmdPowerXpressRequestHighPerformance` symbols
  (legacy `game_launch/game.cpp:35`). They are **not** present in `main.cpp`
  yet. This is missing porting work, not a C-ism — recorded here so it is not
  assumed done. (Must stay exported symbols of the executable to have effect.)

______________________________________________________________________

## Testing (recommendation — not a modernization item)

There is deliberately **no** `tests/launcher/` unit suite; the argv→`HostArgs`
behaviour is covered end-to-end by `tests/host/test_host.cpp` (exercising
`HostArgs`/`Host::Main`) and the server hl-smoke integration test (boundary spec
§ Tests — a recorded judgment, not missing coverage).

**Recommendation (low priority):** *if* the argv surface grows (a flavor/mode
switch per the Q-21 door, or non-trivial `-basedir`/`-rodir` resolution), a
small pure-function unit test over `get_arg` / `has_flag` and the resulting
`HostArgs` fill would be cheap and would pin the parsing contract without
booting the engine. Until then, the end-to-end coverage is the right call for a
state-free bootstrap.
