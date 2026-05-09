# Engine String And Path Utility TODO

## Purpose

Plan modernization of shared string and path helpers. This area is useful but
quirk-heavy, so it follows info strings and hashes.

## Scope

Candidate areas:

- case-sensitive and case-insensitive compare helpers;
- token-safe copy/concat helpers;
- file basename, extension, and path normalization helpers;
- slash normalization and platform path quirks;
- helpers currently used by filesystem, renderer, model loading, and config
  parsing.

## Method

- Inventory behavior before moving anything.
- Preserve existing compatibility quirks unless a task explicitly changes them.
- Prefer small helper-by-helper migrations rather than one large utility
  rewrite.

## Phase 40 Tasks: String And Path Utilities

- [ ] `ENG-STRPATH-001` Audit shared string/path helpers and rank them by
  caller breadth, compatibility risk, and testability.
  Evidence:

- [ ] `ENG-STRPATH-002` Add focused tests for path extension, basename,
  slash-normalization, case comparison, and bounded copy/concat behavior.
  Evidence:

- [ ] `ENG-STRPATH-003` Decide which helpers belong in `src/engine`, which
  belong in `src/utilities`, and which must remain public C utilities.
  Evidence:

- [ ] `ENG-STRPATH-004` Add modern helper implementations only after tests pin
  legacy edge cases.
  Evidence:

- [ ] `ENG-STRPATH-005` Replace one narrow helper group and verify with focused
  tests, full tests, and a runtime smoke if filesystem or config parsing is
  touched.
  Evidence:
