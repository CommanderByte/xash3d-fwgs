# Design Paradigms — Round 1: Survey and Open Questions

> **Status**: survey only — no decisions made  
> **Scope**: all five completed subsystems; patterns identified, inconsistencies
> flagged, open questions raised for discussion  
> **Next step**: discuss each open question and record decisions in follow-up docs

---

## 1. What This Document Is

The five subsystems completed so far (utilities, memory, filesystem, platform,
cmd_cvar) were written incrementally, each solving immediate design problems
without a cross-cutting standard agreed up-front. This document catalogues what
patterns are in use, where they differ, and what questions need answers before
more subsystems are added.

The goal is not to rewrite what works. It is to identify where the ad-hoc choices
across subsystems will create friction as the engine grows — and to agree on
defaults that new subsystems should follow.

---

## 2. Current Pattern Inventory

### 2.1 Object model per subsystem

| Subsystem | Encapsulation | Lifecycle | ABI surface |
|-----------|--------------|-----------|-------------|
| `utilities` | Free functions in `xash::utilities` namespace | None — pure functions | None |
| `memory` | Free functions in `xash::memory` namespace, global pool registry | None explicit — pools created on demand | None currently; `PoolHandle` is an ABI-stable 32-bit token |
| `filesystem` | `Filesystem` pimpl class + `IFilesystem` vtable interface | `Init(...)` → `ActivateGame(...)` → `Rescan()` → `Shutdown()` | `IFilesystem` vtable (VFileSystem009 shim) |
| `platform` | Free functions in `xash::platform`, `xash::platform::console`, `xash::platform::crash` | `crash::install_handler()` idempotent; Android JNI init via `call_once` | None |
| `cmd_cvar` | `CmdCvarContext` pimpl class | `init(params)` → use → `shutdown()` | `CvarAbi*` C struct list; GoldSrc compat shim at link time |

**Observation**: three distinct models coexist — pure stateless library
(utilities), global-registry-with-free-functions (memory, platform), and
pimpl-context-class (filesystem, cmd_cvar). None of these is wrong per se, but
there is no documented rule for when to choose each one.

---

### 2.2 Pimpl variants

Both `Filesystem` and `CmdCvarContext` use the pimpl idiom but with a subtle
difference:

```cpp
// filesystem: unique_ptr — destructor and moves can be defaulted in the .cpp
struct Impl;
std::unique_ptr<Impl> impl_;

// cmd_cvar: raw pointer — destructor and moves are manually defined in the .cpp
struct Impl;
Impl *impl_ = nullptr;
```

The raw-pointer variant avoids the compile error described in the pimpl-move
instructions note (instantiating `unique_ptr<Impl>` before `Impl` is complete in
the header) by never using `unique_ptr` in the header at all. The `unique_ptr`
variant avoids it by declaring (not defaulting) the move and destructor in the
header and defining them in the `.cpp`.

Both approaches are correct. Two valid approaches to the same problem, both in
active use, is a maintenance surprise for anyone writing a third class.

---

### 2.3 Init / shutdown lifecycle patterns

| Subsystem | Init style | Return type | Shutdown |
|-----------|-----------|------------|---------|
| `Filesystem` | `Init(sv, sv, sv, sv={})`  direct positional args | `bool` | `void Shutdown()` |
| `CmdCvarContext` | `init(const CmdCvarInitParams &)` — struct | `bool` | `void shutdown() noexcept` |
| `memory` | No init call — pools created on demand | — | No shutdown — `destroy_pool()` per pool |
| `platform` | No init except `crash::install_handler()` | — | No shutdown |
| `utilities` | No init | — | No shutdown |

**Observation**: `Filesystem` passes multiple positional `string_view` args;
`CmdCvarContext` uses a params struct. When a third subsystem with a non-trivial
init is written, which style should it use?

---

### 2.4 Error handling

Across the five subsystems, four different return-value strategies are used:

| Pattern | Subsystems / operations |
|---------|------------------------|
| `bool` | `Filesystem::Init`, `ActivateGame`, `MountArchive`, `WriteFile`, `Rename`, `Delete`; `CmdCvarContext::init` |
| `nullptr` | `Filesystem::Open` → `nullptr` on failure; `cvar_find` → `nullptr` if not found; `mem_alloc` → `nullptr` on OOM |
| `std::optional<T>` | `Filesystem::FileSize`, `FileTime`, `DiskPath`, `FindLibrary`, `CRC32File`, `MD5File` |
| `void` (silent) | `cbuf_add_text`, `CmdCvarContext::shutdown`, `destroy_pool`, most platform calls |

No `std::expected<T, E>` or custom `Result<T>` type is in use. No standard
`ErrorCode` enum exists.

There is no documented rule for when to use `bool` vs `optional<T>` vs
returning `nullptr`. The current split looks opportunistic rather than systematic:
`optional<T>` is used where `T` is small and value-semantic; `nullptr` where the
return type is already a pointer; `bool` for void-or-fail operations.

---

### 2.5 Threading contracts

The threading analyses document five distinct postures:

| Subsystem | Thread safety | Mechanism | Documented contract |
|-----------|--------------|-----------|---------------------|
| `utilities` | Fully thread-safe | Stateless | Implicit (pure functions) |
| `memory` | Allocation/counter ops thread-safe; lifecycle single-owner | `std::atomic` CAS (slot claim), `memory_order_relaxed` counters | Partial — in threading doc, not public header |
| `filesystem` | Queries thread-safe; lifecycle main-thread-only | `std::shared_mutex` on `search_paths` | Partial — `active_game`, `game_loaded`, `gamedir` unguarded |
| `platform` | Mostly thread-safe; `console::read_line` main-thread-only | `assert_main_thread()` guard | Documented in threading doc |
| `cmd_cvar` | Main-thread-only (implied) | None yet | Not yet formally documented |

**Observation**: `assert_main_thread()` exists in `platform/` but is not used
in `filesystem/` (where it would prevent the undocumented lifecycle races) or
`cmd_cvar/`. The pattern exists but is applied inconsistently.

---

### 2.6 C++ feature usage

Features **in use** today:

- `std::string_view` — read-only string params in public APIs
- `std::span` — dynlib export validation
- `std::optional<T>` — nullable value returns
- `std::array<T,N>` — fixed-count internal buffers
- `std::unique_ptr<T>` — pimpl, file handles
- `std::shared_mutex` — filesystem reader-writer
- `std::atomic<T>` — memory pool state, counters
- `std::call_once` — Android JNI singleton init
- `enum class` — `SlotState`, `CvarWriteSource`, `TokenFlags`
- `inline constexpr` — all limits, constants
- `[[nodiscard]]` — all operations returning a value the caller must check
- `noexcept` — all internal APIs
- `[[gnu::weak]]` — build-info symbols

Features **available in C++20 but not yet used**:

- `std::expected<T, E>` — richer error returns without exceptions (C++23 stdlib, but
  available in MSVC 19.34+ / Clang 16+ / libc++ 16 as an extension or via `<expected>`)
- `std::jthread` — cooperative-cancel threads
- Concepts / `requires` — generic constraints
- C++20 modules (`.ixx`/`module`) — not practical with current CMake/MSVC support
- `std::format` — intentionally avoided (heap allocation risk in `bad_alloc` paths)
- `std::coroutine` — not yet relevant

---

### 2.7 ABI / plugin interface patterns

| Mechanism | Where used | Purpose |
|-----------|-----------|---------|
| `IFilesystem` vtable | `filesystem/` | VFileSystem009-compatible C++ vtable ABI for legacy DLL shim |
| `CvarAbi` C struct / linked list | `cmd_cvar/` | GoldSrc `cvar_t*` ABI for game DLLs |
| `dynlib::LibHandle` + `get_symbol` | `platform/` | Load and bind game/renderer/menu DLLs |
| `XASH_GOLDSRC_COMPAT` CMake option | `cmd_cvar/` | Link-time selection of compat shim — zero `#ifdef` in core logic |
| `extern "C"` entry points | Not yet defined in rewrite | How game/renderer DLLs will be bootstrapped |

**Observation**: `IFilesystem` is the only formal C++ interface (vtable) defined.
There is no agreed pattern for whether future subsystems exposed to plugins should
also have `I<Subsystem>` interfaces, or whether DLL bridging will be handled
differently (e.g. C-struct function tables, or always-static-linked modules).

---

## 3. Open Questions

These questions are raised for discussion; no answer is recorded here.
Each question links to the section that prompted it.

---

### Q-1: When should a subsystem be a class vs free functions?

**Context**: §2.1. Memory and platform are free functions over global/static
state. Filesystem and cmd_cvar are classes. Both work today because there is
only one of each. If we ever need multiple filesystem instances (e.g. a sandboxed
loader for a mod), the free-function model has nowhere to put the second instance.

**Options to consider**:
- A: Default to pimpl class for everything stateful; free functions only for
  genuinely stateless code (utilities, platform pure functions).
- B: Keep global-state subsystems as free functions but document the "this is a
  deliberate singleton" decision explicitly.
- C: Both A and B: stateful = class, stateless = free functions, singletons are
  explicitly documented and live in a global `EngineContext` struct.

---

### Q-2: Should there be a root `EngineContext` that owns all subsystem instances?

**Context**: §2.1. Today there is no central owner. The host layer (not yet
written) will need to own and order the init/shutdown of every subsystem.

**Implications**:
- If subsystems are all classes, an `EngineContext` struct holding them by value
  (or as `std::unique_ptr<Subsystem>`) provides a natural, destructor-ordered
  shutdown sequence.
- If some subsystems are global state (memory), the ordering is implicit and
  fragile.
- An `EngineContext` also enables dependency injection in tests without mocking
  globals.

**Questions**:
- Does `xash3dpp_memory`'s global pool registry need to become a class to fit
  in this model?
- Does `platform` (pure OS wrappers) belong in `EngineContext` or stay ambient?

---

### Q-3: Standardize the pimpl variant — `unique_ptr<Impl>` or raw `Impl*`?

**Context**: §2.2. Both variants are in use. The instructions file documents the
`unique_ptr` approach. The raw-pointer approach is equally valid.

**Preference to establish**: should new subsystems default to `unique_ptr<Impl>`
(and follow the declare-in-header / define-in-.cpp pattern documented in the
instructions) or raw `Impl*` (simpler in the header but slightly more manual
ownership)?

---

### Q-4: Init params — positional args or params struct?

**Context**: §2.3.

- **Positional args** (`Filesystem::Init`): more concise for small param sets;
  extends poorly when new optional params are added (breaking change or default-
  argument creep).
- **Params struct** (`CmdCvarInitParams`): forward-compatible; new fields can be
  added with defaults; named fields avoid parameter-order confusion.

**Question**: should new subsystems with non-trivial init always use a named
`<Subsystem>InitParams` struct?

---

### Q-5: Standardize error return patterns

**Context**: §2.4. The current mix (`bool` / `nullptr` / `optional<T>` / silent
`void`) is not wrong but is undocumented.

**Possible rules to establish**:
- `bool` for operations that are void-or-fail where the caller rarely needs to
  distinguish failure modes.
- `optional<T>` for operations that return a value which may not exist (not an
  error, just "not found" or "not available").
- `nullptr` only on pointer-returning functions where null is the natural "not
  found" sentinel.
- `std::expected<T, ErrorCode>` (once a compiler baseline is confirmed) for
  operations that can fail with diagnosable reasons.

**Question**: is `std::expected` available on the minimum compiler baseline?
(MSVC 19.34 / Clang 16 / GCC 12 — all available on modern toolchains.) If so,
should it become the standard for failable operations?

---

### Q-6: Threading model — what threads will the engine have?

> **Status**: ✅ DECIDED — see `threading-model.md` for the full specification.

**Summary of decisions**:
- Six `ThreadRole` values: `Main`, `AudioCallback`, `AudioDecoder`, `Worker`,
  `Render` (deferred Chunk 10), `NetIO` (deferred).
- Worker pool (2–4 threads) started at `Host::init()`; audio threads at
  `Sound::init()`.
- Async asset loading via `JobToken<T>` (atomic status + `unique_ptr` move).
- `assert_main_thread()` replaced by `assert_thread_role(ThreadRole::Main)`,
  defined in `platform/thread_role.hpp`.
- Render thread optional — renderer plugin declares `wants_render_thread`;
  double-buffered `RenderFrame` is the main↔render boundary.
- Network I/O thread deferred; Chunk 2 must keep all socket calls inside
  `NET_GetPacket`/`NET_SendPacket` to enable transparent later migration.
- Entity thinks, player physics, input, and all DLL calls are main-thread-only;
  world queries (BSP trace, PVS) are concurrent-read-safe after map load.
- Two filesystem threading hazards must be fixed before end of Chunk 3.
- Interface rule: query functions take `const T&`; mutation is explicit;
  compute and commit phases are separated in the server tick.

---

### Q-7: Every `ISubsystem` vtable interface or only for legacy ABI?

**Context**: §2.7. `IFilesystem` exists specifically to support the VFileSystem009
legacy ABI. No other subsystem has a vtable interface.

**Question**: going forward, should each subsystem that may be exposed to a plugin
DLL have a corresponding `I<Subsystem>` vtable, or is the assumption that all
subsystems are statically linked into the engine and plugins call into them via
function-pointer tables that are set up at DLL load time (the GoldSrc model)?

The answer determines whether the engine's plugin contract is C++ vtables
(fragile across compiler versions) or C function-pointer structs (ABI-stable,
compatible with any compiler).

---

### Q-8: How should `std::string_view` cross DLL/ABI boundaries?

**Context**: §2.7, §2.6. `std::string_view` is used for public API params
(e.g. `cvar_find(std::string_view name)`). It is not a stable ABI type — its
layout is implementation-defined across compilers.

**Options**:
- Keep `string_view` in all intra-engine calls (same binary/same compiler).
- At every DLL boundary, use `const char*` + `size_t` (or just `const char*`)
  and wrap in `string_view` on entry.
- Define a custom `StringRef { const char* data; size_t len; }` that is
  ABI-stable and implicitly converts to `string_view` inside the engine.

---

### Q-9: Ownership vocabulary across subsystem boundaries

**Context**: `pool_ptr<T>` exists as a `unique_ptr<T, PoolDeleter>` alias. The
`Filesystem::Open` returns `unique_ptr<File>`. These are consistent.

**Question**: should there be a documented ownership vocabulary:
- `pool_ptr<T>` — pool-backed, deleter returns to pool
- `unique_ptr<T>` — heap-backed (pimpl only)
- raw `T*` — borrowed reference, no ownership
- `span<T>` — borrowed, non-owning view

…and should raw `T*` ever appear in a public API (it does today: `cvar_find`
returns `Cvar*`, `cvar_get_list` returns `CvarAbi*`)? If raw `T*` means
"borrowed reference with engine lifetime", should that be documented as a type
alias (`using BorrowedRef<T> = T*`) for intent clarity?

---

### Q-10: Modular plugin / DLL bootstrap convention

**Context**: §2.7. The legacy engine bootstraps renderer/game/menu DLLs via
`extern "C" CreateAPI()`-style entry points. The rewrite has not yet defined
how this will work.

**Questions**:
- Should each plugin DLL export a single `extern "C" void* CreatePlugin(int version)`?
- Should the engine use C function-pointer structs (like `ref_api_t`, `DLL_FUNCTIONS`
  from the legacy ABI) or C++ vtable interfaces?
- Can the rewrite adopt a cleaner model (e.g. a versioned plugin descriptor struct)
  without breaking existing game DLLs?

---

## 4. Proposed Discussion Order

Some questions are prerequisites for others:

```
Q-6 (threading model)
  └─► Q-2 (EngineContext — needs threading model to define ownership)
        └─► Q-1 (class vs free functions — needs EngineContext decision)
              └─► Q-3 (pimpl variant — follows from Q-1)
                    └─► Q-4 (init params — follows from Q-3)

Q-7 (ISubsystem vtable)
  └─► Q-8 (string_view at ABI boundaries)
        └─► Q-10 (plugin bootstrap convention)
              └─► Q-9 (ownership vocabulary)

Q-5 (error returns) — standalone, can be resolved independently
```

**Q-6 is complete.** Next: Q-2 → Q-1, as those two determine the shape of every
future subsystem. Q-7 and Q-5 can run in parallel since they are narrower.

---

## 5. What Is Already Working Well

Not everything needs changing. These patterns are solid and should be preserved:

- **Three-tier stats/debug model** (always-on / `XASH_STATS` / `XASH_DEBUG_*`) —
  documented in `debug-stats-design.md`.
- **`limits.hpp` + `XASH_LIMIT_*` override macros** — clean and extensible.
- **`XASH_GOLDSRC_COMPAT` link-time selection** — zero `#ifdef` in core.
- **`[[nodiscard]]` + `noexcept` on all APIs** — consistently applied.
- **Boundary docs before implementation** — good discipline.
- **`assert_thread_role()` pattern** — `ThreadRole` enum in `platform/`, applied
  at every subsystem boundary (Q-6 decided).
- **`pool_ptr<T>` + `PoolDeleter`** — ownership is explicit and correct.
- **Framework-primitive-first rule** (utilities/memory/filesystem/platform over
  stdlib/OS) — now codified in instructions.
