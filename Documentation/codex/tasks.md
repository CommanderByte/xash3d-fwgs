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
  Evidence: `Documentation/codex/done/todo/utilities_todo.md`.
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
  `Documentation/codex/done/todo/archive_registry_todo.md`,
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
  `Documentation/codex/done/todo/directory_backend_todo.md`.
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
  Evidence: `Documentation/codex/done/todo/pak_backend_todo.md`.
- [x] `FS-BACKEND-002` Add migration TODO for WAD backend.
  Evidence: `Documentation/codex/done/todo/wad_backend_todo.md`.
- [x] `FS-BACKEND-003` Add migration TODO for ZIP/PK3 backend.
  Evidence: `Documentation/codex/done/todo/zip_backend_todo.md`.
- [x] `FS-BACKEND-004` Add migration TODO for Android assets backend.
  Evidence: `Documentation/codex/done/todo/android_assets_backend_todo.md`.
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
  Evidence: `Documentation/codex/done/todo/filesystem_state_todo.md`.
- [x] `FS-STATE-003` Add target-neutral `FilesystemState` scaffold and tests.
  Evidence: `src/include/filesystem/filesystem_state.hpp`,
  `src/filesystem/filesystem_state.cpp`,
  `tests/filesystem/filesystem_state.cpp`; command
  `.\waf.bat build --targets=test_filesystem_state`.
- [x] `FS-STATE-004` Add read-only snapshot capture from current
  `filesystem.c` globals.
  Evidence: `src/include/filesystem/compat/filesystem_runtime_adapter.h`,
  `src/filesystem/compat/filesystem_runtime_adapter.cpp`, and `FS_SyncStateFromGlobals`
  in `filesystem/filesystem.c`; the original state adapter was folded into
  the runtime adapter during Phase 26.
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
  Evidence: `Documentation/codex/done/todo/path_policy_todo.md`.
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
  Evidence: `Documentation/codex/done/todo/game_hierarchy_todo.md`.
- [x] `FS-HIER-003` Add mount request record type and tests.
  Evidence: `src/include/filesystem/game_hierarchy_builder.hpp`,
  `tests/filesystem/game_hierarchy_builder.cpp`.
- [x] `FS-HIER-004` Build hierarchy mount requests before applying them to
  legacy search paths.
  Evidence: `src/filesystem/game_hierarchy_builder.cpp`,
  `Documentation/codex/modern/filesystem/game-hierarchy-builder.md`.
- [x] `FS-HIER-005` Route `FS_LoadGameInfo` mount construction through the
  hierarchy builder.
  Evidence: `filesystem/game_hierarchy_adapter.cpp`, `filesystem/filesystem.c`;
  commands `.\waf.bat build --targets=test_filesystem_game_hierarchy_builder,test_hierarchy,test_rodir,test_directpath`,
  `.\waf.bat build`, and Windows runtime smoke with `+fs_path +quit`.

## Phase 15: File Handle Operations

- [x] `FS-FILE-001` Add focused TODO list for file handle migration.
  Evidence: `Documentation/codex/todo/file_handle_todo.md`.
- [x] `FS-FILE-002` Add focused tests for seek/read/write/decompression edge
  cases before extraction.
  Evidence: `tests/filesystem/file-handle.c`; command
  `.\waf.bat build --targets=test_file-handle`.
- [x] `FS-FILE-003` Add first target-neutral helper around `file_t`
  operations.
  Evidence: `src/include/filesystem/file_handle_ops.hpp`,
  `src/filesystem/file_handle_ops.cpp`,
  `tests/filesystem/file_handle_ops.cpp`; command
  `.\waf.bat build --targets=test_filesystem_file_handle_ops`.
- [x] `FS-FILE-004` Document decompression and backup-handle ownership rules.
  Evidence: `Documentation/codex/modern/filesystem/file-handle-ownership.md`.
- [x] `FS-FILE-005` Route tell/eof/seek cursor math through `FileHandleOps`.
  Evidence: `src/filesystem/compat/file_handle_ops_adapter.cpp`,
  `filesystem/filesystem.c`; commands
  `.\waf.bat build --targets=test_filesystem_file_handle_ops,test_file-handle`
  and direct execution of both test binaries.

## Phase 16: Search Result Assembly

- [x] `FS-SEARCH-001` Add focused TODO list for search result migration.
  Evidence: `Documentation/codex/done/todo/search_results_todo.md`.
- [x] `FS-SEARCH-002` Add tests for result ordering and duplicate filtering.
  Evidence: `tests/filesystem/search-results.c`; command
  `.\waf.bat build --targets=test_search-results`.
- [x] `FS-SEARCH-003` Add target-neutral `SearchResultBuilder`.
  Evidence: `src/include/filesystem/search_result_builder.hpp`,
  `src/filesystem/search_result_builder.cpp`,
  `tests/filesystem/search_result_builder.cpp`; command
  `.\waf.bat build --targets=test_filesystem_search_result_builder`.
- [x] `FS-SEARCH-004` Route `FS_Search` result assembly through the builder.
  Evidence: `src/filesystem/compat/search_result_builder_adapter.cpp`,
  `filesystem/filesystem.c`; command
  `.\waf.bat build --targets=test_search-results,test_filesystem_search_result_builder`.

## Phase 17: Library Locator

- [x] `FS-LIB-001` Add focused TODO list for library locator migration.
  Evidence: `Documentation/codex/done/todo/library_locator_todo.md`.
- [x] `FS-LIB-002` Expand library lookup tests for direct-path and relative
  path quirks.
  Evidence: `tests/filesystem/dll-lookup.c`; command
  `.\waf.bat build --targets=test_dll-lookup`.
- [x] `FS-LIB-003` Add target-neutral library short-path normalization helper.
  Evidence: `src/include/filesystem/library_locator.hpp`,
  `src/filesystem/library_locator.cpp`,
  `tests/filesystem/library_locator.cpp`; command
  `.\waf.bat build --targets=test_filesystem_library_locator`.
- [x] `FS-LIB-004` Route `FS_FindLibrary` through `LibraryLocator`.
  Evidence: `filesystem/library_locator_adapter.cpp`,
  `filesystem/filesystem.c`; command
  `.\waf.bat build --targets=test_dll-lookup,test_filesystem_library_locator`.

## Phase 18: Compatibility Facades

- [x] `FS-COMPAT-001` Add focused TODO list for legacy compatibility facades.
  Evidence: `Documentation/codex/done/todo/compatibility_facades_todo.md`.
- [x] `FS-COMPAT-002` Add ABI drift checklist before larger rewires.
  Evidence: `Documentation/codex/modern/filesystem/abi-drift-checklist.md`.
- [x] `FS-COMPAT-003` Add wrapper-only tests when internal helpers touch
  public methods.
  Evidence: `tests/filesystem/interface.cpp`; command
  `.\waf.bat build --targets=test_interface`.

## Phase 19: Filesystem Folder Migration Map

- [x] `FS-MAP-001` Classify remaining `filesystem/` files by migration role.
  Evidence: `Documentation/codex/modern/filesystem/filesystem-folder-migration-map.md`.
- [x] `FS-MAP-002` Add implementation-body migration TODO list.
  Evidence: `Documentation/codex/done/todo/backend_implementation_migration_todo.md`.
- [x] `FS-MAP-003` Add `Documentation/codex/done/` archival policy before
  moving completed TODO or audit files.
  Evidence: `Documentation/codex/done/README.md`,
  `Documentation/codex/done/todo/README.md`; completed TODO files moved from
  `Documentation/codex/todo/` to `Documentation/codex/done/todo/`.

## Phase 20: WAD Implementation Body Migration

- [x] `FS-WAD-IMPL-001` Audit `filesystem/wad.c` responsibilities and map each
  function to `WadBackend`, adapter-only code, or shared runtime support.
  Evidence: `Documentation/codex/done/audit/filesystem/wad-implementation-audit.md`.
- [x] `FS-WAD-IMPL-002` Expand WAD tests for any uncovered parsing, lookup,
  load, or archive-in-archive behavior found during the audit.
  Evidence: `tests/filesystem/wad-archive.c`; `.\waf.bat build
  --targets=test_wad-archive` passed on 2026-05-09.
- [x] `FS-WAD-IMPL-003A` Move WAD type mapping, sorted lump insertion, and
  binary lump lookup into `src/filesystem/wad_backend.cpp`.
  Evidence: `src/filesystem/wad_backend.cpp`,
  `filesystem/wad_backend_adapter.cpp`, `filesystem/wad.c`;
  `.\waf.bat build --targets=test_filesystem_wad_backend,test_wad-archive`
  passed on 2026-05-09, and `build\filesystem\test_wad-archive.exe` returned
  exit code `0`.
- [x] `FS-WAD-IMPL-003B` Move WAD header parsing and lump-table normalization
  into `src/filesystem/wad_backend.cpp`.
  Evidence: `src/filesystem/wad_backend.cpp`,
  `filesystem/wad_backend_adapter.cpp`, `filesystem/wad.c`;
  `.\waf.bat build --targets=test_filesystem_wad_backend,test_wad-archive`
  passed on 2026-05-09.
- [x] `FS-WAD-IMPL-004` Move WAD open/load/search behavior into
  `src/filesystem/wad_backend.cpp` while preserving the C callback adapter.
  Evidence: `src/filesystem/wad_backend.cpp`,
  `filesystem/wad_backend_adapter.cpp`, `filesystem/wad.c`;
  `.\waf.bat build --targets=test_filesystem_wad_backend,test_wad-archive`
  passed on 2026-05-09.
- [x] `FS-WAD-IMPL-005` Shrink `filesystem/wad.c` to adapter-only or document
  remaining blockers.
  Evidence: `filesystem/wad.c` now delegates WAD parsing, lookup, search, open
  orchestration, and lump reads through `filesystem/wad_backend_adapter.cpp`;
  remaining `wfile_t`, callback registration, and `FS_AddWad_Fullpath`
  ownership blockers are documented in
  `Documentation/codex/done/audit/filesystem/wad-implementation-audit.md`.

## Phase 21: PAK Implementation Body Migration

- [x] `FS-PAK-IMPL-001` Audit `filesystem/pak.c` responsibilities and expand
  tests for uncovered PAK parsing/search/open behavior.
  Evidence: `Documentation/codex/done/audit/filesystem/pak-implementation-audit.md`,
  `tests/filesystem/archive-order.c`.
- [x] `FS-PAK-IMPL-002` Move PAK parsing and file table lookup into
  `src/filesystem/pak_backend.cpp`.
  Evidence: `src/filesystem/pak_backend.cpp`,
  `filesystem/pak_backend_adapter.cpp`, `filesystem/pak.c`;
  `.\waf.bat build --targets=test_filesystem_pak_backend,test_archive-order`
  passed on 2026-05-09.
- [x] `FS-PAK-IMPL-003` Move PAK open/search behavior into
  `src/filesystem/pak_backend.cpp` while preserving the C callback adapter.
  Evidence: `src/filesystem/pak_backend.cpp`,
  `filesystem/pak_backend_adapter.cpp`, `filesystem/pak.c`;
  `.\waf.bat build --targets=test_filesystem_pak_backend,test_archive-order`
  passed on 2026-05-09.
- [x] `FS-PAK-IMPL-004` Shrink `filesystem/pak.c` to adapter-only or document
  remaining blockers.
  Evidence: `filesystem/pak.c` now delegates PAK parsing, entry lookup, search,
  open orchestration, and packed-entry open through
  `filesystem/pak_backend_adapter.cpp`; remaining `pack_t`,
  `FS_AddPak_Fullpath`, and `FS_CheckForQuakePak` ownership blockers are
  documented in
  `Documentation/codex/done/audit/filesystem/pak-implementation-audit.md`.

## Phase 22: ZIP/PK3 Implementation Body Migration

- [x] `FS-ZIP-IMPL-001` Audit `filesystem/zip.c` responsibilities, including
  central directory parsing, unsupported compression, and deflated handles.
  Evidence: `Documentation/codex/done/audit/filesystem/zip-implementation-audit.md`.
- [x] `FS-ZIP-IMPL-002` Expand ZIP/PK3 tests for uncovered edge cases before
  moving parser code.
  Evidence: `tests/filesystem/zip-archive.c`,
  `tests/filesystem/zip_backend.cpp`.
- [x] `FS-ZIP-IMPL-003` Move ZIP/PK3 parsing and lookup into
  `src/filesystem/zip_backend.cpp`.
  Evidence: `src/filesystem/zip_backend.cpp`,
  `filesystem/zip_backend_adapter.cpp`, `filesystem/zip.c`;
  `.\waf.bat build --targets=test_filesystem_zip_backend,test_zip-archive`
  passed on 2026-05-09.
- [x] `FS-ZIP-IMPL-004` Move stored/deflated open/search setup into
  `src/filesystem/zip_backend.cpp` while preserving file-handle behavior.
  Evidence: `src/filesystem/zip_backend.cpp`,
  `filesystem/zip_backend_adapter.cpp`, `filesystem/zip.c`;
  `.\waf.bat build --targets=test_filesystem_zip_backend,test_zip-archive`
  passed on 2026-05-09.
- [x] `FS-ZIP-IMPL-005` Shrink `filesystem/zip.c` to adapter-only or document
  remaining blockers.
  Evidence: `filesystem/zip.c` now delegates ZIP parsing, lookup, search, open
  orchestration, open-entry policy, and load-file behavior through
  `filesystem/zip_backend_adapter.cpp`; remaining `zip_t` and
  `FS_AddZip_Fullpath` ownership blockers are documented in
  `Documentation/codex/done/audit/filesystem/zip-implementation-audit.md`.

## Phase 23: Directory Implementation Body Migration

- [x] `FS-DIR-IMPL-001` Audit `filesystem/dir.c` cache, search, and
  case-insensitive path repair responsibilities.
  Evidence: `Documentation/codex/done/audit/filesystem/directory-implementation-audit.md`.
- [x] `FS-DIR-IMPL-002` Expand directory tests for uncovered cache refresh,
  case repair, and platform path quirks.
  Evidence: `tests/filesystem/directory_backend.cpp`;
  `.\waf.bat build --targets=test_filesystem_directory_backend,test_caseinsensitive,test_pk3dir`
  passed on 2026-05-09.
- [x] `FS-DIR-IMPL-003` Move directory cache/search behavior into
  `src/filesystem/directory_backend.cpp`.
  Evidence: `src/filesystem/directory_backend.cpp`,
  `filesystem/dir_backend_adapter.cpp`, `filesystem/dir.c`;
  `.\waf.bat build --targets=test_filesystem_directory_backend,test_caseinsensitive,test_pk3dir`
  passed on 2026-05-09.
- [x] `FS-DIR-IMPL-004` Shrink `filesystem/dir.c` to adapter-only or document
  remaining blockers.
  Evidence: `filesystem/dir.c` now delegates cache population, cache refresh,
  case repair, loose-file lookup/open, and directory search through
  `filesystem/dir_backend_adapter.cpp`; remaining platform probing,
  `dir_t` allocation, and searchpath callback registration blockers are
  documented in
  `Documentation/codex/done/audit/filesystem/directory-implementation-audit.md`.

## Phase 24: Android Assets Implementation Body Migration

- [x] `FS-ANDROID-IMPL-001` Audit `filesystem/android.c` platform-specific
  runtime responsibilities.
  Evidence: `Documentation/codex/done/audit/filesystem/android-assets-implementation-audit.md`.
- [x] `FS-ANDROID-IMPL-002` Move target-neutral Android asset behavior into
  `src/filesystem/android_assets_backend.cpp`.
  Evidence: `src/filesystem/android_assets_backend.cpp`,
  `filesystem/android_assets_backend_adapter.cpp`, `filesystem/android.c`,
  `tests/filesystem/android_assets_backend.cpp`;
  `.\waf.bat build --targets=test_filesystem_android_assets_backend` passed
  on 2026-05-09.
- [x] `FS-ANDROID-IMPL-003` Keep Android headers and runtime calls behind
  platform adapters so desktop builds remain clean.
  Evidence: Android JNI and `AAssetManager` calls remain inside
  `filesystem/android.c` behind `#if XASH_ANDROID`; desktop helper tests pass
  with opaque fake asset handles.

## Phase 25: Filesystem Runtime Ownership

- [x] `FS-RUNTIME-001` Design `FilesystemRuntime` ownership boundaries for
  state, search paths, write path, gameinfo mounts, archive registry, and
  diagnostics.
  Evidence: `Documentation/codex/modern/filesystem/filesystem-runtime-ownership.md`,
  `src/include/filesystem/filesystem_runtime.hpp`.
- [x] `FS-RUNTIME-002` Move search path list ownership behind
  `FilesystemRuntime` while preserving `fs_api_t` behavior.
  Evidence: `src/filesystem/filesystem_runtime.cpp`,
  `src/filesystem/compat/filesystem_runtime_adapter.cpp`, `filesystem/filesystem.c`,
  `tests/filesystem/filesystem_runtime.cpp`;
  `.\waf.bat build --targets=test_filesystem_runtime,test_filesystem_state,test_archive-order,test_file-handle,test_hierarchy,test_search-results`
  passed on 2026-05-09.
- [x] `FS-RUNTIME-003` Move `file_t` allocation/lifetime behind runtime-owned
  handle helpers.
  Evidence: `src/filesystem/filesystem_runtime.cpp`,
  `src/filesystem/compat/filesystem_runtime_adapter.cpp`, `filesystem/filesystem.c`,
  `tests/filesystem/filesystem_runtime.cpp`;
  `.\waf.bat build --targets=test_filesystem_runtime,test_filesystem_state,test_archive-order,test_file-handle,test_hierarchy,test_search-results`
  passed on 2026-05-09.
- [x] `FS-RUNTIME-004` Move target-neutral rescan planning behind runtime
  helpers and document remaining gameinfo blockers.
  Evidence: `FilesystemRuntime::beginRescan` now owns target-neutral rescan
  planning for mount flag masking, direct-path reset, and localization
  language selection; gameinfo parsing and `FI.games` ownership remain legacy
  blockers documented in
  `Documentation/codex/modern/filesystem/filesystem-runtime-ownership.md`.

## Phase 26: Facade Thinning And Build Convergence

- [x] `FS-FACADE-001` Route `VFileSystem009.cpp` methods through the modern
  runtime while preserving the public vtable and `CreateInterface` behavior.
  Evidence: `src/filesystem/valve_path_resolver.cpp`,
  `src/filesystem/compat/filesystem_runtime_adapter.cpp`, and
  `filesystem/VFileSystem009.cpp`; `VFileSystem009.h` was unchanged.
- [x] `FS-FACADE-002` Update `filesystem/wscript` so modern implementation
  bodies are the primary source of filesystem behavior.
  Evidence: `src/wscript` now builds `modern_filesystem`, and
  `filesystem/wscript` links that library while explicitly listing facade,
  backend-adapter, and C/C++ adapter sources instead of globbing every source
  file as undifferentiated legacy implementation.
- [x] `FS-FACADE-003` Remove or quarantine obsolete legacy implementation
  files once they are adapter-only and unused.
  Evidence: `filesystem/filesystem_state_adapter.h` was folded into
  `src/include/filesystem/compat/filesystem_runtime_adapter.h`; `filesystem_state_adapter.cpp`
  was already removed in Phase 25.
- [x] `FS-FACADE-004` Move completed TODO and audit documents into
  `Documentation/codex/done/` after implementation evidence is complete.
  Evidence: completed backend audit notes moved to
  `Documentation/codex/done/audit/filesystem/`; completed TODOs remain under
  `Documentation/codex/done/todo/`.

## Phase 27: Export Dependency Audit And Trim Planning

- [x] `FS-SURFACE-001` Verify the actual `filesystem_stdio` export set.
  Evidence: `Documentation/codex/modern/filesystem/export-dependency-audit.md`;
  built Windows DLL export table contains `CreateInterface` and `GetFSAPI`.
- [x] `FS-SURFACE-002` Audit in-repo engine and utility dependencies on the
  filesystem ABI.
  Evidence: `Documentation/codex/modern/filesystem/export-dependency-audit.md`.
- [x] `FS-SURFACE-003` Fix export-list metadata to match the supported
  runtime loader contract.
  Evidence: `filesystem/exports.txt` now lists both `CreateInterface` and
  `GetFSAPI`.
- [x] `FS-SURFACE-004` Identify trim candidates that do not require an ABI
  break.
  Evidence: `Documentation/codex/modern/filesystem/export-dependency-audit.md`.

## Phase 28: Modern Handler Migration And Legacy Adapter Boundary

- [x] `FS-HANDLER-001` Define the first `LegacyAdapter` boundary for
  compatibility calls.
  Evidence: `Documentation/codex/done/todo/modern_filesystem_handlers_todo.md`,
  `src/include/filesystem/compat/filesystem_facade_adapter.h`.
- [x] `FS-HANDLER-002` Move `VFileSystem009.cpp` off
  `filesystem_internal.h` through a narrow facade adapter.
  Evidence: `src/include/filesystem/compat/filesystem_facade_adapter.h`,
  `src/filesystem/compat/filesystem_facade_adapter.cpp`,
  `src/filesystem/compat/VFileSystem009.cpp`;
  `.\waf.bat build --targets=test_interface`,
  `build\filesystem\test_interface.exe`, and `.\waf.bat build` passed on
  2026-05-09.
- [x] `FS-HANDLER-003` Extract `FS_Search` result assembly into a modern
  handler while preserving public `search_t`.
  Evidence: `src/filesystem/compat/search_result_builder_adapter.cpp`,
  `src/include/filesystem/compat/search_result_builder_adapter.h`,
  `filesystem/filesystem.c`;
  commands `.\waf.bat build --targets=test_search-results,test_filesystem_search_result_builder`,
  direct `build\filesystem\test_search-results.exe`, and direct
  `build\src\test_filesystem_search_result_builder.exe`,
  `.\waf.bat build --targets=test_interface`, and direct
  `build\filesystem\test_interface.exe` passed on
  2026-05-09.
- [x] `FS-HANDLER-004` Move file handle operation bodies behind modern
  handlers while preserving public `file_t` opacity.
  Evidence: `src/include/filesystem/file_handle_ops.hpp`,
  `src/filesystem/file_handle_ops.cpp`,
  `src/include/filesystem/compat/file_handle_ops_adapter.h`,
  `src/filesystem/compat/file_handle_ops_adapter.cpp`,
  `filesystem/filesystem.c`,
  `tests/filesystem/file_handle_ops.cpp`; command
  `.\waf.bat build --targets=test_filesystem_runtime,test_filesystem_file_handle_ops,test_file-handle,test_archive-order,test_wad-archive,test_zip-archive`
  passed 6/6 tests on 2026-05-09; `.\waf.bat build` passed 25/25
  tests.
- [x] `FS-HANDLER-005` Move searchpath allocation and callback registration
  toward runtime-owned mount handlers.
  Evidence: `src/include/filesystem/filesystem_runtime.hpp`,
  `src/filesystem/filesystem_runtime.cpp`,
  `src/include/filesystem/compat/filesystem_runtime_adapter.h`,
  `src/filesystem/compat/filesystem_runtime_adapter.cpp`,
  `src/include/filesystem/compat/searchpath_mount_adapter.h`,
  `src/filesystem/compat/searchpath_mount_adapter.cpp`,
  `src/filesystem/compat/dir.c`, `src/filesystem/compat/pak.c`,
  `src/filesystem/compat/wad.c`, `src/filesystem/compat/zip.c`,
  `src/filesystem/compat/android.c`, `tests/filesystem/filesystem_runtime.cpp`;
  command
  `.\waf.bat build --targets=test_filesystem_runtime,test_filesystem_file_handle_ops,test_file-handle,test_archive-order,test_wad-archive,test_zip-archive`
  passed 6/6 tests on 2026-05-09; `.\waf.bat build` passed 25/25
  tests.
- [x] `FS-HANDLER-006` Split `filesystem_internal.h` into focused private
  compatibility headers.
  Evidence: `filesystem/filesystem_internal.h`,
  `src/include/filesystem/compat/private/filesystem_private_types.h`,
  `src/include/filesystem/compat/private/filesystem_private_globals.h`,
  `src/include/filesystem/compat/private/filesystem_private_memory.h`,
  `src/include/filesystem/compat/private/filesystem_private_api.h`; commands
  `.\waf.bat build --targets=test_filesystem_runtime,test_filesystem_file_handle_ops,test_file-handle,test_archive-order,test_wad-archive,test_zip-archive,test_interface`,
  `.\waf.bat build`, direct `build\filesystem\test_interface.exe`,
  direct `build\filesystem\test_file-handle.exe`, direct
  `build\filesystem\test_archive-order.exe`, and direct
  `build\src\test_filesystem_runtime.exe` passed on 2026-05-09.
- [x] `FS-HANDLER-007` Create a gameinfo snapshot/query plan before moving
  `FI.games`.
  Evidence: `Documentation/codex/modern/filesystem/gameinfo-snapshot-plan.md`.

## Phase 29: Filesystem Folder Decluttering And Compat Relocation

- [x] `FS-DECLUTTER-001` Move compatibility adapter sources and headers out of
  the legacy `filesystem/` folder.
  Evidence: `Documentation/codex/done/todo/filesystem_decluttering_todo.md`,
  `src/filesystem/compat/`, `src/include/filesystem/compat/`,
  `filesystem/wscript`; `filesystem/` now contains 17 files; command
  `.\waf.bat build --targets=test_interface,test_file-handle,test_archive-order,test_wad-archive,test_zip-archive,test_filesystem_runtime`
  passed on 2026-05-09; `.\waf.bat build`, direct
  `build\filesystem\test_interface.exe`, direct
  `build\filesystem\test_file-handle.exe`, direct
  `build\filesystem\test_archive-order.exe`, direct
  `build\filesystem\test_wad-archive.exe`, direct
  `build\filesystem\test_zip-archive.exe`, direct
  `build\src\test_filesystem_runtime.exe`, and direct
  `build\src\test_filesystem_file_handle_ops.exe` passed.
- [x] `FS-DECLUTTER-002` Move private compatibility headers out of
  `filesystem/` once the remaining legacy `.c` bodies include narrower paths.
  Evidence: private headers moved to
  `src/include/filesystem/compat/private/`; `filesystem/` now contains 13
  files; command
  `.\waf.bat build --targets=test_interface,test_file-handle,test_archive-order,test_wad-archive,test_zip-archive,test_filesystem_runtime`
  passed on 2026-05-09; `.\waf.bat build`, direct
  `build\filesystem\test_interface.exe`, direct
  `build\filesystem\test_file-handle.exe`, direct
  `build\filesystem\test_archive-order.exe`, direct
  `build\filesystem\test_wad-archive.exe`, direct
  `build\filesystem\test_zip-archive.exe`, direct
  `build\src\test_filesystem_runtime.exe`, and direct
  `build\src\test_filesystem_file_handle_ops.exe` passed.
- [x] `FS-DECLUTTER-003` Split `filesystem.c` into export/runtime glue and
  focused legacy compatibility bodies.
  Evidence: `src/filesystem/compat/stringlist_legacy.cpp`,
  `src/filesystem/compat/memory_legacy.cpp`, `filesystem/filesystem.c`;
  command
  `.\waf.bat build --targets=test_interface,test_file-handle,test_archive-order,test_wad-archive,test_zip-archive,test_filesystem_runtime`
  passed on 2026-05-09; `.\waf.bat build`, direct
  `build\filesystem\test_interface.exe`, direct
  `build\filesystem\test_file-handle.exe`, direct
  `build\filesystem\test_archive-order.exe`, direct
  `build\filesystem\test_wad-archive.exe`, direct
  `build\filesystem\test_zip-archive.exe`, direct
  `build\src\test_filesystem_runtime.exe`, and direct
  `build\src\test_filesystem_file_handle_ops.exe` passed.
- [x] `FS-DECLUTTER-004` Move `VFileSystem009.cpp` into a compatibility source
  location while preserving `VFileSystem009.h` in the public legacy folder.
  Evidence: `src/filesystem/compat/VFileSystem009.cpp`,
  `filesystem/VFileSystem009.h`, `filesystem/wscript`; command
  `.\waf.bat build --targets=test_interface,test_file-handle,test_archive-order,test_wad-archive,test_zip-archive,test_filesystem_runtime`
  passed on 2026-05-09; `.\waf.bat build`, direct
  `build\filesystem\test_interface.exe`, direct
  `build\filesystem\test_file-handle.exe`, direct
  `build\filesystem\test_archive-order.exe`, direct
  `build\filesystem\test_wad-archive.exe`, direct
  `build\filesystem\test_zip-archive.exe`, direct
  `build\src\test_filesystem_runtime.exe`, and direct
  `build\src\test_filesystem_file_handle_ops.exe` passed.
- [x] `FS-DECLUTTER-005` Reassess whether `dir.c`, `pak.c`, `wad.c`, `zip.c`,
  and `android.c` can become smaller backend shims or move under compat.
  Evidence: `src/filesystem/compat/dir.c`,
  `src/filesystem/compat/pak.c`, `src/filesystem/compat/wad.c`,
  `src/filesystem/compat/zip.c`, `src/filesystem/compat/android.c`,
  `filesystem/wscript`; `filesystem/` now contains 7 files; command
  `.\waf.bat build --targets=test_interface,test_file-handle,test_archive-order,test_wad-archive,test_zip-archive,test_filesystem_runtime`
  passed on 2026-05-09; `.\waf.bat build`, direct
  `build\filesystem\test_interface.exe`, direct
  `build\filesystem\test_file-handle.exe`, direct
  `build\filesystem\test_archive-order.exe`, direct
  `build\filesystem\test_wad-archive.exe`, direct
  `build\filesystem\test_zip-archive.exe`, direct
  `build\src\test_filesystem_runtime.exe`, and direct
  `build\src\test_filesystem_file_handle_ops.exe` passed.
- [x] `FS-DECLUTTER-006` Update the folder migration map after each physical
  move.
  Evidence: `Documentation/codex/modern/filesystem/filesystem-folder-migration-map.md`,
  `Documentation/codex/done/todo/filesystem_decluttering_todo.md`.

## Deferred Stage: Filesystem Logging And Diagnostics Streamlining

- [!] `FS-LOG-001` Inventory filesystem logging and fatal-error call sites.
  Evidence: `Documentation/codex/deferred/todo/filesystem_logging_todo.md`.
  Resume condition: engine console/logging ownership has been audited.
- [!] `FS-LOG-002` Define a filesystem logging facade backed by existing
  engine callbacks.
  Evidence: deferred until the engine console/common layer defines the
  preferred logging and fatal-error policy.
- [!] `FS-LOG-003` Add structured log categories for filesystem runtime work.
  Evidence: deferred until `FS-LOG-002` resumes.
- [!] `FS-LOG-004` Add tests for log capture where behavior depends on
  diagnostics.
  Evidence: deferred until the logging facade design is no longer
  filesystem-local guesswork.
- [!] `FS-LOG-005` Route modern backend/handler code through the logging
  facade.
  Evidence: deferred to avoid coupling filesystem internals to a premature
  console design.
- [!] `FS-LOG-006` Define release-build behavior for trace-heavy filesystem
  diagnostics.
  Evidence: deferred until the engine logging policy exists.

## Phase 30: Game Launch Modularization Pilot

- [x] `LAUNCH-001` Audit current launcher responsibilities and platform
  branches.
  Evidence: `Documentation/codex/modern/game-launch/architecture.md`.
- [x] `LAUNCH-002` Capture current launch behavior in modern documentation.
  Evidence: `Documentation/codex/modern/game-launch/architecture.md`.
- [x] `LAUNCH-003` Add a minimal launcher TODO/test strategy before extraction.
  Evidence: `Documentation/codex/modern/game-launch/architecture.md`,
  `tests/launcher/README.md`.
- [x] `LAUNCH-004` Create `src/launcher/` and `src/include/launcher/` only when
  a helper is ready to move.
  Evidence: `src/include/launcher/launch_settings.hpp`,
  `src/launcher/launch_settings.cpp`.
- [x] `LAUNCH-005` Extract the first target-neutral helper with tests.
  Evidence: `src/include/launcher/launch_settings.hpp`,
  `src/launcher/launch_settings.cpp`, `tests/launcher/launch_settings.cpp`;
  command `.\waf.bat build --targets=test_launcher_launch_settings,xash3d`
  passed on 2026-05-09.
  Notes: `GetDefaultLaunchSettings()` now owns the build-configured
  `XASH_GAMEDIR` fallback and `XASH_DISABLE_MENU_CHANGEGAME` calculation.
- [x] `LAUNCH-006` Rebuild and smoke test the launcher on Windows defaults.
  Evidence: command
  `.\waf.bat build --targets=test_launcher_launch_settings,xash3d` passed;
  full `.\waf.bat build` passed 41/41 tests;
  copied `build\game_launch\xash3d.exe` to `run-win32`; command
  `.\xash3d.exe -dev 2 -log +wait +wait +quit` exited 0 and logged
  `FS_LoadProgs`, `FS_InitStdio`, `Time to first frame: 0.440 seconds`,
  `COM_FreeLibrary: Unloading filesystem_stdio.dll`, and
  `Stopped with reason "command"` on 2026-05-09.
- [x] `LAUNCH-007` Move engine DLL/SO loading and export lookup behind a
  launcher helper while initially keeping entry points in `game_launch/`.
  Evidence: `src/include/launcher/engine_library.hpp`,
  `src/launcher/engine_library.cpp`, `tests/launcher/engine_library.cpp`;
  command
  `.\waf.bat build --targets=test_launcher_engine_library,test_launcher_launch_settings,xash3d`
  passed; command `.\xash3d.exe -dev 2 -log +wait +wait +quit` exited 0
  from `run-win32` and logged `Time to first frame: 0.406 seconds` on
  2026-05-09.
- [x] `LAUNCH-008` Move shared launch sequencing and Windows argv ownership
  into launcher helpers.
  Evidence: `src/include/launcher/application.hpp`,
  `src/launcher/application.cpp`,
  `src/include/launcher/platform/win32_argv.hpp`,
  `src/launcher/platform/win32_argv.cpp`, `tests/launcher/application.cpp`;
  command
  `.\waf.bat build --targets=test_launcher_application,test_launcher_engine_library,test_launcher_launch_settings,xash3d`
  passed; command `.\xash3d.exe -dev 2 -log +wait +wait +quit` exited 0
  from `run-win32` and logged `Time to first frame: 0.414 seconds` on
  2026-05-09.
- [x] `LAUNCH-009` Define launcher source/resource layout policy before moving
  platform assets.
  Evidence: `Documentation/codex/modern/game-launch/layout-policy.md`.
- [x] `LAUNCH-010` Move Windows launcher resources into a platform resource
  subfolder and update `game_launch/wscript`.
  Evidence: `resources/launcher/windows/game.rc`,
  `resources/launcher/windows/icon-xash-material.ico`,
  `resources/launcher/source/icon-xash-material.png`,
  `game_launch/wscript`; command `.\waf.bat build --targets=xash3d`
  passed; command `.\xash3d.exe -dev 2 -log +wait +wait +quit` exited 0
  from `run-win32` on 2026-05-09.

- [x] `LAUNCH-011` Move the thin executable entry source into the `src`
  launcher tree.
  Evidence: `src/launcher/platform/entry.cpp`,
  `src/launcher/platform/README.md`, `game_launch/wscript`; command
  `.\waf.bat build --targets=test_launcher_application,test_launcher_engine_library,test_launcher_launch_settings,xash3d`
  passed; direct `build\src\test_launcher_application.exe`,
  `build\src\test_launcher_engine_library.exe`, and
  `build\src\test_launcher_launch_settings.exe` passed; copied
  `build\game_launch\xash3d.exe` to `run-win32`; command
  `.\xash3d.exe -dev 2 -log +wait +wait +quit` with
  `XASH3D_BASEDIR=C:\git\xash3d-fwgs\run-win32` and
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`
  exited 0 and logged `Time to first frame: 0.430 seconds`,
  `COM_FreeLibrary: Unloading filesystem_stdio.dll`, and
  `Stopped with reason "command"`; command
  `.\waf.bat build --alltests` passed 45/45 tests on 2026-05-09.

- [x] `LAUNCH-012` Document platform-support expectations for the modular
  launcher layout.
  Evidence: `Documentation/codex/modern/game-launch/platform-support.md`.

## Phase 31: Launcher Platform And Runtime Configuration Split

- [x] `LAUNCH-013` Move dynamic-library OS calls out of shared launcher code
  and into `src/launcher/platform/`.
  Evidence: `src/include/launcher/platform/library.hpp`,
  `src/launcher/platform/library_win32.cpp`,
  `src/launcher/platform/library_posix.cpp`,
  `src/launcher/engine_library.cpp`;
  command
  `.\waf.bat build --targets=test_launcher_application,test_launcher_engine_library,test_launcher_launch_settings,xash3d`
  passed; command `.\waf.bat build --alltests` passed 45/45 tests on
  2026-05-09.

- [x] `LAUNCH-014` Move platform environment setup out of shared launch
  sequencing.
  Evidence: `src/include/launcher/platform/environment.hpp`,
  `src/launcher/platform/environment_default.cpp`,
  `src/launcher/platform/environment_sailfish.cpp`,
  `src/launcher/application.cpp`.

- [x] `LAUNCH-015` Add optional runtime launcher configuration with compiled
  default fallback.
  Evidence: `src/include/launcher/launch_settings.hpp`,
  `src/launcher/launch_settings.cpp`, `src/launcher/config.cpp`,
  `resources/launcher/launcher.example.json`,
  `tests/launcher/launch_settings.cpp`; copied `build\game_launch\xash3d.exe`
  to `run-win32`; command `.\xash3d.exe -dev 2 -log +wait +wait +quit` with
  `XASH3D_BASEDIR=C:\git\xash3d-fwgs\run-win32` and
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`
  exited 0 and logged `Time to first frame: 0.407 seconds`,
  `COM_FreeLibrary: Unloading filesystem_stdio.dll`, and
  `Stopped with reason "command"` on 2026-05-09.

- [x] `LAUNCH-017` Decide whether launcher JSON parsing should stay minimal or
  move to a shared third-party JSON parser.
  Evidence: `Documentation/codex/modern/game-launch/json-policy.md`.

## Phase 32: Launcher Platform Source Selection Cleanup

- [x] `LAUNCH-016` Move Windows argument ownership fully under the platform
  folder.
  Evidence: `src/include/launcher/platform/win32_argv.hpp`,
  `src/launcher/platform/win32_argv.cpp`, `src/launcher/platform/entry.cpp`.

- [x] `LAUNCH-018` Replace mixed platform implementation files with
  Waf-selected platform sources.
  Evidence: `src/wscript`, `src/launcher/platform/library_win32.cpp`,
  `src/launcher/platform/library_posix.cpp`,
  `src/launcher/platform/environment_default.cpp`,
  `src/launcher/platform/environment_sailfish.cpp`; command
  `.\waf.bat build --targets=test_launcher_application,test_launcher_engine_library,test_launcher_launch_settings,xash3d`
  passed; command `.\waf.bat build --alltests` passed 45/45 tests; runtime
  smoke logged `Time to first frame: 0.407 seconds` on 2026-05-09.

## Phase 33: Launcher Build Ownership Consolidation

- [x] `LAUNCH-019` Move launcher executable target ownership into `src/wscript`
  and remove the obsolete `game_launch` subproject.
  Evidence: `wscript`, `src/wscript`, deleted `game_launch/wscript`;
  command
  `.\waf.bat build --targets=test_launcher_application,test_launcher_engine_library,test_launcher_launch_settings,xash3d`
  passed on 2026-05-09; command `.\waf.bat build --alltests` passed 45/45
  tests; copied `build\src\xash3d.exe`, `build\engine\xash.dll`, and
  `build\filesystem\filesystem_stdio.dll` to `run-win32`; command
  `.\xash3d.exe -dev 2 -log +wait +wait +quit` exited 0 and logged
  `Time to first frame: 0.566 seconds`,
  `COM_FreeLibrary: Unloading filesystem_stdio.dll`, and
  `Stopped with reason "command"` on 2026-05-09.

- [x] `LAUNCH-020` Update launcher docs and the codebase map after removing
  `game_launch/`.
  Evidence: `Documentation/codex/codebase-map.md`,
  `Documentation/codex/README.md`,
  `Documentation/codex/modern/game-launch/README.md`,
  `Documentation/codex/modern/game-launch/layout-policy.md`,
  `Documentation/codex/modern/game-launch/platform-support.md`,
  `Documentation/codex/modern/game-launch/architecture.md`,
  `Documentation/codex/windows-build-run-notes.md`.

## Phase 34: Engine/Common Audit

- [x] `ENG-AUDIT-001` Audit `engine/` scale, build ownership, and major
  directory responsibilities.
  Evidence: `Documentation/codex/legacy/engine/common-audit.md`.

- [x] `ENG-AUDIT-002` Audit `engine/common/` clusters, global state, and
  coupling hotspots.
  Evidence: `Documentation/codex/legacy/engine/common-audit.md`.

- [x] `ENG-AUDIT-003` Recommend the next practical modernization pilot.
  Evidence: `Documentation/codex/legacy/engine/common-audit.md`,
  `Documentation/codex/done/todo/engine_common_todo.md`.
  Decision: start with command/cvar ownership and tests before changing
  console/logging or host lifecycle.

## Phase 35: Modern Engine Skeleton

- [x] `ENG-STRUCT-001` Create the initial `src/engine/` modernization
  structure without wiring it into the build.
  Evidence: `src/engine/README.md`, `src/include/engine/README.md`.

- [x] `ENG-STRUCT-002` Reserve engine implementation lanes for command/cvar,
  console/logging, filesystem bridge, host lifecycle, memory, models, network,
  and platform facades.
  Evidence: `src/engine/*/README.md`, `src/include/engine/*/README.md`.

- [x] `ENG-STRUCT-003` Mark command/cvar as the first intended implementation
  lane while keeping the other folders as future homes only.
  Evidence: `src/engine/README.md`,
  `Documentation/codex/done/todo/engine_common_todo.md`.

## Phase 36: Command/Cvar Baseline

- [x] `ENG-CMD-001` Document `cmd.c`, `cvar.c`, and `base_cmd.c` ownership,
  lifecycle, and legacy quirks.
  Evidence: `Documentation/codex/legacy/engine/command-cvar-baseline.md`.
- [x] `ENG-CMD-002` Inventory existing command/cvar tests under
  `XASH_ENGINE_TESTS` and decide which behavior should become standalone unit
  tests.
  Evidence: `Documentation/codex/legacy/engine/command-cvar-baseline.md`.
- [x] `ENG-CMD-003` Add BaseCmd-level tests for duplicate typed entries and
  command/cvar/alias name collisions.
  Evidence: `tests/engine/base_command_registry.cpp`; command
  `.\waf.bat build --targets=test_engine_base_command_registry`.
  Note: higher-level `Cmd_AddCommandEx`, alias creation, and
  `Cvar_RegisterVariable` duplicate policy is covered under `ENG-CMD-003B`.
- [x] `ENG-CMD-003B` Add higher-level tests for duplicate command/cvar
  registration policy.
  Evidence: `engine/common/cmd.c`, `engine/common/cvar.c`; command
  `.\waf.bat build --targets=xash_tests` passed; command
  `.\waf.bat build --alltests` passed 46/46 tests on 2026-05-09.
- [x] `ENG-CMD-004` Add tests for privileged and filterable command/cvar
  behavior.
  Evidence: existing and expanded `Test_RunCmd` / `Test_RunCvar` coverage in
  `engine/common/cmd.c` and `engine/common/cvar.c`; command
  `.\waf.bat build --targets=xash_tests` passed; command
  `.\waf.bat build --alltests` passed 46/46 tests on 2026-05-09.
- [x] `ENG-CMD-005` Add tests for command buffer insertion, execution order,
  `wait`, and overflow behavior.
  Evidence: `Test_RunCommandBufferPolicy` in `engine/common/cmd.c`; command
  `.\waf.bat build --targets=xash_tests` passed; command
  `.\waf.bat build --alltests` passed 46/46 tests on 2026-05-09.
- [x] `ENG-CMD-006` Decide whether the first implementation helper should be a
  C adapter around `src/engine/commands/` or a private C++ helper used directly
  by `cmd.c` and `cvar.c`.
  Decision: start with a private C++ `BaseCommandRegistry` helper and standalone
  tests; add the C adapter only after the helper semantics are pinned.
  Evidence: `Documentation/codex/modern/engine/basecmd-migration-guide.md`,
  `src/include/engine/commands/base_command_registry.hpp`.

## Phase 37: Command/Cvar Migration Pilot

- [x] `ENG-CMD-007` Create the first command/cvar implementation adapter only
  after Phase 36 has enough behavior coverage to compare against.
  Evidence: `engine/common/base_cmd_adapter.h`,
  `engine/common/base_cmd_adapter.cpp`, `engine/common/base_cmd.c`; command
  `.\waf.bat build --targets=xash_tests` passed; command
  `.\waf.bat build --alltests` passed 46/46 tests on 2026-05-09.
- [x] `ENG-CMD-008` Introduce a private helper under `src/engine/commands/`
  without changing the public `Cmd_*`, `Cbuf_*`, `Cvar_*`, or `BaseCmd_*`
  surface.
  Evidence: `src/include/engine/commands/base_command_registry.hpp`,
  `src/engine/commands/base_command_registry.cpp`,
  `tests/engine/base_command_registry.cpp`; command
  `.\waf.bat build --targets=test_engine_base_command_registry`.
- [x] `ENG-CMD-009` Build and run the focused command/cvar tests plus full
  `.\waf.bat build --alltests` after the first helper extraction.
  Evidence: command
  `.\waf.bat build --targets=test_engine_base_command_registry` passed; command
  `.\waf.bat build --alltests` passed 46/46 tests on 2026-05-09.
- [x] `ENG-CMD-009A` Add a parallel/shadow verification step before replacing
  the legacy BaseCmd table.
  Evidence: `tests/engine/base_command_registry.cpp`; command
  `.\waf.bat build --targets=test_engine_base_command_registry` passed;
  command `.\waf.bat build --alltests` passed 46/46 tests on 2026-05-09.
  Note: this started as a pre-routing shadow comparison. `BaseCommandRegistry`
  is now authoritative through `engine/common/base_cmd_adapter.cpp`, while the
  focused shadow test remains as the lower-level behavior mirror.
- [x] `ENG-CMD-010` Run a Windows runtime smoke after command/cvar behavior is
  touched, because startup scripts and cvar registration are launch-sensitive.
  Evidence: copied `build\src\xash3d.exe`, `build\engine\xash.dll`, and
  `build\filesystem\filesystem_stdio.dll` to `run-win32`; ran
  `.\xash3d.exe -dev 2 -log +wait +wait +quit` with
  `XASH3D_BASEDIR=C:\git\xash3d-fwgs\run-win32` and
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`.
  Exit code was 0; `engine.log` recorded `Time to first frame: 0.568 seconds`,
  `COM_FreeLibrary: Unloading filesystem_stdio.dll`, and
  `Stopped with reason "command"` on 2026-05-09.

## Phase 38: Info String Rewrite

- [x] `ENG-INFO-001` Audit `infostring.c` format rules, invalid characters,
  max-size behavior, important-key behavior, star-key handling, and largest-key
  removal policy.
  Evidence: `Documentation/codex/legacy/engine/info-string-baseline.md` and
  `Documentation/codex/done/todo/engine_infostring_todo.md`.
- [x] `ENG-INFO-002` Add standalone and parallel tests for `Info_*` behavior.
  Evidence: `tests/engine/info_string.cpp` covers the modern core; `Test_InfoStrings`
  in `engine/common/common.c` covers the routed legacy `Info_*` API.
- [x] `ENG-INFO-003` Add a modern info-string implementation and route the
  existing C API through it only after tests prove compatibility.
  Evidence: `src/include/engine/info_string.hpp`, `src/engine/info_string.cpp`,
  `engine/common/infostring.cpp`, and `Documentation/codex/modern/engine/info-string-migration-guide.md`.
- [x] `ENG-INFO-004` Run focused tests, `.\waf.bat build --alltests`, and a
  Windows runtime smoke after routing.
  Evidence: `.\waf.bat build --targets=test_engine_info_string` passed;
  `.\waf.bat build --targets=xash_tests` passed; `.\waf.bat build --alltests`
  passed 47/47; Windows smoke copied the rebuilt launcher/engine/filesystem DLLs
  to `run-win32`, launched with `XASH3D_BASEDIR=C:\git\xash3d-fwgs\run-win32`
  and `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached `Time to first frame: 0.565 seconds`, and stopped with reason
  `command`.

## Phase 39: Hash And Checksum Helpers

- [x] `ENG-HASH-001` Audit hash/checksum helpers such as `COM_HashKey`, their
  callers, and known-output compatibility requirements.
  Evidence: `Documentation/codex/legacy/engine/hash-checksum-baseline.md` and
  `Documentation/codex/done/todo/engine_hash_todo.md`.
- [x] `ENG-HASH-002` Add tests for hash/checksum vectors, case folding,
  hash-size assumptions, and edge inputs.
  Evidence: `public/tests/test_crclib.c` covers public `crclib` hash, CRC32,
  block-sequence, and MD5 vectors; `tests/utilities/hash.cpp` covers modern
  hash/checksum helpers.
- [x] `ENG-HASH-003` Replace one narrow implementation group behind existing C
  symbols after focused tests and full tests pass.
  Evidence: `COM_HashKey` now routes through
  `src/utilities/compat/crclib_hash.cpp` to `src/utilities/hash.cpp`;
  `BaseCommandRegistry` uses the same utility. Focused tests passed:
  `.\waf.bat build --targets=test_crclib`,
  `.\waf.bat build --targets=test_utilities_hash`, and
  `.\waf.bat build --targets=test_engine_base_command_registry`;
  `.\waf.bat build --alltests` passed 49/49; Windows smoke reached
  `Time to first frame: 0.589 seconds` and stopped with reason `command`.

## Phase 40: String And Path Utilities

- [x] `ENG-STRPATH-001` Audit shared string/path helpers and rank them by
  caller breadth, compatibility risk, and testability.
  Evidence: `Documentation/codex/legacy/engine/string-path-baseline.md` and
  `Documentation/codex/done/todo/engine_string_path_todo.md`.
- [x] `ENG-STRPATH-002` Add focused tests for path extension, basename,
  slash-normalization, case comparison, and bounded copy/concat behavior.
  Evidence: new `public/tests/test_path.c` and `tests/utilities/path.cpp`; older
  public tests `test_filebase`, `test_fileext`, `test_efp`, and `test_strings`
  still cover existing behavior.
- [x] `ENG-STRPATH-003` Migrate one narrow helper group after deciding whether
  it belongs in `src/engine`, `src/utilities`, or public C utility space.
  Evidence: path helpers belong in `src/utilities`; public C exports route
  through `src/utilities/compat/crtlib_path.cpp` to `src/utilities/path.cpp`.
  Focused tests passed: `.\waf.bat build --targets=test_path`,
  `.\waf.bat build --targets=test_utilities_path`, and
  `.\waf.bat build --targets=test_filebase,test_fileext,test_efp,test_strings`.
  `.\waf.bat build --alltests` passed 51/51. Runtime smoke reached
  `Time to first frame: 0.576 seconds` and stopped with reason `command`.

## Phase 41: Command Buffer Primitive

- [x] `ENG-CBUF-001` Audit command buffer ownership, command splitting,
  quote/comment handling, insertion behavior, overflow behavior, filtered
  buffer behavior, and `wait` semantics.
  Evidence: `Documentation/codex/legacy/engine/command-buffer-baseline.md` and
  `Documentation/codex/done/todo/engine_command_buffer_todo.md`.
- [x] `ENG-CBUF-002` Expand command-buffer tests for semicolon/newline
  splitting, CRLF, comments, quotes, inserted alias text, and overflow paths.
  Evidence: `engine/common/cmd.c` `Test_RunCommandBufferPolicy` covers legacy
  runtime behavior; `tests/engine/command_buffer.cpp` covers the standalone
  modern primitive with matching splitter and overflow cases.
- [x] `ENG-CBUF-003` Add a modern command-buffer primitive and route `Cbuf_*`
  mechanics through it while keeping command dispatch policy stable.
  Evidence: `src/include/engine/commands/command_buffer.hpp`,
  `src/engine/commands/command_buffer.cpp`, and
  `engine/common/command_buffer_adapter.cpp`; dispatch policy remains in
  `engine/common/cmd.c`.
- [x] `ENG-CBUF-004` Run focused tests, `xash_tests`, full tests, and Windows
  runtime smoke after routing.
  Evidence: `.\waf.bat build --targets=test_engine_command_buffer`,
  `.\waf.bat build --targets=xash_tests`, and `.\waf.bat build --alltests`
  passed 52/52. `.\waf.bat install --destdir=C:\git\xash3d-fwgs\run-win32`
  refreshed the runtime; `.\xash3d.exe -dev 2 -log +wait +wait +quit` exited
  0, reached `Time to first frame: 0.534 seconds`, and stopped with reason
  `command`.

## Phase 42: Network Buffer Primitive

- [x] `ENG-NETBUF-001` Audit `net_buffer.c/.h` read/write helpers, overflow
  behavior, bit order, endian behavior, string handling, coordinate/angle
  encoding, and caller expectations.
  Evidence: `Documentation/codex/legacy/engine/network-buffer-baseline.md` and
  `Documentation/codex/done/todo/engine_netbuffer_todo.md`.
- [x] `ENG-NETBUF-002` Add golden-vector and round-trip tests comparing legacy
  and modern behavior on deterministic byte streams.
  Evidence: existing `xash_tests` vectors in `engine/common/net_buffer.c`,
  added modern vectors in `tests/engine/network_buffer.cpp`, and
  `Test_Buffer_ModernExciseShadow`.
- [x] `ENG-NETBUF-003` Add a modern private buffer primitive and route one
  narrow helper group only after compatibility tests pass.
  Evidence: `src/include/engine/network/network_buffer.hpp`,
  `src/engine/network/network_buffer.cpp`, and
  `engine/common/network_buffer_adapter.cpp`; only `MSG_ExciseBits` is routed
  in this phase.
- [x] `ENG-NETBUF-004` Run focused tests, full tests, and multiplayer/protocol
  smoke where practical.
  Evidence: `.\waf.bat build --targets=test_engine_network_buffer`,
  `.\waf.bat build --targets=xash_tests`, and `.\waf.bat build --alltests`
  passed 53/53. `.\waf.bat install --destdir=C:\git\xash3d-fwgs\run-win32`
  refreshed the runtime; `.\xash3d.exe -dev 2 -log +wait +wait +quit` exited
  0, reached `Time to first frame: 0.519 seconds`, and stopped with reason
  `command`.

## Phase 43: Console And Logging Ownership

- [x] `ENG-LOG-001` Audit definitions and callers for `Con_Printf`,
  `Con_DPrintf`, `Con_Reportf`, `Log_Printf`, `Sys_Print`, and
  `Sys_PrintLog`.
  Evidence: `Documentation/codex/todo/engine_logging_todo.md`,
  `Documentation/codex/legacy/engine/console-logging-baseline.md`.
- [x] `ENG-LOG-002` Document output ownership, filtering, color/control prefix
  behavior, log file lifecycle, shutdown footer behavior, and fatal-path
  constraints.
  Evidence: `Documentation/codex/legacy/engine/console-logging-baseline.md`,
  `Documentation/codex/modern/engine/console-logging-migration-guide.md`.
- [ ] `ENG-LOG-003` Add focused tests or a test seam for target-neutral message
  formatting/filtering behavior where practical.
  Evidence: pending implementation pass.
- [x] `ENG-LOG-004` Decide how modern debugging utilities and deferred
  filesystem logging cleanup should feed engine output.
  Evidence: `Documentation/codex/modern/engine/console-logging-migration-guide.md`.
- [ ] `ENG-LOG-005` Run focused tests, full tests, and Windows runtime smoke if
  any output path changes.
  Evidence: no output path changed during the audit pass; pending code changes.
- [x] `ENG-LOG-006` Document background console backend ownership, command
  input hierarchy, and per-platform capability expectations.
  Evidence: `Documentation/codex/modern/engine/platform-console-backends.md`,
  `Documentation/codex/legacy/engine/console-logging-baseline.md`.
- [x] `ENG-LOG-007` Add internal platform-console capability/config types,
  null backend, and focused unit tests.
  Evidence: `src/include/engine/console/platform_console_backend.hpp`,
  `src/engine/console/platform_console_backend.cpp`,
  `tests/engine/platform_console_backend.cpp`; commands
  `.\waf.bat build --targets=test_engine_platform_console_backend`,
  `.\waf.bat build --targets=test_engine_base_command_registry,test_engine_command_buffer,test_engine_info_string,test_engine_network_buffer,test_engine_platform_console_backend`,
  direct execution of `build\src\test_engine_platform_console_backend.exe`,
  and `.\waf.bat build` passed 24/24 executed tests.
- [x] `ENG-LOG-008` Add a POSIX/Linux-style background console backend wrapper
  with injectable output/input and tests that preserve current `Platform_Input`
  semantics: output is available after initialization, command reads require a
  dedicated host configuration, disabled input returns no command, and missing
  input/output capabilities are honored.
  Evidence: `src/include/engine/console/platform_console_backend.hpp`,
  `src/engine/console/platform_console_backend.cpp`,
  `tests/engine/platform_console_backend.cpp`; commands
  `.\waf.bat build --targets=test_engine_platform_console_backend`,
  direct execution of `build\src\test_engine_platform_console_backend.exe`,
  `.\waf.bat build --targets=test_engine_base_command_registry,test_engine_command_buffer,test_engine_info_string,test_engine_network_buffer,test_engine_platform_console_backend`,
  and `.\waf.bat build` passed 24/24 executed tests.
- [ ] `ENG-LOG-009` Wrap Win32 external console output/input/lifecycle behind
  a platform console backend while preserving the existing `Wcon_*` C surface.
  Evidence:
- [ ] `ENG-LOG-010` Decide whether Android/iOS/Switch/Vita should use explicit
  output-only backends or remain direct `Sys_PrintStdout()` platform branches
  until the router phase.
  Evidence:
- [ ] `ENG-LOG-011` Route the legacy POSIX/Linux `Platform_Input()` and stdout
  output path through the POSIX backend once a POSIX validation build is
  available.
  Evidence:

## Phase 44: System Platform Facade Audit

- [ ] `ENG-SYS-001` Audit `system.c`, `system.h`, and platform source
  responsibilities.
  Evidence: `Documentation/codex/todo/engine_platform_todo.md`.
- [ ] `ENG-SYS-002` Document which branches can move to `engine/platform/`
  without changing `Sys_*` callers.
  Evidence:
- [ ] `ENG-SYS-003` Identify target-neutral helpers that can gain focused tests
  before any code movement.
  Evidence:
- [ ] `ENG-SYS-004` Move one narrow platform-neutral or platform-selected helper
  only if the audit finds a low-risk candidate.
  Evidence:
- [ ] `ENG-SYS-005` Run focused tests, full tests, and Windows runtime smoke if
  any platform path changes.
  Evidence:

## Phase 990: Memory Pools And Allocation

- [ ] `ENG-MEM-001` Audit `zone.c`, allocation families, ownership rules, pool
  lifecycle, debug reporting, and shutdown behavior.
  Evidence: `Documentation/codex/todo/engine_memory_todo.md`.
- [ ] `ENG-MEM-002` Add tests for pool lifecycle, realloc, null/zero-size
  behavior, string duplication ownership, and shutdown cleanup.
  Evidence:
- [ ] `ENG-MEM-003` Extract or rewrite one tiny helper group before considering
  a broad allocator abstraction.
  Evidence:
- [ ] `ENG-MEM-004` Run focused tests, full tests, and extended runtime smoke
  after any allocator-path change.
  Evidence:

## Phase 1000: Commit And Review Hygiene

- [ ] `REVIEW-001` Push modernization commits to remote branch.
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
| 2026-05-09 | DEC-015 | Use a fixed-capacity ordered registry template for first shared registry work, keeping mount-order and filesystem policy outside the generic utility. | `src/include/utilities/registry.hpp`, `done/todo/utilities_todo.md` |
| 2026-05-09 | DEC-016 | Store the first live directory backend bridge pointer inside the private `dir_t` root object instead of changing `searchpath_t` layout. | `filesystem/dir.c`, `done/todo/directory_backend_todo.md` |
| 2026-05-09 | DEC-017 | Keep legacy archive factory function pointers in `filesystem.c` during first registry integration, and expose modern archive descriptor metadata through a small C adapter. | `filesystem/archive_registry_adapter.h`, `filesystem/filesystem.c`, `done/todo/archive_registry_todo.md` |
| 2026-05-09 | DEC-018 | Use the same behavior-neutral bridge pattern for the first PAK backend pilot: private C++ backend object, C adapter, legacy callback implementation retained behind hooks. | `src/include/filesystem/pak_backend.hpp`, `filesystem/pak_backend_adapter.h`, `filesystem/pak.c` |
| 2026-05-09 | DEC-019 | Use the PAK bridge pattern for WAD while explicitly forwarding `pfnLoadFile`, because WAD lump loading is the primary read path. | `src/include/filesystem/wad_backend.hpp`, `filesystem/wad_backend_adapter.h`, `filesystem/wad.c` |
| 2026-05-09 | DEC-020 | Use the same bridge pattern for ZIP/PK3 while explicitly preserving stored/deflated load paths and unsupported compression failure behavior. | `src/include/filesystem/zip_backend.hpp`, `filesystem/zip_backend_adapter.h`, `filesystem/zip.c`, `tests/filesystem/zip-archive.c` |
| 2026-05-09 | DEC-021 | Keep Android runtime code behind `XASH_ANDROID`, but allow the target-neutral `AndroidAssetsBackend` class and adapter declarations to compile on desktop when they avoid Android headers and APIs. | `modularization-plan/android-assets-backend-plan.md`, `done/todo/android_assets_backend_todo.md` |
| 2026-05-09 | DEC-022 | Start `filesystem.c` migration with a target-neutral `FilesystemState` scaffold and tests before routing legacy globals through it. | `done/todo/filesystem_state_todo.md`, `src/include/filesystem/filesystem_state.hpp`, `tests/filesystem/filesystem_state.cpp` |
| 2026-05-09 | DEC-023 | Preserve current path rejection semantics exactly in `PathPolicy`, including direct-path bypass after empty-path rejection and the single leading `../` strip quirk. | `src/include/filesystem/path_policy.hpp`, `tests/filesystem/path_policy.cpp`, `filesystem/filesystem.c` |
| 2026-05-09 | DEC-024 | Treat `src/filesystem` as the long-term implementation home and shrink `filesystem/` toward stable facades plus temporary adapters. | `modern/filesystem/filesystem-folder-migration-map.md`, `done/todo/backend_implementation_migration_todo.md` |
| 2026-05-09 | DEC-025 | Keep `VFileSystem009.h` frozen, and move compatibility logic behind private runtime adapter calls instead of exposing modern C++ types through the public vtable. | `filesystem/VFileSystem009.cpp`, `src/include/filesystem/valve_path_resolver.hpp`, `tests/filesystem/interface.cpp` |
| 2026-05-09 | DEC-026 | Consolidate legacy compatibility toward a future `src/filesystem/legacy_adapter.cpp` boundary while keeping `filesystem/` as the temporary export/facade layer. | `done/todo/modern_filesystem_handlers_todo.md`, `modern/filesystem/export-dependency-audit.md` |
| 2026-05-09 | DEC-027 | Defer filesystem logging facade work until the engine console/logging ownership pass, because direct `Con_*` calls are engine-owned behavior rather than filesystem-owned policy. | `deferred/todo/filesystem_logging_todo.md` |
| 2026-05-09 | DEC-028 | Use `game_launch` as the next modularization pilot, initially keeping platform entry points in `game_launch/` while moving only small target-neutral helpers into `src/launcher/`. | `done/todo/game_launch_todo.md`, `modern/game-launch/architecture.md` |
| 2026-05-09 | DEC-029 | Move the launcher entry source into `src/launcher/platform/`, leaving `game_launch/wscript` as the executable target wrapper and `resources/launcher/` as the platform asset home. | `modern/game-launch/layout-policy.md`, `modern/game-launch/platform-support.md` |
| 2026-05-09 | DEC-030 | Keep platform-dependent launcher calls under `src/launcher/platform/` and allow a flat optional `launcher.json` to override safe startup defaults while compiled defaults remain the fallback. | `modern/game-launch/architecture.md`, `resources/launcher/launcher.example.json` |
| 2026-05-09 | DEC-031 | Prefer Waf-selected launcher platform implementation files over mixed-platform source files, and defer a JSON dependency until there is a second runtime reader or the launcher schema grows beyond flat defaults. | `modern/game-launch/platform-support.md`, `modern/game-launch/json-policy.md` |
| 2026-05-09 | DEC-032 | Let `src/wscript` own the `xash3d` launcher executable target and remove the empty `game_launch/` wrapper so launcher code and build ownership stay together. | `src/wscript`, `modern/game-launch/layout-policy.md` |
| 2026-05-09 | DEC-033 | Treat `engine/common` as multiple shared runtime services rather than one subsystem, and use command/cvar as the next engine-side pilot before host lifecycle or console/logging changes. | `legacy/engine/common-audit.md`, `done/todo/engine_common_todo.md` |
| 2026-05-09 | DEC-034 | Reserve `src/engine/` and `src/include/engine/` as modern engine internals with narrow ownership lanes, but do not build-wire a lane until its legacy behavior and tests are planned. | `src/engine/README.md`, `src/include/engine/README.md` |
| 2026-05-09 | DEC-035 | Start BaseCmd migration with a private C++ registry helper and standalone tests, then add a C adapter later once legacy routing is ready. | `modern/engine/basecmd-migration-guide.md`, `src/include/engine/commands/base_command_registry.hpp` |
| 2026-05-09 | DEC-036 | Use a parallel/shadow verification phase for BaseCmd before making the modern registry authoritative, so focused tests can compare legacy and modern behavior directly. | `modern/engine/basecmd-migration-guide.md`, `tasks.md` |
| 2026-05-09 | DEC-037 | Route legacy `BaseCmd_*` through a private C adapter backed by `BaseCommandRegistry`, keeping `base_cmd.h` and public command/cvar surfaces unchanged. | `engine/common/base_cmd_adapter.cpp`, `engine/common/base_cmd.c`, `modern/engine/basecmd-migration-guide.md` |
| 2026-05-09 | DEC-038 | Use six ordered low-level engine candidates for the next rewrite-style migrations: info strings, hash/checksum helpers, string/path utilities, command buffer, memory pools, and network buffers. | `done/todo/engine_infostring_todo.md`, `done/todo/engine_hash_todo.md`, `done/todo/engine_string_path_todo.md`, `done/todo/engine_command_buffer_todo.md`, `todo/engine_memory_todo.md`, `done/todo/engine_netbuffer_todo.md` |
| 2026-05-09 | DEC-039 | Implement info strings as a direct low-level C++ replacement under `src/engine` while exporting the unchanged legacy `Info_*` C surface from `engine/common/infostring.cpp`. | `legacy/engine/info-string-baseline.md`, `modern/engine/info-string-migration-guide.md`, `src/engine/info_string.cpp`, `engine/common/infostring.cpp` |
| 2026-05-09 | DEC-040 | Treat `public/crclib.h` as the legacy C ABI and `src/utilities` as the implementation home; route `COM_HashKey` through a compatibility export while leaving CRC32/MD5 C symbols in `public/crclib.c` until a dedicated performance pass. | `legacy/engine/hash-checksum-baseline.md`, `modern/engine/hash-checksum-migration-guide.md`, `src/utilities/compat/crclib_hash.cpp`, `src/utilities/hash.cpp`, `public/tests/test_crclib.c` |
| 2026-05-09 | DEC-041 | Treat `public/crtlib.h` path helpers as public C ABI and route the non-inline path helper implementations through `src/utilities/path.*`, while deferring parser, formatting, and inline copy/compare helpers to narrower later passes. | `legacy/engine/string-path-baseline.md`, `modern/engine/string-path-migration-guide.md`, `src/utilities/compat/crtlib_path.cpp`, `src/utilities/path.cpp`, `public/tests/test_path.c` |
| 2026-05-09 | DEC-042 | Defer broad memory-pool modernization to Phase 990 and promote network buffers to Phase 42, using golden byte vectors and narrow helper routing before any broad `MSG_*` rewrite. | `todo/engine_memory_todo.md`, `done/todo/engine_netbuffer_todo.md`, `legacy/engine/network-buffer-baseline.md`, `modern/engine/network-buffer-migration-guide.md` |
| 2026-05-09 | DEC-043 | Use console/logging ownership as the next engine phase because it unlocks deferred filesystem logging cleanup, then use the launcher/platform lessons for a `system.c` facade audit. | `todo/engine_logging_todo.md`, `todo/engine_platform_todo.md`, `legacy/engine/common-audit.md` |
