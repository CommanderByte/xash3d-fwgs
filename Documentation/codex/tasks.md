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
- [x] `MODERN-DEBUG-008` Implement first filesystem debug snapshot structs.
  Evidence: `src/include/filesystem/debug_snapshot.hpp`,
  `src/filesystem/debug_snapshot.cpp`,
  `tests/filesystem/debug_snapshot.cpp`; command `.\waf.bat build`.
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

## Phase 4B: Modern Shared Utilities

- [x] `MODERN-UTIL-001` Add focused utility TODO list.
  Evidence: `Documentation/codex/todo/utilities_todo.md`.
- [x] `MODERN-UTIL-002` Add fixed-capacity ordered registry utility with
  explicit duplicate-key behavior.
  Evidence: `src/include/utilities/registry.hpp`,
  `tests/utilities/registry.cpp`; command `.\waf.bat build`.
- [x] `MODERN-UTIL-003` Add exact and ASCII case-insensitive C-string key
  comparators for registry users such as archive extension descriptors.
  Evidence: `src/include/utilities/registry.hpp`,
  `tests/utilities/registry.cpp`; command `.\waf.bat build`.
- [x] `MODERN-UTIL-004` Instantiate archive registry metadata over the generic
  registry.
  Evidence: `src/include/filesystem/archive_registry.hpp`,
  `src/filesystem/archive_registry.cpp`,
  `tests/filesystem/archive_registry.cpp`; command `.\waf.bat build`.
- [x] `FS-REG-001` Add archive registry scaffold and descriptor types.
  Evidence: `src/include/filesystem/archive_registry.hpp`,
  `src/filesystem/archive_registry.cpp`,
  `Documentation/codex/todo/archive_registry_todo.md`,
  `tests/filesystem/archive_registry.cpp`; command `.\waf.bat build`.
- [x] `FS-REG-002` Add default archive descriptors for PAK, PK3, PK3DIR, and
  WAD without routing mounts through the registry yet.
  Evidence: `src/filesystem/archive_registry.cpp`,
  `tests/filesystem/archive_registry.cpp`; command `.\waf.bat build`.
- [x] `FS-REG-003` Add filesystem registry snapshot records for `fs_registry`.
  Evidence: `src/include/filesystem/registry_snapshot.hpp`,
  `src/filesystem/registry_snapshot.cpp`,
  `tests/filesystem/registry_snapshot.cpp`; command `.\waf.bat build`.

## Phase 5: Directory Backend Pilot

- [x] `FS-IMPL-001` Add private C++ directory backend design sketch.
  Evidence: `src/include/filesystem/search_path_backend.hpp`,
  `src/include/filesystem/directory_backend.hpp`,
  `Documentation/codex/todo/directory_backend_todo.md`.
- [x] `FS-IMPL-002` Introduce the smallest possible internal helper without
  changing behavior.
  Evidence: `src/filesystem/search_path_backend.cpp`,
  `src/filesystem/directory_backend.cpp`,
  `tests/filesystem/directory_backend.cpp`; command `.\waf.bat build`.
- [x] `FS-IMPL-003` Keep `FS_AddDir_Fullpath`, `FS_InitDirectorySearchpath`,
  and `FS_FixFileCase` callable from C.
  Evidence: `filesystem/dir.c`, `filesystem/dir_backend_adapter.h`,
  `filesystem/dir_backend_adapter.cpp`; command `.\waf.bat build`.
- [x] `FS-IMPL-004` Run filesystem unit tests after first helper extraction.
  Evidence: command `.\waf.bat build` passed 15/15 tests, including legacy
  filesystem tests.
- [x] `FS-IMPL-005` Run Windows runtime smoke test after first helper
  extraction.
  Evidence: refreshed `run-win32/filesystem_stdio.dll` from
  `build/filesystem/filesystem_stdio.dll`, then ran
  `.\xash3d.exe -dev 2 -log +fs_path +quit` from `run-win32` with
  `XASH3D_BASEDIR=C:\git\xash3d-fwgs\run-win32` and
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`.
  Exit code was `0`; `engine.log` printed the Steam `valve` directory and WAD
  search paths and stopped with reason `"command"`.

## Phase 6: Archive Registry Integration

- [x] `FS-REG-004` Add a C-compatible archive registry adapter for legacy
  filesystem code.
  Evidence: `filesystem/archive_registry_adapter.h`,
  `filesystem/archive_registry_adapter.cpp`; command `.\waf.bat build`.
- [x] `FS-REG-005` Route archive extension support checks through the modern
  archive registry.
  Evidence: `filesystem/filesystem.c`, `tests/filesystem/no-init.c`; command
  `.\waf.bat build`.
- [x] `FS-REG-006` Route archive mount extension detection through the modern
  archive registry while keeping legacy mount factories.
  Evidence: `FS_AddArchive_Fullpath` in `filesystem/filesystem.c`; command
  `.\build\filesystem\test_archive-order.exe`, including a direct
  `MountArchive_Fullpath("direct.PAK", FS_GAMEDIR_PATH)` assertion.
- [x] `FS-REG-007` Route game-directory archive scan ordering through the
  modern archive registry while preserving PAK -> PK3 -> PK3DIR -> WAD order.
  Evidence: `FS_AddGameDirectory` in `filesystem/filesystem.c`; commands
  `.\build\filesystem\test_archive-order.exe`,
  `.\build\filesystem\test_wad-archive.exe`, and
  `.\build\filesystem\test_zip-archive.exe`.
- [x] `FS-REG-008` Run filesystem unit tests after registry integration.
  Evidence: command `.\waf.bat build` passed; explicit follow-up commands
  `.\build\filesystem\test_archive-order.exe`,
  `.\build\filesystem\test_wad-archive.exe`,
  `.\build\filesystem\test_zip-archive.exe`, and
  `.\build\filesystem\test_no-init.exe` passed.
- [x] `FS-REG-009` Run Windows runtime smoke test after registry integration.
  Evidence: refreshed `run-win32/filesystem_stdio.dll` from the current build
  and ran `.\xash3d.exe -dev 2 -log +fs_path +quit` with the Steam Half-Life
  install as `XASH3D_RODIR`. Exit code was `0`; `engine.log` showed WAD and
  PK3 mounts, printed search paths, and stopped with reason `"command"`.

## Phase 7: Backend And Policy Migration Prep

- [x] `FS-BACKEND-001` Add migration TODO for PAK backend.
  Evidence: `Documentation/codex/todo/pak_backend_todo.md`.
- [x] `FS-BACKEND-002` Add migration TODO for WAD backend.
  Evidence: `Documentation/codex/todo/wad_backend_todo.md`.
- [x] `FS-BACKEND-003` Add migration TODO for ZIP/PK3 backend.
  Evidence: `Documentation/codex/todo/zip_backend_todo.md`.
- [x] `FS-BACKEND-004` Add migration TODO for Android assets backend.
  Evidence: `Documentation/codex/todo/android_assets_backend_todo.md`.
- [x] `FS-BACKEND-005` Add tests for archive mount idempotency and unsupported
  archive failure.
  Evidence: `tests/filesystem/archive-order.c`; command
  `.\waf.bat build --targets=test_archive-order,test_pk3dir`.
- [x] `FS-BACKEND-006` Add `pk3dir` behavior tests.
  Evidence: `tests/filesystem/pk3dir.c`; command
  `.\waf.bat build --targets=test_archive-order,test_pk3dir`.
- [x] `FS-STATE-001` Document current `filesystem.c` global state ownership.
  Evidence: `Documentation/codex/modularization-plan/filesystem-state-ownership.md`.
- [x] `FS-POLICY-001` Document path policy extraction candidates.
  Evidence: `Documentation/codex/modularization-plan/path-policy-candidates.md`.
- [x] `FS-HIER-001` Add hierarchy coverage for falldir, custom/downloaded,
  HD, LV, addon, and localization mounts.
  Evidence: `tests/filesystem/hierarchy.c`; command
  `.\waf.bat build --targets=test_hierarchy,test_dll-lookup`.
- [x] `FS-DLL-001` Add `FS_FindLibrary` behavior tests before DLL lookup
  refactors.
  Evidence: `tests/filesystem/dll-lookup.c`; command
  `.\waf.bat build --targets=test_hierarchy,test_dll-lookup`.
- [x] `FS-BACKEND-007` Run filesystem unit tests after Phase 7 coverage work.
  Evidence: command `.\waf.bat build` passed; explicit follow-up commands
  `.\build\filesystem\test_archive-order.exe`,
  `.\build\filesystem\test_pk3dir.exe`,
  `.\build\filesystem\test_hierarchy.exe`, and
  `.\build\filesystem\test_dll-lookup.exe` passed.
- [x] `FS-BACKEND-008` Run Windows runtime smoke test after Phase 7 coverage
  work.
  Evidence: refreshed `run-win32/filesystem_stdio.dll` from the current build
  and ran `.\xash3d.exe -dev 2 -log +fs_path +quit` with the Steam Half-Life
  install as `XASH3D_RODIR`. Exit code was `0`; `engine.log` showed directory,
  WAD, and PK3 mounts, printed search paths, and stopped with reason
  `"command"`.

## Phase 8: PAK Backend Pilot

- [x] `FS-PAK-001` Add private `PakBackend` skeleton mirroring
  `ISearchPathBackend`.
  Evidence: `src/include/filesystem/pak_backend.hpp`,
  `src/filesystem/pak_backend.cpp`,
  `tests/filesystem/pak_backend.cpp`; command
  `.\waf.bat build --targets=test_filesystem_pak_backend,test_archive-order,test_wad-archive`.
- [x] `FS-PAK-002` Add C adapter bridge for legacy PAK callbacks.
  Evidence: `filesystem/pak_backend_adapter.h`,
  `filesystem/pak_backend_adapter.cpp`; command
  `.\waf.bat build --targets=test_filesystem_pak_backend,test_archive-order,test_wad-archive`.
- [x] `FS-PAK-003` Forward PAK print, close, open, file time, find, and search
  callbacks through the bridge.
  Evidence: `filesystem/pak.c`; command
  `.\waf.bat build --targets=test_filesystem_pak_backend,test_archive-order,test_wad-archive`.
- [x] `FS-PAK-004` Run filesystem unit tests after PAK bridge.
  Evidence: command `.\waf.bat build` passed 15/15 selected tests; explicit
  follow-up commands `.\build\src\test_filesystem_pak_backend.exe`,
  `.\build\filesystem\test_archive-order.exe`,
  `.\build\filesystem\test_wad-archive.exe`, and
  `.\build\filesystem\test_no-init.exe` passed.
- [x] `FS-PAK-005` Run Windows runtime smoke test after PAK bridge.
  Evidence: refreshed `run-win32/filesystem_stdio.dll` from the current build
  and ran `.\xash3d.exe -dev 2 -log +fs_path +quit` with the Steam Half-Life
  install as `XASH3D_RODIR`. Exit code was `0`; `engine.log` printed search
  paths and stopped with reason `"command"`.

## Phase 9: WAD Backend Pilot

- [x] `FS-WAD-001` Add private `WadBackend` skeleton mirroring
  `ISearchPathBackend`, including the WAD load-file callback.
  Evidence: `src/include/filesystem/wad_backend.hpp`,
  `src/filesystem/wad_backend.cpp`,
  `tests/filesystem/wad_backend.cpp`; command
  `.\waf.bat build --targets=test_filesystem_wad_backend,test_wad-archive`.
- [x] `FS-WAD-002` Add C adapter bridge for legacy WAD callbacks.
  Evidence: `filesystem/wad_backend_adapter.h`,
  `filesystem/wad_backend_adapter.cpp`; command
  `.\waf.bat build --targets=test_filesystem_wad_backend,test_wad-archive`.
- [x] `FS-WAD-003` Forward WAD print, close, open, file time, find, search,
  and lump load callbacks through the bridge.
  Evidence: `filesystem/wad.c`; command
  `.\waf.bat build --targets=test_filesystem_wad_backend,test_wad-archive`.
- [x] `FS-WAD-004` Run filesystem unit tests after WAD bridge.
  Evidence: command `.\waf.bat build` passed 17/17 selected tests; explicit
  follow-up commands `.\build\src\test_filesystem_wad_backend.exe`,
  `.\build\filesystem\test_wad-archive.exe`,
  `.\build\filesystem\test_archive-order.exe`, and
  `.\build\filesystem\test_no-init.exe` passed.
- [x] `FS-WAD-005` Run Windows runtime smoke test after WAD bridge.
  Evidence: refreshed `run-win32/filesystem_stdio.dll` from the current build
  and ran `.\xash3d.exe -dev 2 -log +fs_path +quit` with the Steam Half-Life
  install as `XASH3D_RODIR`. Exit code was `0`; `engine.log` showed WAD
  mounts, printed search paths, and stopped with reason `"command"`.

## Phase 10: ZIP/PK3 Backend Pilot

- [x] `FS-ZIP-001` Add private `ZipBackend` skeleton mirroring
  `ISearchPathBackend`, including the ZIP load-file callback.
  Evidence: `src/include/filesystem/zip_backend.hpp`,
  `src/filesystem/zip_backend.cpp`,
  `tests/filesystem/zip_backend.cpp`; command
  `.\waf.bat build --targets=test_filesystem_zip_backend,test_zip-archive`.
- [x] `FS-ZIP-002` Add C adapter bridge for legacy ZIP callbacks.
  Evidence: `filesystem/zip_backend_adapter.h`,
  `filesystem/zip_backend_adapter.cpp`; command
  `.\waf.bat build --targets=test_filesystem_zip_backend,test_zip-archive`.
- [x] `FS-ZIP-003` Forward ZIP print, close, open, file time, find, search,
  and load callbacks through the bridge.
  Evidence: `filesystem/zip.c`; command
  `.\waf.bat build --targets=test_filesystem_zip_backend,test_zip-archive`.
- [x] `FS-ZIP-004` Expand ZIP negative coverage for corrupt archives and
  unsupported compression.
  Evidence: `tests/filesystem/zip-archive.c`; command
  `.\waf.bat build --targets=test_filesystem_zip_backend,test_zip-archive`.
- [x] `FS-ZIP-005` Run filesystem unit tests after ZIP bridge.
  Evidence: command `.\waf.bat build` passed 18/18 selected tests; explicit
  follow-up commands `.\build\src\test_filesystem_zip_backend.exe`,
  `.\build\filesystem\test_zip-archive.exe`,
  `.\build\filesystem\test_wad-archive.exe`, and
  `.\build\filesystem\test_no-init.exe` passed.
- [x] `FS-ZIP-006` Run Windows runtime smoke test after ZIP bridge.
  Evidence: refreshed `run-win32/filesystem_stdio.dll` from the current build
  and ran `.\xash3d.exe -dev 2 -log +fs_path +quit` with the Steam Half-Life
  install as `XASH3D_RODIR`. Exit code was `0`; `engine.log` showed
  `valve/extras.pk3` mounted, printed search paths, and stopped with reason
  `"command"`.

## Phase 11: Android Assets Backend Plan

- [x] `FS-ANDROID-001` Analyze Android backend build and runtime impact before
  adding bridge code.
  Evidence: `Documentation/codex/modularization-plan/android-assets-backend-plan.md`.
- [x] `FS-ANDROID-002` Add target-neutral `AndroidAssetsBackend` skeleton and
  hook-forwarding tests.
  Evidence: `src/include/filesystem/android_assets_backend.hpp`,
  `src/filesystem/android_assets_backend.cpp`,
  `tests/filesystem/android_assets_backend.cpp`.
- [x] `FS-ANDROID-003` Add C adapter bridge without pulling Android headers
  into desktop builds.
  Evidence: `filesystem/android_assets_backend_adapter.h`,
  `filesystem/android_assets_backend_adapter.cpp`.
- [x] `FS-ANDROID-004` Forward Android asset print, close, open, file time,
  find, search, and load callbacks through the bridge behind `XASH_ANDROID`.
  Evidence: `filesystem/android.c`.
- [x] `FS-ANDROID-005` Run desktop unit tests after Android bridge scaffolding.
  Evidence: command
  `.\waf.bat build --targets=test_filesystem_android_assets_backend` passed;
  command `.\waf.bat clean build` passed 32/32 tests.
- [x] `FS-ANDROID-006` Run Windows runtime smoke test after Android bridge
  scaffolding.
  Evidence: refreshed `run-win32/filesystem_stdio.dll` from the current build
  and ran `.\xash3d.exe -dev 2 -log +fs_path +quit` with the Steam Half-Life
  install as `XASH3D_RODIR`. Exit code was `0`; `engine.log` printed search
  paths and stopped with reason `"command"`.
- [x] `FS-ANDROID-007` Capture Android build/runtime validation plan or
  evidence when an Android validation path is available.
  Evidence: `Documentation/codex/modularization-plan/android-assets-backend-plan.md`
  captures the Android build/device validation path; runtime execution remains
  deferred until an Android validation target is available.

## Phase 12: Filesystem.c State Extraction

- [x] `FS-STATE-002` Add focused TODO list for `filesystem.c` state
  migration.
  Evidence: `Documentation/codex/todo/filesystem_state_todo.md`.
- [x] `FS-STATE-003` Add target-neutral `FilesystemState` scaffold and tests.
  Evidence: `src/include/filesystem/filesystem_state.hpp`,
  `src/filesystem/filesystem_state.cpp`,
  `tests/filesystem/filesystem_state.cpp`; command
  `.\waf.bat build --targets=test_filesystem_state`.
- [x] `FS-STATE-004` Add read-only snapshot capture from current
  `filesystem.c` globals.
  Evidence: `filesystem/filesystem_state_adapter.h`,
  `filesystem/filesystem_state_adapter.cpp`, and `FS_SyncStateFromGlobals` in
  `filesystem/filesystem.c`.
- [x] `FS-STATE-005` Route `FS_AllowDirectPaths` through the new state helper.
  Evidence: `FS_SetDirectPaths` and `FS_DirectPathsEnabled` in
  `filesystem/filesystem.c`.
- [x] `FS-STATE-006` Route root/base/game/rodir/language assignment through
  state setters.
  Evidence: `FS_SetRootDir`, `FS_SetBaseDir`, `FS_SetGameDir`,
  `FS_SetReadOnlyDir`, and `FS_SetLanguage` in `filesystem/filesystem.c`.
- [x] `FS-STATE-007` Route search path list head and write path through state
  accessors.
  Evidence: `FS_SearchPaths`, `FS_SetSearchPaths`, `FS_WritePath`, and
  `FS_SetWritePath` in `filesystem/filesystem.c`.
- [x] `FS-STATE-008` Run desktop tests after the state scaffold.
  Evidence: command `.\waf.bat clean build` passed 33/33 tests.
- [x] `FS-STATE-009` Run Windows runtime smoke test after the state scaffold.
  Evidence: refreshed `run-win32/filesystem_stdio.dll` from the current build
  and ran `.\xash3d.exe -dev 2 -log +fs_path +quit` with the Steam Half-Life
  install as `XASH3D_RODIR`. Exit code was `0`; `engine.log` printed search
  paths and stopped with reason `"command"`.

## Phase 13: Path Policy Extraction

- [x] `FS-PATH-001` Add focused TODO list for path policy migration.
  Evidence: `Documentation/codex/todo/path_policy_todo.md`.
- [x] `FS-PATH-002` Add target-neutral path rejection helper and tests.
  Evidence: `src/include/filesystem/path_policy.hpp`,
  `src/filesystem/path_policy.cpp`, `tests/filesystem/path_policy.cpp`.
- [x] `FS-PATH-003` Route `FS_CheckNastyPath` through `PathPolicy`.
  Evidence: `filesystem/path_policy_adapter.h`,
  `filesystem/path_policy_adapter.cpp`, `filesystem/filesystem.c`.
- [x] `FS-PATH-004` Route direct-path relative path compatibility through
  `PathPolicy`.
  Evidence: `FS_PathPolicy_StripDirectRelativePrefix` usage in
  `filesystem/filesystem.c`.
- [x] `FS-PATH-005` Route write-mode mutation detection through a dedicated
  policy helper.
  Evidence: `FS_PathPolicy_IsWriteMode` usage in `filesystem/filesystem.c`.
- [x] `FS-PATH-006` Run desktop tests after path policy extraction.
  Evidence: command
  `.\waf.bat build --targets=test_filesystem_path_policy,test_directpath,test_caseinsensitive,test_rodir`
  passed 4/4 tests; command `.\waf.bat clean build` passed 34/34 tests.
- [x] `FS-PATH-007` Run Windows runtime smoke test after path policy
  extraction.
  Evidence: refreshed `run-win32/filesystem_stdio.dll` from the current build
  and ran `.\xash3d.exe -dev 2 -log +fs_path +quit` with the Steam Half-Life
  install as `XASH3D_RODIR`. Exit code was `0`; `engine.log` printed search
  paths and stopped with reason `"command"`.

## Phase 14: Game Hierarchy Builder

- [x] `FS-HIER-002` Add focused TODO list for game hierarchy migration.
  Evidence: `Documentation/codex/todo/game_hierarchy_todo.md`.
- [ ] `FS-HIER-003` Add mount request record type and tests.
  Evidence:
- [ ] `FS-HIER-004` Build hierarchy mount requests before applying them to
  legacy search paths.
  Evidence:
- [ ] `FS-HIER-005` Route `FS_LoadGameInfo` mount construction through the
  hierarchy builder.
  Evidence:

## Phase 15: File Handle Operations

- [x] `FS-FILE-001` Add focused TODO list for file handle migration.
  Evidence: `Documentation/codex/todo/file_handle_todo.md`.
- [ ] `FS-FILE-002` Add focused tests for seek/read/write/decompression edge
  cases before extraction.
  Evidence:
- [ ] `FS-FILE-003` Add first target-neutral helper around `file_t`
  operations.
  Evidence:

## Phase 16: Search Result Assembly

- [x] `FS-SEARCH-001` Add focused TODO list for search result migration.
  Evidence: `Documentation/codex/todo/search_results_todo.md`.
- [ ] `FS-SEARCH-002` Add tests for result ordering and duplicate filtering.
  Evidence:
- [ ] `FS-SEARCH-003` Add target-neutral `SearchResultBuilder`.
  Evidence:
- [ ] `FS-SEARCH-004` Route `FS_Search` result assembly through the builder.
  Evidence:

## Phase 17: Library Locator

- [x] `FS-LIB-001` Add focused TODO list for library locator migration.
  Evidence: `Documentation/codex/todo/library_locator_todo.md`.
- [ ] `FS-LIB-002` Expand library lookup tests for direct-path and relative
  path quirks.
  Evidence:
- [ ] `FS-LIB-003` Add target-neutral library short-path normalization helper.
  Evidence:
- [ ] `FS-LIB-004` Route `FS_FindLibrary` through `LibraryLocator`.
  Evidence:

## Phase 18: Compatibility Facades

- [x] `FS-COMPAT-001` Add focused TODO list for legacy compatibility facades.
  Evidence: `Documentation/codex/todo/compatibility_facades_todo.md`.
- [ ] `FS-COMPAT-002` Add ABI drift checklist before larger rewires.
  Evidence:
- [ ] `FS-COMPAT-003` Add wrapper-only tests when internal helpers touch
  public methods.
  Evidence:

## Phase 19: Commit And Review Hygiene

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
| 2026-05-09 | DEC-015 | Use a fixed-capacity ordered registry template for first shared registry work, keeping mount-order and filesystem policy outside the generic utility. | `src/include/utilities/registry.hpp`, `todo/utilities_todo.md` |
| 2026-05-09 | DEC-016 | Store the first live directory backend bridge pointer inside the private `dir_t` root object instead of changing `searchpath_t` layout. | `filesystem/dir.c`, `todo/directory_backend_todo.md` |
| 2026-05-09 | DEC-017 | Keep legacy archive factory function pointers in `filesystem.c` during first registry integration, and expose modern archive descriptor metadata through a small C adapter. | `filesystem/archive_registry_adapter.h`, `filesystem/filesystem.c`, `todo/archive_registry_todo.md` |
| 2026-05-09 | DEC-018 | Use the same behavior-neutral bridge pattern for the first PAK backend pilot: private C++ backend object, C adapter, legacy callback implementation retained behind hooks. | `src/include/filesystem/pak_backend.hpp`, `filesystem/pak_backend_adapter.h`, `filesystem/pak.c` |
| 2026-05-09 | DEC-019 | Use the PAK bridge pattern for WAD while explicitly forwarding `pfnLoadFile`, because WAD lump loading is the primary read path. | `src/include/filesystem/wad_backend.hpp`, `filesystem/wad_backend_adapter.h`, `filesystem/wad.c` |
| 2026-05-09 | DEC-020 | Use the same bridge pattern for ZIP/PK3 while explicitly preserving stored/deflated load paths and unsupported compression failure behavior. | `src/include/filesystem/zip_backend.hpp`, `filesystem/zip_backend_adapter.h`, `filesystem/zip.c`, `tests/filesystem/zip-archive.c` |
| 2026-05-09 | DEC-021 | Keep Android runtime code behind `XASH_ANDROID`, but allow the target-neutral `AndroidAssetsBackend` class and adapter declarations to compile on desktop when they avoid Android headers and APIs. | `modularization-plan/android-assets-backend-plan.md`, `todo/android_assets_backend_todo.md` |
| 2026-05-09 | DEC-022 | Start `filesystem.c` migration with a target-neutral `FilesystemState` scaffold and tests before routing legacy globals through it. | `todo/filesystem_state_todo.md`, `src/include/filesystem/filesystem_state.hpp`, `tests/filesystem/filesystem_state.cpp` |
| 2026-05-09 | DEC-023 | Preserve current path rejection semantics exactly in `PathPolicy`, including direct-path bypass after empty-path rejection and the single leading `../` strip quirk. | `src/include/filesystem/path_policy.hpp`, `tests/filesystem/path_policy.cpp`, `filesystem/filesystem.c` |
