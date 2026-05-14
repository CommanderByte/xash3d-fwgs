---
applyTo: "xash3dpp/**"
---

## xash3dpp Conventions

This tree is a self-contained C++ project, intended to become its own repository.
The legacy engine at the repo root is the behavioural reference only.

- **Language**: C++20. No exceptions, no RTTI (`/EHs-c- /GR-` on MSVC; `-fno-exceptions -fno-rtti` on GCC/Clang).
- **Build system**: CMake (not Waf). Add targets under `xash3dpp/cmake/`.
- **Tests**: go in `xash3dpp/tests/`. Mirror the subsystem path: `src/filesystem/` → `tests/filesystem/`.
- **Public headers** (C ABI surfaces exposed to game DLLs) live in `xash3dpp/include/`. Internal C++ headers stay beside their `.cpp` files in `src/`.
- **Third-party deps**: vendor under `xash3dpp/3rdparty/`. Do not reuse the repo-root `3rdparty/`.
- **Design notes** for a subsystem go in `xash3dpp/docs/` before implementation starts.

## Before Writing Any Code

1. Read the corresponding legacy subsystem using search and file tools.
2. Write a short boundary note in `xash3dpp/docs/` covering: what the module exposes, what invariants it must preserve, and any quirks found in the legacy code.
3. Then implement.
