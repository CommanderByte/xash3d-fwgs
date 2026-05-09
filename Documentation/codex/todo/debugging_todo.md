# Debugging TODO

## Purpose

This TODO list cross-checks the modern debugging architecture against the code
that currently exists under `src/debugging/`, `src/include/debugging/`, and
`tests/debugging/`.

Canonical design references:

- `Documentation/codex/modern/debugging/architecture.md`
- `Documentation/codex/modern/debugging/api-inventory.md`
- `Documentation/codex/modern/thread-safe-debugging-utilities.md`

## Current Implemented Slice

- [x] Build-wired `src/` as a top-level subproject.
  Evidence: `wscript`, `src/wscript`.
- [x] Created shared debugging include area.
  Evidence: `src/include/debugging/`.
- [x] Created shared debugging implementation area.
  Evidence: `src/debugging/`.
- [x] Added shared schema, header, status, and output format types.
  Evidence: `src/include/debugging/debug_types.hpp`.
- [x] Added synchronous debug sink interface.
  Evidence: `src/include/debugging/debug_sink.hpp`.
- [x] Added fixed caller-buffer debug sink.
  Evidence: `src/include/debugging/buffer_sink.hpp`,
  `tests/debugging/debugging.cpp`.
- [x] Added logging levels, categories, records, sink interface, gate, and
  logger facade.
  Evidence: `src/include/debugging/logging.hpp`, `src/debugging/logging.cpp`,
  `tests/debugging/debugging.cpp`.
- [x] Added shared snapshot header writer declarations.
  Evidence: `src/include/debugging/snapshot_writer.hpp`.
- [x] Added human snapshot header writer.
  Evidence: `src/debugging/snapshot_writer.cpp`.
- [x] Added JSON snapshot header writer and JSON string escaping helper.
  Evidence: `src/debugging/snapshot_writer.cpp`,
  `src/debugging/json_writer.cpp`.
- [x] Added shared streaming JSON writer object.
  Evidence: `src/include/debugging/json_writer.hpp`,
  `src/debugging/json_writer.cpp`, `tests/debugging/debugging.cpp`.
- [x] Added trace level, category, event, stats, and gate declarations.
  Evidence: `src/include/debugging/trace.hpp`.
- [x] Added bounded trace queue with explicit overflow policy.
  Evidence: `src/include/debugging/trace.hpp`, `src/debugging/trace.cpp`,
  `tests/debugging/debugging.cpp`.
- [x] Added runtime trace gate behavior.
  Evidence: `src/debugging/trace.cpp`.
- [x] Added failure-only test sink.
  Evidence: `tests/debugging/debug_test_common.hpp`.
- [x] Added first debugging unit test target.
  Evidence: `tests/debugging/debugging.cpp`; command `.\waf.bat build`.

## Immediate Cleanup And Hardening

- [x] Normalize the new debugging C++ source spacing toward normal C++ call and
  condition style.
  Evidence: `src/include/debugging/*.hpp`, `src/debugging/snapshot_writer.cpp`,
  `tests/debugging/*.hpp`, `tests/debugging/*.cpp`.
- [x] Use `.hpp` for private modern C++ debugging headers so mixed C/C++ code
  can distinguish C++-only contracts from legacy C-compatible `.h` headers.
  Evidence: `src/include/debugging/*.hpp`,
  `tests/debugging/debug_test_common.hpp`.
- [x] Keep `DebugStatus` local to the debugging layer instead of aliasing it to
  a future release/core `Status`.
  Rationale: release-oriented code should not need to depend on debug sinks,
  JSON helpers, trace gates, or any other debugging utility surface. The main
  counterargument is duplicated status plumbing, but a small adapter can bridge
  debug failures to a future core status type at subsystem boundaries if that
  becomes useful.
  Evidence: `src/include/debugging/debug_types.hpp`,
  `Documentation/codex/modern/debugging/api-inventory.md`.
- [x] Add explicit test coverage for sink write failure propagation from every
  writer.
  Evidence: `tests/debugging/debugging.cpp`.
- [x] Add explicit test coverage for large JSON string escaping that forces
  `WriteSpan` chunking.
  Evidence: `tests/debugging/debugging.cpp`.
- [x] Add explicit test coverage for null JSON string input.
  Evidence: `tests/debugging/debugging.cpp`.

## Shared Utility TODO

- [x] Add `trace.hpp` with `TraceLevel`, `TraceCategory`, and `TraceEvent`.
  Evidence: `src/include/debugging/trace.hpp`.
- [x] Add a runtime `TraceGate` interface with cheap disabled checks.
  Evidence: `src/include/debugging/trace.hpp`, `src/debugging/trace.cpp`,
  `tests/debugging/debugging.cpp`.
- [x] Add compile-time trace gating policy notes before implementing macros.
  Evidence: `Documentation/codex/modern/debugging/trace-gating-policy.md`.
- [x] Add `BoundedTraceQueue`.
  Decision: Implemented as a fixed-capacity caller-owned queue without worker
  threads. It can support trace producers without committing to async runtime
  behavior yet.
  Evidence: `src/include/debugging/trace.hpp`, `src/debugging/trace.cpp`.
- [x] Add `TraceQueueStats` with dropped-event counters.
  Evidence: `src/include/debugging/trace.hpp`.
- [ ] Add optional `ConsoleDebugSink` adapter once console ownership is clear.
- [ ] Add optional engine console log sink once console ownership is clear.
- [ ] Add optional file/tooling sinks only after command output is stable.
- [x] Add a shared JSON object/array writer if snapshot JSON grows beyond
  header-sized output.
  Decision: Added a small streaming writer over `IDebugSink` before filesystem
  snapshots so subsystem serializers do not hand-roll object state.
  Evidence: `src/include/debugging/json_writer.hpp`,
  `src/debugging/json_writer.cpp`, `tests/debugging/debugging.cpp`.
- [x] Decide whether `{fmt}` should back human formatting before adding a
  formatting facade.
  Decision: Prefer `{fmt}` later for human log/debug formatting, behind
  engine-owned helpers. Do not add it until typed formatting is needed by real
  producers or command output.
  Evidence: `Documentation/codex/modern/debugging/formatting-policy.md`.
- [ ] Decide whether `spdlog` should be an optional `ILogSink` backend after
  engine console routing is stable.
- [x] Decide whether RapidJSON should back JSON writing before adding larger
  snapshot serializers.
  Decision: Defer RapidJSON for now. The current need is deterministic debug
  serialization, not JSON parsing or DOM construction. Keep the writer boundary
  small enough that RapidJSON, yyjson, or another backend can replace the
  implementation later if filesystem snapshots become complex enough to justify
  a vendored dependency.
  Evidence: `Documentation/codex/modern/debugging/architecture.md`,
  `Documentation/codex/modern/debugging/api-inventory.md`.

## Filesystem Pilot TODO

- [ ] Add `FilesystemMountRecord`.
- [ ] Add `FilesystemMountSnapshot`.
- [ ] Decide initial snapshot ownership: caller-provided storage, arena, or
  snapshot-owned vector-like buffer.
- [ ] Add filesystem mount capture adapter over existing `searchpath_t` state.
- [ ] Add direct unit tests for mount snapshot ordering.
- [ ] Add human formatter for filesystem mount snapshots.
- [ ] Add JSON writer for filesystem mount snapshots.
- [ ] Add command adapter for `fs_path_verbose` once console wiring is clear.
- [ ] Add `FilesystemLookupStep` and `FilesystemLookupSnapshot`.
- [ ] Add `fs_why <path>` capture and tests after mount snapshots are stable.
- [ ] Add `FilesystemRegistrySnapshot` after the archive registry design starts
  moving into code.

## Thread-Safety TODO

- [x] Verify current shared debugging utility thread-safety guarantees.
  Evidence: `Documentation/codex/modern/debugging/thread-safety-audit.md`,
  `tests/debugging/debugging.cpp`; command `.\waf.bat build`.
- [x] Make `TraceGate` and `LogGate` data-race resistant for concurrent reads
  and runtime setting updates.
  Evidence: `src/include/debugging/trace.hpp`,
  `src/include/debugging/logging.hpp`, `src/debugging/trace.cpp`,
  `src/debugging/logging.cpp`.
- [x] Make `BoundedTraceQueue` data-race resistant for concurrent push/pop and
  stats access.
  Evidence: `src/include/debugging/trace.hpp`, `src/debugging/trace.cpp`,
  `tests/debugging/debugging.cpp`.
- [x] Record that sink implementation thread-safety is sink-specific and that
  `Logger` does not own sink lifetime.
  Evidence: `Documentation/codex/modern/debugging/thread-safety-audit.md`.
- [x] Record expected logger sink lifetime for engine-owned debug builds.
  Evidence: `Documentation/codex/modern/debugging/thread-safety-audit.md`.
- [ ] Document which first filesystem debug capture operations are owner-thread
  only.
- [ ] Ensure snapshot capture never formats while holding filesystem ownership.
- [ ] Ensure published snapshot records do not point to mutable linked-list
  nodes unless lifetime is explicitly guaranteed.
- [ ] Add dropped-event accounting before any async trace queue is enabled.
- [ ] Add shutdown behavior for async debug workers before they exist in
  runtime builds.

## Documentation TODO

- [ ] Update `api-inventory.md` whenever a proposed type is implemented,
  renamed, or deferred.
- [ ] Update `architecture.md` if implementation chooses a different ownership
  model than caller-provided buffers.
- [ ] Update `tasks.md` with evidence after every TODO item graduates into the
  main phase tracker.
- [ ] Record build/test commands when new debugging tests are added.
