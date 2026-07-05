# Layer Model — target dependency tiers

> **Date**: 2026-07-06 (D-1 dependency-graph hardening preflight)\
> **Scope**: CMake target layering + header include-direction for `xash3dpp/`\
> **Status**: verdicts recorded; two cycles removed, one inversion deferred\
> **Related**: `extension-goals.md` (P-3/P-6), Q-2/Q-21/Q-22,
> the 6B retrofit chunk in `implementation-plan.md`

______________________________________________________________________

## 1. The tiers

Bottom-up; every link edge points DOWN this list (one-way):

| Tier | Targets | Contents |
|------|---------|----------|
| base | `xash3dpp_utilities` | pure functions: strings, paths, math, hash, UTF |
| base | `xash3dpp_memory` | pool registry (the documented Q-1 singleton exception) |
| base | `xash3dpp_platform` | OS abstraction (sys/io/console/crash/sockets) **+ the diagnostics tier**: `log.cpp` and `thread_role.cpp` are HOSTED here (files remain under `src/core/`, the API remains `xash::core` and `<xash3dpp/core/...>`) so the base layer is self-contained — platform's own console/crash/sockets assert thread roles, and the default log sink is platform console |
| engine | `xash3dpp_core` | engine-tier primitives: `Clock`, `ErrorCode` |
| engine | `xash3dpp_filesystem`, `xash3dpp_cmd_cvar`, `xash3dpp_networking`, `xash3dpp_map_loader` | subsystems |
| engine | `xash3dpp_server` | game-simulation host |
| top | `xash3dpp_host` | `EngineContext` owner **+ the ABI accessor state** (`engine_context_accessor.cpp` — the host owns the pointer's lifecycle) |
| top | `xash3dpp_abi` | C direct-export shims; consumes the accessor one-way (abi → host) |
| top | `xash3dpp_launcher` | argv bootstrap |

Conventions this table encodes:

- **Namespace ≠ target.** `xash::core`'s diagnostics API is implemented inside
  `xash3dpp_platform`; `xash::abi`'s accessor state is implemented inside
  `xash3dpp_host`. Header paths and namespaces are API stability surfaces;
  target membership is a build concern — they may differ when layering
  demands it, and the owning CMakeLists documents it.
- **Include-direction is the acceptance criterion**, not link edges alone —
  CMake tolerates static-lib link cycles, so a clean link graph can still
  hide upward includes.

## 2. D-1 verdicts (2026-07-06)

| Claim | Verdict | Remedy |
|-------|---------|--------|
| `core ⇄ platform` link cycle (platform used `core::detail` thread helpers; core's log sink is platform console) | **CONFIRMED** | Diagnostics TUs (`log.cpp`, `thread_role.cpp`) rehomed into `xash3dpp_platform` (source-list membership only — no file moves, no namespace churn, no behavior change); platform's core link dropped → one-way `core → platform` |
| `host ⇄ abi` link cycle (abi held the accessor state host writes; abi's shim calls host) | **CONFIRMED** | Accessor state extracted to `src/host/engine_context_accessor.cpp` (header path unchanged); host's abi link dropped; abi's PUBLIC host/core links narrowed to PRIVATE → one-way `abi → host` |
| `platform/os_socket.hpp` re-exports networking types (`IpFamily`, `NetAddress`, `NetError`, `Result`) — a layer inversion (platform → networking upward include) | **CONFIRMED — DEFERRED** | No low-churn seam exists: `NetAddress` carries parsing implementation and ~24 networking TUs consume these types in place. Remedy when the trigger fires: extract a `net_base` types layer. **Owner**: the first satellite that needs sockets without networking (likely the G-5 scripting spike or G-1 MCP transport). Tag: `TODO(net-base)` — recorded here, not in code (the inversion spans headers, not one site). |

Also landed with D-1: the `mem_alloc`/`mem_realloc` `> UINT32_MAX`
representability guard (honest label: a small behavior fix — requests the
32-bit `AllocHeader::payload_size` cannot represent now fail through the OOM
path instead of silently truncating; unreachable on 32-bit targets) and the
`XASH_PRINTF_FORMAT` portability macro replacing the raw `[[gnu::format]]`
at `core::logf`.

## 3. Toolchain policy (C++23)

`xash3dpp/` intentionally requires modern toolchains: the root CMake pins
C++23 (`std::expected` is in active use per Q-5), MSVC VS2022 is the primary
compiler, and GCC/Clang parity is maintained by flag pairs
(`/EHs-c- /GR-` ↔ `-fno-exceptions -fno-rtti`, Q-18 FP flags). The **legacy
engine tree remains the compatibility path** for older platforms — do not
introduce an `expected`-like fallback or downgrade the standard unless a
real supported platform forces it (that decision would be a register entry,
not a quiet CMake edit). Per-target `cxx_std_20` declarations are historical
minimums being normalized to `cxx_std_23` by the 6B wave.

## 4. Keeping it true

- `dep_scan.py` (CLI-only) is the mechanical check: InitParams/namespace
  edges + cycle detection, **plus** (D-1) the CMake target link graph
  (`link_edges`/`link_cycles`) and the cross-subsystem include-direction
  census (`include_edges`/`include_cycles`).
- Post-D-1 baseline (2026-07-06): `link_cycles: []` — the target graph is
  acyclic. `include_cycles` carries exactly four entries, each accounted:

  | Include cycle | Status |
  |---------------|--------|
  | `core <-> platform` | expected by construction — core-pathed diagnostics headers hosted in platform (§1) |
  | `abi <-> host` | expected by construction — abi-pathed accessor header, state hosted in host (§1) |
  | `networking <-> platform` | the deferred `os_socket.hpp` type re-export (§2, `TODO(net-base)`) |
  | `cmd_cvar <-> core` | **discovered by this census** — `core::Clock` couples to cvar types while cmd_cvar uses core diagnostics; link graph is one-way (core does NOT link cmd_cvar) so this is header-level only. Owner: the 6B core session (S3) adjudicates — types-only coupling may be acceptable-with-note, or Clock's cvar surface gets a seam. |

  A NEW entry in either cycle list needs a documented deferral here or a
  redesign — never silence.
- New targets declare their tier in their CMakeLists header comment.
