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

*Refresh 2026-07-20 (tree-wide modernization audit, 16 subsystems in
parallel): the idiom-level read is unchanged — still zero High-tier C-isms in
the argv scan itself — but the campaign's fact-base corrections
(`CORRECTIONS.md`) surfaced a **confirmed, adversarially-verified defect**
that lives at this file's own call site and was missing from the 2026-07-06
pass entirely: `main.cpp:90-91` never wires a real `EngineContext` before
calling `Host::Main`, so every dependency pointer `Host::Main` builds is null
(`host.cpp:321`) and the shipped binary always runs "standalone." This is now
**H-1** — the report's first-ever High-priority item — because it sits on the
P-3 door and is a named blocker for Chunk 12/13. `L-1`/`L-2` line citations
were tightened against the current file; a third Low item (`L-3`, a stale
include comment) was added; the Testing section was corrected to name the one
test file that actually exercises a wired `EngineContext` (it is not
`test_host.cpp`).*

## Summary

`src/launcher/main.cpp` is a thin bootstrap: register the Main thread role, scan
argv into a stack `HostArgs`, construct `Host`, and return `Host::Main`. It
already uses `noexcept` pure helpers, no heap, no getopt, and `std::string`
only where the platform header path demands it. There is essentially nothing to
"modernise" — the raw `char**` is the OS contract and the `std::strcmp` argv
scan is the correct, allocation-free idiom for it.

What remains on the pure-idiom axis is a three-item low-priority tail (one
locale-free numeric parse, one cosmetic helper-signature note, one stale
include comment) plus a **completeness** gap that is not a modernization item
(the Optimus/PowerXpress exports are documented as living here but are not
yet defined) and a **testing** recommendation (no `tests/launcher/` unit
suite). Those are recorded in their own sections so a future reader does not
mistake them for C-isms to refactor.

Separately, this refresh adds one **High**-priority item that is not an idiom
finding at all: the shipped binary's only `Host` construction call
(`main.cpp:90-91`) never builds or threads through a real `EngineContext`, so
`Host::Main` always runs with every dependency pointer null. This is a
confirmed wiring defect, not a C-ism — it earns its High tier because it sits
directly on the P-3 (`EngineContext`) door and is a named precondition for
Chunk 12/13 (see H-1).

______________________________________________________________________

## High-priority opportunities

### H-1: Shipped `Host` construction never wires a real `EngineContext` — every dependency pointer is null `[EXT:P-3][EXT:G-2]`

- **File(s)**: `xash3dpp/src/launcher/main.cpp:90-91` (`xash::Host host;
  return host.Main(args);`); `xash3dpp/src/host/host.cpp:309-321`
  (`Host::Main` building `HostInitParams`).

- **Current pattern**: `main()` constructs a default `Host` and calls
  `Main(args)` directly — no `EngineContext` is built anywhere on this path.
  `Host::Main` fills `HostInitParams` from the launcher's `HostArgs` and then
  leaves every dependency pointer at its default with the in-code comment
  "dep pointers intentionally left null (standalone path)"
  (`host.cpp:312,321`). `EngineContext` is a real, pinned context root with
  all four copy/move ops deleted and declaration-order init/reverse-order
  shutdown (`include/xash3dpp/host/engine_context.hpp:86-116`), but the
  shipped `.exe` never instantiates one. The only production TU that builds a
  fully-wired `EngineContext` (with `IPlatformSockets` injected) is a test —
  `tests/host/test_engine_context_networking.cpp` — not the binary; even
  `tests/host/test_host.cpp` calls `Host::init`/`Host::Main` through the same
  null-dep `HostInitParams` shape as `main.cpp` (`test_host.cpp:18-28`).

- **Suggested replacement**: Not independently fixable at the launcher today
  — `sound`, `input`, and `imagelib` are still leaf static libs with **zero**
  link edges into `xash3dpp_host` (only their own `tests/` targets link
  them; `grep xash3dpp_sound|xash3dpp_input|xash3dpp_imagelib
  xash3dpp/src/*/CMakeLists.txt` returns only each library's own
  definition), so there is nothing yet for a wired `EngineContext` to hold
  for those satellites. Record this as a **hard precondition**, not a
  standing task: when Chunk 12 (client/sound/input) and Chunk 13 (imagelib)
  add the first production link edges from `xash3dpp_host`, the same commit
  must replace this null-dep `HostInitParams` construction with one that
  builds a real `EngineContext` and threads its dependency pointers through
  — otherwise the new link edges are CMake-graph-only, repeating the exact
  bypass pattern this finding documents for a third and fourth subsystem.

- **Boundary-safe**: Yes — internal dependency wiring only, no frozen-ABI or
  external-header surface touched.

- **Rationale**: This is a confirmed, adversarially-verified defect from this
  campaign's fact-base corrections (`CORRECTIONS.md`, "Confirmed defects"):
  "The shipped launcher never constructs `EngineContext`... the code is
  worse than the finding claims." It is promoted to High and tagged
  `[EXT:P-3]` because `EngineContext` is the P-3 context-first door's pinned
  root, and to `[EXT:G-2]` because G-2 (game ABI v2) rebinds through that
  same root — a launcher that never builds it means the door has never been
  exercised in production. It is also a named blocker in the tree-wide
  audit's own pre-Chunk-12 priority list (`launcher/main.cpp:90` cited
  alongside the T_NetIO and cvar-storage decisions as one of the five things
  to close before Chunk 12 starts) and the CMake-linkage lens's obligation
  list (`sound/input/imagelib link edges must land alongside, not instead
  of, fixing the launcher's null-dependency Host::Main bypass`).
  **Consumer**: Chunk 12 (client, sound/input wiring) and Chunk 13
  (imagelib wiring) — `scheduled-chunk-N`, both named in
  `implementation-plan.md`.

______________________________________________________________________

## Medium-priority opportunities

*(none)* — the argv scan, the `HostArgs` fill, and the static link are all
already the intended shape. Anything that *looks* like a Medium item is either
the OS entry ABI (out of scope) or the flavor/mode-select door tracked in the
boundary spec's `## Extension axes (Q-21)` (a preservation door, not a refactor).

______________________________________________________________________

## Low-priority opportunities

### L-1: `-dev` level via `std::atoi` → `std::from_chars` `[robustness]`

- **File(s)**: `xash3dpp/src/launcher/main.cpp:87-88` — the `-dev` branch
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

- **File(s)**: `xash3dpp/src/launcher/main.cpp:29-45` — the two `static` argv
  helpers.

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

### L-3: Stale `<cstdlib>` include comment cites an unused reason `[doc-accuracy]`

- **File(s)**: `xash3dpp/src/launcher/main.cpp:16`.

- **Current pattern**: `#include <cstdlib>      // atoi, EXIT_SUCCESS` —
  the comment claims two reasons for the include. Only `std::atoi`
  (line 88) is actually used; `host.Main(args)`'s `int` return is forwarded
  directly to the OS (`main.cpp:91` / the `WinMain` return), and a repo-wide
  read of the file finds no `EXIT_SUCCESS`/`EXIT_FAILURE` literal anywhere.

- **Suggested replacement**: Drop `, EXIT_SUCCESS` from the comment, or — if
  the intent was to document the `WinMain`/`main` return-code contract —
  state that explicitly instead of implying it comes from `<cstdlib>`.
  Trivial doc-accuracy fix, no behavior change.

- **Boundary-safe**: Yes.

- **Rationale**: Purely cosmetic; flagged only because a stale
  include-justification comment in a file this small is disproportionately
  likely to be trusted at face value and copy-pasted into a new bootstrap
  TU (e.g. a future dedicated-server-only entry point).

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

### C-2: `sound` / `input` / `imagelib` have zero link edges into `xash3dpp_host` — see H-1

- **File(s)**: `xash3dpp/src/launcher/main.cpp:90-91`; every
  `xash3dpp/src/{sound,input,content/imagelib}/CMakeLists.txt`.

- **Gap**: cross-referenced from H-1 rather than duplicated in full there —
  recorded separately because it is a CMake-linkage completeness gap, not an
  idiom finding. `grep xash3dpp_sound|xash3dpp_input|xash3dpp_imagelib`
  across every `src/*/CMakeLists.txt` returns only each library's own
  target definition; only their `tests/` targets link them today. This is
  the reason H-1's null-dependency `Host::Main` bypass cannot be fixed at
  the launcher in isolation — there is no production dependency object yet
  for a wired `EngineContext` to hold for these three subsystems. Chunk 12
  (sound/input) and Chunk 13 (imagelib) own both halves: adding the link
  edge and fixing H-1 in the same commit.

______________________________________________________________________

## Testing (recommendation — not a modernization item)

There is deliberately **no** `tests/launcher/` unit suite; the argv→`HostArgs`
behaviour is covered end-to-end by `tests/host/test_host.cpp` (exercising
`HostArgs`/`Host::Main`) and the server hl-smoke integration test (boundary spec
§ Tests — a recorded judgment, not missing coverage).

**Correction (2026-07-20)**: `test_host.cpp` exercises `Host::init`/
`Host::Main` through the same null-dependency `HostInitParams` shape as the
shipped `main.cpp` (`test_host.cpp:18-28`, `// dep pointers left null —
standalone / test mode`) — it does **not** cover the wired-`EngineContext`
path. The only TU in the tree that builds a fully-wired `EngineContext`
(with `IPlatformSockets` injected) is
`tests/host/test_engine_context_networking.cpp`, and it is a test, never the
binary (see H-1). The end-to-end judgment below still stands for the
argv-parsing surface this report is scoped to; it does not extend to the
dependency-wiring gap, which is tracked as a defect (H-1 / C-2), not a
testing gap.

**Recommendation (low priority):** *if* the argv surface grows (a flavor/mode
switch per the Q-21 door, or non-trivial `-basedir`/`-rodir` resolution), a
small pure-function unit test over `get_arg` / `has_flag` and the resulting
`HostArgs` fill would be cheap and would pin the parsing contract without
booting the engine. Until then, the end-to-end coverage is the right call for a
state-free bootstrap.
