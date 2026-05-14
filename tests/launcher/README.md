# Launcher Tests

This folder contains unit tests for target-neutral launcher helpers built by
`src/wscript`.

These tests should avoid opening GUI message boxes, loading real engine DLLs,
or depending on Steam assets. Runtime launch smoke tests remain documented in
`Documentation/codex/windows-build-run-notes.md`.

## Current Coverage

- `application.cpp` covers the shared launcher application runner with a fake
  engine library, including shutdown and load-failure behavior.
- `launch_settings.cpp` covers engine library labels, exported engine symbol
  names, SDL2 dependency probe metadata, default game directory propagation, and
  menu change-game flag calculation.
- `engine_library.cpp` covers default engine-library state, unloaded run
  failure behavior, and idempotent unload behavior without loading a real
  engine library.
