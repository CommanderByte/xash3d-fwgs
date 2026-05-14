# String And Path Migration Guide

## Ownership

String and path helpers are subsystem-neutral utilities. They should live under
`src/utilities` when migrated, with `public/crtlib.h` remaining the C-compatible
ABI for legacy callers.

Do not move these helpers into `src/engine` unless the helper depends on engine
state, command/cvar ownership, filesystem handles, or console output.

## Current Shape

```text
public/crtlib.h
        |
        +-- numeric conversion C symbols
        |       |
        |       +-- src/utilities/compat/crtlib_conversion.cpp
        |               |
        |               +-- src/utilities/conversion.cpp
        |
        +-- path helper C symbols
                |
                +-- src/utilities/compat/crtlib_path.cpp
                        |
                        +-- src/utilities/path.cpp
```

Inline helpers such as `COM_FixSlashes`, `Q_strncpy`, and `Q_strncat` still live
in `public/crtlib.h`. Modern code can use `src/utilities/path.hpp` directly
when it does not need the C surface.

## Rules

- Keep `public/crtlib.h` C-compatible.
- Route public C symbols through `src/utilities/compat/` when the
  implementation moves to C++.
- Preserve path parsing quirks unless a task explicitly declares a behavior
  cleanup.
- Keep formatting, parsing, and wildcard matching out of the path helper module.
- Keep numeric conversion quirks in `src/utilities/conversion.*`, not in path
  helpers.
- Add public C tests for ABI behavior and `tests/utilities` coverage for modern
  helper behavior.

## Deferred Helpers

The numeric conversion family moved in Phase 47; see
`public-crt-conversion-guide.md`.

The next safe candidates are probably `Q_strnlwr` or `Q_memfgets`. Bounded
copy/concat remain inline ABI helpers. `COM_ParseFileSafe`, wildcard matching,
and `Q_vsnprintf` should be separate phases because they are config, script,
and console sensitive.
