# Design Paradigms — Round 1: Survey and Open Questions

> **Status**: all questions decided — see individual Q sections below  
> **Scope**: all five completed subsystems  
> **Application**: §4 records what applies now, when next touched, and new code only

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
| `VFileSystem009` legacy C++ ABI shim | `filesystem/` | GoldSrc `IFileSystem` v9 vtable for legacy DLL consumers — wraps the concrete `Filesystem` class directly |
| `CvarAbi` C struct / linked list | `cmd_cvar/` | GoldSrc `cvar_t*` ABI for game DLLs |
| `dynlib::LibHandle` + `get_symbol` | `platform/` | Load and bind game/renderer/menu DLLs |
| `XASH_GOLDSRC_COMPAT` CMake option | `cmd_cvar/` | Link-time selection of compat shim — zero `#ifdef` in core logic |
| `extern "C"` entry points | Not yet defined in rewrite | How game/renderer DLLs will be bootstrapped |

**Observation**: there is **no `IFilesystem` C++ interface** in the rewrite —
the VFileSystem009 shim wraps the concrete `Filesystem` class directly. The only
formal vtable interfaces in source are four small **internal seams** that exist
for testability and policy injection, not for crossing a DLL boundary:

| Interface | Defined in | Role |
|-----------|-----------|------|
| `ICvarObserver` | `cmd_cvar/observers.hpp` | Notifies when a cvar value changes (debug tooling, mirror state) |
| `ITrustOracle` | `cmd_cvar/observers.hpp` | Answers "is this stuffcmd batch from a trusted source?" |
| `ICompatPolicy` | `private/cmd_cvar/compat_policy.hpp` | Routes GoldSrc behavioural quirks (link-time selection via `XASH_GOLDSRC_COMPAT`) |
| `ISearchBackend` | `private/filesystem/search_backend.hpp` | Pak / WAD / dir backend dispatch — a small internal vtable, never exported |

All four are intra-process seams, Q-7 conformant (small, focused interfaces with
a single concrete production implementation plus a test fake), and none of them
cross a DLL boundary.

---

## 3. Open Questions

These questions are raised for discussion; no answer is recorded here.
Each question links to the section that prompted it.

---

### Q-1: When should a subsystem be a class vs free functions?

> **Status**: ✅ DECIDED

**Decision**: The deciding criterion is whether the subsystem has state that must
be initialised and shut down in a specific order.

- **Stateful subsystem** → pimpl class, owned by `EngineContext`.
- **Stateless / pure OS wrappers** → free functions in namespace; no lifecycle.
- **`memory`** → documented deliberate exception: global pool registry that must
  outlive `EngineContext`; stays as free functions over global state. This choice
  is intentional and must be noted at every `EngineContext` definition site.
- **`platform`** → ambient free functions; no `EngineContext` membership needed.

Applied to existing subsystems: `utilities` and `platform` are correctly free
functions and stay that way. `filesystem` and `cmd_cvar` are correctly pimpl
classes. `memory` is the documented singleton exception.

---

### Q-2: Should there be a root `EngineContext` that owns all subsystem instances?

> **Status**: ✅ DECIDED

**Decision**: Yes. `EngineContext` is a flat struct that owns all stateful
subsystem instances as direct members in declaration (init) order. C++ guarantees
member construction in declaration order and destruction in reverse — this
provides deterministic, destructor-ordered shutdown with no explicit shutdown
calls needed.

```cpp
struct EngineContext {
    Filesystem      filesystem;
    CmdCvarContext  cmd_cvar;
    // Chunk 2: NetworkContext  networking;
    // Chunk 3: HostContext     host;
};
```

**Dependencies are injected at construction via `<Subsystem>InitParams` structs**;
no global `g_engine` accessor. GoldSrc compat function-pointer structs
(`enginefuncs_t`, etc.) are populated by the host layer via adapter lambdas that
capture subsystem references — no global access needed.

`memory` and `platform` are explicitly exempt: memory must outlive `EngineContext`;
platform has no meaningful state to own.

---

### Q-3: Standardize the pimpl variant — `unique_ptr<Impl>` or raw `Impl*`?

> **Status**: ✅ DECIDED

**Decision**: New subsystems use `unique_ptr<Impl>` with the
declare-in-header / define-in-`.cpp` pattern documented in
`xash3dpp.instructions.md`.

`cmd_cvar`'s raw `Impl*` variant is grandfathered in; not changed proactively.
Bring into conformance the next time `cmd_cvar` is modified for another reason.

---

### Q-4: Init params — positional args or params struct?

> **Status**: ✅ DECIDED

**Decision**: Every subsystem with init parameters uses a named
`<Subsystem>InitParams` struct — no threshold. The params struct carries both
configuration values and injected dependencies, making all dependencies explicit
at the call site.

Subsystems with no init at all (`utilities`, `platform`, `memory`) are exempt.

`Filesystem::Init` with positional args is grandfathered in; it naturally migrates
to the params struct form when `EngineContext` is written and the init call moves
there.

---

### Q-5: Standardize error return patterns

> **Status**: ✅ DECIDED

**Rules for existing patterns** (apply to all new code now):

| Pattern | Use for |
|---------|---------|
| `[[nodiscard]] bool` | Void-or-fail where failure reason does not matter to callers |
| `[[nodiscard]] std::optional<T>` | Value may legitimately be absent — not an error, just "not found" |
| `[[nodiscard]] T*` (nullable) | Pointer return where null is the natural absent sentinel |
| `void` | Infallible operations, or failure handled internally with a fallback |

**Internal logging rule**: functions returning a failure indicator must emit a
diagnostic before returning, unless the absent case is a normal "not found" query
(in which case `optional<T>` returning `nullopt` is silent by contract).

**`std::expected<T, ErrorCode>`**: deferred. Introduced at Chunk 2 (networking)
as the standard for subsystems where failure reason matters to callers. A central
`ErrorCode` enum is defined at Chunk 2 and extended per subsystem. When the
diagnostics channel is ready, error events flow as typed
`{ subsystem_id, error_code, timestamp }` structs rather than strings.

All error return values carry `[[nodiscard]]`.

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
  defined in `core/thread_role.hpp`.
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

> **Status**: ✅ DECIDED

**Decision**: Use the right mechanism for the right context.

- **Internal seam (same binary, same compiler)** → C++ abstract class with vtable
  (`I<Subsystem>`). Supports dependency injection and test mocking. `IFilesystem`
  is the canonical reference.
- **DLL boundary (game DLL, client DLL, renderer DLL, menu DLL)** → C
  function-pointer struct. C++ vtable layout is not guaranteed across compilers.
  The legacy ABIs (`enginefuncs_t`, `DLL_FUNCTIONS`, `ref_api_t`) use C structs
  for this reason and are preserved exactly.

An `I<Subsystem>` vtable may be defined for internal dependency injection even
for subsystems that also have a DLL-boundary C struct. They are separate layers:
the engine uses the C++ interface internally; a DLL shim translates at the edge.

Any method on an `I<X>` interface exposed via a DLL shim must be implementable
with `const char*` on the outer face (no `std::string_view` crossing the DLL edge).

---

### Q-8: How should `std::string_view` cross DLL/ABI boundaries?

> **Status**: ✅ DECIDED

**Rule**: `std::string_view` is used freely within the same binary (intra-engine,
same compiler). At any `extern "C"` or DLL boundary, use `const char*` (with
optional `size_t` length if the callee needs it). The receiving side wraps in
`std::string_view` immediately on entry.

No custom `StringRef` type is needed — the rule is simple enough to follow
without a new type.

---

### Q-9: Ownership vocabulary across subsystem boundaries

> **Status**: ✅ DECIDED

**Vocabulary table** (all new APIs must conform):

| Type | Semantics |
|------|-----------|
| `pool_ptr<T>` | Owned; deleter returns memory to the pool the object came from |
| `std::unique_ptr<T>` | Owned; heap-backed. Used only for pimpl before a subsystem pool exists |
| `T*` (raw) | Borrowed reference — caller must not delete; document with `// @lifetime: engine` |
| `std::span<const T>` | Default non-owning view of a contiguous range |
| `std::span<T>` | Non-owning mutable view — only when intentionally writing through |
| `std::string_view` | Non-owning string |

**Pool selection reflects lifetime, not type**: objects living for the process go
in the long-lived pool; per-session objects in the session pool; per-frame scratch
in the frame pool. `pool_ptr<T>` communicates ownership; which pool communicates
lifetime.

Raw `T*` in public APIs (e.g. `cvar_find` returning `Cvar*`) is an
engine-lifetime borrowed reference. Document intent with `// @lifetime: engine`
on the function declaration. No `BorrowedRef<T>` type alias — documentation over
type aliasing.

`std::span<const T>` is the default for non-owning views; `std::span<T>` requires
explicit justification at the call site.

---

### Q-10: Modular plugin / DLL bootstrap convention

> **Status**: ✅ DECIDED

**Legacy ABIs** (`eiface.h`, `cdll_int.h`, `ref_api.h`) are preserved exactly.

**New plugin types** (Vulkan renderer is the first candidate, Chunk 10) use a
versioned C descriptor struct:

```c
typedef struct plugin_descriptor_s {
    uint32_t    plugin_api_version;      /* API version the plugin was built for */
    uint32_t    min_engine_api_version;  /* minimum engine version required */
    uint32_t    struct_size;             /* sizeof(this) — forward-compatible extension */
    const char *name;
    void       *(*create)(const engine_api_t *engine);
    void        (*destroy)(void *plugin);
} plugin_descriptor_t;

/* DLL exports one symbol: */
PLUGIN_EXPORT const plugin_descriptor_t *GetPluginDescriptor(void);
```

**Version check is two-way**: engine checks `min_engine_api_version ≤ engine_version`;
plugin's `create()` checks the engine's `api_version`. `XASH3DPP_PLUGIN_API_VERSION`
is defined in the public SDK header for compile-time checks.

`struct_size` enables forward compatibility: a plugin compiled against a newer
descriptor loads on an older engine — the engine reads only up to its own
`sizeof(plugin_descriptor_t)`, ignoring unknown trailing fields.

Deferred to Chunk 10. No new plugin types before then.

---

## 4. Application Schedule

All ten open questions are decided. This section records when each rule applies.

### 4.1 Must happen before Chunk 3

Later chunks depend on these; they are scheduled work, not opportunistic.

- **`EngineContext` design** (Q-2): the host subsystem (Chunk 3) needs a central
  owner for all subsystem instances and a dependency-injection story before it
  can be written.
- **Filesystem threading hazards** (Q-6 / `threading-model.md` §10): must be
  fixed before the worker pool starts. Two specific sites identified there.
- **`assert_thread_role` replacing `assert_main_thread`** (Q-6): low effort;
  required before any background threading work begins.

### 4.2 Bring into conformance when next touched

Do not rewrite for consistency alone. When a subsystem is modified for another
reason, bring it into conformance with these rules at the same time:

- **Q-3**: `cmd_cvar` raw `Impl*` → `unique_ptr<Impl>`
- **Q-4**: `Filesystem::Init` positional args → `FilesystemInitParams` struct
  (naturally happens when `EngineContext` is written)
- **Q-5**: add `// @lifetime: engine` annotations to raw-pointer-returning
  functions in existing subsystems

### 4.3 New code only

These rules apply from the first line of any new subsystem:

- `EngineContext` membership; dependency injection via params struct (Q-1, Q-2)
- `unique_ptr<Impl>` pimpl pattern (Q-3)
- `<Subsystem>InitParams` struct for any init parameters (Q-4)
- Error return rules: `bool` / `optional<T>` / nullable `T*` / `void` (Q-5)
- Internal `I<Subsystem>` vtables for injectable seams; C structs at DLL
  boundaries (Q-7, Q-8)
- Ownership vocabulary table (Q-9)
- `std::expected<T, ErrorCode>` for rich failure modes, from Chunk 2 (Q-5)
- Versioned C plugin descriptor for new plugin types, from Chunk 10 (Q-10)

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
- **`EngineContext` + dependency injection** — all stateful subsystems owned in
  init order; deps via params struct; no global accessor (Q-2 decided).
- **`<Subsystem>InitParams` struct** — carries both config and injected deps;
  all new subsystems with init parameters must use one (Q-4 decided).
- **Ownership vocabulary** — `pool_ptr` / `unique_ptr` (pimpl only) / raw `T*`
  (borrowed, `@lifetime`) / `span<const T>` / `string_view` (Q-9 decided).
