# Codex Task List

This is the living task list for the modularization work. Keep completed items
in place for auditability. When a task is finished, mark it done and add the
commit, test command, document link, or manual verification note that proves it.

## Status Legend

- `[ ]` Todo.
- `[~]` In progress.
- `[x]` Done.
- `[!]` Blocked or needs a decision.

## Audit Rules

- Do not delete completed tasks.
- Prefer adding evidence over rewriting history.
- Record behavior-sensitive discoveries as notes under the relevant task.
- Link to documents, commits, or commands wherever possible.
- If a task changes scope, add a dated note instead of silently replacing it.

## Phase 0: Baseline And Documentation

- [x] `DOC-001` Create Codex documentation folder.
  Evidence: `Documentation/codex/README.md`.
- [x] `DOC-002` Map the existing repository structure.
  Evidence: `Documentation/codex/codebase-map.md`.
- [x] `DOC-003` Draft modular C++ rewrite feasibility analysis.
  Evidence: `Documentation/codex/modular-cpp-rewrite-feasibility.md`.
- [x] `DOC-004` Capture Windows build and runtime setup.
  Evidence: `Documentation/codex/windows-build-run-notes.md`.
- [x] `DOC-005` Add setup script for local Windows runtime dependencies.
  Evidence: `scripts/setup-windows-runtime.ps1`.
- [x] `DOC-006` Add modularization planning folder and roadmap.
  Evidence: `Documentation/codex/modularization-plan/`.
- [x] `DOC-007` Add legacy filesystem architecture overview with diagrams.
  Evidence: `Documentation/codex/legacy/filesystem/architecture.md`.
- [x] `DOC-010` Create root test planning folder for behavior and fixture
  documentation.
  Evidence: `tests/README.md`, `tests/filesystem/README.md`.
- [x] `DOC-008` Capture baseline `fs_path` output from the known-good Windows
  runtime.
  Evidence: `Documentation/codex/legacy/filesystem/windows-fs-path-baseline.md`.
- [x] `DOC-009` Add a concise glossary for filesystem terms: search path,
  write path, rodir, gamedir, basedir, falldir, archive, pk3dir.
  Evidence: `Documentation/codex/filesystem-glossary.md`.

## Phase 1: Filesystem Boundary Documentation

- [x] `FS-DOC-001` Create `boundary-filesystem.md`.
  Evidence: `Documentation/codex/modularization-plan/boundary-filesystem.md`.
- [x] `FS-DOC-002` Document `fs_api_t` fields and compatibility expectations.
  Evidence: `Documentation/codex/modularization-plan/boundary-filesystem.md`.
- [x] `FS-DOC-003` Document `VFileSystem009` vtable expectations.
  Evidence: `Documentation/codex/modularization-plan/boundary-filesystem.md`.
- [x] `FS-DOC-004` Document `searchpath_t` callback contracts.
  Evidence: `Documentation/codex/modularization-plan/boundary-filesystem.md`.
- [x] `FS-DOC-005` Document `file_t` ownership and lifecycle rules.
  Evidence: `Documentation/codex/modularization-plan/boundary-filesystem.md`.
- [x] `FS-DOC-006` Document `search_t` allocation/free ownership.
  Evidence: `Documentation/codex/modularization-plan/boundary-filesystem.md`.
- [x] `FS-DOC-007` Document `FS_LoadFile` versus `FS_LoadFileMalloc`
  allocator expectations.
  Evidence: `Documentation/codex/modularization-plan/boundary-filesystem.md`.
- [x] `FS-DOC-008` Document `FS_FindLibrary` DLL lookup behavior.
  Evidence: `Documentation/codex/modularization-plan/boundary-filesystem.md`.
- [x] `FS-DOC-009` Document direct path behavior and path rejection rules.
  Evidence: `Documentation/codex/modularization-plan/boundary-filesystem.md`.

## Phase 2: Filesystem Test Coverage

- [x] `FS-TEST-001` Confirm how filesystem tests are built and run on Windows.
  Suggested command: `.\waf.bat configure --enable-tests --sdl2=...` then
  `.\waf.bat build`.
  Evidence: `tests/filesystem/windows-test-baseline.md`.
- [x] `FS-TEST-001A` Decide when root `tests/` should become a build-integrated
  Waf test subproject versus remaining a planning/fixture home.
  Decision: Keep compiled filesystem tests under `tests/filesystem`, built by
  `filesystem/wscript`, until a shared cross-module test target is needed.
- [x] `FS-TEST-001B` Add shared filesystem test loader helper and migrate
  existing filesystem tests to use it.
  Evidence: `tests/filesystem/fs_test_common.h`; command `.\waf.bat build`
  passed filesystem tests 3/3.
- [x] `FS-TEST-002` Add a fixture helper for temporary directories and files.
  Evidence: `tests/filesystem/fs_test_common.h`; command `.\waf.bat build`
  passed filesystem tests 3/3.
- [x] `FS-TEST-003` Expand directory case-fixing tests for nested paths.
  Evidence: `tests/filesystem/caseinsensitive.c`; command `.\waf.bat build`
  passed filesystem tests 3/3.
- [x] `FS-TEST-004` Test cache refresh when files appear after initial scan.
  Evidence: `tests/filesystem/caseinsensitive.c`; command `.\waf.bat build`
  passed filesystem tests 3/3.
- [x] `FS-TEST-005` Test write path creation through `FS_Open(..., "wb", ...)`.
  Evidence: `tests/filesystem/caseinsensitive.c`; command `.\waf.bat build`
  passed filesystem tests 3/3.
- [x] `FS-TEST-006` Test path rejection for `..`, absolute paths, and colon
  paths when direct paths are disabled.
  Evidence: `tests/filesystem/caseinsensitive.c`; command `.\waf.bat build`
  passed filesystem tests 3/3.
- [x] `FS-TEST-007` Test direct-path behavior when `FS_AllowDirectPaths(true)`
  is enabled.
  Evidence: `tests/filesystem/directpath.c`; command `.\waf.bat build`
  passed filesystem tests 5/5.
- [x] `FS-TEST-008` Add search path ordering test for loose file overriding
  archive file.
  Evidence: `tests/filesystem/archive-order.c`; command `.\waf.bat build`
  passed filesystem tests 5/5.
- [x] `FS-TEST-009` Add search path ordering test for gamefolder overriding
  basedir.
  Evidence: `tests/filesystem/hierarchy.c`; command `.\waf.bat clean build`
  passed all tests 18/18.
- [x] `FS-TEST-010` Test `gamedironly` filtering.
  Evidence: `tests/filesystem/hierarchy.c`; command `.\waf.bat clean build`
  passed all tests 18/18.
- [x] `FS-TEST-011` Add basic PAK open/search fixture test.
  Evidence: `tests/filesystem/archive-order.c`; command `.\waf.bat build`
  passed filesystem tests 5/5.
- [x] `FS-TEST-012` Add basic ZIP/PK3 stored-file fixture test.
  Evidence: `tests/filesystem/zip-archive.c`; command `.\waf.bat clean build`
  passed all tests 18/18.
- [x] `FS-TEST-013` Add ZIP/PK3 deflated-file load test.
  Evidence: `tests/filesystem/zip-archive.c`; command `.\waf.bat clean build`
  passed all tests 18/18.
- [x] `FS-TEST-014` Add WAD lump lookup fixture test.
  Evidence: `tests/filesystem/wad-archive.c`; command `.\waf.bat clean build`
  passed all tests 19/19.
- [x] `FS-TEST-015` Test WADs mounted from archives.
  Evidence: `tests/filesystem/wad-archive.c`; command `.\waf.bat clean build`
  passed all tests 19/19.
- [x] `FS-TEST-016` Add `rodir` plus writable root precedence test.
  Evidence: `tests/filesystem/rodir.c`; command `.\waf.bat clean build`
  passed all tests 20/20.
- [x] `FS-TEST-017` Add `CreateInterface("VFileSystem009")` regression test
  coverage beyond simple lookup.
  Evidence: `tests/filesystem/interface.cpp`; command `.\waf.bat clean build`
  passed all tests 20/20.
- [x] `FS-TEST-018` Add `CreateInterface("XashFileSystem004")` copied table
  behavior test.
  Evidence: `tests/filesystem/interface.cpp`; command `.\waf.bat clean build`
  passed all tests 20/20.

## Phase 3: Filesystem Design Decisions

- [x] `FS-DESIGN-001` Decide whether first backend conversion uses `dir.cpp`
  or a C adapter plus adjacent C++ implementation file.
  Decision: Use a C adapter plus adjacent private C++ implementation for the
  first directory backend pilot.
  Evidence: `Documentation/codex/modularization-plan/filesystem-modern-design.md`.
- [x] `FS-DESIGN-002` Decide C++ exception and RTTI policy for filesystem C++
  internals.
  Decision: Do not require exceptions or RTTI yet; use explicit status/result
  semantics and keep exceptions from crossing legacy boundaries if enabled
  later.
  Evidence: `Documentation/codex/modularization-plan/filesystem-modern-design.md`.
- [x] `FS-DESIGN-003` Define the internal backend interface shape.
  Decision: Use a narrow internal `ISearchPathBackend` interface shaped like
  current `searchpath_t` callbacks.
  Evidence: `Documentation/codex/modularization-plan/filesystem-modern-design.md`.
- [x] `FS-DESIGN-004` Define how legacy `searchpath_t` callbacks adapt to the
  new backend objects.
  Decision: Keep `searchpath_t` as the linked-list adapter node while C
  callback shims forward into private backend objects.
  Evidence: `Documentation/codex/modularization-plan/filesystem-modern-design.md`.
- [x] `FS-DESIGN-005` Define archive registry responsibilities.
  Decision: Build a generic registry primitive, instantiate an archive
  registry from it, and keep mount-order policy outside registry ownership.
  Evidence: `Documentation/codex/modularization-plan/filesystem-modern-design.md`.
- [x] `FS-DESIGN-006` Define debugging utility names and output formats.
  Decision: Provide human-readable commands plus JSON output generated from
  debug snapshot records.
  Evidence: `Documentation/codex/modularization-plan/filesystem-modern-design.md`,
  `Documentation/codex/modularization-plan/filesystem-debug-utilities.md`.

## Phase 4: Debugging Utilities

- [x] `FS-DEBUG-001` Add `fs_path_verbose` design note.
  Evidence: `Documentation/codex/modularization-plan/filesystem-debug-utilities.md`.
- [x] `FS-DEBUG-002` Add `fs_why <path>` design note.
  Evidence: `Documentation/codex/modularization-plan/filesystem-debug-utilities.md`.
- [x] `FS-DEBUG-003` Add `fs_find_all <path>` design note.
  Evidence: `Documentation/codex/modularization-plan/filesystem-debug-utilities.md`.
- [x] `FS-DEBUG-004` Add `fs_registry` design note.
  Evidence: `Documentation/codex/modularization-plan/filesystem-debug-utilities.md`.
- [x] `FS-DEBUG-005` Decide whether machine-readable debug output should be
  JSON, key/value text, or both.
  Decision: JSON first for machine-readable output; human-readable output
  remains separate and is generated from the same debug snapshots.
  Evidence: `Documentation/codex/modularization-plan/filesystem-debug-utilities.md`.

## Phase 4A: Modern Shared Debug Utilities

- [x] `MODERN-DEBUG-001` Create `Documentation/codex/modern/` for intended
  modern internals, separate from legacy architecture notes.
  Evidence: `Documentation/codex/modern/README.md`.
- [x] `MODERN-DEBUG-002` Define the thread-safe debug snapshot capture model.
  Evidence: `Documentation/codex/modern/thread-safe-debugging-utilities.md`.
- [x] `MODERN-DEBUG-003` Define human and JSON output flow from shared
  snapshots.
  Evidence: `Documentation/codex/modern/thread-safe-debugging-utilities.md`.
- [x] `MODERN-DEBUG-004` Define async trace queue and overflow policy
  requirements.
  Evidence: `Documentation/codex/modern/thread-safe-debugging-utilities.md`.
- [x] `MODERN-DEBUG-005` Create reserved source and private include folders
  for the modern debugging utility layer.
  Evidence: `src/debugging/README.md`, `src/include/debugging/README.md`.
- [x] `MODERN-DEBUG-006` Draft detailed modern debugging utility architecture.
  Evidence: `Documentation/codex/modern/debugging/architecture.md`.
- [x] `MODERN-DEBUG-007` Define proposed debugging namespaces, classes,
  structs, and first implementation order.
  Evidence: `Documentation/codex/modern/debugging/api-inventory.md`.
- [x] `MODERN-DEBUG-007A` Implement initial shared debug status, sink, and
  snapshot writer contracts.
  Evidence: `src/include/debugging/debug_types.hpp`,
  `src/include/debugging/debug_sink.hpp`,
  `src/include/debugging/snapshot_writer.hpp`,
  `src/debugging/snapshot_writer.cpp`.
- [ ] `MODERN-DEBUG-008` Implement first filesystem debug snapshot structs.
  Evidence:
- [x] `MODERN-DEBUG-009` Add first synchronous debug sink for tests or console
  output.
  Evidence: `tests/debugging/debug_test_common.hpp`.
- [x] `MODERN-DEBUG-010` Add tests for shared debug sink, JSON escaping, and
  snapshot header formatting.
  Evidence: `tests/debugging/debugging.cpp`; command `.\waf.bat build`.
- [x] `MODERN-DEBUG-011` Add focused debugging TODO list cross-checked against
  the modern architecture and API inventory.
  Evidence: `Documentation/codex/todo/debugging_todo.md`.
- [x] `MODERN-DEBUG-012` Normalize spacing in the new debugging C++ source and
  test files.
  Evidence: `src/include/debugging/`, `src/debugging/`, `tests/debugging/`.
- [x] `MODERN-DEBUG-013` Rename private modern C++ debugging headers to `.hpp`.
  Evidence: `src/include/debugging/*.hpp`,
  `tests/debugging/debug_test_common.hpp`.
- [x] `MODERN-DEBUG-014` Decide `DebugStatus` remains local to the debugging
  utility layer rather than aliasing a future release/core status type.
  Evidence: `src/include/debugging/debug_types.hpp`,
  `Documentation/codex/todo/debugging_todo.md`.
- [x] `MODERN-DEBUG-015` Add missing writer test coverage for sink failures,
  large JSON string chunking, and null JSON input.
  Evidence: `tests/debugging/debugging.cpp`; command `.\waf.bat build`.
- [x] `MODERN-DEBUG-016` Add initial trace types and runtime trace gate.
  Evidence: `src/include/debugging/trace.hpp`, `src/debugging/trace.cpp`,
  `tests/debugging/debugging.cpp`; command `.\waf.bat build`.
- [x] `MODERN-DEBUG-017` Add shared fixed-buffer debug sink for no-allocation
  output capture.
  Evidence: `src/include/debugging/buffer_sink.hpp`,
  `tests/debugging/debugging.cpp`; command `.\waf.bat build`.
- [x] `MODERN-DEBUG-018` Document compile-time trace gating policy before
  adding trace macros or async trace producers.
  Evidence: `Documentation/codex/modern/debugging/trace-gating-policy.md`.
- [x] `MODERN-DEBUG-019` Add initial logging facade with levels, categories,
  records, sink interface, gate, and logger dispatch.
  Evidence: `src/include/debugging/logging.hpp`, `src/debugging/logging.cpp`,
  `tests/debugging/debugging.cpp`; command `.\waf.bat build`.
- [x] `MODERN-DEBUG-020` Add bounded trace queue with explicit overflow
  policies and tests.
  Evidence: `src/include/debugging/trace.hpp`, `src/debugging/trace.cpp`,
  `tests/debugging/debugging.cpp`; command `.\waf.bat build`.
- [x] `MODERN-DEBUG-021` Document `{fmt}` as the preferred future human
  formatting backend, deferred until real producers need typed formatting.
  Evidence: `Documentation/codex/modern/debugging/formatting-policy.md`.
- [x] `MODERN-DEBUG-022` Verify and harden current shared debugging utility
  multithreading behavior.
  Evidence: `src/include/debugging/trace.hpp`,
  `src/include/debugging/logging.hpp`, `src/debugging/trace.cpp`,
  `src/debugging/logging.cpp`,
  `Documentation/codex/modern/debugging/thread-safety-audit.md`,
  `tests/debugging/debugging.cpp`; command `.\waf.bat build`.
- [x] `MODERN-DEBUG-023` Add shared streaming JSON writer object before
  filesystem snapshot serializers.
  Decision: Use a small `IDebugSink`-backed writer now and defer RapidJSON
  until JSON parsing, DOM mutation, or deeper schema complexity justifies the
  dependency.
  Evidence: `src/include/debugging/json_writer.hpp`,
  `src/debugging/json_writer.cpp`,
  `Documentation/codex/modern/debugging/api-inventory.md`,
  `tests/debugging/debugging.cpp`; command `.\waf.bat build`.

## Phase 5: Directory Backend Pilot

- [ ] `FS-IMPL-001` Add private C++ directory backend design sketch.
  Evidence:
- [ ] `FS-IMPL-002` Introduce the smallest possible internal helper without
  changing behavior.
  Evidence:
- [ ] `FS-IMPL-003` Keep `FS_AddDir_Fullpath`, `FS_InitDirectorySearchpath`,
  and `FS_FixFileCase` callable from C.
  Evidence:
- [ ] `FS-IMPL-004` Run filesystem unit tests after first helper extraction.
  Evidence:
- [ ] `FS-IMPL-005` Run Windows runtime smoke test after first helper
  extraction.
  Evidence:

## Phase 6: Commit And Review Hygiene

- [ ] `REVIEW-001` Push documentation commits to remote branch.
  Evidence:
- [ ] `REVIEW-002` Open draft PR for documentation and baseline setup.
  Evidence:
- [ ] `REVIEW-003` Add PR checklist for compatibility-preserving filesystem
  modernization.
  Evidence:
- [ ] `REVIEW-004` Review each implementation PR for public ABI drift.
  Evidence:

## Decision Log

| Date | ID | Decision | Evidence |
| --- | --- | --- | --- |
| 2026-05-09 | DEC-001 | Modernize inward while preserving legacy outward interfaces. | `modularization-plan/filesystem-pilot.md` |
| 2026-05-09 | DEC-002 | Use filesystem directory backend as the first serious pilot candidate. | `modularization-plan/filesystem-pilot.md` |
| 2026-05-09 | DEC-003 | Reserve `src/` for future reusable C++ internals, but do not wire it into the build until the adapter pilot proves out. | `src/README.md`, `modularization-plan/filesystem-modern-design.md` |
| 2026-05-09 | DEC-004 | Use explicit filesystem status/result semantics before requiring C++ exceptions or RTTI. | `modularization-plan/filesystem-modern-design.md` |
| 2026-05-09 | DEC-005 | Use debug snapshots with human formatters and JSON serializers for filesystem diagnostics. | `modularization-plan/filesystem-debug-utilities.md` |
| 2026-05-09 | DEC-006 | Keep reusable modernization utilities behind private facades, and do not expose third-party utility types through public ABI boundaries. | `modularization-plan/cross-cutting-utilities.md`, `src/include/README.md` |
| 2026-05-09 | DEC-007 | Prepare for threading with explicit ownership, immutable snapshots, short lock windows, and bounded async queues before broad multithreaded behavior changes. | `modularization-plan/cross-cutting-utilities.md` |
| 2026-05-09 | DEC-008 | Put intended modern utility contracts under `Documentation/codex/modern/`, leaving `legacy/` for current architecture descriptions. | `modern/README.md` |
| 2026-05-09 | DEC-009 | Debug commands must capture immutable snapshots before formatting or serializing output, so future thread-safety does not depend on printing while holding runtime locks. | `modern/thread-safe-debugging-utilities.md` |
| 2026-05-09 | DEC-010 | Reserve `src/debugging/` and `src/include/debugging/` for the modern debugging utility layer, but do not wire them into the build until the first tested implementation slice exists. | `src/debugging/README.md`, `src/include/debugging/README.md`, `modern/debugging/architecture.md` |
| 2026-05-09 | DEC-011 | Use `xash::debugging` for shared modern debugging contracts and `xash::filesystem::debugging` for filesystem pilot records until they prove reusable. | `modern/debugging/api-inventory.md` |
| 2026-05-09 | DEC-012 | Use `.hpp` for private modern C++ debugging headers while leaving legacy C-compatible headers on their existing `.h` convention. | `src/include/debugging/README.md`, `todo/debugging_todo.md` |
| 2026-05-09 | DEC-013 | Keep `DebugStatus` local to the debugging layer so release-oriented core code does not depend on debugging utilities. | `src/include/debugging/debug_types.hpp`, `todo/debugging_todo.md` |
| 2026-05-09 | DEC-014 | Defer RapidJSON and use a small streaming JSON writer behind `IDebugSink` until larger snapshot serializers need a third-party JSON backend. | `src/include/debugging/json_writer.hpp`, `modern/debugging/api-inventory.md` |
