# Subsystem Plan Template

Copy this template when drafting a subsystem migration note.

## Subsystem

Name:

Current owner files:

Related public headers:

Related build targets:

## Current Responsibilities

- Responsibility 1.
- Responsibility 2.
- Responsibility 3.

## Public Or Compatibility Boundaries

- C ABI exports:
- Callback tables:
- Shared structs:
- File formats or protocols:
- Behavior quirks:

## Current Coupling

- Global state used:
- Cross-subsystem calls:
- Compile-time feature flags:
- Platform-specific paths:

## Proposed Internal Shape

- New internal types:
- Functions that remain wrappers:
- State that can move early:
- State that must not move yet:

## Test And Smoke Coverage

- Existing tests:
- New tests needed:
- Manual smoke recipe:
- Logs or artifacts to compare:

## Migration Steps

1. Step one.
2. Step two.
3. Step three.

## Risks

- Risk:
  Mitigation:

## Open Questions

- Question:
