# Utilities TODO

## Purpose

This TODO tracks subsystem-neutral utility work under `src/include/utilities/`
and `src/utilities/`.

The first consumer is expected to be the filesystem archive registry, but these
helpers should stay free of filesystem policy.

## Registry Utility

- [x] Add a fixed-capacity ordered registry primitive.
  Evidence: `src/include/utilities/registry.hpp`.
- [x] Keep registry implementation header-only while it is template-based.
  Evidence: `src/include/utilities/registry.hpp`, `src/utilities/README.md`.
- [x] Add explicit duplicate-key policy.
  Decision: Support `Reject` and `Replace`; replacement preserves original
  order.
  Evidence: `src/include/utilities/registry.hpp`,
  `tests/utilities/registry.cpp`.
- [x] Add stable enumeration by registration order.
  Evidence: `StaticRegistry::recordAt`; `tests/utilities/registry.cpp`.
- [x] Add string-key comparators for exact and ASCII case-insensitive matching.
  Evidence: `CStringKeyEqual`, `CaseInsensitiveCStringKeyEqual`.
- [x] Add focused unit tests.
  Evidence: `tests/utilities/registry.cpp`; command `.\waf.bat build`.

## Archive Registry Follow-Up

- [x] Define archive registry entry metadata over the generic registry.
  Evidence: `src/include/filesystem/archive_registry.hpp`.
- [x] Decide archive extension matching policy: exact lower-case only or
  case-insensitive extension lookup.
  Decision: Use case-insensitive C-string keys for extension descriptors.
  Evidence: `src/include/filesystem/archive_registry.hpp`,
  `Documentation/codex/todo/archive_registry_todo.md`.
- [x] Preserve legacy archive scan order outside the generic registry.
  Decision: Archive descriptors carry `scanPriority`; the generic registry only
  preserves registration order.
  Evidence: `src/filesystem/archive_registry.cpp`,
  `Documentation/codex/todo/archive_registry_todo.md`.
- [x] Add filesystem registry snapshot records after archive metadata exists.
  Evidence: `src/include/filesystem/registry_snapshot.hpp`,
  `src/filesystem/registry_snapshot.cpp`.
- [x] Add tests for PAK, PK3, PK3DIR, and WAD archive descriptor lookup.
  Evidence: `tests/filesystem/archive_registry.cpp`,
  `tests/filesystem/registry_snapshot.cpp`.

## Design Rules

- The registry owns registration order and duplicate behavior.
- The registry does not own mount order, filesystem path policy, logging, or
  runtime mutation.
- Prefer caller/static storage and deterministic capacity until a real consumer
  requires dynamic allocation.
