# Design Decisions — Code Style and Conventions

> **Status**: all questions decided — see individual Q sections below
> **Scope**: naming, instrumentation, assertions, logging, and test conventions
> **Relationship to decisions-architecture.md**: that document covers structural paradigms (object model,
> threading, error returns, ownership, plugin ABI). This document covers the remaining
> consistency questions identified by surveying the first five completed subsystems.
> **Application**: §4 records what applies now, when next touched, and new code only

______________________________________________________________________

## 1. What This Document Is

The five subsystems completed so far were also surveyed for coding-style consistency
beyond the structural decisions recorded in `decisions-architecture.md`. This
document records decisions on: naming conventions, `[[nodiscard]]` completeness,
`constexpr` policy, integer type policy, runtime assertions, the diagnostic/logging
channel, copy/move semantics, and test framework.

______________________________________________________________________

## 2. Observations Already Consistent — No Debate Needed

The following patterns are unanimous across all existing subsystems. They are recorded
as confirmed conventions, not open questions.

| Convention | Rule |
|------------|------|
| Include guards | `#pragma once` in every header — no traditional `#ifndef` guards |
| Type aliases | `using T = ...` — never `typedef` |
| `noexcept` | All public functions are `noexcept`. No exceptions flow across subsystem boundaries. |
| `std::` qualification | Full `std::` qualification everywhere in headers. No `using namespace` or `using std::X` at header scope. |
| Named constants | `inline constexpr` in headers — never `#define` for numeric constants |

______________________________________________________________________

## 3. Decisions

______________________________________________________________________

### NODISCARD (QA): `[[nodiscard]]` completeness

> **Status**: ✅ DECIDED

**Observation**: `memory.hpp` applies `[[nodiscard]]` comprehensively. `filesystem.hpp`
has none. No rule existed for when to omit it.

**Decision**: `[[nodiscard]]` is the **default** for every non-`void` return. Omitting
it requires a documented reason at the declaration site.

Apply to:

- All functions returning error indicators (`bool`, `optional<T>`, `expected<T,E>`)
- All functions returning owned resources (`pool_ptr`, `unique_ptr`, handles)
- All functions returning computed values where discarding is a likely bug
  (`stats()`, hash results, arithmetic helpers)

Omit only when the result is genuinely advisory or the function is intentionally
called for side-effects only, and document the omission:

```cpp
// [[nodiscard]] omitted — result is advisory; caller may ignore if already logged
void report_stats() noexcept;
```

`filesystem.hpp`'s missing annotations are a gap; add them opportunistically when
the file is next touched for another reason.

______________________________________________________________________

### CONSTEXPR_POLICY (QB): `constexpr` policy

> **Status**: ✅ DECIDED

**Decision**: Mark a function `constexpr` when its entire body is evaluable at
compile time (no dynamic allocation, no I/O, no non-`constexpr` calls) and when
compile-time evaluation is a plausible use case. Do not force it.

`constexpr` applies naturally to:

- Value-type comparison and conversion operators (`PoolHandle::operator==`, `operator bool`)
- Short arithmetic / bit-manipulation helpers over inputs with no side effects
- `valid()` and similar predicates on handle types

Do not add `constexpr` retroactively. It should follow naturally from the
implementation.

______________________________________________________________________

### NAMING_FN (QE): Method naming — `PascalCase` or `snake_case`?

> **Status**: ✅ DECIDED

**Observation**: `Filesystem` uses PascalCase methods (`Init`, `Open`, `FileExists`,
`LoadFile`); `CmdCvarContext` uses snake_case (`init`, `shutdown`). Free functions
are universally snake_case (`mem_alloc`, `cvar_find`, `create_pool`). The two
class-method styles are inconsistent.

**Decision**: **`snake_case` for all member functions on implementation classes.**

Rationale: consistent with the majority of existing member functions (`init`,
`shutdown`, `cvar_find`), with all free functions, and with the C++ STL. PascalCase
for methods conflates methods visually with type names.

Full naming table:

| Category | Convention | Examples |
|----------|-----------|---------|
| Types (class, struct, enum) | `PascalCase` | `Filesystem`, `PoolHandle`, `LogLevel` |
| Member functions | `snake_case` | `init()`, `shutdown()`, `open()`, `file_exists()` |
| Free functions | `snake_case` | `mem_alloc()`, `cvar_find()`, `create_pool()` |
| Namespaces | `lowercase` | `xash::filesystem`, `xash::memory`, `xash::platform` |
| Macros | `UPPER_SNAKE_CASE` | `XASH_ASSERT`, `XASH_GOLDSRC_COMPAT` |
| File names | `snake_case.{hpp,cpp}` | `filesystem.hpp`, `cmd_cvar.cpp` |
| Private member variables | `snake_case_` (trailing `_`) | `impl_`, `handle_`, `stats_` |
| Local variables | `snake_case` | `pool`, `pos`, `entries` |
| Constants (`inline constexpr`) | `k_snake_case` | `k_null_pool`, `k_max_path` |

**Exceptions:**

- `IFilesystem` vtable method names match the VFileSystem009 legacy ABI (`Open`,
  `BaseDir`, etc.). These are ABI-fixed and explicitly exempt from snake_case.
- Any `I<X>` vtable method whose name is dictated by a legacy ABI is exempt.

**Grandfathered**: `Filesystem`'s PascalCase methods (`Init`, `Open`, etc.) are not
changed proactively. Migrate to snake_case when those methods are touched for another
reason (opportunistic conformance, same rule as Q-3/Q-4).

______________________________________________________________________

### NAMING_ENUM (QF): Enum value naming

> **Status**: ✅ DECIDED

**Observation**: `memory.hpp` uses the Google-style `k` prefix (`kSystem`, `kArena`,
`kNullPool`). No other subsystem has defined `enum class` values yet.

**Decision**: **`PascalCase` without `k` prefix** for all `enum class` values in
new code.

Rationale: `enum class` already provides the namespace (`AllocStrategy::System`),
so the `k` prefix adds no disambiguation value and introduces visual noise. `PascalCase`
is consistent with how types are named throughout the project.

```cpp
// New standard:
enum class LogLevel    { Verbose, Info, Warning, Error, Fatal };
enum class AllocStrategy { System, Arena, Slab };
enum class ThreadRole  { Main, AudioCallback, AudioDecoder, Worker, Render, NetIO };

// Grandfathered in memory.hpp (migrate opportunistically):
// kSystem, kArena, kSlab, kNullPool
```

`memory.hpp`'s `k`-prefixed values migrate to `PascalCase` when memory is next
touched for another reason.

______________________________________________________________________

### INT_TYPES (QG): Integer type policy

> **Status**: ✅ DECIDED

**Rules by context:**

| Context | Type | Rationale |
|---------|------|-----------|
| Sizes, counts, lengths, buffer capacities | `std::size_t` | Correct type for `sizeof`-family quantities; matches STL |
| GoldSrc ABI values (entity index, edict number, model index, player slot) | `int` | Match the ABI exactly — widening or shrinking breaks interop |
| Internal xash3dpp tokens and handles | `uint32_t` | Fixed-width; these are not sizes |
| File offsets | `int64_t` | Files can exceed 2 GB |
| Bitmask flags (internal) | `uint32_t` | Unsigned, fixed-width; matches GoldSrc flag width |
| Protocol integers (packet lengths, channel IDs) | explicit-width per protocol spec | Document the source spec |
| Loop counters over small known ranges | `int` | Standard C++ idiom; explicit width adds no value |
| Boolean quantities | `bool` | Never `int`, `uint8_t`, or Win32 `BOOL` |
| Pointer differences | `std::ptrdiff_t` | Signed; handles negative differences |
| Opaque memory addresses | `std::uintptr_t` | Portable unsigned address type |

**Additional rules:**

- Never use bare `unsigned` or `unsigned int` — always qualify with a width (`uint32_t`)
  or use the semantic type (`size_t`).
- Never silently cast `size_t` to `int`. When a GoldSrc `int` index must be compared
  to an internal `size_t` count, validate `count ≤ INT_MAX` before casting and
  document the reason.

______________________________________________________________________

### ASSERTIONS (QH): Runtime assertion strategy

> **Status**: ✅ DECIDED

**Context**: Q-5 error returns handle *expected failures* — "file not found",
"network timeout". Assertions handle *invariant violations* — states that are
impossible if the code is correct. These are distinct mechanisms with separate tools.

**Decision**: Two tiers, defined in `core/assert.hpp`.

**Tier 1: `XASH_ASSERT(expr)` — debug-only**

Compiles to a no-op in `NDEBUG` builds. Use for: invariants that "should never be
violated in correct code" but where a release build might limp on safely.

```cpp
#ifndef NDEBUG
#   define XASH_ASSERT(expr) \
        do { if (!(expr)) {                                             \
            core::log(core::LogLevel::Fatal, "assert", #expr); \
            platform::crash::abort();                                   \
        } } while(0)
#else
#   define XASH_ASSERT(expr) ((void)0)
#endif
```

**Tier 2: `XASH_FATAL(expr, msg)` — always enabled**

Never stripped. Use for: unrecoverable invariant violations that must not be silently
ignored in any build — NULL pointer that truly cannot be NULL, shutdown called twice
on a subsystem, pool handle referencing a destroyed pool.

```cpp
#define XASH_FATAL(expr, msg)                                                 \
    do { if (!(expr)) {                                                       \
        core::log(core::LogLevel::Fatal, "assert", msg ": " #expr);  \
        platform::crash::abort();                                             \
    } } while(0)
```

**Decision table:**

| Condition | Mechanism |
|-----------|-----------|
| Expected operational failure (file not found, network error) | Q-5: `bool`/`optional`/`expected` + `core::log(Error, ...)` |
| Invariant — "should never happen", debug crash acceptable | `XASH_ASSERT(expr)` |
| Invariant — must never happen in any build | `XASH_FATAL(expr, msg)` |
| Thread role contract | `assert_thread_role(role)` (threading model, Q-6) |

Do **not** use `assert()` from `<cassert>` directly. It lacks the logging call, its
macro expansion under MSVC can surprise (`__LINE__` in `__FILE__` paths), and it
provides no message parameter.

______________________________________________________________________

### LOGGING (QI): Diagnostic / logging channel

> **Status**: ✅ DECIDED

**Context**: Q-5 says "emit a diagnostic before returning a failure indicator". The
logging mechanism itself was left undefined. `XASH_ASSERT`/`XASH_FATAL` (QH above)
depend on it. Networking (Chunk 2) needs it for structured error reporting.

**Design constraints:**

- Callable before `EngineContext` is constructed (startup failures, early-init errors)
- Callable from any thread including `T_AudioCallback`
- No heap allocation in the log call itself
- `noexcept`
- Long-term: route to a diagnostics channel; hook must exist from day one

**Decision: `core::log` free functions in `core/log.hpp`**

```cpp
namespace xash::core {

enum class LogLevel { Verbose, Info, Warning, Error, Fatal };

// Core: no allocation, no formatting. Safe in any context, any thread.
void log(LogLevel level, std::string_view tag, std::string_view msg) noexcept;

// Formatted: formats to a 512-char stack buffer via platform::snprintf, then
// calls log(). Truncates silently on overflow. Safe anywhere log() is.
void logf(LogLevel level, std::string_view tag,
          XASH_PRINTF_FORMAT const char *fmt, ...) noexcept;

// Routing callback — set by the diagnostics subsystem when ready.
// Pass nullptr to revert to default console output.
using LogCallback = void (*)(LogLevel, std::string_view tag,
                             std::string_view msg) noexcept;
void log_set_callback(LogCallback cb) noexcept;

} // namespace xash::core
```

**Tag convention**: subsystem name in `snake_case`, lower-case, matching the
subsystem's directory name: `"filesystem"`, `"networking"`, `"cmd_cvar"`, `"platform"`.

**Default output format** (wraps `platform::console::write`):

```text
[filesystem][WARN]: could not open "valve/pak0.pak"
[networking][ERROR]: connect timeout after 10s
```

**`platform::console::write` vs `core::log`**:

- `platform::console::write` is the **correct and encouraged** mechanism for
  intentional game-console output: command echoes, `cvarlist`/`cmdlist`
  listings, help text, progress messages directed at the player or developer.
  The game console is a first-class output channel.
- `core::log` is the **C++ subsystem diagnostic channel**: allocation
  failures, invalid arguments, internal invariant violations, and any
  condition a C++ caller needs to be informed of alongside a failure return.
  Its default implementation wraps `console::write` with the structured
  `[tag][LEVEL]:` prefix, so messages are still visible in the console.
- `printf` / `fprintf` / `std::cout` are **always forbidden** in `src/`.
  They bypass both channels and cannot be routed or filtered.

**Log level policy:**

| Level | Use for | Always emitted? |
|-------|---------|-----------------|
| `Verbose` | High-frequency debug info | Only when `XASH_VERBOSE` defined |
| `Info` | Normal operational messages (game dir activated, pak mounted) | Yes |
| `Warning` | Unexpected but recoverable — caller was not notified via return | Yes |
| `Error` | Operation failed; caller was **also** notified via return value | Yes |
| `Fatal` | Assertion failures (`XASH_FATAL`, `XASH_ASSERT`) | Yes — then `crash::abort()` |

**Rule for ERROR_RETURN (Q-5) compliance**: "emit a diagnostic" means call
`core::log(LogLevel::Error, tag, msg)` or `core::logf(...)` *before*
the `return false` / `return std::nullopt` at the **public API boundary**.
The return value tells the caller the operation failed; the log tells diagnostics
*why*. Private helper functions propagate failure silently — logging at every level
produces duplicate messages for a single error.

**`Verbose` guard pattern** (zero overhead when `XASH_VERBOSE` is absent):

```cpp
#ifdef XASH_VERBOSE
    core::logf(LogLevel::Verbose, "filesystem", "scan: %s", path.data());
#endif
```

**Thread safety**: the implementation must be thread-safe (atomic callback pointer
or a fast spinlock). Callers do not synchronise.

**Deferred**: structured `{ subsystem_id, error_code, timestamp }` events for the
diagnostics channel are deferred to the diagnostics subsystem (Chunk TBD).
`log_set_callback` is the hook that will route to it.

**Before Chunk 2 (scheduling)**: `core/log.hpp` and its implementation must
exist before networking is written, since networking errors are the first case where
`LogLevel::Error` is needed in production code.

______________________________________________________________________

### COPY_MOVE (QJ): Copy/move semantics for subsystem types

> **Status**: ✅ DECIDED

No general rule existed. Five categories cover all cases:

| Category | Copy | Move | Examples |
|----------|------|------|---------|
| **Subsystem context class** (pimpl, owned by `EngineContext`) | `= delete` | declared in header, `= default` in `.cpp` | `Filesystem`, `CmdCvarContext` |
| **`EngineContext` itself** | `= delete` | `= delete` | — |
| **Value aggregates** (`*Stats`, `*InitParams`, `*Config`) | compiler-generated | compiler-generated | `PoolStats`, `FilesystemInitParams` |
| **Copyable handle** (handle is just a value/index) | compiler-generated | compiler-generated | `PoolHandle` |
| **Exclusive-ownership handle** (wraps a kernel object) | `= delete` | declared + defined | future `FileHandle` |
| **RAII wrapper** (scoped resource, not a subsystem) | `= delete` | declared + defined | `ScopedPool` |

**Why `EngineContext` deletes move**: `EngineContext` is the root owner. Moving it
would invalidate all captured `T*` / `I<X>*` references stored in `*InitParams`
structs and held by subsystems. It must occupy a stable address for its entire
lifetime. It is constructed once in `main()` (or `Host::init`) and never moved.

**Subsystem context move pattern** (same as Q-3 / pimpl move note in instructions):

```cpp
// In the header (Impl is incomplete here):
Filesystem(Filesystem&&) noexcept;
Filesystem& operator=(Filesystem&&) noexcept;

// In the .cpp (Impl is complete here):
Filesystem::Filesystem(Filesystem&&) noexcept            = default;
Filesystem& Filesystem::operator=(Filesystem&&) noexcept = default;
```

______________________________________________________________________

### TEST_MACROS (QK): Test framework

> **Status**: ✅ DECIDED

**Observation**: Existing tests use a hand-rolled `CHECK` macro and `g_pass`/`g_fail`
counters. Functional but not standardised across files.

**Decision**: Keep hand-rolled. Standardise the macro set in a shared header
`xash3dpp/tests/test_helpers.hpp`.

**Standard macro set:**

```cpp
// Non-fatal: records failure, test continues
#define CHECK(expr)
#define CHECK_EQ(a, b)    // (a) == (b)
#define CHECK_NE(a, b)    // (a) != (b)
#define CHECK_STREQ(a, b) // std::strcmp((a), (b)) == 0
#define CHECK_LT(a, b)    // (a) < (b)
#define CHECK_LE(a, b)    // (a) <= (b)

// Fatal: records failure, calls std::abort() — acceptable in test binaries
#define REQUIRE(expr)
```

**Test function convention:**

```cpp
static void test_<feature>() {
    // test body — use CHECK / REQUIRE
}

int main() {
    test_feature_a();
    test_feature_b();
    // ...
    return (g_fail > 0) ? 1 : 0;
}
```

**Revisit trigger**: when the project reaches ~15 test files, or when fixtures,
parameterised tests, or death tests are needed — evaluate **doctest** at that point.
doctest supports `DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS`, is compatible
with `/EHs-c-`, and can be vendored as a single header. If we switch, doctest is the
preferred library over GoogleTest (which requires exceptions).

______________________________________________________________________

### ARRAY_SIZE_STACK (QL): No oversized `std::array` in class bodies

> **Status**: ✅ DECIDED — lesson learned from production segfault in `PacketPool`

**Rule**: Any `std::array<T, N>` **member variable** where `N * sizeof(T) > 65 536` (64 KB)
is forbidden in class bodies. Such an array would overflow the 1 MB default Windows
thread stack if the object is ever constructed on the stack, and it bloats the BSS
segment when it is heap-allocated.

Use `std::vector<T>` (reserve/resize `N` in the constructor) or
`std::unique_ptr<std::array<T, N>>` instead.

**The bug** (recorded so it is never repeated): `PacketPool` originally declared
`std::array<Slot, 64> slots_` where each `Slot` held a `std::array<std::byte, 16384>`.
64 × 16 384 = 1 048 576 bytes — exactly the 1 MB Windows default thread stack.
The object was heap-allocated, but the BSS reservation still resulted in a
stack-overflow abort on first access through the VTable. Replacing `slots_` with
`std::vector<Slot>` (with `slots_.resize(64)` in the constructor) fixed it.

**Safe thresholds** (rule of thumb):

| Aggregate size | In a stack local | In a class member | Verdict |
|---------------|-----------------|-------------------|---------|
| ≤ 4 KB | ✅ | ✅ | Always safe |
| 4 KB – 64 KB | ✅ with care | ✅ (if class is heap-only) | Safe; add comment |
| > 64 KB | ❌ | ❌ | Use `std::vector` or `unique_ptr` |

**Fix pattern**:
```cpp
// ❌ Forbidden for large N:
std::array<Slot, slot_count> slots_;

// ✅ Correct:
std::vector<Slot> slots_;   // in .cpp constructor: slots_.resize(slot_count);
```

______________________________________________________________________

### NS_QUALIFY (QM): Absolute qualification for sibling-namespace references

> **Status**: ✅ DECIDED — lesson learned from compile failure in `xash::networking::`

**Rule**: Inside any nested namespace `xash::X::`, any reference to a **sibling**
namespace (`xash::Y::`) must use the absolute leading `::`:

```cpp
// Inside xash::networking::  — WRONG (compiler may find xash::networking::limits):
auto n = limits::net_max_datagram;           // ❌

// CORRECT — always routes to xash::limits, regardless of nesting:
auto n = ::xash::limits::net_max_datagram;   // ✅
```

**Why it matters**: C++ unqualified name lookup walks outward through enclosing
scopes. Inside `xash::networking::`, `limits` first matches any `limits` declared
inside `xash::networking::` (or in an inline-namespace layer). The top-level
`xash::limits` is only reached if nothing shadows it. The bug may compile silently
in some TUs and fail in others depending on header-include order.

**Applies to all xash sibling namespaces** accessed from within a nested namespace:
`::xash::limits::`, `::xash::utilities::`, `::xash::memory::`, `::xash::platform::`,
`::xash::core::`, etc.

**Not required for `std::`** — `std` is a top-level name and is never shadowed by
anything inside an `xash::X::` scope. The existing rule "full `std::` qualification"
still applies; `::std::` is not required.

**Using-declarations at `.cpp` file scope are fine**:
```cpp
// In a .cpp, outside any namespace — unambiguous:
using ::xash::limits::net_max_datagram;
```

**Note for headers**: prefer the full `::xash::Y::Z` form in headers; using-declarations
at header scope pollute every including TU.

______________________________________________________________________

## 4. Application Schedule

### 4.1 Must happen before Chunk 2

These are blocking for networking (Chunk 2):

- **QI: `core::log`** — networking errors are the first production use of
  `LogLevel::Error`. `core/log.hpp` and its implementation must exist first.
- **QH: `core/assert.hpp`** — `XASH_ASSERT` and `XASH_FATAL` must be defined
  before any subsystem relies on them. Low-effort; can be done in a single small PR.

### 4.2 Bring into conformance when next touched

- **QE**: `Filesystem` PascalCase methods → snake_case when any method is changed
- **QA**: `filesystem.hpp` missing `[[nodiscard]]` → add when the file is next modified
- **QF**: `memory.hpp` `kSystem`, `kArena`, `kSlab`, `kNullPool` → drop `k` prefix
  when memory is next modified
- **QK**: `test_filesystem.cpp` hand-rolled macros → migrate to standard set when
  tests are next extended

### 4.3 New code only

All rules in this document apply from the first line of any new subsystem or test file:

- `snake_case` member functions (QE)
- `[[nodiscard]]` on every non-void return by default (QA)
- `constexpr` on pure value-type operations (QB)
- `PascalCase` enum values (QF)
- Integer type rules: `size_t` for sizes, `int` for GoldSrc ABI, `uint32_t` for
  internal tokens, `int64_t` for file offsets (QG)
- `XASH_ASSERT` / `XASH_FATAL` for invariant checking (QH)
- `core::log` for C++ subsystem diagnostics; `platform::console::write` for
  intentional game-console output; never `printf`/`fprintf`/`std::cout` (QI)
- Copy/move semantics per category table (QJ)
- Standard test macro set (QK)
