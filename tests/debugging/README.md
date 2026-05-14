# Debugging Utility Tests

This folder contains focused tests for the modern debugging utility layer.

The tests are built by `src/wscript` when `--enable-tests` is set. They cover
shared utility behavior before subsystem-specific adapters, such as filesystem
debug snapshots, start depending on it.

Initial coverage:

- debug status helpers
- synchronous buffer sink behavior
- JSON string escaping
- streaming JSON writer object/array state handling
- human and JSON snapshot header writers
- log gates and logger dispatch
- trace gates and bounded trace queue behavior

These tests should stay independent of real game assets and engine runtime
state.
