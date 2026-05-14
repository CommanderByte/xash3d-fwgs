# xash3dpp — Modular C++ Rewrite

This tree is the self-contained seed for the modular C++ rewrite of Xash3D FWGS.
It is intended to eventually become its own repository (and be consumed here as a
submodule), so it is structured as a standalone project from the start.

The original engine source remains at the repository root and is the authoritative
legacy reference. New modules land here, subsystem by subsystem, while the legacy
tree stays buildable and runnable throughout.

## Ground Rules

- Preserve C-compatible ABI surfaces (game DLL, client DLL, renderer, filesystem
  plugin, shared SDK headers). Internal implementation language is C++.
- Keep exceptions and RTTI disabled for engine internals unless a later decision
  explicitly revisits this.
- Every subsystem must be independently buildable and manually smoke-testable
  before the next subsystem starts.
- Record compatibility-sensitive quirks instead of silently removing them.

The legacy incremental-upgrade analysis lives in
[Documentation/codex/](../Documentation/codex/) and is kept for reference, but
the architecture for this rewrite will be determined by a fresh analysis.

## Directory Layout

```
xash3dpp/
  src/          C++ source
  include/      Public C-compatible headers (ABI surfaces exposed to game DLLs)
  tests/        Unit and integration tests
  docs/         Project-local design and architecture notes
  cmake/        CMake modules and toolchain files
  tools/        Build helpers, code-generation scripts, linting configs
  3rdparty/     Vendored third-party dependencies
```
