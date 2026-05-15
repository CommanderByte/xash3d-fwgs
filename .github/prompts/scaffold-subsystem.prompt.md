---
name: "Scaffold new xash3dpp subsystem"
description: "Create the full directory skeleton for a new xash3dpp subsystem: public headers, private headers, CMakeLists, stub implementation, and a test harness. Wires in xash3dpp_memory and xash3dpp_utilities. Use when starting a brand-new subsystem from scratch."
argument-hint: "subsystem name, e.g. 'sound', 'renderer', 'physics', 'networking'"
agent: agent
tools: [read, search, edit]
---

# Scaffold new subsystem: `$ARGUMENTS`

Create the full skeleton for the **$ARGUMENTS** subsystem inside `xash3dpp/`.

---

## Step 0 — Check what already exists

Before touching any file, read the existing subsystems to avoid duplicating
infrastructure that is already provided:

1. **Memory** — [`xash3dpp/include/xash3dpp/memory/memory.hpp`](../../xash3dpp/include/xash3dpp/memory/memory.hpp)
   All dynamic allocations must go through `create_pool` / `mem_alloc` /
   `pool_new` etc. Never use `malloc`, `new`, or `std::make_unique` directly
   inside the subsystem.

2. **Utilities** — scan `xash3dpp/include/xash3dpp/utilities/` for helpers
   that may already exist: string operations, path joining, hashing, CRC/MD5,
   math, etc. Do not re-implement anything found here.

3. **Platform layer** — check `xash3dpp/include/xash3dpp/platform/` for any
   OS-abstraction types or functions relevant to `$ARGUMENTS`.

4. **Existing subsystems as structural reference** — read
   `xash3dpp/src/filesystem/CMakeLists.txt` and
   `xash3dpp/src/memory/CMakeLists.txt` to understand the canonical CMake
   target shape. Mirror that shape exactly.

Collect the names of any utilities/platform helpers you will use. Record them
in a brief comment at the top of the implementation stub.

---

## Step 2 — Write the boundary doc

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

## Open questions
Design decisions still outstanding.
```

---

## Step 3 — Create the public header

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
//     bool Init(/* parameters */);
//     void Shutdown();
//
// private:
//     struct Impl;
//     std::unique_ptr<Impl> impl_;
// };

} // namespace xash::$ARGUMENTS
```

Follow the pimpl pattern used by `xash::filesystem::Filesystem` — no internal
types leak into the public header.

---

## Step 4 — Create the CMake target

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
target_link_libraries(xash3dpp_$ARGUMENTS
    PUBLIC  xash3dpp_utilities
    PUBLIC  xash3dpp_memory
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

---

## Step 5 — Create the stub implementation

Create `xash3dpp/src/$ARGUMENTS/$ARGUMENTS.cpp`:

```cpp
// xash3dpp — $ARGUMENTS subsystem implementation
// Legacy reference: <path, or "n/a">
//
// Existing subsystems used:
//   xash3dpp_memory    — pool-backed allocations
//   xash3dpp_utilities — <list helpers>

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

bool <Subsystem>::Init(/* params */)
{
    impl_->pool_ = xash::memory::create_pool("$ARGUMENTS");
    return static_cast<bool>(impl_->pool_);
}

void <Subsystem>::Shutdown()
{
    // Release all pool-owned resources before destroying the pool.
    if (impl_->pool_) {
        xash::memory::destroy_pool(impl_->pool_);
        impl_->pool_ = {};
    }
}

} // namespace xash::$ARGUMENTS
```

**Note**: `Impl` itself is heap-allocated via `std::make_unique` (before the
pool exists). Everything the subsystem allocates *after* `Init()` must go
through `impl_->pool_`.

---

## Step 6 — Create the test harness

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
// Covers: Init/Shutdown lifecycle, <add function names as the API grows>

#include <xash3dpp/$ARGUMENTS/$ARGUMENTS.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <cstdio>

static int g_pass = 0, g_fail = 0;

#define CHECK(expr) \
    do { if (expr) { ++g_pass; } \
         else { ++g_fail; std::printf("FAIL [line %d]: %s\n", __LINE__, #expr); } } while(0)

// ---------------------------------------------------------------------------
// Lifecycle smoke test
// ---------------------------------------------------------------------------

static void test_init_shutdown()
{
    xash::$ARGUMENTS::<Subsystem> s;
    CHECK( s.Init() );
    s.Shutdown();
    // Re-init must work (idempotent lifecycle).
    CHECK( s.Init() );
    s.Shutdown();
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

## Step 7 — Verify

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
