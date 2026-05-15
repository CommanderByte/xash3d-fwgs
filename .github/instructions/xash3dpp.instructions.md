---
applyTo: "xash3dpp/**"
---

## xash3dpp Conventions

This tree is a self-contained C++ project, intended to become its own repository.
The legacy engine at the repo root is the behavioural reference only.

- **Language**: C++20. No exceptions, no RTTI (`/EHs-c- /GR-` on MSVC; `-fno-exceptions -fno-rtti` on GCC/Clang).
- **Build system**: CMake (not Waf). Add targets under `xash3dpp/cmake/`.
- **Tests**: go in `xash3dpp/tests/`. Mirror the subsystem path: `src/filesystem/` → `tests/filesystem/`.
- **Public headers** (API exposed to other subsystems) live in `xash3dpp/include/xash3dpp/<subsystem>/`. Private/internal headers — including implementation-detail headers shared between TUs of the same subsystem — live in `xash3dpp/include/xash3dpp/private/<subsystem>/`, mirroring the public tree. Only `.cpp` files go in `xash3dpp/src/`; do **not** put `.hpp` files under `src/`. A single CMake `PATTERN "private" EXCLUDE` rule keeps private headers out of any install target.
- **Third-party deps**: vendor under `xash3dpp/3rdparty/`. Do not reuse the repo-root `3rdparty/`.
- **Design notes** for a subsystem go in `xash3dpp/docs/` before implementation starts.

## Before Writing Any Code

1. Read the corresponding legacy subsystem using search and file tools.
2. Write a short boundary note in `xash3dpp/docs/` covering: what the module exposes, what invariants it must preserve, and any quirks found in the legacy code.
3. Then implement.

## Recurring Patterns — Mandatory

### limits.hpp
Subsystem-specific buffer sizes, pool capacities, and fixed-count limits belong
in `xash3dpp/include/xash3dpp/limits.hpp`, not as magic literals in headers or
source files.  Use the `#ifndef XASH_LIMIT_<NAME>` / `inline constexpr` /
`#else` / `#endif` override pattern.  Group limits under a `// <subsystem>
subsystem` comment block.

### Pimpl move operations
When a class owns a `std::unique_ptr<Impl>`, deleting its copy constructor
also suppresses the implicit move constructor.  The correct pattern:

- **Header** (where `Impl` is incomplete): *declare* the move operations:
  ```cpp
  <Class>(<Class>&&) noexcept;
  <Class>& operator=(<Class>&&) noexcept;
  ```
- **`.cpp`** (where `Impl` is complete): *define* them:
  ```cpp
  <Class>::<Class>(<Class>&&) noexcept            = default;
  <Class>& <Class>::operator=(<Class>&&) noexcept = default;
  ```

Writing `= default` in the header triggers instantiation of
`unique_ptr<Impl>`'s destructor before `Impl` is defined, causing a
compile error in every TU that includes the header.

### Compat isolation
Use a CMake option (e.g. `XASH_GOLDSRC_COMPAT`) to select between two `.cpp`
files at link time (`compat_goldsrc.cpp` / `compat_null.cpp`).  Zero
`#ifdef` guards in core logic.
