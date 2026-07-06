# Design Decisions — Architecture and Ownership

> **Status**: all questions decided — see individual Q sections below\
> **Scope**: all five completed subsystems\
> **Application**: §4 records what applies now, when next touched, and new code only

______________________________________________________________________

## 1. What This Document Is

The five subsystems completed so far (utilities, memory, filesystem, platform,
cmd_cvar) were written incrementally, each solving immediate design problems
without a cross-cutting standard agreed up-front. This document catalogues what
patterns are in use, where they differ, and what questions need answers before
more subsystems are added.

The goal is not to rewrite what works. It is to identify where the ad-hoc choices
across subsystems will create friction as the engine grows — and to agree on
defaults that new subsystems should follow.

______________________________________________________________________

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

______________________________________________________________________

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

______________________________________________________________________

### 2.3 Init / shutdown lifecycle patterns

| Subsystem | Init style | Return type | Shutdown |
|-----------|-----------|------------|---------|
| `Filesystem` | `Init(sv, sv, sv, sv={})` direct positional args | `bool` | `void Shutdown()` |
| `CmdCvarContext` | `init(const CmdCvarInitParams &)` — struct | `bool` | `void shutdown() noexcept` |
| `memory` | No init call — pools created on demand | — | No shutdown — `destroy_pool()` per pool |
| `platform` | No init except `crash::install_handler()` | — | No shutdown |
| `utilities` | No init | — | No shutdown |

**Observation**: `Filesystem` passes multiple positional `string_view` args;
`CmdCvarContext` uses a params struct. When a third subsystem with a non-trivial
init is written, which style should it use?

______________________________________________________________________

### 2.4 Error handling

Across the five subsystems, four different return-value strategies are used:

| Pattern | Subsystems / operations |
|---------|------------------------|
| `bool` | `Filesystem::Init`, `ActivateGame`, `MountArchive`, `WriteFile`, `Rename`, `Delete`; `CmdCvarContext::init` |
| `nullptr` | `Filesystem::Open` → `nullptr` on failure; `cvar_find` → `nullptr` if not found; `mem_alloc` → `nullptr` on OOM |
| `std::optional<T>` | `Filesystem::FileSize`, `FileTime`, `DiskPath`, `FindLibrary`, `CRC32File`, `MD5File` |
| `void` (silent) | `cbuf_add_text`, `CmdCvarContext::shutdown`, `destroy_pool`, most platform calls |

`std::expected<T, E>` is now in active use for richer failure modes
(`map_loader`, `networking`, `content`, and `imagelib` have typed result
surfaces), and `core::ErrorCode` is the shared engine error enum for common
engine failures.

There is no documented rule for when to use `bool` vs `optional<T>` vs
returning `nullptr`. The current split looks opportunistic rather than systematic:
`optional<T>` is used where `T` is small and value-semantic; `nullptr` where the
return type is already a pointer; `bool` for void-or-fail operations.

______________________________________________________________________

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

______________________________________________________________________

### 2.6 C++ feature usage

Features **in use** today:

- `std::string_view` — read-only string params in public APIs
- `std::span` — dynlib export validation
- `std::optional<T>` — nullable value returns
- `std::expected<T,E>` — typed success/failure returns
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

Features **available but not yet used broadly**:

- `std::jthread` — cooperative-cancel threads
- Concepts / `requires` — generic constraints
- C++20 modules (`.ixx`/`module`) — not practical with current CMake/MSVC support
- `std::format` — intentionally avoided (heap allocation risk in `bad_alloc` paths)
- `std::coroutine` — not yet relevant

______________________________________________________________________

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
formal vtable interfaces in source are **internal seams** that exist
for testability, policy injection, or protocol-variant dispatch — never for
crossing a DLL boundary:

| Interface | Defined in | Role |
|-----------|-----------|------|
| `ICvarObserver` | `cmd_cvar/observers.hpp` | Notifies when a cvar value changes (debug tooling, mirror state) |
| `ITrustOracle` | `cmd_cvar/observers.hpp` | Answers “is this stuffcmd batch from a trusted source?” |
| `ICompatPolicy` | `private/cmd_cvar/compat_policy.hpp` | Routes GoldSrc behavioural quirks (link-time selection via `XASH_GOLDSRC_COMPAT`) |
| `ISearchBackend` | `private/filesystem/search_backend.hpp` | Pak / WAD / dir backend dispatch — a small internal vtable, never exported |
| `IProtocolDriver` | `private/networking/protocol_driver.hpp` | Per-protocol wire-format policy (GoldSrc 48, Xash 49); multiple production impls expected |

These are all intra-process seams. Each has at least one test fake. Seams that
model protocol or format variants (e.g. `IProtocolDriver`) are expected to have
multiple production implementations — see the Q-7 addendum and Q-14 for the
rules governing multi-implementation seams.

______________________________________________________________________

## 3. Open Questions

These questions are raised for discussion; no answer is recorded here.
Each question links to the section that prompted it.

All 22 questions (Q-1 … Q-22) are decided.

### Question index

| Q | Name | Q | Name |
|---|------|---|------|
| Q-1 | SUBSYSTEM_CLASS | Q-11 | SATELLITE_PLACEMENT |
| Q-2 | ENGINE_CONTEXT | Q-12 | COMPAT_SCOPE |
| Q-3 | PIMPL_MOVE | Q-13 | ALLOC_POLICY |
| Q-4 | DI_PARAMS | Q-14 | DRIVER_INHERITANCE |
| Q-5 | ERROR_RETURN | Q-15 | PRECONDITION_DOCS |
| Q-6 | THREADING | Q-16 | CONST_CAST_ISOLATION |
| Q-7 | INTERFACE_ABI | Q-17 | INTERFACE_SIGNATURE_IMPACT |
| Q-8 | STRING_VIEW_BOUNDARY | Q-18 | PM_FP_MODEL |
| Q-9 | OWNERSHIP | Q-19 | PHS_PLACEMENT |
| Q-10 | PLUGIN_VERSION | Q-20 | EDICT_STORE |
| Q-21 | EXTENSION_POSTURE | Q-22 | LIFECYCLE_MODEL |

File order is historical — Q-14 appears before Q-13 below; do not renumber.

______________________________________________________________________

## 3a. Boundary-spec open questions (OQ crosswalk)

Boundary specs carry their own doc-local `OQ-n` lists. **Convention**:
existing docs keep their local numbering and are cited with compound keys
(`server-boundary#OQ-1`, `host-boundary#OQ-2`); **new** boundary docs must
use prefixed IDs (`SRV-OQ-n`, `CL-OQ-n`, …) so bare references stay
unambiguous. An OQ whose answer shapes more than its own subsystem is
**promoted** to a numbered Q entry in this register when decided — Q-18
(PM_FP_MODEL) is the worked example: raised as the pm_shared float-vs-fixed
landmine, decided with its own design brief
(`pm-determinism-decision.md`), and recorded as a register entry.

This table is the live index. Statuses are recorded as-is; nothing is
decided by this table.

| Doc | OQs | Status | Blocks |
|-----|-----|--------|--------|
| `networking-boundary` | OQ-1 … OQ-8 | all closed (decided in-doc) | — |
| `host-boundary` | OQ-1 … OQ-11 | closed, except **OQ-8 deferred** (Sys_NewInstance restart mechanism; re-evaluate after server/client chunks) | none currently |
| `map_loader-boundary` | — (post-implementation spec; its "(OQ-2)" cite means host-boundary#OQ-2) | — | — |
| `server-boundary` | OQ-1 (PHS placement) | ✅ decided 2026-07-04 — promoted to **Q-19 (PHS_PLACEMENT)** | — |
| `server-boundary` | OQ-2 (studio-hull option seam) | open (seam shape at scaffold; null provider until Chunk 7) | impl |
| `server-boundary` | OQ-3 (HPAK placement) | open (may stub uploads for the milestone) | impl |
| `server-boundary` | OQ-4 (listen-server capability seam) | open (shape now, null impl for dedicated) | impl |
| `server-boundary` | OQ-5 (`entvars_t` internal representation) | ✅ decided 2026-07-04 — promoted to **Q-20 (EDICT_STORE)** | — |
| `server-boundary` | OQ-6 (64-bit string-pool strategy) | open (legacy-Windows baseline = heap arena + INT-range fallback) | impl |
| `server-boundary` | OQ-7 (compat routing via ICompatPolicy) | open (proposal in-doc) | impl |
| `server-boundary` | OQ-8 (dedicated-milestone scope trims) | open (stub-marker proposal in-doc) | impl |
| `server-boundary` | OQ-9 (threading posture) | open (assumption: all entry points main-thread) | analysis |

______________________________________________________________________

### SUBSYSTEM_CLASS (Q-1): When should a subsystem be a class vs free functions?

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

______________________________________________________________________

### ENGINE_CONTEXT (Q-2): Should there be a root `EngineContext` that owns all subsystem instances?

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

______________________________________________________________________

### PIMPL_MOVE (Q-3): Standardize the pimpl variant — `unique_ptr<Impl>` or raw `Impl*`?

> **Status**: ✅ DECIDED

**Decision**: New subsystems use `unique_ptr<Impl>` with the
declare-in-header / define-in-`.cpp` pattern documented in
`xash3dpp.instructions.md`.

`cmd_cvar`'s raw `Impl*` variant is grandfathered in; not changed proactively.
Bring into conformance the next time `cmd_cvar` is modified for another reason.

______________________________________________________________________

### DI_PARAMS (Q-4): Init params — positional args or params struct?

> **Status**: ✅ DECIDED

**Decision**: Every subsystem with init parameters uses a named
`<Subsystem>InitParams` struct — no threshold. The params struct carries both
configuration values and injected dependencies, making all dependencies explicit
at the call site.

Subsystems with no init at all (`utilities`, `platform`, `memory`) are exempt.

`Filesystem::Init` with positional args is grandfathered in; it naturally migrates
to the params struct form when `EngineContext` is written and the init call moves
there.

______________________________________________________________________

### ERROR_RETURN (Q-5): Standardize error return patterns

> **Status**: ✅ DECIDED

**Rules for existing patterns** (apply to all new code now):

| Pattern | Use for |
|---------|---------|
| `[[nodiscard]] bool` | Void-or-fail where failure reason does not matter to callers |
| `[[nodiscard]] std::optional<T>` | Value may legitimately be absent — not an error, just "not found" |
| `[[nodiscard]] T*` (nullable) | Pointer return where null is the natural absent sentinel |
| `void` | Infallible operations, or failure handled internally with a fallback |

**Internal logging rule**: the **public API function** — the first entry point
reachable from outside the subsystem — must emit a diagnostic before returning
failure. Private and internal helper functions may propagate failure silently
upward; requiring them to log at every level produces cascading duplicate messages
for a single user-visible error. Exception: `optional<T>` returning `nullopt` for
a "not found" query is always silent by contract.

**Log level at the public boundary** — use the most specific level that fits:

| Condition | Level | Rationale |
|-----------|-------|-----------|
| Unexpected failure returned to caller (invalid arg, resource exhausted, I/O error) | `LogLevel::Error` | Caller gets a failure return *and* the log records why |
| Expected/high-frequency protocol event with no error return (stale sequence, duplicate datagram) | `LogLevel::Verbose` | Would spam logs at higher levels; caller already handles it silently |
| Recoverable degradation the caller is not told about | `LogLevel::Warning` | Something unexpected happened but the subsystem masked it internally |
| Internal invariant violation (logic bug — should never happen) | `XASH_FATAL` / `LogLevel::Fatal` | Terminates after logging |

**`std::expected<T, ErrorCode>`**: active for subsystems where failure reason
matters to callers. `core::ErrorCode` is defined in `core/error.hpp` for shared
engine failures; subsystem-specific result aliases may use narrower local error
enums where that keeps the public surface clearer. When the diagnostics channel
is ready, error events flow as typed `{ subsystem_id, error_code, timestamp }`
structs rather than strings.

All error return values carry `[[nodiscard]]`.

______________________________________________________________________

### THREADING (Q-6): Threading model — what threads will the engine have?

> **Status**: ✅ DECIDED — see `threading-model.md` for the full specification.

**Summary of decisions**:

- Six `ThreadRole` values: `Main`, `AudioCallback`, `AudioDecoder`, `Worker`,
  `Render` (deferred until the renderer/client split), `NetIO` (deferred).
- Worker pool (2–4 threads) started at `Host::init()`; audio threads at
  `Sound::init()`.
- Async asset loading via `JobToken<T>` (atomic status + `unique_ptr` move).
- `assert_main_thread()` replaced by `assert_thread_role(ThreadRole::Main)`,
  defined in `core/thread_role.hpp`.
- Render thread optional — renderer plugin declares `wants_render_thread`;
  double-buffered `RenderFrame` is the main↔render boundary.
- Network I/O thread deferred; networking keeps socket access behind the
  platform/networking seams so later migration remains transparent.
- Entity thinks, player physics, input, and all DLL calls are main-thread-only;
  world queries (BSP trace, PVS) are concurrent-read-safe after map load.
- Two filesystem threading hazards must be fixed before end of Chunk 3.
- Interface rule: query functions take `const T&`; mutation is explicit;
  compute and commit phases are separated in the server tick.

______________________________________________________________________

### INTERFACE_ABI (Q-7): Every `ISubsystem` vtable interface or only for legacy ABI?

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

**Addendum (Chunk 2 — networking)**: The "single concrete production
implementation plus a test fake" observation described the state of the five
pre-networking subsystems; it is not a rule. Any `I<X>` seam that explicitly
exists to model **protocol variants** (e.g. `IProtocolDriver`) is expected to
have multiple production implementations — one per variant. When adding such a
seam, document the selection axis (e.g. wire-protocol number, capability flag)
in the boundary spec and ensure every production variant is exercised by tests.

______________________________________________________________________

### STRING_VIEW_BOUNDARY (Q-8): How should `std::string_view` cross DLL/ABI boundaries?

> **Status**: ✅ DECIDED

**Rule**: `std::string_view` is used freely within the same binary (intra-engine,
same compiler). At any `extern "C"` or DLL boundary, use `const char*` (with
optional `size_t` length if the callee needs it). The receiving side wraps in
`std::string_view` immediately on entry.

No custom `StringRef` type is needed — the rule is simple enough to follow
without a new type.

______________________________________________________________________

### OWNERSHIP (Q-9): Ownership vocabulary across subsystem boundaries

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

______________________________________________________________________

### PLUGIN_VERSION (Q-10): Modular plugin / DLL bootstrap convention

> **Status**: ✅ DECIDED

**Legacy ABIs** (`eiface.h`, `cdll_int.h`, `ref_api.h`) are preserved exactly.

**New plugin types** (renderer/backend candidates are expected around the
Chunk 13 renderer decision) use a versioned C descriptor struct:

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

Deferred until the first new plugin type is introduced; for the current plan,
settle renderer/backend policy before Chunk 13 implementation starts.

______________________________________________________________________

### SATELLITE_PLACEMENT (Q-11): Where do small same-layer satellite features live — separate target, or fold into the parent subsystem?

> **Status**: ✅ DECIDED

**Context**: Several subsystems naturally have small "satellite" features that
share the parent's layer but not its core concern — e.g. networking has an
HTTP downloader and a master-server list; filesystem may grow archive-format
plugins; client may grow demo recording and screenshot capture. Folding
everything into the parent inflates the target and obscures separation of
concerns; spinning every feature into its own target multiplies CMake
boilerplate.

**Decision** — apply the following test:

| Criterion | Same target as parent | Separate target |
|-----------|----------------------|-----------------|
| Shares the parent's wire/format protocol (lives or dies with it) | ✓ | |
| Has its own independent protocol/state machine | | ✓ |
| Pulls in a different external dependency the parent does not need | | ✓ |
| Consumer surface fits in a small interface the parent exposes | ✓ | |
| Useful without the parent at runtime | | ✓ |

A satellite scores **≥ 2 "separate" criteria** → separate target.

When the answer is "separate":

- The new target lives under the parent's logical area (`xash3dpp_<feature>`).
- A grouping pass at the end of a chunk may move several related satellite
  targets into a shared subdirectory (e.g. `src/net/{networking,http,master_list}`)
  — this is a build-system reshuffle, not a code change.
- The satellite gets its own boundary spec under `docs/boundaries/`.

**Applied to networking** (decided in `boundaries/networking-boundary.md`):

- **HTTP downloader** → separate target `xash3dpp_http`. Distinct state
  machine, TCP rather than UDP, different host tick entry point (`HTTP_Run`),
  shares only miniz at the dependency level.
- **Master-server list** → stays inside `xash3dpp_networking`. Speaks UDP OOB
  packets that only netchan can frame; consumer surface (server) is small;
  not useful without the networking transport layer.

Future small features should re-apply this test at their boundary-spec stage.

______________________________________________________________________

### COMPAT_SCOPE (Q-12): Should `ICompatPolicy` be per-subsystem or a single engine-wide compat router?

> **Status**: ✅ DECIDED

**Context**: `cmd_cvar` introduced `ICompatPolicy` for routing GoldSrc
behavioural quirks. As more subsystems acquire compat surfaces (networking
SPLITPACKET-vs-SPLITPACKETGS, server-DLL `entvars_t` quirks, save-format
versioning, content-loader WAD oddities) it would be easy to merge them all
into one giant `IEngineCompatPolicy` table. That is the wrong direction.

**Decision**: **Per-subsystem `ICompatPolicy`** — each subsystem that has
behavioural quirks owns its own small policy interface, named for the
subsystem (e.g. `cmd_cvar::ICompatPolicy`, `networking::IProtocolDriver`,
`server::ICompatPolicy`). Each is selected at link time by the same
`XASH_GOLDSRC_COMPAT` CMake option (or feature-specific variant such as
`XASH_NET_COMPRESSION` for orthogonal toggles).

**Why per-subsystem**:

- Each subsystem's quirk table is small, focused, and individually reviewable.
- The compat surface is a documented part of that subsystem's boundary spec —
  hard to lose track of, easy to remove when a quirk is no longer needed.
- A global table would force every subsystem to depend on every other
  subsystem's compat header — exactly the kind of cross-dependency the
  rewrite is removing.
- Test fakes are trivial — each subsystem ships a `compat_null.cpp` linked
  when the CMake option is OFF, plus a `MockCompatPolicy` in tests.

**Naming convention**: the interface lives in
`include/xash3dpp/private/<subsystem>/compat_policy.hpp` (or a more specific
name where the role is narrower than "all compat", e.g.
`networking/protocol_driver.hpp`). It is **never** exported in the public
subsystem header.

**Cross-cutting compat quirks** (e.g. the wire-protocol version implied by
the connected client) are passed in via the relevant subsystem's `InitParams`
struct or `setup()` argument, not stored in a shared global. The networking
`IProtocolDriver` model in `boundaries/networking-boundary.md` is the
reference example of per-feature compat selection within a subsystem.

Deferred to chunk-by-chunk application: each new subsystem decides its own
compat scope and documents it in its boundary spec.

______________________________________________________________________

### DRIVER_INHERITANCE (Q-14): When may a concrete `I<X>` implementation subclass another concrete implementation?

> **Status**: ✅ DECIDED (Chunk 2 — networking)

**Context**: `XashProtocolDriver` was written as a subclass of
`GoldSrcProtocolDriver` rather than a direct implementation of `IProtocolDriver`.
The two drivers share all wire-codec logic (w1/w2 encoding, reliable-bit masking,
overflow handling); they differ only in four metadata methods (`name()`,
`split_format()`, `delta_tables()`) and the `sends_qport()` policy flag.
Inheriting from the concrete base eliminates codec duplication and keeps the
variant focused on the single axis where it diverges.

**Decision**: When two concrete implementations of `I<X>` share **all** algorithm
logic and differ only in declared metadata or policy flags, one may subclass the
other via the **template-method pattern**:

- The base class implements all shared codec/algorithm logic. It uses its own
  overridable virtual methods as policy points (e.g. `sends_qport()`) so that
  the derived class can change only the policy answers, not the algorithm steps.
- The derived class overrides only the distinguishing virtuals. Its `final`
  keyword may be applied to the derived class, but the **base must NOT be
  `final`** to permit the subclass.
- Any future implementation that diverges in actual **algorithm steps** (not
  merely policy flags) must subclass `I<X>` directly, never the concrete base,
  to avoid coupling to implementation details.

**Decision criterion** — at each new variant, ask: "does this differ in an
algorithm step, or only in a policy/metadata answer?"

| Difference | Correct form |
|------------|--------------|
| Metadata only (name, format id, capability flag) | Subclass the concrete base |
| One algorithm step changes | Subclass `I<X>` directly |
| Entirely different algorithm | New independent implementation of `I<X>` |

**Applicability beyond networking**: the same rule applies wherever a seam has
multiple protocol or format variants:

- **Renderer backends**: two backends (e.g. GL ES 3.0 vs. WebGL) sharing the
  render-graph algorithm but differing in texture-format capability flags →
  subclass the concrete base.
- **Audio codecs**: two codecs sharing the mixing loop but differing in
  sample-rate policy → subclass the concrete base.
- **Archive formats**: two backends sharing streaming but differing in
  compression-table layout → subclass `I<X>` directly (algorithm step differs).

This rule is enforced during code review; the "does it diverge in an algorithm
step?" question should appear explicitly in the PR description when a new
variant is added.

______________________________________________________________________

### ALLOC_POLICY (Q-13): How should `std::vector` and STL containers relate to the framework pool allocator?

> **Status**: ✅ DECIDED

**Context**: `memory::pool_new<T>` and `memory::mem_alloc` give pool accounting
(`memlist`), lifetime-scoped arenas, and fragmentation control. `std::vector<T>`
and other STL containers with the default allocator use `::operator new` directly,
bypassing the pool. Pool-invisible allocations cannot be lifetime-scoped, appear in
no `memlist` report, and may cause unbounded OS-heap traffic on the per-frame path.

**Options considered**:

- **A — explicit pools only (current implicit practice)**: `pool_new<T>` / `mem_alloc`
  for objects; STL containers free to use `::operator new`. Zero implementation cost,
  but STL growth on the hot path produces unpredictable allocation latency spikes,
  long-run heap fragmentation, and cross-thread cache-line contention on the heap lock.

- **B — pre-reserve discipline (chosen)**: STL containers remain `::operator new`-backed
  but **hot-path** containers must call `.reserve(N)` at init time using a named limit
  from `limits.hpp`. After reserve, every `push_back` within capacity is a single
  cache-warm write — zero OS calls, zero lock contention, zero fragmentation growth
  during gameplay. Cold-path containers (init, parsing, loading, teardown) are exempt.
  The one-time `reserve()` at init pays one OS allocation; all frame-rate-sensitive paths
  are fully deterministic thereafter.

- **C — `PoolAllocator<T>` for STL**: route STL allocation through the framework pool
  via a C++ stateful allocator. Performance analysis shows this only helps if the pool
  is a fixed-size slab or bump-pointer arena — neither compatible with `std::vector`'s
  variable-size growth strategy. Over a general-purpose pool, Option C adds pool-bookkeeping
  overhead on top of what is still a `malloc`-equivalent, making hot-path cost *higher*
  than Option B. C's true value is **observability** (`memlist` coverage), not
  performance. Implementation cost is also high: stateful allocator `rebind`,
  `propagate_on_container_*`, allocator-equality semantics, and MSVC debug-iterator
  interaction all require careful implementation.

**Decision**: **Option B — pre-reserve discipline.**

| Code path | Rule |
|-----------|------|
| **Hot** — per-frame (packet recv/send, entity updates, physics) | `pool_new<T>` for objects. `std::vector` allowed only if `.reserve(N)` is called at init using a `limits.hpp` constant. Mark the member with `// @pre-reserved: <LIMIT_NAME>`. |
| **Warm** — occasional (netchan fragment accumulation, config reload) | `std::vector` freely. `pool_new<T>` preferred for long-lived objects. |
| **Cold** — init, parsing, resource loading, teardown | `std::vector` and `std::make_unique` both acceptable. No annotation required. |

**`PoolAllocator<T>` migration trigger** (deferred — do not implement before
then): heap fragmentation or allocation latency visible in profiler data, OR a
subsystem has > 500 KB of STL container memory invisible to `memlist`. Document
the trigger event in a new Q entry when it is met.

**Audit enforcement** (`detail-audit` check `ALLOC_POLICY`):
- Any `std::vector` or `std::deque` class member in a hot-path class body that
  lacks a `// @pre-reserved: <LIMIT_NAME>` comment → **WARNING**
- Pre-reserve annotation present but no `.reserve()` in `init()` or constructor
  → **WARNING**
- Custom `PoolAllocator<T>` implementation landed before the migration trigger is
  met → **BLOCKER**

______________________________________________________________________

### PRECONDITION_DOCS (Q-15): Documenting non-trivial API preconditions

> **Status**: ✅ DECIDED

**Context**: `Netchan::process()` requires that the caller has already matched
the incoming datagram to this specific channel by source address (and, for
GoldSrc, by qport). This invariant cannot be expressed as a type — violating it
causes silent sequence-state corruption rather than an obvious crash. A `// Pre:`
comment was added ad-hoc; the pattern needs to be a named convention.

**Decision**: Any public function with a non-trivial precondition that cannot be
expressed as a type parameter must carry a `// Pre:` comment immediately before
(or on) the declaration.

A precondition is **non-trivial** when both:
- No type-system enforcement is possible (no `std::span`, `gsl::not_null`, etc. can model it), **AND**
- Violating it causes silent corruption or undefined behaviour rather than an obvious assertion failure.

```cpp
// Pre: the caller has already identified this channel as the correct
//      destination for `datagram` by matching the source address.
//      Routing a datagram to the wrong channel silently corrupts sequence state.
[[nodiscard]] bool process( MessageBuf& datagram ) noexcept;
```

`// Post:` may be used symmetrically for guaranteed postconditions.

**Scope**: public API declarations only. Private helper functions document
invariants with `XASH_ASSERT` rather than `// Pre:` comments.

______________________________________________________________________

### CONST_CAST_ISOLATION (Q-16): `const_cast` must live in a named abstraction

> **Status**: ✅ DECIDED

**Context**: `MessageBuf` supports both read-mode (over `span<const byte>`) and
write-mode (over `span<byte>`). Its internal representation stores a mutable span;
constructing the read-mode view requires exactly one `const_cast`. Placing that
cast inline at every `process()` call site distributes a SAFETY argument that must
be maintained centrally.

**Decision**: A `const_cast` away from `const` in production code must be wrapped
in a **named function** with a `// SAFETY:` comment explaining the invariant that
makes the cast correct.

```cpp
// SAFETY: rebind_read constructs a read-only view. The span is only ever
//         passed to read-path functions; no mutation occurs after this call.
void rebind_read( std::span<const std::byte> buf ) noexcept;
```

Inline `const_cast` at call sites is **forbidden** — the reasoning is silently
duplicated or dropped at every copy.

**Exception**: `const_cast` in test files for test-setup purposes is permitted,
with the same `// SAFETY:` comment.

**Note**: this rule is about *reasoning locality*, not frequency. Even a single
`const_cast` needs a named home.

______________________________________________________________________

### INTERFACE_SIGNATURE_IMPACT (Q-17): `assess-impact` required for `I<X>` signature changes

> **Status**: ✅ DECIDED

**Context**: Adding `bool is_server_socket` to `IProtocolDriver::read_packet_header()`
required updating: the interface header, both concrete implementations
(`GoldSrcProtocolDriver`, `XashProtocolDriver`), the `StubDriver` in
`test_netchan.cpp`, `Netchan::process()`, and all call sites in
`test_protocol_driver_registry.cpp`. An informal grep pass could easily miss the
test stub or a seldom-used call site, producing a build that compiles per-TU but
links to an ODR-violating stub that silently returns wrong data.

**Decision**: Any change to an `I<X>` interface signature (adding, removing, or
reordering parameters; changing a parameter type; adding or removing `const` or
`noexcept`) requires:

1. Running `assess-impact` before touching any implementation file.
2. Listing in the commit message or PR description: every concrete implementation
   updated, every test stub updated, and every direct call site updated.

**Blast radius template** for any `I<X>` method change:

| Category | What to search for |
|----------|--------------------|
| Interface declaration | `I<X>.hpp` — the pure-virtual declaration |
| Concrete implementations | `class A : public I<X>` — in any source file, any subsystem |
| Test stubs | `class StubX : public I<X>` — in test files |
| Direct call sites | any `.method(` call through an `I<X>*` or `I<X>&` reference |
| Documentation | boundary spec, design notes, any doc referencing the signature |

**Fix order**: interface header → concrete impls → test stubs → call sites → docs.
Commit only after all five categories are updated and the build is green.

______________________________________________________________________

### PM_FP_MODEL (Q-18): engine float math is strict-by-default, relaxable per presentation target

> **Status**: ✅ DECIDED (2026-07-04; full analysis in `pm-determinism-decision.md`)

**Context**: The engine does not own player-movement math — mod authors compile
pm_shared into both their game DLL and client DLL (frozen float `playermove_t`
ABI, invoked through the `pfnPM_Move` seam), so no engine-side representation
change can make the *system* deterministic; fixed-point on our side would only
diverge from the mods' float half and change which jumps are makeable.
Meanwhile FMA contraction (`-ffp-contract=fast`, the GCC/Clang default at
`-O2`) lets identical source produce different last-bit results on x64 vs ARM
— and trace/prediction code is full of knife-edge comparisons that a single
ULP can flip. Legacy release builds are effectively strict already (`/O2`
without any fast-math), and residual mod-vs-engine drift is absorbed by the
proven prediction-reconciliation + wire-quantisation loop.

**Decision**: All engine-side PM/trace/physics math stays `float`, matching
the frozen ABI. The FP compilation model is **strict by default, globally**:
`/fp:precise` (MSVC, explicit) and `-ffp-contract=off` (GCC/Clang), set in
the root `CMakeLists.txt`; fast-math build modes are forbidden for the
simulation-critical set. Future relaxation is an explicit per-target **option**
(mirroring the link-time compat-isolation pattern): `xash3dpp_relax_fp(target)`
in `cmake/fp_model.cmake`.

**Netcode vs. non-netcode implications**:

| Target class | Rule | Rationale |
|--------------|------|-----------|
| Simulation-critical: `xash3dpp_networking`, `xash3dpp_map_loader` (world/trace/PVS), future `world`/`physics`/`server`, `utilities` math on their paths | **Never relax.** Strict FP forever; golden trace fixtures gate the map_loader chunk. | Results feed traces, prediction, and the wire; cross-arch bit-reproducibility eliminates a whole class of "works on x86, not on ARM" physics divergences. |
| Presentation-side: renderer, particles, audio DSP (all future) | May call `xash3dpp_relax_fp()` when profiling justifies it. | Their math never crosses the wire or feeds simulation; FMA/fast-math is a low-single-digit-% FLOP win with zero netcode risk. |

**Costs**: `/fp:precise` is the MSVC default (zero change); `-ffp-contract=off`
forgoes FMA in trace loops that are memory/branch-bound anyway — unmeasurable
at frame level. Revisit only if a future non-GoldSrc protocol wants lockstep
cross-platform simulation; that would be a new opt-in quantised path, not a
change to the compat engine.

______________________________________________________________________

### PHS_PLACEMENT (Q-19): PHS lives in map_loader as a load-time query module

> **Status**: ✅ DECIDED (2026-07-04; raised as `server-boundary#OQ-1`)

**Context**: Legacy builds the PHS in `mod_bmodel.c` (`Mod_CalcPHS`,
OpenMP-parallel) at map load, derived purely from BSP visdata; it is
immutable afterwards. The server is the sole consumer (`pfnSetFatPAS`,
`SV_Multicast` PAS routing, radius 8). The map_loader boundary deferred
`Mod_CalcPHS` and the PHS row source of `Mod_FatPVS` to Chunk 6, but the
fat-vis recursive BSP walk itself already lives in `xash3dpp_map_loader`.

**Decision**: `xash3dpp_map_loader` gains a `phs` query module (built during
world load, immutable after — same home and lifecycle as PVS). Rationale:
all BSP-derived immutable data stays in one place (the Q-6 immutability
posture), the existing fat-vis walk is reused instead of duplicated in
server code, and any build parallelism stays inside load per the
server-boundary OQ-9 threading posture. The server consumes it through the
map_loader query API only. Parity gate: byte-parity golden fixture against
GoldSrc PHS dumps (`mod_bmodel.c:3845-3859` parity note). Landing as part
of Chunk 6 scope (it is server-driven work even though the code lives in
map_loader).

______________________________________________________________________

### EDICT_STORE (Q-20): one ABI-exact edict store behind a zero-cost seam

> **Status**: ✅ DECIDED (2026-07-04; raised as `server-boundary#OQ-5`)

**Context**: Game DLLs see raw `edict_t` arrays and do byte-offset
arithmetic over them (`PEntityOfEntOffset` is a byte offset from the
`svgame.edicts` base); gameplay depends on stale entvars persisting into
slot reuse; the save format mirrors exact `entvars_t` field order. Any
shadow representation must project at every one of the 159 engine-func +
50 DLL-func boundary crossings — each projection bug a silent gameplay
divergence, doubling the state the Q-18 determinism regime must keep
honest. A future non-GoldSrc game ABI is a **load-time flavor** (one game
DLL per server process), so what keeps that door open is not the data
layout but how coupled engine-internal code is to `entvars_t`.

**Decision**: The ABI-exact `edict_t` array is the **single authoritative
store** — no shadow copies, no projection. One arena class owns allocation
and lifecycle (ED_Alloc free-list, serialnumbers, `freetime` grace,
deliberate stale-field reuse). Engine-internal code addresses entities by
index/ref through **inline typed accessors that compile to direct array
access** (zero runtime cost, no divergence possible). Raw
`entvars_t`/`edict_t` access is confined to the owners of the memory
contract: the ABI shim (`src/abi/` + `src/server/abi/`), the pmove bridge,
and the Chunk 8 save serializer. Enforcement: a compliance-scan rule (no
`->v.` outside the allowed set) added when the server scaffold lands.
Handleization or a new ABI format is a post-parity, load-time
binding/arena flavor behind this seam — never a Chunk 6 concern.

______________________________________________________________________

### EXTENSION_POSTURE (Q-21): long-term extension goals are a binding review axis

> **Status**: ✅ DECIDED (2026-07-05; full requirements in `extension-goals.md`)

**Context**: The rewrite's long-term purpose includes experimental features
beyond GoldSrc parity: an in-engine MCP service, a multithreading-suitable
game ABI ("v2") with a reworked HL SDK, a dedicated debug thread, and
expanded in-game debugging (G-1 … G-4 in `extension-goals.md`). None is
scheduled, and parity-first chunks legitimately keep legacy-shaped cores
while the frozen ABI demands it (Q-20). But everyday rewrite decisions can
silently close the doors those features need — a new file-scope global, a
context-less entry point, a debug backdoor that bypasses typed surfaces, a
whole-runtime function signature. The threading model already contains the
enabling rules (§8.1 context parameters, §9 Rule 3 compute/commit, Rule 4 no
new globals); what was missing was a named requirements register and workflow
hooks binding work to it.

**Decision**: `docs/design/extension-goals.md` is the binding requirements
document for extension posture. Concretely:

- **Boundary specs** include an **"Extension axes (Q-21)"** section
  evaluating the subsystem against the goals (G-1 … G-4) and primitives
  (P-1 … P-6); a reasoned "none apply" is a valid answer.
- **Modernization audits** tag findings that open or protect a door
  (`[EXT:G-n]` / `[EXT:P-n]`) and promote them one priority tier.
- **The door rules bind new code** (see §4.3): context-first entry points
  with no new file-scope mutable state beyond documented ABI exceptions
  (P-3); off-main access only via the inbox / published-snapshot patterns
  (P-1, P-2); introspection through typed surfaces, never backdoors (P-4);
  narrowest-state signatures for free functions over runtime aggregates
  (P-5); experimental features placed as satellite targets per the Q-11
  test (P-6).
- **Feature design stays out of scope until promoted**: a goal enters
  `implementation-plan.md` only through its own design brief
  (`pm-determinism-decision.md` is the precedent); ABI v2 additionally
  captures the existing SDK-rework prototype's constraints as brief input.
- **Parity precedence**: inside a parity-gated subsystem, byte-exact GoldSrc
  behaviour wins over a door rule; the deviation is recorded in
  `extension-goals.md` as known door-debt.

Enforcement is by the workflow surface (the `analyse-subsystem` and
`analyse-modernization` prompts carry the hooks) and reviewer attention at
the normal gates; no automated scanner rule yet — add one if a door
violation ever slips a review.

______________________________________________________________________

### LIFECYCLE_MODEL (Q-22): classes with invariants, pool-owned allocation, narrowest-state signatures

> **Status**: ✅ DECIDED (2026-07-06; applied retroactively by **Chunk 6B**,
> the full-retrofit wave)

**Context**: The register regulated subsystem *packaging* (Q-1..Q-4) but not
internal shape — whether a subsystem's internals are encapsulated classes or
free functions over a runtime aggregate was unregulated, and the completed
subsystems diverged (networking/cmd_cvar encapsulated; server procedural per
its parity-first sanction). Pool accounting was likewise idiomatic but
unwritten: the tree already ships the preferred pool-owned-class shape
(`File`, `ISearchBackend`) without a rule naming it. The long-term goals
(Q-21) need lifecycle clarity, visible read/write sets, and complete pool
accounting.

**Decision**:

1. **State with invariants lives in a class with an RAII lifecycle.**
   Free-functions-over-aggregate style is reserved for **orchestrators**
   (frame loops, lifecycle sequencing, ABI dispatch tables). This refines
   Q-1's subsystem-level criterion down into subsystem internals; Q-1's
   packaging rules are unchanged.
2. **Pool-owned-class idiom (canonical)**: a `create_<thing>` factory
   holding the injected `PoolHandle` constructs via `pool_new<T>`; the class
   overrides **both** `operator delete` overloads (unsized + sized) routing
   to `mem_free`; a plain `std::unique_ptr<T>` (default deleter) then owns
   it. Precedents: filesystem `File` + `ISearchBackend`; references:
   `memory.hpp` `PoolDeleter` note, `architecture/memory/typed-helpers.md`.
3. **Class-scoped `operator new` is forbidden** — it cannot carry the
   injected handle, forcing a global or thread-local pool, which violates
   the Q-2 no-globals DI model.
4. **Smart-pointer policy**: `std::make_unique<T>` is banned except for
   pimpl `Impl` (Q-3/Q-9 unchanged). `std::unique_ptr<T>` with the default
   deleter is allowed exactly when `T` carries the `operator delete` pair
   and construction went through a `pool_new` factory.
5. **Alignment**: `pool_new<T>` requires `alignof(T) ≤ 8` — the pool
   `AllocHeader` guarantees only ≥8-byte payload alignment. Enforced by a
   `static_assert` in `pool_new`. If aligned allocation ever lands, the
   aligned-`operator delete` overload set must be revisited with it.
6. **Narrowest-state signatures**: free functions over aggregates take the
   smallest sub-aggregate they touch; whole-aggregate parameters are
   reserved for orchestrators. (Elevates extension-goals P-5 to binding.)
7. **Naming riders**: pool-factory verb is `create_<thing>` (the lone
   `make_os_file` outlier renames at its 6B session); methods promoted from
   legacy-echo free functions drop the now-redundant subsystem prefix
   (`sv_run_cmd` → `ClientMachinery::run_cmd`) with the rename recorded in
   the **crosswalk** so parity greppability survives; pool display names are
   subsystem-prefixed snake_case when a subsystem owns several.
8. **Promotion safety**: classes promoted from aggregates whose storage is
   self-bound (buffers bound into owning-struct storage, back-pointers)
   preserve **address stability** — copy/move deleted per QJ unless an
   explicit rebind path exists.

**Enforcement**: compliance rules `class-operator-new` (blocker),
`make-unique-outside-pimpl`, `operator-delete-pairing`, and the revised
`unique-ptr-nonpimpl`; detail-audit CHECK-LIFECYCLE; reviewer charter §14.
Applies to all new code; the completed subsystems are brought into
conformance by Chunk 6B.

______________________________________________________________________

## 4. Application Schedule

All open questions are decided. This section records when each rule applies.

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
- Multi-implementation seams: when `I<X>` models protocol variants, one impl
  per variant is expected — document selection axis in boundary spec (Q-7 addendum)
- Concrete-subclass (template-method) pattern: subclass a concrete base only
  when variants differ in policy flags, not algorithm steps (Q-14)
- Ownership vocabulary table (Q-9)
- `std::expected<T, ErrorCode>` for rich failure modes where callers need typed
  failure details (Q-5)
- Versioned C plugin descriptor for future new plugin types; renderer/backend
  policy is settled before Chunk 13 implementation starts (Q-10)
- Separate-target test for satellite features at boundary-spec stage (Q-11)
- Per-subsystem `ICompatPolicy` (or feature-specific variant) named for the
  subsystem; link-time selected; never exported publicly (Q-12)
- Hot-path `std::vector` members pre-reserved at init; member marked
  `// @pre-reserved: <LIMIT_NAME>` (Q-13)
- Non-trivial API preconditions documented with `// Pre:` on the declaration (Q-15)
- `const_cast` away from `const` wrapped in a named function with `// SAFETY:` (Q-16)
- `I<X>` interface signature changes require `assess-impact` first; commit message
  must enumerate all impls, test stubs, and call sites updated (Q-17)
- Float math strict-by-default (`/fp:precise`, `-ffp-contract=off`); relaxation
  is a per-presentation-target option via `xash3dpp_relax_fp()`, never for
  simulation-critical targets (Q-18)
- BSP-derived immutable query data (PVS, PHS, hulls) lives in `map_loader`,
  built at load; consumers use the query API, never rebuild or cache their
  own copies (Q-19)
- Server entity state: the ABI-exact edict array is the single store; engine
  internals go through the zero-cost typed accessor facade; raw
  `entvars_t`/`edict_t` access only in the ABI shim, pmove bridge, and save
  serializer (Q-20)
- Extension posture: the `extension-goals.md` door rules — context-first
  entry points (no new file-scope mutable state beyond documented ABI
  exceptions), off-main access only via inbox / published-snapshot patterns,
  introspection via typed surfaces, narrowest-state signatures over runtime
  aggregates, experimental features as Q-11 satellites; boundary specs carry
  an "Extension axes" section (Q-21)
- Lifecycle model: state with invariants → RAII class; orchestrators may
  stay free functions; pool-owned classes use the `create_<thing>` factory +
  dual-`operator delete` idiom; class `operator new` forbidden;
  `make_unique` only for pimpl `Impl`; `unique_ptr<T>` default deleter only
  over the operator-delete pair; `alignof(T) ≤ 8` for `pool_new`;
  narrowest-state signatures; promoted-method renames recorded in the
  crosswalk (Q-22)

______________________________________________________________________

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
