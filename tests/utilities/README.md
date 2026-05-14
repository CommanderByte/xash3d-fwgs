# Utility Tests

This folder contains focused tests for subsystem-neutral modern utilities.

Current coverage:

- `registry.cpp`: fixed-capacity ordered registry behavior, duplicate policy,
  replacement, stable enumeration, and string-key helpers

These tests should avoid depending on engine runtime state.
