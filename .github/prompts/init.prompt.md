---
name: "Init — xash3dpp repo onboarding"
description: "Onboard to the xash3dpp rewrite repo. Run this at the start of any rewrite session."
agent: agent
---

# Repo Onboarding — xash3dpp

## Repository Layout

This repository has two distinct zones. Understand the boundary before doing anything else.

| Zone | Path | Purpose |
|------|------|---------|
| **Legacy** | Everything at the repo root (`engine/`, `filesystem/`, `public/`, `ref/`, `common/`, `pm_shared/`, `android/`, `3rdparty/`, `game_launch/`, `scripts/`, `src/`, `tests/`, `utils/`) | Original Xash3D FWGS C codebase. Authoritative reference for behaviour and ABI contracts. |
| **Rewrite** | `xash3dpp/` | New modular C++ project, structured as a self-contained future repo. All new work goes here. |

Documentation in `Documentation/codex/` is from a previous incremental-upgrade attempt. It is **legacy reference only** — treat its architecture decisions as notes, not requirements.

## The One Rule

> **Do not model the rewrite on the legacy code structure.** The legacy tree exists to read behaviour from, not to copy patterns into `xash3dpp/`.

When reading the legacy code, ask: *what does this do and what invariants must be preserved?* Not: *how is this structured?*

## Rewrite Principles

- External C ABI surfaces must be preserved (game DLL, client DLL, renderer, filesystem plugin, shared SDK headers in `common/`, `engine/*.h`, `pm_shared/`).
- Internal implementation language is C++. Exceptions and RTTI are disabled unless explicitly decided otherwise.
- Each new module must be independently buildable before the next one starts.
- Compatibility quirks are documented and preserved, not cleaned away silently.

## How to Work

1. **Analyse first.** Before writing any `xash3dpp/` code, read the relevant legacy subsystem using the available tools (semantic search, file reads, call hierarchy). Form a clear picture of the behaviour contract.
2. **Write a boundary spec.** What must the new module expose? What must it preserve? Capture this in `xash3dpp/docs/` before implementation.
3. **Implement in `xash3dpp/src/`.** Keep new code self-contained; do not reference legacy paths from build files.
4. **Verify against legacy behaviour.** Use the legacy code as the ground truth for any behavioural question.

## Mandatory: Use Framework Primitives

Before reaching for a stdlib or OS API, check whether the framework already
provides it. Using the framework is **required**, not optional.

| Need | Framework primitive |
|------|---------------------|
| Strings / parsing | `utilities::strncpy`, `stricmp`, `snprintf`, `parse_token`, `Tokenizer` (`utilities/string.hpp`) |
| Path manipulation | `utilities::path_join`, `file_base`, `fix_slashes`, `replace_extension` (`utilities/path.hpp`) |
| Hashing (CRC-32, MD5) | `utilities::Crc32Hasher`, `Md5Hasher`, `crc32()` (`utilities/hash.hpp`) |
| Math / matrices | `utilities::Vec3`, `Matrix3x4`, `dot`, `normalize` (`utilities/math.hpp`, `matrix.hpp`) |
| Dynamic allocation | `memory::mem_alloc`, `mem_calloc`, `mem_free`, `pool_new<T>` (`memory/memory.hpp`) |
| File I/O | `IFilesystem` injected interface (`filesystem/filesystem.hpp`) |
| Time | `platform::get_time()` (`platform/sys.hpp`) |
| Console output | `platform::console::write(std::string_view)` (`platform/console.hpp`) |

**Never use** `malloc/free`, `new/delete` (except `std::make_unique<Impl>` for pimpl),
`fopen/fclose/FILE*`, raw `strlen/strcpy/sprintf`, custom CRC/hash implementations,
or direct OS timer / I/O calls outside the `platform/` or `filesystem/` subsystems.

## Current State

Five subsystems are complete and tested.  Use `analyse-subsystem` to scope the
next one.

### `xash3dpp_utilities` — complete
- **Library**: `xash3dpp/src/utilities/`; headers in `xash3dpp/include/xash3dpp/utilities/`
- **Modules**: `atlas`, `build`, `dynlib`, `hash`, `math`, `matrix`, `path`, `string`, `swap`, `utf`
- **Tests**: `xash3dpp/tests/utilities/` — one `test_<module>.cpp` per module; CTest target `test_utilities`

### `xash3dpp_memory` — complete
- **Library**: `xash3dpp/src/memory/`; public headers in `xash3dpp/include/xash3dpp/memory/`;
  private headers in `xash3dpp/include/xash3dpp/private/memory/`
- **Design**: pool accounting facade over `malloc`/`free`; 128-slot registry;
  `SlotState` atomic CAS for concurrent `create_pool`; `std::atomic<OomHandler>`
  for the OOM callback
- **Tests**: `xash3dpp/tests/memory/`

### `xash3dpp_filesystem` — complete
- **Library**: `xash3dpp/src/filesystem/`; headers in `xash3dpp/include/xash3dpp/filesystem/`
- **Design**: `Filesystem` pimpl class; `VFileSystem009Adapter` is the legacy
  ABI shim (wraps `Filesystem` directly); PAK/WAD/ZIP archive backends;
  `ISearchBackend` vtable for backend dispatch; case-insensitive directory
  cache; `std::shared_mutex` guards the search-path deque
- **Tests**: `xash3dpp/tests/filesystem/`

### `xash3dpp_platform` — complete
- **Library**: `xash3dpp/src/platform/`; public headers in
  `xash3dpp/include/xash3dpp/platform/`
- **Design**: Single Porting Layer — all OS-specific code lives here.
  Win32 and POSIX backends for `sys` (time, sleep, env), `console` (stdin
  reader), and `crash` (signal/exception handler); Android JNI bootstrap via
  `std::call_once`
- **Tests**: `xash3dpp/tests/platform/`

### `xash3dpp_core` — complete
- **Library**: `xash3dpp/src/core/`; public headers in
  `xash3dpp/include/xash3dpp/core/`; private headers in
  `xash3dpp/include/xash3dpp/private/core/`
- **Design**: Cross-cutting singletons shared by all subsystems — structured
  logging (`core::log`, `core::logf`, `LogLevel` enum), assertion macros
  (`XASH_ASSERT`, `XASH_FATAL`), and thread-role registration
  (`core::register_thread_role`, `core::assert_thread_role`); log sink writes
  via `platform::console::write()`
- **Tests**: `xash3dpp/tests/core/`

### `xash3dpp_cmd_cvar` — complete
- **Library**: `xash3dpp/src/cmd_cvar/`; public headers in
  `xash3dpp/include/xash3dpp/cmd_cvar/`; private headers in
  `xash3dpp/include/xash3dpp/private/cmd_cvar/`
- **Design**: Single `CmdCvarContext` pimpl class; pimpl move ctor/dtor defined
  in `context.cpp` (not `= default` in header); `XASH_GOLDSRC_COMPAT` CMake
  option selects `compat_goldsrc.cpp` vs `compat_null.cpp` at link time —
  zero `#ifdef` in core; `CircularBuffer<T,N>` private template for change log
- **Tests**: `xash3dpp/tests/cmd_cvar/`

**Common build setup**: CMake at `xash3dpp/CMakeLists.txt`; C++20;
no exceptions (`/EHs-c-`); no RTTI (`/GR-`); build tree at `xash3dpp/build/`.

## Cross-Cutting Design Decisions

### Debug and Stats Instrumentation

A three-tier model governs all measurement and tracing across subsystems.
Full analysis: [`xash3dpp/docs/design/debug-stats-design.md`](../../xash3dpp/docs/design/debug-stats-design.md)

| Tier | Guard | When compiled in |
|------|-------|-----------------|
| Always-on | none | All builds — ≤ 1 relaxed atomic per event |
| `XASH_STATS` | `#if XASH_STATS` | Profiling and debug builds |
| `XASH_DEBUG_<SUBSYSTEM>` | `#if XASH_DEBUG_<SUBSYSTEM>` | Dev builds only |

**Key rules**: never gate counter *increments* on a runtime bool (always
measure; gate *output*); never format strings on hot paths; every non-trivial
subsystem exposes `const Stats& stats() const noexcept`.

Long-term plan: a dedicated `diagnostics` subsystem will provide a UDP stats
stream and TCP command channel for external tooling — deferred until at least
three subsystems have stats structs.

### Threading Model

Full specification: [`xash3dpp/docs/design/threading-model.md`](../../xash3dpp/docs/design/threading-model.md)

Six thread roles are defined in `core/thread_role.hpp`. Registration at
thread start + `core::assert_thread_role()` in debug builds enforce the
contracts.

| Thread | Starts | Drives |
|--------|--------|--------|
| `T_Main` | process start | game loop, server tick, client tick, input, cmd/cvar, physics, all DLL calls |
| `T_AudioCallback` | `Sound::init()` | OS audio output — must never allocate or block |
| `T_AudioDecoder` | `Sound::init()` | OGG/Opus decode → PCM SPSC ring → `T_AudioCallback` |
| `T_Worker[0..N]` | `Host::init()` | async asset load, PVS per client, delta encode, HTTP I/O |
| `T_Render` | `Renderer::init()` | GPU submission — **deferred Chunk 10**, optional (Vulkan only) |
| `T_NetIO` | deferred | socket I/O — **deferred**, triggered by HTTP/DNS need or high player count |

**Key rules for new subsystems:**
- Query functions take `const T&` (immutable context) — never read a mutable global.
- Mutation is explicit at the call site; compute and commit are separate phases.
- `WorldData` is immutable after activation — all BSP/trace/PVS queries are concurrent-read-safe.
- Async jobs use `JobToken<T>` (atomic `JobStatus` + `unique_ptr<T>` move on completion).
- No `recvfrom`/`sendto` calls outside the networking subsystem — packet I/O goes through `NET_GetPacket`/`NET_SendPacket` only.
- `assert_thread_role(ThreadRole::Main)` at the top of every main-thread-only public function.
- Two filesystem threading hazards must be fixed before end of Chunk 3 (see document §10).

### Design Paradigms (Q-1 through Q-10)

Full decisions: [`xash3dpp/docs/design/design-paradigms-round1.md`](../../xash3dpp/docs/design/design-paradigms-round1.md)

**Object model and ownership:**
- Stateful subsystems → pimpl class owned by `EngineContext` in init order.
- `memory` and `platform` are explicit exceptions (global/ambient; no EngineContext slot).
- New subsystems use `unique_ptr<Impl>`; `cmd_cvar`'s raw `Impl*` is grandfathered.
- Every subsystem with init parameters uses `<Subsystem>InitParams` struct — no positional arg threshold.
- Dependencies injected via params struct; no global `g_engine` accessor.

**Error returns:**
- `[[nodiscard]] bool` for void-or-fail; `optional<T>` for absent-not-error; nullable `T*` for pointer returns; `void` for infallible.
- Functions returning a failure indicator must emit a diagnostic internally before returning.
- `std::expected<T, ErrorCode>` deferred to Chunk 2; never call `.value()`.

**Interface and ABI:**
- Internal seams: C++ `I<Subsystem>` vtable. DLL boundaries: C function-pointer struct.
- Legacy ABIs (`enginefuncs_t`, `DLL_FUNCTIONS`, `ref_api_t`) are preserved exactly.
- New plugin types (first: Vulkan renderer, Chunk 10): versioned C descriptor struct with `struct_size` and two-way version check.
- `std::string_view` freely intra-engine; `const char*` at `extern "C"` / DLL boundaries — wrap on entry.

**Ownership vocabulary:**
- `pool_ptr<T>` (pool-backed owned) · `unique_ptr<T>` (pimpl only) · raw `T*` (borrowed, `// @lifetime: engine`) · `span<const T>` (default view) · `string_view` (string view).
- Pool selection reflects lifetime: process pool / session pool / frame pool.

### Code Style and Conventions (Round 2)

Full decisions: [`xash3dpp/docs/design/design-paradigms-round2.md`](../../xash3dpp/docs/design/design-paradigms-round2.md)

**Naming:**
- Types: `PascalCase` · Member functions: `snake_case` · Free functions: `snake_case` · Namespaces: `lowercase` · Macros: `UPPER_SNAKE_CASE` · Files: `snake_case.{hpp,cpp}` · Private members: `trailing_` · `enum class` values: `PascalCase` (no `k` prefix) · `constexpr` constants: `k_snake_case`

**Instrumentation:**
- `[[nodiscard]]` is the default on all non-`void` returns; omission requires justification.
- `XASH_ASSERT(expr)` — debug-only invariant check (`core/assert.hpp`). `XASH_FATAL(expr, msg)` — always-on unrecoverable invariant. Never use `assert()` from `<cassert>`.
- `core::log(LogLevel, tag, msg)` / `core::logf(LogLevel, tag, fmt, ...)` for all diagnostic output — not `printf`, not `console::write` directly.
- `core::register_thread_role(ThreadRole)` at thread start; `core::assert_thread_role(ThreadRole)` inside main-thread-only functions.

**Integer types:** `size_t` for sizes · `int` for GoldSrc ABI · `uint32_t` for internal tokens · `int64_t` for file offsets · `bool` for booleans · no bare `unsigned`.

**Copy/move:** Subsystem context = move-only. `EngineContext` = non-moveable. Value aggregates = copyable. Handles = copyable unless exclusive-ownership.

**Tests:** Hand-rolled `CHECK`/`REQUIRE` macros from `test_helpers.hpp`. Revisit with doctest at ~15 test files.
