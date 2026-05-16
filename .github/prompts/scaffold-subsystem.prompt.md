---
name: "Scaffold new xash3dpp subsystem"
description: "Create the full directory skeleton for a new xash3dpp subsystem: public headers, private headers, CMakeLists, stub implementation, and a test harness. Wires in xash3dpp_memory, xash3dpp_utilities, and xash3dpp_filesystem where appropriate. Use when starting a brand-new subsystem from scratch."
argument-hint: "subsystem name, e.g. 'sound', 'renderer', 'physics', 'networking'"
mode: agent
tools: [read, search, edit]
model: claude-sonnet-4-6
---

# Scaffold new subsystem: `$ARGUMENTS`

Create the full skeleton for the **$ARGUMENTS** subsystem inside `xash3dpp/`.

---

## Step 0 — Check what already exists

Before touching any file, read the existing subsystems to avoid duplicating
infrastructure that is already provided:

1. **Memory** — [`xash3dpp/include/xash3dpp/memory/memory.hpp`](../../xash3dpp/include/xash3dpp/memory/memory.hpp)
   All dynamic allocations must go through `create_pool` / `mem_alloc` /
   `pool_new` / `pool_dup` etc. `malloc`, `new`, `std::make_unique` are
   forbidden inside the subsystem — the *only* exception is
   `std::make_unique<Impl>()` for the pimpl struct itself (allocated before
   the subsystem pool exists).

2. **Utilities** — do **not** re-implement anything already in
   `xash3dpp/include/xash3dpp/utilities/`. Key headers:
   - `string.hpp` — `strncpy`, `stricmp`, `snprintf`, `atoi`, `parse_token`, `Tokenizer`
   - `path.hpp` — `path_join`, `file_base`, `replace_extension`, `fix_slashes`
   - `hash.hpp` — `Crc32Hasher`, `Md5Hasher`, `crc32()` (use these; never write a custom hash)
   - `math.hpp` / `matrix.hpp` — `Vec3`, `Matrix3x4`, `dot`, `normalize`, …
   - `utf.hpp` — `Utf8Decoder`, `encode_utf8`, `utf16_to_utf8`

   Architecture reference: [`xash3dpp/docs/architecture/utilities/`](../../xash3dpp/docs/architecture/utilities/README.md)

3. **Filesystem** — if `$ARGUMENTS` reads config files, model data, or any
   on-disk resource, link against `xash3dpp_filesystem` and accept a
   `Filesystem&` reference in the init-params struct. Never call `fopen`,
   `fclose`, `FILE *`, `CreateFile`, or `open()` directly.
   Architecture reference: [`xash3dpp/docs/architecture/filesystem/`](../../xash3dpp/docs/architecture/filesystem/README.md)

4. **Platform layer** — for monotonic time use `platform::get_time()`;
   for console output use `platform::console::write()`; for debugger detection
   use `platform::is_debugger_present()`. Do not call OS timer or I/O APIs
   directly. Check `xash3dpp/include/xash3dpp/platform/` before reaching for
   any OS primitive.

   If `$ARGUMENTS` requires socket I/O, it must route through `IPlatformSockets`
   (not yet implemented — requirements in
   [`xash3dpp/docs/architecture/platform/sockets.md`](../../xash3dpp/docs/architecture/platform/sockets.md)).
   Do **not** call `::socket()`, `::bind()`, `::sendto()`, `::recvfrom()`, or
   any Winsock / BSD-socket API directly outside
   `src/platform/*/os_socket.cpp`. Record `IPlatformSockets` as a blocker in
   the boundary spec if networking is required.

5. **Core** — all structured logging must use `core::log` / `core::logf`
   (never `printf` or `platform::console::write` directly); use `XASH_ASSERT`
   and `XASH_FATAL` (never `assert()` from `<cassert>`); call
   `core::register_thread_role(ThreadRole::Main)` at thread-start and
   `core::assert_thread_role(ThreadRole::Main)` inside every main-thread-only
   public function.
   Headers: `core/log.hpp`, `core/assert.hpp`, `core/thread_role.hpp`

6. **Existing subsystems as structural reference** — read
   `xash3dpp/src/filesystem/CMakeLists.txt` and
   `xash3dpp/src/memory/CMakeLists.txt` to understand the canonical CMake
   target shape. Mirror that shape exactly.

7. **Satellite features** — list any sub-features of `$ARGUMENTS` that are
   conceptually distinct (e.g. a downloader, a discovery/heartbeat protocol,
   an async resolver). For each, apply the Q-11 satellite test from
   [`decisions-architecture.md §Q-11`](../../xash3dpp/docs/design/decisions-architecture.md):
   score ≥ 2 → **separate CMake target**; score < 2 → same target. Record
   the verdict in the boundary spec (Step 1) and set up the extra target(s)
   in Step 3 if required.

Collect the names of any utilities/platform helpers you will use. Record them
in a brief comment at the top of the implementation stub.

---

## Step 1 — Write the boundary doc

Create `xash3dpp/docs/boundaries/$ARGUMENTS-boundary.md` before writing any
code. Use this template:

```markdown
# <Subsystem> Boundary Spec

## Responsibility
One paragraph.

## External ABI contracts
Game DLL / client DLL hooks this module drives, or "None — fully internal."

## Interface (what the rest of the engine calls)
| Symbol | Kind | Description |
|--------|------|-------------|
| ...    | ...  | ...         |

## Dependencies (what this module calls)
| Subsystem | Why |
|-----------|-----|
| xash3dpp_memory    | Pool-backed allocations |
| xash3dpp_utilities | (list specific helpers used) |
| ...        | ... |

## Owned state
Significant global / static state.

## Quirks and invariants
- Bullet list of non-obvious behaviours the rewrite must preserve.

## Satellite components
Sub-features evaluated with the Q-11 separation test.
| Feature | Criteria met | Verdict (same / separate) |
|---------|-------------|---------------------------|
| ...     | ...         | ...                        |

## Open questions
Design decisions still outstanding.
```

---

## Step 2 — Create the public header

Create `xash3dpp/include/xash3dpp/$ARGUMENTS/$ARGUMENTS.hpp`:

```cpp
#pragma once
// xash3dpp — <Subsystem> public API
// Legacy reference: <path in legacy tree, or "n/a — new subsystem">

#include <cstddef>
#include <cstdint>
// Add other standard headers as needed.
// Do NOT include private/ headers here.

namespace xash::$ARGUMENTS {

// Forward-declare the pimpl class if the main class is non-trivial.
// class <Subsystem>;

// ---------------------------------------------------------------------------
// Public types
// ---------------------------------------------------------------------------

// TODO: declare public structs / enums here.

// ---------------------------------------------------------------------------
// Main class (pimpl pattern)
// ---------------------------------------------------------------------------

// class <Subsystem> {
// public:
//     <Subsystem>();
//     ~<Subsystem>();
//
//     <Subsystem>(const <Subsystem>&)            = delete;
//     <Subsystem>& operator=(const <Subsystem>&) = delete;
//
//     bool init(/* parameters */);
//     void shutdown();
//
// private:
//     struct Impl;
//     std::unique_ptr<Impl> impl_;
// };

} // namespace xash::$ARGUMENTS
```

Follow the pimpl pattern used by `xash::filesystem::Filesystem` — no internal
types leak into the public header.

**Pimpl move operations**: Deleting the copy constructor (required when owning
`std::unique_ptr<Impl>`) also suppresses the implicit move constructor.
Explicitly *declare* the move constructor and move-assignment in the header:
```cpp
    <Class>(<Class>&&) noexcept;
    <Class>& operator=(<Class>&&) noexcept;
```
But **define them as `= default` in the `.cpp` file** where `Impl` is
complete — never `= default` in the header.  Putting `= default` in the
header instantiates `unique_ptr<Impl>`'s destructor in every TU that includes
the header, before `Impl` is defined, causing a compile error.

---

## Step 3 — Create the CMake target

Create `xash3dpp/src/$ARGUMENTS/CMakeLists.txt`:

```cmake
add_library(xash3dpp_$ARGUMENTS STATIC
    $ARGUMENTS.cpp
    # Add further .cpp files as the subsystem grows.
)

target_include_directories(xash3dpp_$ARGUMENTS
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_$ARGUMENTS PUBLIC cxx_std_20)

# xash3dpp_memory is always required — every subsystem that allocates uses it.
# xash3dpp_utilities provides string, path, hash, and math helpers.
# xash3dpp_filesystem is required if this subsystem reads or writes any files.
target_link_libraries(xash3dpp_$ARGUMENTS
    PUBLIC  xash3dpp_utilities
    PUBLIC  xash3dpp_memory
    # PUBLIC  xash3dpp_filesystem   # uncomment if the subsystem uses the VFS
    # PUBLIC  xash3dpp_platform     # uncomment if the subsystem uses OS primitives (time, crash, console)
)

# Platform-specific additions go here, e.g.:
# if(WIN32)
#     target_link_libraries(xash3dpp_$ARGUMENTS PRIVATE ...)
# endif()
```

Then add to `xash3dpp/CMakeLists.txt` (or the appropriate parent
`CMakeLists.txt`) under the existing `add_subdirectory` block:

```cmake
add_subdirectory(src/$ARGUMENTS)
```

**Subsystem limits**: If `$ARGUMENTS` uses any fixed buffer sizes, pool
capacities, or count limits, add them to
`xash3dpp/include/xash3dpp/limits.hpp` under a new `// $ARGUMENTS subsystem`
comment block, following the existing `#ifndef XASH_LIMIT_* / inline constexpr /
#else / #endif` pattern.  Do not put magic number literals in headers or
source files.

---

## Step 4 — Create the stub implementation

Create `xash3dpp/src/$ARGUMENTS/$ARGUMENTS.cpp`:

```cpp
// xash3dpp — $ARGUMENTS subsystem implementation
// Legacy reference: <path, or "n/a">
//
// Existing subsystems used:
//   xash3dpp_memory     — pool-backed allocations
//   xash3dpp_utilities  — <list helpers, e.g. path::join, string::stricmp>
//   xash3dpp_filesystem — <remove if not needed; lists file I/O ops used>

#include <xash3dpp/$ARGUMENTS/$ARGUMENTS.hpp>
#include <xash3dpp/memory/memory.hpp>
// Include utility headers for helpers identified in Step 0.

namespace xash::$ARGUMENTS {

// ---------------------------------------------------------------------------
// Pimpl body
// ---------------------------------------------------------------------------

struct <Subsystem>::Impl {
    xash::memory::PoolHandle pool_;
    // TODO: subsystem state
};

<Subsystem>::<Subsystem>()  : impl_{std::make_unique<Impl>()} {}
<Subsystem>::~<Subsystem>() = default;

bool <Subsystem>::init(/* params */)
{
    impl_->pool_ = xash::memory::create_pool("$ARGUMENTS");
    return static_cast<bool>(impl_->pool_);
}

void <Subsystem>::shutdown()
{
    // Release all pool-owned resources before destroying the pool.
    if (impl_->pool_) {
        xash::memory::destroy_pool(impl_->pool_);
        impl_->pool_ = {};
    }
}

} // namespace xash::$ARGUMENTS
```

**Stats struct** (add to the public header if the subsystem has non-trivial hot
paths — see `xash3dpp/docs/design/debug-stats-design.md` for the full model):

```cpp
// Three-tier instrumentation — add only the tiers this subsystem needs.
struct $ARGUMENTS_Stats {
    // Tier 1 — always-on (no guard): ≤ 1 relaxed atomic per event.
    // std::atomic<std::uint64_t> items_processed{0};

#if XASH_STATS
    // Tier 2 — lightweight bookkeeping: peak counts, high-water marks.
    // std::uint32_t peak_active_count{0};
#endif

#if XASH_DEBUG_$ARGUMENTS
    // Tier 3 — dev-only heavy tracing: change logs, histograms, break helpers.
#endif
};
```

Expose it from the context class:
```cpp
// In the public class:
const $ARGUMENTS_Stats& stats() const noexcept;
```

**Rules**: never gate counter *increments* on a runtime bool; never format
strings inside a hot path (format on demand in a query command instead).

Add the move-operation definitions here (alongside the destructor):
```cpp
<Class>::<Class>(<Class>&&) noexcept            = default;
<Class>& <Class>::operator=(<Class>&&) noexcept = default;
```

**Note**: `Impl` itself is heap-allocated via `std::make_unique` (before the
pool exists). Everything the subsystem allocates *after* `Init()` must go
through `impl_->pool_`.

---

## Step 5 — Create the test harness

Create `xash3dpp/tests/$ARGUMENTS/CMakeLists.txt`:

```cmake
find_package(Threads REQUIRED)

add_executable(test_$ARGUMENTS test_$ARGUMENTS.cpp)
target_link_libraries(test_$ARGUMENTS PRIVATE
    xash3dpp_$ARGUMENTS
    xash3dpp_memory
    Threads::Threads
)
add_test(NAME test_$ARGUMENTS COMMAND test_$ARGUMENTS)
```

Create `xash3dpp/tests/$ARGUMENTS/test_$ARGUMENTS.cpp`:

```cpp
// xash3dpp — $ARGUMENTS subsystem tests
// Covers: init/shutdown lifecycle, <add function names as the API grows>

#include <xash3dpp/$ARGUMENTS/$ARGUMENTS.hpp>
#include <xash3dpp/memory/memory.hpp>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Lifecycle smoke test
// ---------------------------------------------------------------------------

static void test_init_shutdown()
{
    xash::$ARGUMENTS::<Subsystem> s;
    CHECK( s.init() );
    s.shutdown();
    // Re-init must work (idempotent lifecycle).
    CHECK( s.init() );
    s.shutdown();
}

// TODO: add functional tests as the API grows.

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    test_init_shutdown();

    std::printf("$ARGUMENTS: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
```

Add the test directory to the tests `CMakeLists.txt` (usually
`xash3dpp/tests/CMakeLists.txt`):

```cmake
add_subdirectory($ARGUMENTS)
```

---

## Step 6 — Verify

Run `get_errors` on every file you created or modified. Fix any diagnostics
before finishing.

Confirm:
- [ ] Boundary doc written in `docs/boundaries/$ARGUMENTS-boundary.md`
- [ ] Public header at `include/xash3dpp/$ARGUMENTS/$ARGUMENTS.hpp`
- [ ] No internal types in the public header
- [ ] CMake target links `xash3dpp_memory` and `xash3dpp_utilities`
- [ ] `add_subdirectory` added to the parent `CMakeLists.txt`
- [ ] Stub implementation uses `create_pool` / `destroy_pool`
- [ ] Test harness compiles and runs cleanly
- [ ] No `malloc`, `free`, `new`, `delete` outside of `std::make_unique<Impl>`
- [ ] No `.hpp` files under `src/`.  Any header shared between TUs but not public lives in `include/xash3dpp/private/$ARGUMENTS/`.
- [ ] Pimpl move ctor/assignment declared in header, defined `= default` in `.cpp`
- [ ] Any fixed buffer/count limits added to `limits.hpp` with `XASH_LIMIT_*` override macros
- [ ] Stats struct defined for hot-path subsystems (`<Subsystem>Stats` with appropriate tiers)
- [ ] `stats() const noexcept` accessor exposed from the context class
- [ ] No string formatting in hot paths — raw counters only

---

## Step 7 — Commit

Once Step 6 Verify is fully green:

```
git add xash3dpp/src/$ARGUMENTS/ xash3dpp/include/xash3dpp/$ARGUMENTS/ \
        xash3dpp/include/xash3dpp/private/$ARGUMENTS/ \
        xash3dpp/tests/$ARGUMENTS/ xash3dpp/CMakeLists.txt \
        xash3dpp/docs/boundaries/$ARGUMENTS-boundary.md
git commit -m "$ARGUMENTS: scaffold subsystem skeleton"
```

Commit message bullets:

- Boundary doc location.
- Public header / CMake target name.
- What the stub implements vs. what is left as TODO.
