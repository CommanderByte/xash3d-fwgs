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

## Phase 43: System Console And Logging Ownership

- [x] `ENG-LOG-001` Audit definitions and callers for `Con_Printf`,
  `Con_DPrintf`, `Con_Reportf`, `Log_Printf`, `Sys_Print`, and
  `Sys_PrintLog`.
  Evidence: `Documentation/codex/done/todo/engine_logging_todo.md`,
  `Documentation/codex/legacy/engine/console-logging-baseline.md`.
- [x] `ENG-LOG-002` Document output ownership, filtering, color/control prefix
  behavior, log file lifecycle, shutdown footer behavior, and fatal-path
  constraints.
  Evidence: `Documentation/codex/legacy/engine/console-logging-baseline.md`,
  `Documentation/codex/modern/engine/console-logging-migration-guide.md`.
- [x] `ENG-LOG-003` Add focused tests or a test seam for target-neutral message
  formatting/filtering behavior where practical.
  Evidence: `src/include/engine/console/system_console_message.hpp`,
  `src/engine/console/system_console_message.cpp`,
  `tests/engine/system_console_message.cpp`; commands
  `.\waf.bat build --targets=test_engine_system_console_message` passed
  1/1 tests and `.\waf.bat build` passed 29/29 tests.
- [x] `ENG-LOG-004` Decide how modern debugging utilities and deferred
  filesystem logging cleanup should feed engine output.
  Evidence: `Documentation/codex/modern/engine/console-logging-migration-guide.md`.
- [x] `ENG-LOG-005` Run focused tests, full tests, and Windows runtime smoke if
  any output path changes.
  Evidence: focused/full validation evidence is recorded under `ENG-LOG-007`,
  `ENG-LOG-008`, `ENG-LOG-009`, and `ENG-LOG-013`; the live Windows runtime
  smoke for the first routed `Wcon_*` path is recorded under `ENG-LOG-013`.
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
- [x] `ENG-LOG-009` Add a Win32 external-console backend wrapper with
  injectable output/input/lifecycle operations and tests for current `Wcon_*`
  behavior boundaries: print/show/status/read are normal capabilities,
  input-disable and command registration are dedicated-only, missing
  capabilities are honored, and shutdown is idempotent.
  Evidence: `src/include/engine/console/platform_console_backend.hpp`,
  `src/engine/console/platform_console_backend.cpp`,
  `tests/engine/platform_console_backend.cpp`; commands
  `.\waf.bat build --targets=test_engine_platform_console_backend`,
  direct execution of `build\src\test_engine_platform_console_backend.exe`,
  `.\waf.bat build --targets=test_engine_base_command_registry,test_engine_command_buffer,test_engine_info_string,test_engine_network_buffer,test_engine_platform_console_backend`,
  and `.\waf.bat build` passed 24/24 executed tests.
- [x] `ENG-LOG-010` Move Android/iOS/Switch/Vita output-only backend decisions
  out of the Windows-focused Phase 43 pass.
  Evidence: `Documentation/codex/todo/non_windows_console_backend_todo.md`,
  Phase 801 below.
- [x] `ENG-LOG-011` Move live POSIX/Linux console routing and validation out of
  Phase 43.
  Evidence: `Documentation/codex/todo/posix_console_backend_todo.md`, Phase 800
  below.
- [x] `ENG-LOG-012` Document the rendered in-game console sink boundary and
  defer code extraction until a later router/client-rendering phase.
  Evidence: `Documentation/codex/modern/engine/rendered-console-sink.md`.
- [x] `ENG-LOG-013` Route the existing Win32 `Wcon_*` C functions through the
  Win32 backend wrapper and run a Windows runtime smoke test.
  Evidence: `engine/platform/win32/con_win.c`,
  `src/include/engine/console/platform_console_backend_adapter.h`,
  `src/engine/console/platform_console_backend_adapter.cpp`; commands
  `.\waf.bat build --targets=test_engine_platform_console_backend` passed
  1/1 tests, `.\waf.bat build` passed 28/28 tests, and Windows runtime smoke
  copied `build\engine\xash.dll` plus `build\filesystem\filesystem_stdio.dll`
  into `run-win32`, then ran
  `run-win32\xash3d.exe -dev 2 -log +fs_path +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`.
  The smoke log `run-win32\engine.log` reached renderer initialization and
  stopped with reason `command` at May 10 2026 00:03:23 local time. The quick
  `+quit` smoke did not emit a first-frame timing marker.

## Phase 44: System Platform Facade Audit

- [x] `ENG-SYS-001` Audit `system.c`, `system.h`, and platform source
  responsibilities.
  Evidence: `Documentation/codex/done/todo/engine_platform_todo.md`,
  `Documentation/codex/legacy/engine/system-platform-facade-audit.md`.
- [x] `ENG-SYS-002` Document which branches can move to `engine/platform/`
  without changing `Sys_*` callers.
  Evidence: `Documentation/codex/legacy/engine/system-platform-facade-audit.md`,
  `Documentation/codex/modern/engine/system-platform-facade-plan.md`.
- [x] `ENG-SYS-003` Identify target-neutral helpers that can gain focused tests
  before any code movement.
  Evidence: change-game command-line censor helper selected in
  `Documentation/codex/legacy/engine/system-platform-facade-audit.md`.
- [x] `ENG-SYS-004` Move one narrow platform-neutral or platform-selected helper
  only if the audit finds a low-risk candidate.
  Evidence: `src/include/engine/platform/command_line.hpp`,
  `src/include/engine/platform/command_line_adapter.h`,
  `src/engine/platform/command_line.cpp`, `engine/common/system.c`,
  `tests/engine/platform_command_line.cpp`.
- [x] `ENG-SYS-005` Run focused tests, full tests, and Windows runtime smoke if
  any platform path changes.
  Evidence: `.\waf.bat build --targets=test_engine_platform_command_line`
  passed 1/1 tests, `.\waf.bat build` passed 30/30 tests, and Windows runtime
  smoke copied `build\engine\xash.dll` plus
  `build\filesystem\filesystem_stdio.dll` into `run-win32`, then ran
  `run-win32\xash3d.exe -dev 2 -log +fs_path +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`.
  The smoke log `run-win32\engine.log` reached renderer initialization and
  stopped with reason `command` at May 10 2026 00:14:26 local time. The quick
  `+quit` smoke did not emit a first-frame timing marker.

## Phase 45: Command-Line Facade Consolidation

- [x] `ENG-CMDLINE-001` Audit `Sys_CheckParm`, `_Sys_GetParmFromCmdLine`, and
  `Sys_GetIntFromCmdLine` for target-neutral behavior.
  Evidence: `Documentation/codex/done/todo/engine_command_line_todo.md`.
- [x] `ENG-CMDLINE-002` Add modern command-line view, argument lookup, and value
  lookup helpers.
  Evidence: `src/include/engine/platform/command_line.hpp`,
  `src/engine/platform/command_line.cpp`.
- [x] `ENG-CMDLINE-003` Add C adapter functions for legacy C callers.
  Evidence: `src/include/engine/platform/command_line_adapter.h`,
  `src/engine/platform/command_line.cpp`.
- [x] `ENG-CMDLINE-004` Route legacy command-line facades through the adapter
  while preserving `Q_strncpy` and `Q_atoi` behavior in C.
  Evidence: `engine/common/system.c`,
  `Documentation/codex/modern/engine/command-line-facade-plan.md`.
- [x] `ENG-CMDLINE-005` Add focused tests for lookup order, case-insensitive
  matching, null entry handling, missing values, and C adapter parity.
  Evidence: `tests/engine/platform_command_line.cpp`.
- [x] `ENG-CMDLINE-006` Run focused tests, full tests, and Windows runtime
  smoke, recording first-frame timing when available.
  Evidence: `.\waf.bat build --targets=test_engine_platform_command_line`
  passed 1/1 tests, `.\waf.bat build --alltests` passed 56/56 tests, and
  Windows runtime
  smoke copied `build\engine\xash.dll` plus
  `build\filesystem\filesystem_stdio.dll` into `run-win32`, then ran
  `.\xash3d.exe -dev 2 -log +fs_path +quit` from `run-win32` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`.
  The fresh smoke log `run-win32\engine.log` reached renderer initialization
  and stopped with reason `command` at May 10 2026 00:39:34 local time. The
  quick `+quit` smoke did not emit a first-frame timing marker.

## Phase 46: Low-Risk Standalone Straggler Sweep

- [x] `ENG-STRAG-001` Audit current low-risk public/engine utility stragglers
  against existing modern helpers and tests.
  Evidence: `Documentation/codex/done/todo/low_risk_stragglers_todo.md`,
  `Documentation/codex/modern/engine/standalone-stragglers-roadmap.md`.
- [x] `ENG-STRAG-002` Start with CRC32 table/constants by removing duplication
  or routing public CRC lookup through the existing modern checksum helper.
  Evidence: `src/include/utilities/checksum.hpp`,
  `src/utilities/checksum.cpp`,
  `src/include/utilities/compat/checksum_adapter.h`,
  `src/utilities/compat/checksum_adapter.cpp`, `public/crclib.c`,
  `public/wscript`.
- [x] `ENG-STRAG-003` Preserve public `CRC32_Init`, `CRC32_Final`,
  `CRC32_ProcessByte`, `CRC32_ProcessBuffer`, and `CRC32_BlockSequence`
  behavior.
  Evidence: public ABI declarations remain in `public/crclib.h`; public CRC
  implementations remain in `public/crclib.c` and now use
  `Xash_Crc32Table()`.
- [x] `ENG-STRAG-004` Add or confirm tests for CRC known vectors,
  byte-vs-buffer parity, empty buffer finalization, block sequence negative
  sequence handling, payload clamp, and sequence wraparound.
  Evidence: `public/tests/test_crclib.c`, `tests/utilities/hash.cpp`.
- [x] `ENG-STRAG-005` Run focused CRC tests, modern checksum/hash tests,
  `.\waf.bat build --alltests`, and a smoke test if public linkage changes
  engine/runtime binaries.
  Evidence: `.\waf.bat build --targets=test_crclib,test_utilities_hash --alltests`
  passed 2/2 focused tests, `.\waf.bat build --alltests` passed 56/56 tests,
  and Windows runtime smoke copied `build\engine\xash.dll`,
  `build\filesystem\filesystem_stdio.dll`, and `build\ref\gl\ref_gl.dll` into
  `run-win32`, then ran `.\xash3d.exe -dev 2 -log +fs_path +quit` from
  `run-win32` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`.
  The smoke log `run-win32\engine.log` reached renderer initialization and
  stopped with reason `command` at May 10 2026 12:32:29 local time. The quick
  `+quit` smoke did not emit a first-frame timing marker.

## Phase 47: Public CRT Micro-Seams

- [x] `ENG-CRT-001` Audit `public/crtlib.c` and `public/crtlib.h` for narrow
  helper families with existing tests.
  Evidence: `Documentation/codex/done/todo/engine_crt_todo.md`.
- [x] `ENG-CRT-002` Pick one parser or conversion helper family only after its
  golden behavior is explicit.
  Evidence: selected numeric conversion family: `Q_atoi_hex`, `Q_atoi`,
  `Q_atof`, and `Q_atov`; see
  `Documentation/codex/modern/engine/public-crt-conversion-guide.md`.
- [x] `ENG-CRT-003` Keep public inline/header ABI stable while moving any
  implementation behind modern utilities or compat bridges.
  Evidence: `src/include/utilities/conversion.hpp`,
  `src/utilities/conversion.cpp`,
  `src/utilities/compat/crtlib_conversion.cpp`, `public/crtlib.c`,
  `public/wscript`.
- [x] `ENG-CRT-004` Run focused public tests plus `.\waf.bat build --alltests`.
  Evidence: `.\waf.bat build --targets=test_atoi,test_utilities_conversion --alltests`
  passed 2/2 focused tests, `.\waf.bat build --alltests` passed 57/57 tests,
  and Windows runtime smoke copied `build\engine\xash.dll`,
  `build\filesystem\filesystem_stdio.dll`, and `build\ref\gl\ref_gl.dll` into
  `run-win32`, then ran `.\xash3d.exe -dev 2 -log +fs_path +quit` from
  `run-win32` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`.
  The smoke log `run-win32\engine.log` reached renderer initialization and
  stopped with reason `command` at May 10 2026 12:43:58 local time. The quick
  `+quit` smoke did not emit a first-frame timing marker.

## Phase 48: Public Folder Relocation Sweep

- [x] `PUB-SWEEP-001` Audit `public/` for small project-owned helper
  implementations that can move behind `src/utilities` without changing public
  headers or ABI.
  Evidence: `Documentation/codex/done/todo/public_folder_sweep_todo.md`,
  `Documentation/codex/modern/public/public-folder-sweep-roadmap.md`.
- [x] `PUB-SWEEP-002` Move the remaining `crclib` MD5 family behind
  `src/utilities/md5.*` and a public C compatibility export.
  Evidence: `src/include/utilities/md5.hpp`, `src/utilities/md5.cpp`,
  `src/utilities/compat/crclib_md5.cpp`, `public/crclib.c`,
  `tests/utilities/md5.cpp`; `.\waf.bat build --targets=test_crclib,test_utilities_md5 --alltests`
  passed 2/2 focused tests on 2026-05-10.
- [x] `PUB-SWEEP-003` Move low-risk `crtlib` text helpers such as
  `Q_strnlwr` and `Q_memfgets` after modern coverage records legacy edge
  cases.
  Evidence: `src/include/utilities/text.hpp`, `src/utilities/text.cpp`,
  `src/utilities/compat/crtlib_text.cpp`, `public/crtlib.c`,
  `public/tests/test_strings.c`, `tests/utilities/text.cpp`;
  `.\waf.bat build --targets=test_strings,test_utilities_text --alltests`
  passed 2/2 focused tests on 2026-05-10.
- [x] `PUB-SWEEP-004` Move the atlas block allocator implementation behind
  `src/utilities/atlas.*` while preserving `atlas_t` and `Atlas_*`.
  Evidence: `src/include/utilities/atlas.hpp`, `src/utilities/atlas.cpp`,
  `src/utilities/compat/atlas_adapter.cpp`, `public/atlas.h`,
  `tests/utilities/atlas.cpp`; `public/atlas.c` was removed.
- [x] `PUB-SWEEP-005` Split pure build-number calculation from generated VCS
  data without changing `Q_buildnum_iso` or `Q_buildnum`.
  Evidence: `src/include/utilities/build_number.hpp`,
  `src/utilities/build_number.cpp`,
  `src/utilities/compat/build_number_adapter.cpp`, `public/build.c`,
  `tests/utilities/build_number.cpp`.
- [x] `PUB-SWEEP-006` Add focused UTF helper tests before any `utflib`
  migration.
  Evidence: `public/tests/test_utflib.c`; `public/wscript` now builds
  `test_utflib`.
- [x] `PUB-SWEEP-007` Defer `getopt`, `miniz`, math, matrix, and swap helpers
  unless a dedicated later phase owns their risks.
  Evidence: `Documentation/codex/done/todo/public_folder_sweep_todo.md`,
  `Documentation/codex/modern/public/public-folder-sweep-roadmap.md`.
- [x] `PUB-SWEEP-008` Run focused public tests, modern utility tests,
  `.\waf.bat build --alltests`, and a smoke test when runtime-linked public
  helpers change.
  Evidence: focused MD5 and CRT text commands above passed;
  `.\waf.bat build --targets=test_atlas,test_utilities_atlas,test_build,test_utilities_build_number,test_utflib --alltests`
  passed 5/5 focused tests; `.\waf.bat build --alltests` passed 62/62 tests;
  Windows runtime smoke copied `build\engine\xash.dll`,
  `build\filesystem\filesystem_stdio.dll`, and `build\ref\gl\ref_gl.dll` into
  `run-win32`, then ran `.\xash3d.exe -dev 2 -log +fs_path +quit` from
  `run-win32` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`.
  The smoke log `run-win32\engine.log` reached renderer initialization and
  stopped with reason `command` at May 10 2026 13:11:25 local time. The quick
  `+quit` smoke did not emit a first-frame timing marker.

## Phase 49: System User And Runtime Facades

- [x] `ENG-SYSUSER-001` Audit `Sys_GetCurrentUser`, `Sys_GetNativeObject`, and
  small runtime/platform helpers left in `engine/common/system.c`.
  Evidence: `Documentation/codex/legacy/engine/system-user-runtime-audit.md`,
  `Documentation/codex/modern/engine/system-user-runtime-facade-plan.md`.
- [x] `ENG-SYSUSER-002` Extract only a platform-selected helper that can be
  verified on Windows without blocking POSIX validation.
  Evidence: `src/include/engine/platform/current_user.hpp`,
  `src/include/engine/platform/current_user_adapter.h`,
  `src/engine/platform/current_user.cpp`,
  `src/engine/platform/current_user_adapter.cpp`,
  `engine/common/system.c`, `tests/engine/platform_current_user.cpp`;
  `.\waf.bat build --targets=test_engine_platform_current_user` passed 1/1
  focused tests on 2026-05-10; `.\waf.bat build --alltests` passed 63/63;
  the Windows `run-win32\xash3d.exe -dev 2 -log +fs_path +quit` smoke reached
  renderer initialization and stopped with reason `command` at May 10 2026
  13:19:22 local time; the follow-up
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` smoke logged
  time to first frame as 0.501 seconds and stopped with reason `command` at
  May 10 2026 13:20:56 local time.
- [x] `ENG-SYSUSER-003` Defer POSIX/Vita/Switch validation details to the 800
  series when they cannot be tested locally.
  Evidence: `Documentation/codex/todo/non_windows_system_runtime_todo.md`,
  Phase 802.

## Phase 50: Filesystem Bridge And Logging Follow-Up

- [x] `ENG-FSBRIDGE-001` Revisit `engine/common/filesystem_engine.c` after the
  system console/backend phase to see whether any logging or mount bridge code
  can move into modern helpers.
  Evidence: `Documentation/codex/legacy/engine/filesystem-bridge-audit.md`,
  `Documentation/codex/modern/engine/filesystem-bridge-migration-guide.md`,
  `src/include/engine/filesystem/mount_flags.hpp`,
  `src/include/engine/filesystem/mount_flags_adapter.h`,
  `src/engine/filesystem/mount_flags.cpp`,
  `src/engine/filesystem/mount_flags_adapter.cpp`,
  `engine/common/filesystem_engine.c`,
  `tests/engine/filesystem_mount_flags.cpp`;
  `.\waf.bat build --targets=test_engine_filesystem_mount_flags` passed 1/1
  focused tests; `.\waf.bat build --alltests` passed 64/64 on 2026-05-10.
- [x] `ENG-FSBRIDGE-002` Keep rendered in-game console routing out of scope
  until a client/rendering console sink phase exists.
  Evidence: `Documentation/codex/modern/engine/rendered-console-sink.md`,
  `Documentation/codex/modern/engine/filesystem-bridge-migration-guide.md`;
  Windows `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit`
  smoke reached renderer initialization, logged time to first frame as 1.755
  seconds, and stopped with reason `command` at May 10 2026 13:28:25 local
  time.

## Phase 51: Milestone Structure Audit And Roadmap Reset

- [x] `MILESTONE-050-001` Audit the modern and legacy source tree after the
  Phase 50 milestone.
  Evidence: `Documentation/codex/modern/milestone-50-structure-audit.md`.
- [x] `MILESTONE-050-002` Identify whether the current migration path is still
  coherent or has drifted into disconnected helper extraction.
  Decision: The existing patterns are healthy, but the next work should follow
  a coherent server-side engine lane rather than another miscellaneous helper
  sweep.
  Evidence: `Documentation/codex/modern/milestone-50-structure-audit.md`.
- [x] `MILESTONE-050-003` Refresh stale onboarding and modern engine notes that
  still describe the early filesystem and command pilots as future work.
  Evidence: `Documentation/codex/README.md`, `src/engine/README.md`.

## Phase 52: Server Boundary Audit

- [x] `ENG-SERVER-AUDIT-001` Map `engine/server/` ownership, public entry
  points, command/cvar dependencies, save/config outputs, and network payload
  boundaries.
  Evidence: `Documentation/codex/legacy/engine/server-boundary-audit.md`,
  `Documentation/codex/todo/server_migration_todo.md`.
- [x] `ENG-SERVER-AUDIT-002` Rank server files by migration risk and coupling,
  separating pure policy helpers from game DLL, physics, world, and savegame
  logic.
  Evidence: `Documentation/codex/legacy/engine/server-boundary-audit.md`.
- [x] `ENG-SERVER-AUDIT-003` Decide the first server pilot scope and acceptance
  criteria before moving implementation code.
  Decision: Use `sv_filter.c` as the first server pilot, extracting pure filter
  policy into `src/engine/server` while command parsing, file writes, client
  iteration, and `host.realtime` stay adapter-owned.
  Evidence: `Documentation/codex/modern/engine/server-migration-guide.md`,
  `Documentation/codex/todo/server_migration_todo.md`.
- [x] `ENG-SERVER-AUDIT-004` Record smoke-test expectations for dedicated and
  listen-server paths, including time-to-first-frame logging when a full game
  smoke is used.
  Evidence: `Documentation/codex/legacy/engine/server-boundary-audit.md`,
  `Documentation/codex/modern/engine/server-migration-guide.md`.

## Phase 53: Server Filter Pilot

- [x] `ENG-SVFILTER-001` Capture baseline behavior for IP filters, ID filters,
  command surfaces, file persistence, and existing embedded tests.
  Evidence: `Documentation/codex/legacy/engine/server-filter-baseline.md`.
- [x] `ENG-SVFILTER-002` Extract pure filter parsing and matching policy into
  `src/engine/server` with focused tests under `tests/engine`.
  Evidence: `src/include/engine/server/server_filter.hpp`,
  `src/engine/server/server_filter.cpp`,
  `tests/engine/server_filter.cpp`;
  `.\waf.bat build --targets=test_engine_server_filter` passed 1/1.
- [x] `ENG-SVFILTER-003` Route legacy `sv_filter.c` through a C-compatible
  adapter without changing command names, file formats, or ban-list behavior.
  Evidence: `engine/server/server_filter_adapter.h`,
  `engine/server/server_filter_adapter.cpp`, `engine/server/sv_filter.c`,
  `Documentation/codex/modern/engine/server-filter-migration.md`.
- [x] `ENG-SVFILTER-004` Run focused tests, `.\waf.bat build --alltests`, and
  a server/runtime smoke test.
  Evidence: `.\waf.bat build --targets=test_engine_server_filter` passed 1/1;
  `.\waf.bat build --alltests` passed 65/65; Windows runtime smoke copied the
  rebuilt engine/filesystem/ref DLLs into `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.512 seconds, and stopped with reason `command` at
  May 10 2026 14:00:54 local time.

## Phase 54: Server Query Response Builder

- [x] `ENG-SVQUERY-001` Capture baseline source-query response behavior and
  identify byte-stable payloads suitable for golden tests.
  Evidence: `Documentation/codex/legacy/engine/source-query-baseline.md`,
  `tests/engine/source_query.cpp`.
- [x] `ENG-SVQUERY-002` Extract query response payload construction into a
  modern helper with byte-stable protocol encoding.
  Evidence: `src/include/engine/server/source_query.hpp`,
  `src/engine/server/source_query.cpp`,
  `Documentation/codex/modern/engine/source-query-migration.md`.
- [x] `ENG-SVQUERY-003` Preserve legacy query entry points and packet routing
  while switching construction to the modern helper.
  Evidence: `engine/server/source_query_adapter.h`,
  `engine/server/source_query_adapter.cpp`, `engine/server/sv_query.c`.
- [x] `ENG-SVQUERY-004` Run focused golden-payload tests, full tests, and a
  server/runtime smoke test.
  Evidence: focused `.\waf.bat build --targets=test_engine_source_query`
  passed 1/1; `.\waf.bat build --alltests` passed 66/66; Windows runtime smoke
  copied the rebuilt engine DLL into `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.424 seconds, and stopped with reason `command` at
  May 10 2026 14:15:26 local time.

## Phase 55: Server User-Agent And Input Policy

- [x] `ENG-SVUA-001` Capture current `SV_ProcessUserAgent()` behavior:
  32-character lowercase hex UUID requirement, ban ID rejection, optional input
  device list requirement, and per-device rejection messages.
  Evidence: `Documentation/codex/legacy/engine/user-agent-policy-baseline.md`.
- [x] `ENG-SVUA-002` Implement target-neutral user-agent validation inputs and
  result codes under `src/engine/server`.
  Evidence: `src/include/engine/server/user_agent_policy.hpp`,
  `src/engine/server/user_agent_policy.cpp`,
  `Documentation/codex/modern/engine/user-agent-policy-migration.md`.
- [x] `ENG-SVUA-003` Add tests for valid/invalid UUIDs, banned IDs, missing
  input-device lists, and touch/mouse/joystick/VR disallow cases.
  Evidence: `tests/engine/user_agent_policy.cpp`.
- [x] `ENG-SVUA-004` Route `SV_ProcessUserAgent()` through the modern validator
  while keeping `SV_RejectConnection()`, cvar reads, and `SV_CheckID()` legacy
  adapter-owned.
  Evidence: `engine/server/user_agent_policy_adapter.h`,
  `engine/server/user_agent_policy_adapter.cpp`, `engine/server/sv_main.c`.
- [x] `ENG-SVUA-005` Run focused tests, `.\waf.bat build --alltests`, and a
  runtime smoke.
  Evidence: focused `.\waf.bat build --targets=test_engine_user_agent_policy`
  passed 1/1; `.\waf.bat build --alltests` passed 67/67; Windows runtime smoke
  copied the rebuilt engine DLL into `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.426 seconds, and stopped with reason `command` at
  May 10 2026 14:20:42 local time.

## Phase 56: Legacy NetAPI Query String Builder

- [x] `ENG-SVNETINFO-001` Baseline `SV_Info()` and `SV_BuildNetAnswer()` string
  response behavior for details, rules, players, ping, errors, and forbidden
  player lists.
  Evidence: `Documentation/codex/legacy/engine/netapi-info-baseline.md`.
- [x] `ENG-SVNETINFO-002` Decide whether to reuse Phase 54 source-query value
  rows or create a sibling NetAPI info-string builder.
  Evidence: `Documentation/codex/modern/engine/netapi-info-migration.md`.
- [x] `ENG-SVNETINFO-003` Add tests for protected cvar masking and the details
  response that currently comments it should match `SV_SourceQuery_Details`.
  Evidence: `tests/engine/netapi_info.cpp`.
- [x] `ENG-SVNETINFO-004` Route string construction through modern helpers
  while keeping `Netchan_OutOfBandPrint()` and live state reads legacy-owned.
  Evidence: `engine/server/netapi_info_adapter.h`,
  `engine/server/netapi_info_adapter.cpp`, `engine/server/sv_client.c`.
- [x] `ENG-SVNETINFO-005` Run focused tests, `.\waf.bat build --alltests`, and
  a runtime smoke.
  Evidence: focused `.\waf.bat build --targets=test_engine_netapi_info` passed
  1/1; `.\waf.bat build --alltests` passed 68/68; Windows runtime smoke copied
  the rebuilt engine DLL into `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.437 seconds, and stopped with reason `command` at
  May 10 2026 14:29:55 local time.

## Phase 57: Server Connectionless Command Classifier

- [x] `ENG-SVCONNLESS-001` Baseline `SV_ConnectionlessPacket()` command parsing
  and dispatch order, including source-query, short info, NetAPI info, rcon,
  challenge, connect, ping, ack, NAT, and unknown commands.
  Evidence: `Documentation/codex/legacy/engine/connectionless-packet-baseline.md`.
- [x] `ENG-SVCONNLESS-002` Implement a target-neutral classifier for the
  command string and first-token cases under `src/engine/server`.
  Evidence: `src/include/engine/server/connectionless_classifier.hpp`,
  `src/engine/server/connectionless_classifier.cpp`.
- [x] `ENG-SVCONNLESS-003` Add tests for exact `A2S_GOLDSRC_INFO`, single-byte
  source-query requests, command aliases, and unknown commands.
  Evidence: `tests/engine/connectionless_classifier.cpp`.
- [x] `ENG-SVCONNLESS-004` Route only classification through the modern helper,
  while message reads, logging, command handlers, and packet sends remain
  legacy-owned.
  Evidence: `engine/server/connectionless_classifier_adapter.h`,
  `engine/server/connectionless_classifier_adapter.cpp`,
  `engine/server/sv_client.c`.
- [x] `ENG-SVCONNLESS-005` Run focused tests, full tests, and a runtime smoke.
  Evidence: focused `.\waf.bat build --targets=test_engine_connectionless_classifier`
  passed 1/1; `.\waf.bat build --alltests` passed 69/69; Windows runtime
  smoke copied the rebuilt engine DLL into `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.436 seconds, and stopped with reason `command` at
  May 10 2026 14:37:01 local time.

## Phase 58: Server Event Log Formatter

- [x] `ENG-SVLOG-001` Baseline `sv_log.c` timestamp prefix, server cvar log
  lines, open/close messages, and command responses.
  Evidence: `Documentation/codex/legacy/engine/server-event-log-baseline.md`.
- [x] `ENG-SVLOG-002` Implement target-neutral event log formatting helpers,
  without moving file, console, or UDP sinks.
  Evidence: `src/include/engine/server/server_event_log.hpp`,
  `src/engine/server/server_event_log.cpp`.
- [x] `ENG-SVLOG-003` Add tests for timestamp-injected formatting and standard
  server cvar/start/close lines.
  Evidence: `tests/engine/server_event_log.cpp`.
- [x] `ENG-SVLOG-004` Route formatting through the modern helper while keeping
  `FS_*`, `Con_Printf`, `Netchan_OutOfBandPrint`, and command parsing
  legacy-owned.
  Evidence: `engine/server/server_event_log_adapter.h`,
  `engine/server/server_event_log_adapter.cpp`, `engine/server/sv_log.c`.
- [x] `ENG-SVLOG-005` Run focused tests, full tests, and a runtime smoke.
  Evidence: focused `.\waf.bat build --targets=test_engine_server_event_log`
  passed 1/1; `.\waf.bat build --alltests` passed 70/70; Windows runtime
  smoke copied the rebuilt engine DLL into `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.422 seconds, and stopped with reason `command` at
  May 10 2026 14:42:16 local time.

## Phase 59: Server Challenge And Rejection Response Formatter

- [x] `ENG-SVCHAL-001` Baseline `SV_SendChallenge()`,
  `SV_RejectConnection()`, and connection refusal text emitted during
  `SV_ConnectClient()` validation.
  Evidence: `Documentation/codex/legacy/engine/connection-response-baseline.md`.
- [x] `ENG-SVCHAL-002` Decide the safe boundary between challenge-number
  generation, which still depends on `netadr_t`, MD5, and server salt, and
  target-neutral response text formatting.
  Evidence: `Documentation/codex/modern/engine/connection-response-migration.md`.
- [x] `ENG-SVCHAL-003` Add tests for challenge response formatting and the
  three legacy rejection packet strings.
  Evidence: `tests/engine/connection_response.cpp`.
- [x] `ENG-SVCHAL-004` Route response formatting through modern helpers while
  keeping `SV_GetChallenge()`, `SV_CheckChallenge()`, `Con_Reportf()`, and
  `Netchan_OutOfBandPrint()` legacy-owned.
  Evidence: `engine/server/connection_response_adapter.h`,
  `engine/server/connection_response_adapter.cpp`, `engine/server/sv_client.c`.
- [x] `ENG-SVCHAL-005` Run focused tests, full tests, and a runtime smoke.
  Evidence: focused `.\waf.bat build --targets=test_engine_connection_response`
  passed 1/1; `.\waf.bat build --alltests` passed 71/71; Windows runtime
  smoke copied the rebuilt engine DLL into `run-win32`, ran
  `.\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` with
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.427 seconds, and stopped with reason `command` at
  May 10 2026 14:46:31 local time.

## Phase 60: Server Client Command Dispatch Table

- [x] `ENG-SVCMDTABLE-001` Baseline the `ucmd_t` table and
  `SV_ExecuteClientCommand()` lookup behavior, including command case
  sensitivity, unknown-command handling, and state-gated handlers.
  Evidence: `Documentation/codex/legacy/engine/client-command-dispatch-baseline.md`.
- [x] `ENG-SVCMDTABLE-002` Decide whether the first route-through should be a
  target-neutral command metadata table or only a lookup/classification helper.
  Evidence: `Documentation/codex/modern/engine/client-command-dispatch-migration.md`.
- [x] `ENG-SVCMDTABLE-003` Add tests for command lookup and dispatch decisions
  without invoking live `sv_client_t` effects.
  Evidence: `tests/engine/client_command_dispatch.cpp`.
- [x] `ENG-SVCMDTABLE-004` Route lookup/classification through modern helpers
  while keeping handler functions and client mutation legacy-owned.
  Evidence: `engine/server/client_command_dispatch_adapter.h`,
  `engine/server/client_command_dispatch_adapter.cpp`,
  `engine/server/sv_client.c`.
- [x] `ENG-SVCMDTABLE-005` Run focused tests, full tests, runtime smoke, and
  launch the visible game for manual new-game validation.
  Evidence: `.\waf.bat build --targets=test_engine_client_command_dispatch`
  passed, `.\waf.bat build --alltests` passed 72/72 tests, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.414 seconds before stopping with reason `command` at May 10 2026
  14:55 local time. A visible `run-win32\xash3d.exe -dev 2 -log` game process
  was launched for manual new-game validation.

## Phase 61: Custom Resource And Download Boundary Audit

- [x] `ENG-RES-001` Audit `engine/common/custom.c`, `engine/server/sv_custom.c`,
  and the `SV_DownloadFile_f()` path in `sv_client.c` to map resource identity,
  customization, HPAK, and download responsibilities.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`.
- [x] `ENG-RES-002` Identify which parts can become target-neutral resource
  value/policy helpers without moving file I/O, network fragments, or game DLL
  callbacks.
  Evidence: `Documentation/codex/modern/engine/custom-resource-download-boundary.md`.
- [x] `ENG-RES-003` Add baseline notes for custom resource hashes, temp-file
  behavior, download allow/fail rules, and precache checks.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`.
- [x] `ENG-RES-004` Define the first resource/download migration slice and test
  fixtures.
  Evidence: `Documentation/codex/modern/engine/custom-resource-download-boundary.md`.

## Phase 62: Custom Resource Identity Helpers

- [x] `ENG-RESID-001` Baseline resource descriptor comparison, hash/key
  formatting, and custom resource lookup helpers that do not require live file
  or network state.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`,
  `Documentation/codex/modern/engine/custom-resource-download-boundary.md`.
- [x] `ENG-RESID-002` Implement target-neutral resource identity helpers under
  `src/engine/server` or a shared resource namespace if the audit shows client
  reuse.
  Evidence: `src/include/engine/server/resource_identity.hpp`,
  `src/engine/server/resource_identity.cpp`.
- [x] `ENG-RESID-003` Add tests for resource names, hashes, type handling, and
  legacy edge cases.
  Evidence: `tests/engine/resource_identity.cpp`.
- [x] `ENG-RESID-004` Route the smallest safe legacy caller through the helper.
  Evidence: `engine/common/custom_resource_identity_adapter.h`,
  `engine/common/custom_resource_identity_adapter.cpp`,
  `engine/common/custom.c`.
- [x] `ENG-RESID-005` Run focused tests, full tests, and runtime smoke after
  routing the legacy caller.
  Evidence: `.\waf.bat build --targets=test_engine_resource_identity` passed
  1/1, `.\waf.bat build --alltests` passed 73/73, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.427 seconds before stopping with reason `command` at May 10 2026
  15:58 local time.

## Phase 63: Server Download Policy Helper

- [x] `ENG-DL-001` Baseline `SV_DownloadFile_f()` decision ordering, including
  safe-file rejection, `sv_allow_download`, precached-resource enforcement,
  model texture side downloads, custom logo/HPAK handling, and fail responses.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`,
  `Documentation/codex/modern/engine/custom-resource-download-boundary.md`.
- [x] `ENG-DL-002` Implement a target-neutral download decision helper that
  describes allow/fail/sidecar outcomes without opening files or creating
  netchan fragments.
  Evidence: `src/include/engine/server/server_download_policy.hpp`,
  `src/engine/server/server_download_policy.cpp`.
- [x] `ENG-DL-003` Add tests for unsafe paths, disabled downloads, missing
  precache entries, model texture sidecars, and custom logo names.
  Evidence: `tests/engine/server_download_policy.cpp`.
- [x] `ENG-DL-004` Route policy decisions through the helper while keeping
  `FS_*`, HPAK reads, and `Netchan_*` fragment creation legacy-owned.
  Evidence: `engine/server/server_download_policy_adapter.h`,
  `engine/server/server_download_policy_adapter.cpp`, `engine/server/sv_client.c`.
- [x] `ENG-DL-005` Run focused tests, full tests, runtime smoke, and record
  timing.
  Evidence: `.\waf.bat build --targets=test_engine_server_download_policy`
  passed 1/1, `.\waf.bat build --alltests` passed 74/74 tests, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.412 seconds before stopping with reason `command` at May 10 2026
  16:47 local time.

## Phase 64: Client Resource Upload Queue Helper

- [x] `ENG-UPLOADQ-001` Baseline `SV_ParseResourceList()`,
  `SV_EstimateNeededResources()`, `SV_BatchUploadRequest()`, and
  `SV_CheckFile()` ordering, including rate limiting, upload-size limits, HPAK
  presence checks, and `upload "!MD5..."` command generation.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`,
  `Documentation/codex/modern/engine/custom-resource-download-boundary.md`.
- [x] `ENG-UPLOADQ-002` Implement target-neutral upload queue decision helpers
  that operate on resource snapshots and adapter-supplied HPAK presence.
  Evidence: `src/include/engine/server/server_upload_queue.hpp`,
  `src/engine/server/server_upload_queue.cpp`.
- [x] `ENG-UPLOADQ-003` Add tests for invalid descriptors, too-frequent
  updates, missing custom decals, disabled uploads, and max-upload rejection.
  Evidence: `tests/engine/server_upload_queue.cpp`.
- [x] `ENG-UPLOADQ-004` Route the smallest safe upload queue decision through
  the helper while keeping `MSG_*`, allocation, HPAK, and client mutation
  legacy-owned.
  Evidence: `engine/server/server_upload_queue_adapter.h`,
  `engine/server/server_upload_queue_adapter.cpp`, `engine/server/sv_client.c`,
  `engine/server/sv_custom.c`.
- [x] `ENG-UPLOADQ-005` Run focused tests, full tests, runtime smoke, and
  record timing.
  Evidence: `.\waf.bat build --targets=test_engine_server_upload_queue`
  passed 1/1, `.\waf.bat build --alltests` passed 75/75 tests, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.410 seconds before stopping with reason `command` at May 10 2026
  17:06 local time.

## Phase 65: Resource Message Serialization Helper

- [x] `ENG-RESMSG-001` Baseline `SV_SendResource()` and `SV_SendResources()`
  wire output, including custom hash bytes, reserved data bit, and resource
  count limits.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`.
- [x] `ENG-RESMSG-002` Implement a target-neutral resource-row encoder that can
  write into modern network-buffer primitives or caller-provided byte sinks.
  Evidence: `src/include/engine/server/server_resource_message.hpp`,
  `src/engine/server/server_resource_message.cpp`.
- [x] `ENG-RESMSG-003` Add golden tests for resource rows with and without
  custom hashes and reserved consistency payloads.
  Evidence: `tests/engine/server_resource_message.cpp`.
- [x] `ENG-RESMSG-004` Route row encoding through the helper while keeping
  `MSG_BeginServerCmd()`, resource-location messages, and netchan fragments
  legacy-owned.
  Evidence: `engine/server/server_resource_message_adapter.h`,
  `engine/server/server_resource_message_adapter.cpp`, `engine/server/sv_custom.c`.
- [x] `ENG-RESMSG-005` Run focused tests, full tests, runtime smoke, and record
  timing.
  Evidence: `.\waf.bat build --targets=test_engine_server_resource_message`
  passed 1/1, `.\waf.bat build --alltests` passed 76/76 tests, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.404 seconds before stopping with reason `command` at May 10 2026
  17:18 local time.

## Phase 66: Customization Message Serialization Helper

- [x] `ENG-CUSTOMMSG-001` Baseline `SV_SendCustomization()` wire output,
  including player number, resource descriptor fields, full flags byte, and
  optional custom MD5 hash.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`.
- [x] `ENG-CUSTOMMSG-002` Implement a target-neutral customization payload
  encoder that writes through modern network-buffer primitives.
  Evidence: `src/include/engine/server/server_customization_message.hpp`,
  `src/engine/server/server_customization_message.cpp`.
- [x] `ENG-CUSTOMMSG-003` Add golden tests for custom and non-custom
  customization payloads, signed fields, and overflow.
  Evidence: `tests/engine/server_customization_message.cpp`.
- [x] `ENG-CUSTOMMSG-004` Route `SV_SendCustomization()` through the helper
  while keeping `svc_customization` and netchan message ownership in legacy
  code.
  Evidence: `engine/server/server_customization_message_adapter.h`,
  `engine/server/server_customization_message_adapter.cpp`,
  `engine/server/sv_custom.c`.
- [x] `ENG-CUSTOMMSG-005` Run focused tests, full tests, runtime smoke, and
  record timing.
  Evidence: `.\waf.bat build --targets=test_engine_server_customization_message`
  passed 1/1, `.\waf.bat build --targets=xash` passed,
  `.\waf.bat build --alltests` passed 77/77 tests, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.408 seconds before stopping with reason `command` at May 10 2026
  17:40 local time.

## Phase 67: Consistency List Serialization Helper

- [x] `ENG-CONSLIST-001` Baseline `SV_SendConsistencyList()` enable/disable
  conditions, delta encoding, and list terminator behavior.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`.
- [x] `ENG-CONSLIST-002` Implement a target-neutral consistency-list encoder
  for resource-index snapshots.
  Evidence: `src/include/engine/server/server_consistency_list.hpp`,
  `src/engine/server/server_consistency_list.cpp`.
- [x] `ENG-CONSLIST-003` Add golden tests for empty lists, HLTV/single-player
  suppression, small deltas, large deltas, and terminators.
  Evidence: `tests/engine/server_consistency_list.cpp`.
- [x] `ENG-CONSLIST-004` Route consistency-list serialization through the
  helper while keeping client flags and `resource_t` ownership in legacy code.
  Evidence: `engine/server/server_consistency_list_adapter.h`,
  `engine/server/server_consistency_list_adapter.cpp`,
  `engine/server/sv_custom.c`.
- [x] `ENG-CONSLIST-005` Run focused tests, full tests, runtime smoke, and
  record timing.
  Evidence: `.\waf.bat build --targets=test_engine_server_consistency_list`
  passed 1/1, `.\waf.bat build --targets=xash` passed,
  `.\waf.bat build --alltests` passed 78/78 tests, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.421 seconds before stopping with reason `command` at May 10 2026
  17:45 local time.

## Phase 68: Consistency Resource Policy

- [x] `ENG-CONSIST-001` Baseline `SV_TransferConsistencyInfo()` and
  `SV_ParseConsistencyResponse()` for exact-file, same-bounds,
  specified-bounds, invalid type, and bad-resource handling.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`.
- [x] `ENG-CONSIST-002` Implement target-neutral consistency request/response
  policy helpers that consume MD5 and bounds snapshots.
  Evidence: `src/include/engine/server/server_consistency_policy.hpp`,
  `src/engine/server/server_consistency_policy.cpp`.
- [x] `ENG-CONSIST-003` Add tests for MD5 prefix comparison, bounds validation,
  invalid force types, and response-count mismatch.
  Evidence: `tests/engine/server_consistency_policy.cpp`.
- [x] `ENG-CONSIST-004` Route consistency decisions through the helper while
  keeping file hashing, model bounds, drops, client messages, and game DLL
  callbacks legacy-owned.
  Evidence: `engine/server/server_consistency_policy_adapter.h`,
  `engine/server/server_consistency_policy_adapter.cpp`,
  `engine/server/sv_custom.c`.
- [x] `ENG-CONSIST-005` Run focused tests, full tests, runtime smoke, and
  record timing.
  Evidence: `.\waf.bat build --targets=test_engine_server_consistency_policy`
  passed 1/1, `.\waf.bat build --targets=xash` passed,
  `.\waf.bat build --alltests` passed 79/79 tests, and
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` reached first
  frame in 0.409 seconds before stopping with reason `command` at May 10 2026
  17:52 local time.

## Phase 69: Server Resource Catalog Builder

- [x] `ENG-RESCAT-001` Baseline `SV_AddResource()`,
  `SV_CreateResourceList()`, `SV_DetermineResourceType()`, and related
  resource ordering for generic, sound, model, decal, and event entries.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`.
- [x] `ENG-RESCAT-002` Implement a target-neutral resource catalog builder
  that consumes adapter-provided precache, file-size, index, and flag
  snapshots.
  Evidence: `src/include/engine/server/server_resource_catalog.hpp`,
  `src/engine/server/server_resource_catalog.cpp`.
- [x] `ENG-RESCAT-003` Add golden tests for ordering, empty entries, sound
  sentinel names, model wildcard size behavior, flags, indexes, and resource
  type assignment.
  Evidence: `tests/engine/server_resource_catalog.cpp`.
- [x] `ENG-RESCAT-004` Route resource entry planning through the helper while
  keeping filesystem probes, `sv.resources` storage, console output, and
  precache mutation legacy-owned.
  Evidence: `engine/server/server_resource_catalog_adapter.h`,
  `engine/server/server_resource_catalog_adapter.cpp`,
  `engine/server/sv_init.c`.
- [x] `ENG-RESCAT-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `.\waf.bat build --targets=test_engine_server_resource_catalog`
  passed 1/1, `.\waf.bat build --targets=xash` passed,
  `.\waf.bat build --alltests` passed 80/80 tests, and the fresh
  `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit` smoke copied
  `build\engine\xash.dll` into `run-win32`, ran with
  `XASH3D_BASEDIR=C:\git\xash3d-fwgs\run-win32` and
  `XASH3D_RODIR=C:\Program Files (x86)\Steam\steamapps\common\Half-Life`,
  reached first frame in 0.413 seconds, and stopped with reason `command` at
  May 10 2026 18:07 local time.

## Phase 70: Hot Resource Announcement

- [x] `ENG-HOTRES-001` Baseline `SV_SendSingleResource()` name, size, type,
  flag, and `svc_resource` behavior.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`
  Hot Resource Announcement section.
- [x] `ENG-HOTRES-002` Implement a target-neutral hot-resource announcement
  planner that consumes adapter-provided type, index, flags, and file size.
  Evidence: `src/include/engine/server/server_hot_resource.hpp` and
  `src/engine/server/server_hot_resource.cpp`.
- [x] `ENG-HOTRES-003` Add tests for model wildcard resources, sound path
  prefixing, generic resources, empty names, and signed size preservation.
  Evidence: `tests/engine/server_hot_resource.cpp`.
- [x] `ENG-HOTRES-004` Route `SV_SendSingleResource()` through the helper while
  keeping `FS_FileSize()`, reliable datagram ownership, and final
  `SV_SendResource()` delivery legacy-owned.
  Evidence: `engine/server/server_hot_resource_adapter.h`,
  `engine/server/server_hot_resource_adapter.cpp`, and `engine/server/sv_init.c`.
- [x] `ENG-HOTRES-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_server_hot_resource` and `-StopRunningXash`
  passed: focused test 1/1, `xash` build, alltests 81/81, runtime smoke first
  frame 0.550 seconds, stop reason `command`.

## Phase 71: Server Reslist File Policy

- [x] `ENG-RESLIST-001` Baseline `.res` and `reslist.txt` parsing decisions:
  safe-download filtering, slash normalization, sound classification, generic
  fallback, and console reporting.
  Evidence: `Documentation/codex/legacy/engine/custom-resource-download-baseline.md`
  Server Reslist File Policy section.
- [x] `ENG-RESLIST-002` Implement a target-neutral reslist token classifier
  that returns normalized path, resource type, and index route intent.
  Evidence: `src/include/engine/server/server_reslist_policy.hpp` and
  `src/engine/server/server_reslist_policy.cpp`.
- [x] `ENG-RESLIST-003` Add tests for empty tokens, unsafe paths, Windows slash
  input, supported sound formats, unsupported sound paths, and generic
  fallback.
  Evidence: `tests/engine/server_reslist_policy.cpp`.
- [x] `ENG-RESLIST-004` Route `SV_ReadResourceList()` decisions through the
  helper while keeping file loading, `COM_ParseFile()`, console output,
  `SV_SoundIndex()`, and `SV_GenericIndex()` legacy-owned.
  Evidence: `engine/server/server_reslist_policy_adapter.h`,
  `engine/server/server_reslist_policy_adapter.cpp`, and `engine/server/sv_init.c`.
- [x] `ENG-RESLIST-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_server_reslist_policy` and `-StopRunningXash`
  passed: focused test 1/1, `xash` build, alltests 82/82, runtime smoke first
  frame 0.492 seconds, stop reason `command`.

## Phase 72: Client Userinfo Update Message

- [x] `ENG-USERINFO-001` Baseline `SV_FullClientUpdate()` for name-present,
  name-missing, sanitized userinfo, user ID, client index, and hashed CD key
  payload behavior.
  Evidence: `Documentation/codex/legacy/engine/server-userinfo-message-baseline.md`.
- [x] `ENG-USERINFO-002` Implement a target-neutral update-userinfo payload
  encoder that consumes adapter-provided sanitized userinfo and MD5 digest
  snapshots.
  Evidence: `src/include/engine/server/server_userinfo_message.hpp`,
  `src/engine/server/server_userinfo_message.cpp`,
  `engine/server/server_userinfo_message_adapter.h`, and
  `engine/server/server_userinfo_message_adapter.cpp`.
- [x] `ENG-USERINFO-003` Add golden tests for named clients, unnamed clients,
  digest emission, bit layout, and overflow handling.
  Evidence: `tests/engine/server_userinfo_message.cpp`.
- [x] `ENG-USERINFO-004` Route `SV_FullClientUpdate()` serialization through
  the helper while keeping `SV_UserinfoChanged()`, prefix stripping, MD5
  calculation, and destination message ownership legacy-owned unless a smaller
  extraction is clearly safe.
  Evidence: `engine/server/sv_client.c` routes the payload through
  `SV_UserinfoMessage_WritePayload()` after `MSG_BeginServerCmd()` and after
  legacy userinfo sanitization/hash calculation.
- [x] `ENG-USERINFO-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_server_userinfo_message` and `-StopRunningXash`
  passed: focused test 1/1, `xash` build, alltests 83/83, runtime smoke first
  frame 0.517 seconds, stop reason `command`.

## Phase 73: Small Server Service Messages

- [x] `ENG-SVCMSG-001` Baseline compact service writers including
  `SV_FailDownload()`, `SV_BuildReconnect()`, `SV_UpdateClientView()`,
  `SV_TogglePause()`, and `SV_WriteVoiceCodec()`.
  Evidence: `Documentation/codex/legacy/engine/server-service-message-baseline.md`.
- [x] `ENG-SVCMSG-002` Implement target-neutral service message encoders for
  file-transfer failure, reconnect command, set-view, pause, and voice-codec
  payloads.
  Evidence: `src/include/engine/server/server_service_messages.hpp`,
  `src/engine/server/server_service_messages.cpp`,
  `engine/server/server_service_messages_adapter.h`, and
  `engine/server/server_service_messages_adapter.cpp`.
- [x] `ENG-SVCMSG-003` Add golden tests for command bytes, word/bit fields,
  string payloads, empty codec fallback, and overflow handling.
  Evidence: `tests/engine/server_service_messages.cpp`.
- [x] `ENG-SVCMSG-004` Route selected service writers through adapters while
  keeping cvars, server state checks, client selection, and message buffer
  ownership legacy-owned.
  Evidence: `engine/server/sv_client.c`, `engine/server/sv_init.c`, and
  `engine/server/sv_game.c` keep legacy command begins, state/cvar/entity
  decisions, and destination buffers while routing payload bits through
  `SV_ServiceMessage_*Payload()`.
- [x] `ENG-SVCMSG-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_server_service_messages` and `-StopRunningXash`
  passed: focused test 1/1, `xash` build, alltests 84/84, runtime smoke first
  frame 0.493 seconds, stop reason `command`.

## Phase 74: Server Voice Relay Policy

- [x] `ENG-VOICE-001` Baseline `SV_ParseVoiceData()` loopback, frame count,
  size limit, voice enable gates, spawned-client gate, physics callback,
  listener mask, and per-recipient datagram behavior.
  Evidence: `Documentation/codex/legacy/engine/server-voice-relay-baseline.md`.
- [x] `ENG-VOICE-002` Implement a target-neutral voice relay policy helper and,
  if cleanly separable, a `svc_voicedata` payload writer.
  Evidence: `src/include/engine/server/server_voice_relay.hpp`,
  `src/engine/server/server_voice_relay.cpp`,
  `engine/server/server_voice_relay_adapter.h`, and
  `engine/server/server_voice_relay_adapter.cpp`.
- [x] `ENG-VOICE-003` Add tests for oversized packets, disabled voice, sender
  loopback behavior, listener-mask filtering, single-player suppression, and
  datagram-capacity rejection.
  Evidence: `tests/engine/server_voice_relay.cpp`.
- [x] `ENG-VOICE-004` Route relay decisions through the helper while keeping
  message reads, game DLL physics callbacks, recipient iteration, and datagram
  writes legacy-owned.
  Evidence: `engine/server/sv_client.c` keeps incoming message reads,
  `SV_Physics()->pfnVoiceData()`, recipient iteration, and `MSG_BeginServerCmd()`
  legacy-owned while using the helper for gates, recipient decisions, and
  `svc_voicedata` payload bits.
- [x] `ENG-VOICE-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_server_voice_relay` and `-StopRunningXash`
  passed: focused test 1/1, `xash` build, alltests 85/85, runtime smoke first
  frame 0.493 seconds, stop reason `command`.

## Phase 75: Server Text Command Messages

- [x] `ENG-TEXTMSG-001` Baseline `SV_ClientPrintf()`, `SV_BroadcastPrintf()`,
  `SV_BroadcastCommand()`, `pfnClientCommand()`, and related `svc_print` /
  `svc_stufftext` writers for command bytes, text formatting, destination
  selection, and fake-client behavior.
  Evidence: `Documentation/codex/legacy/engine/server-text-message-baseline.md`.
- [x] `ENG-TEXTMSG-002` Implement target-neutral print/stufftext payload
  encoders and, where useful, tiny command-string builders.
  Evidence: `src/include/engine/server/server_text_messages.hpp`,
  `src/engine/server/server_text_messages.cpp`,
  `engine/server/server_text_messages_adapter.h`, and
  `engine/server/server_text_messages_adapter.cpp`.
- [x] `ENG-TEXTMSG-003` Add golden tests for print channels, empty strings,
  formatted reconnect/stufftext-style commands, overflow handling, and append
  after legacy command bytes.
  Evidence: `tests/engine/server_text_messages.cpp`.
- [x] `ENG-TEXTMSG-004` Route selected text-message writers while keeping
  formatting ownership, client iteration, and command dispatch legacy-owned.
  Evidence: `engine/server/sv_cmds.c` and `engine/server/sv_game.c` keep
  formatting, fake-client checks, spawned-client filters, ignored-client
  handling, and `SV_IsValidCmd()` legacy-owned while using text payload
  adapters after legacy command bytes are written.
- [x] `ENG-TEXTMSG-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_server_text_messages` and `-StopRunningXash`
  passed: focused test 1/1, `xash` build, alltests 86/86, runtime smoke first
  frame 0.668 seconds, stop reason `command`.

## Phase 76: Server Sound Message Builder

- [x] `ENG-SOUNDMSG-001` Baseline `SV_BuildSoundMsg()` and callers for spawn
  versus restore sound commands, optional volume/attenuation/pitch fields,
  entity/channel encoding, origin handling, and bounds checks.
  Evidence: `Documentation/codex/legacy/engine/server-sound-message-baseline.md`.
- [x] `ENG-SOUNDMSG-002` Implement target-neutral sound-message flag planning
  and payload serialization.
  Evidence: `src/include/engine/server/server_sound_message.hpp`,
  `src/engine/server/server_sound_message.cpp`,
  `engine/server/server_sound_message_adapter.h`, and
  `engine/server/server_sound_message_adapter.cpp`.
- [x] `ENG-SOUNDMSG-003` Add golden tests for minimal sounds, flagged optional
  fields, restore-sound payloads, large coordinates, invalid sample indexes,
  and overflow handling.
  Evidence: `tests/engine/server_sound_message.cpp`.
- [x] `ENG-SOUNDMSG-004` Route `SV_BuildSoundMsg()` through the helper while
  keeping entity lookup, model/sound indexes, game DLL callbacks, and multicast
  destination ownership legacy-owned.
  Evidence: `engine/server/sv_game.c` keeps sample parsing, `SV_SoundIndex()`,
  entity-index resolution, caller diagnostics, save/restore extra data, and
  multicast routing legacy-owned while using the helper for command planning and
  bit-packed payload serialization.
- [x] `ENG-SOUNDMSG-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_server_sound_message` and `-StopRunningXash`
  passed: focused test 1/1, `xash` build, alltests 87/87, runtime smoke first
  frame 0.478 seconds, stop reason `command`.

## Phase 77: Decal And Static Entity Messages

- [x] `ENG-STATICMSG-001` Baseline `SV_CreateDecal()`, `SV_CreateStaticEntity()`,
  `SV_RestartStaticEnts()`, `SV_RestartDecals()`, and permanent decal restore
  behavior.
  Evidence: `Documentation/codex/legacy/engine/server-static-decal-baseline.md`.
- [x] `ENG-STATICMSG-002` Implement target-neutral `svc_bspdecal` and
  `svc_spawnstatic` helpers for stable message layout.
  Evidence: `src/include/engine/server/server_static_messages.hpp`,
  `src/engine/server/server_static_messages.cpp`,
  `engine/server/server_static_messages_adapter.h`, and
  `engine/server/server_static_messages_adapter.cpp`. Note: `svc_spawnstatic`
  delta payload serialization remains legacy-owned because it depends on active
  delta tables and `MSG_WriteDeltaEntity()`.
- [x] `ENG-STATICMSG-003` Add golden tests for entity/model indexes, decal
  flags, scale, static entity admission gates, and overflow handling.
  Evidence: `tests/engine/server_static_messages.cpp` covers decal payload
  layout, model-index omission for world decals, scale encoding, coordinate
  modes, append-after-command behavior, static-entity admission gates, and
  overflow.
- [x] `ENG-STATICMSG-004` Route selected message payloads through helpers while
  keeping entity validation, resource indexes, signon buffer ownership, and map
  restart iteration legacy-owned.
  Evidence: `engine/server/sv_game.c` keeps signon/reliable buffer ownership,
  renderer/game DLL restore decisions, resource/index lookup, and
  `MSG_WriteDeltaEntity()` legacy-owned while routing `SV_CreateDecal()` payload
  serialization and `SV_CreateStaticEntity()` admission checks through the
  modern helper.
- [x] `ENG-STATICMSG-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_server_static_messages` and `-StopRunningXash`
  passed: focused test 1/1, `xash` build, alltests 88/88, runtime smoke first
  frame 0.488 seconds, stop reason `command`.

## Phase 78: Server Multicast Routing Policy

- [x] `ENG-MCAST-001` Baseline `SV_Multicast()` for destination modes, PVS/PHS
  masks, reliable/unreliable buffers, spectator proxy handling, and usermessage
  rewrite behavior.
  Evidence: `Documentation/codex/legacy/engine/server-multicast-baseline.md`.
- [x] `ENG-MCAST-002` Implement target-neutral recipient/destination policy
  helpers that do not own actual buffer writes.
  Evidence: `src/include/engine/server/server_multicast_policy.hpp` and
  `src/engine/server/server_multicast_policy.cpp`.
- [x] `ENG-MCAST-003` Add tests for broadcast, one-client, PVS/PHS filtered,
  reliable/unreliable, spectator, and invalid destination cases.
  Evidence: `tests/engine/server_multicast_policy.cpp`.
- [x] `ENG-MCAST-004` Route decision-making through the helper while keeping
  visibility mask generation, `sv.multicast`, and final writes legacy-owned.
  Evidence: `engine/server/server_multicast_policy_adapter.h`,
  `engine/server/server_multicast_policy_adapter.cpp`, and
  `engine/server/sv_game.c`.
- [x] `ENG-MCAST-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_server_multicast_policy` and `-StopRunningXash`
  passed: focused test 1/1, `xash` build, alltests 89/89, runtime smoke first
  frame 0.501 seconds, stop reason `command`.

## Phase 79: Game DLL User Message Bridge

- [ ] `ENG-USERMSG-001` Baseline `pfnMessageBegin()`, `pfnMessageEnd()`,
  `pfnWriteByte/Char/Short/Long/Angle/Coord/String/Entity()`, and
  `SV_RewriteMessage()` behavior.
  Evidence:
- [ ] `ENG-USERMSG-002` Implement a modern message-session facade that models
  destination, message id, origin, entity target, rewrite eligibility, and
  payload writes without exposing STL through the ABI.
  Evidence:
- [ ] `ENG-USERMSG-003` Add tests for write primitives, bounds, rewriteable
  messages, usermessage headers, and malformed begin/end sequences.
  Evidence:
- [ ] `ENG-USERMSG-004` Route low-risk payload decisions through the facade
  while keeping the enginefuncs ABI and `sv.multicast` storage legacy-owned.
  Evidence:
- [ ] `ENG-USERMSG-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence:

## Phase 80: Server Command Lifecycle Policy

- [x] `ENG-SVCMD-001` Baseline map/load/save/changelevel/restart command
  validation in `sv_cmds.c`, including argument parsing and console output.
  Evidence: `Documentation/codex/legacy/engine/server-command-lifecycle-baseline.md`.
- [x] `ENG-SVCMD-002` Implement target-neutral command decision helpers for
  map validation, save/load request classification, and lifecycle action plans.
  Evidence: `src/include/engine/server/server_command_lifecycle.hpp` and
  `src/engine/server/server_command_lifecycle.cpp`.
- [x] `ENG-SVCMD-003` Add tests for missing args, invalid maps, background
  maps, save names, quickload/quicksave aliases, and rejected transitions.
  Evidence: `tests/engine/server_command_lifecycle.cpp`.
- [x] `ENG-SVCMD-004` Route selected command decision trees through helpers
  while keeping `Cmd_Argv()`, filesystem probes, cvar mutation, and host command
  execution legacy-owned.
  Evidence: `engine/server/server_command_lifecycle_adapter.h`,
  `engine/server/server_command_lifecycle_adapter.cpp`, and
  `engine/server/sv_cmds.c`.
- [x] `ENG-SVCMD-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_server_command_lifecycle` and `-StopRunningXash`
  passed: focused test 1/1, `xash` build, alltests 90/90, runtime smoke first
  frame 0.503 seconds, stop reason `command`.

## Phase 81: Serverdata And Spawn Handshake

- [x] `ENG-SERVERDATA-001` Baseline `SV_SendServerdata()`, `SV_New_f()`,
  `SV_Spawn_f()`, signon fragments, movevars/userinfo resend flags, and
  developer/multiplayer print behavior.
  Evidence: Added
  `Documentation/codex/legacy/engine/serverdata-spawn-handshake-baseline.md`.
- [x] `ENG-SERVERDATA-002` Implement target-neutral serverdata payload and
  spawn-handshake planning helpers.
  Evidence: Added `src/engine/server/server_spawn_handshake.cpp` and
  `src/include/engine/server/server_spawn_handshake.hpp`.
- [x] `ENG-SERVERDATA-003` Add golden tests for serverdata fields, player box
  bounds, signon number, reconnect fallback, overflow/drop behavior, and
  single-player versus multiplayer branches.
  Evidence: Added `tests/engine/server_spawn_handshake.cpp`; focused target
  `test_engine_server_spawn_handshake` passes.
- [x] `ENG-SERVERDATA-004` Route helper decisions while keeping fragmentation,
  client state mutation, `SV_PutClientInServer()`, and reliable buffer ownership
  legacy-owned.
  Evidence: Added `engine/server/server_spawn_handshake_adapter.cpp` and routed
  `SV_SendServerdata()`, `SV_New_f()`, `SV_Spawn_f()`, `SV_Begin_f()`, and the
  signon-number write through it while leaving fragmentation and mutation in
  `engine/server/sv_client.c`.
- [x] `ENG-SERVERDATA-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_server_spawn_handshake` and `-StopRunningXash`
  passed: focused test 1/1, `xash` build, alltests 91/91, runtime smoke first
  frame 1.839 seconds, stop reason `command`.

## Phase 82: Server Frame Datagram Assembly

- [x] `ENG-FRAME-001` Baseline `SV_SendClientMessages()`, reliable datagram
  copy/fragment decisions, unreliable datagram copy, spectator datagram copy,
  overflow clearing, and resend userinfo/movevars flags.
  Evidence: Added
  `Documentation/codex/legacy/engine/server-frame-datagram-baseline.md`.
- [x] `ENG-FRAME-002` Implement target-neutral frame-send planning helpers for
  copy versus fragment decisions and overflow responses.
  Evidence: Added `src/engine/server/server_frame_datagram.cpp` and
  `src/include/engine/server/server_frame_datagram.hpp`.
- [x] `ENG-FRAME-003` Add tests for small reliable data, fragmented reliable
  data, ignored unreliable overflow, spectator payloads, and resend flags.
  Evidence: Added `tests/engine/server_frame_datagram.cpp`; focused target
  `test_engine_server_frame_datagram` passes.
- [x] `ENG-FRAME-004` Route planning through helpers while keeping netchan,
  frame construction, and actual message writes legacy-owned.
  Evidence: Added `engine/server/server_frame_datagram_adapter.cpp` and routed
  `SV_SendClientDatagram()`, `SV_UpdateToReliableMessages()`, and
  `SV_SendClientMessages()` through it while leaving netchan, frame
  construction, and message writes in `engine/server/sv_frame.c`.
- [x] `ENG-FRAME-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_server_frame_datagram` and `-StopRunningXash`
  passed: focused test 1/1, `xash` build, alltests 92/92, runtime smoke first
  frame 0.504 seconds, stop reason `command`.

## Phase 83: Save/Restore Compatibility Fixtures

- [x] `ENG-SAVE-001` Audit `sv_save.c` binary formats, token tables, landmark
  handling, global state, entity fields, and known compatibility quirks.
  Evidence: Added
  `Documentation/codex/legacy/engine/save-restore-format-baseline.md`.
- [x] `ENG-SAVE-002` Create tiny save/restore fixtures or generated binary
  fixtures that can be tested without shipping game assets.
  Evidence: Added `src/engine/server/save_restore_format.cpp`,
  `src/include/engine/server/save_restore_format.hpp`, and generated fixtures
  in `tests/engine/save_restore_format.cpp`.
- [x] `ENG-SAVE-003` Add read-only parser tests for header, token table, entity
  section, lightstyles, and rejected malformed data.
  Evidence: `test_engine_save_restore_format` covers `VALV`/`JSAV` headers,
  token-table rebasing, `ETABLE`, `Save Header`, `LIGHTSTYLE`, bundled save
  file entries, and malformed magic/version/count/token/section cases.
- [x] `ENG-SAVE-004` Decide the safe modernization boundary for save/restore
  before moving implementation code.
  Evidence: Added
  `Documentation/codex/modern/engine/save-restore-migration-boundary.md`;
  runtime save/load remains legacy-owned until real-save fixtures and game DLL
  field serialization coverage exist.
- [x] `ENG-SAVE-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_save_restore_format` and `-StopRunningXash`
  passed: focused test 1/1, `xash` build, alltests 93/93, runtime smoke first
  frame 0.713 seconds, stop reason `command`.

## Phase 84: Save/Restore Fixture Hardening

- [x] `ENG-SAVEHARD-001` Add sidequest-driven coverage for save/restore
  assumptions that are risky to migrate blindly, especially `.HL3` entity
  patch indexes and packed `viewentity` storage.
  Evidence: `tests/engine/save_restore_format.cpp` now covers invalid `.HL3`
  entity patch indexes and packed `viewentity` short parsing.
- [x] `ENG-SAVEHARD-002` Add a tiny parser or fixture helper for malformed
  entity patch records without routing runtime `sv_save.c` behavior yet.
  Evidence: `ParseSaveRestoreEntityPatch()` in
  `src/engine/server/save_restore_format.cpp`.
- [x] `ENG-SAVEHARD-003` Add an explicit guard or test for the 16-bit
  `viewentity` save-field assumption.
  Evidence: `static_assert(sizeof(short) == kSaveRestorePackedShortBytes)` in
  `src/engine/server/save_restore_format.cpp` and
  `tests/engine/save_restore_format.cpp`.
- [x] `ENG-SAVEHARD-004` Update the save/restore baseline to record
  non-empty-only lightstyle serialization and any fixture-hardening findings.
  Evidence:
  `Documentation/codex/legacy/engine/save-restore-format-baseline.md` and
  `Documentation/codex/modern/engine/save-restore-migration-boundary.md`.
- [x] `ENG-SAVEHARD-005` Run focused save/restore tests, full tests, runtime
  smoke with `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_save_restore_format` and `-StopRunningXash`
  passed: focused test 1/1, `xash` build, alltests 93/93, runtime smoke first
  frame 0.510 seconds, stop reason `command`.

## Phase 85: Client Rate And Userinfo Policy

- [x] `ENG-CLIENTPOL-001` Baseline `SV_ShouldUpdateUserinfo()`,
  `SV_CheckUpdateRate()`, `SV_CheckRate()`, and the low-risk policy portions of
  `SV_UserinfoChanged()`.
  Evidence:
  `Documentation/codex/legacy/engine/client-rate-userinfo-policy-baseline.md`.
- [x] `ENG-CLIENTPOL-002` Implement target-neutral helpers for update-info
  throttling, client rate validation, and compatible userinfo-derived flags
  that can be fed by legacy snapshots.
  Evidence: `src/include/engine/server/client_policy.hpp` and
  `src/engine/server/client_policy.cpp`.
- [x] `ENG-CLIENTPOL-003` Add tests for update intervals, clamped rates,
  missing or malformed userinfo values, and prediction/local-weapons related
  decisions.
  Evidence: `tests/engine/client_policy.cpp`.
- [x] `ENG-CLIENTPOL-004` Route only the smallest safe legacy decisions
  through adapters while keeping cvars, info-string mutation, hashing, logging,
  and client state ownership legacy-owned.
  Evidence: `engine/server/client_policy_adapter.h`,
  `engine/server/client_policy_adapter.cpp`, and `engine/server/sv_client.c`.
- [x] `ENG-CLIENTPOL-005` Run focused tests, full tests, runtime smoke with
  `+wait +wait`, and record first-frame timing.
  Evidence: `scripts/run-phase-validation.ps1` with
  `-FocusedTarget test_engine_client_policy` and `-StopRunningXash` passed:
  focused test 1/1, `xash` build, alltests 94/94, runtime smoke first frame
  0.499 seconds, stop reason `command`.

## Phase 86: Game DLL Bridge Boundary Audit

- [x] `ENG-GAMEDLL-001` Audit the `sv_game.c` enginefuncs table, exported game
  callbacks, entity allocation, string pool, private data ownership, and ABI
  constraints.
  Evidence:
  `Documentation/codex/legacy/engine/game-dll-bridge-baseline.md`.
- [x] `ENG-GAMEDLL-002` Group enginefunc callbacks into planned modern modules
  such as resources, tracing, messaging, entity lifecycle, cvars, and logging.
  Evidence:
  `Documentation/codex/modern/engine/game-dll-bridge-boundary.md`.
- [x] `ENG-GAMEDLL-003` Identify adapter-boundary tests for C-compatible
  callback shims that can be exercised without a real game DLL.
  Evidence:
  `Documentation/codex/modern/engine/game-dll-bridge-boundary.md` test
  strategy.
- [x] `ENG-GAMEDLL-004` Produce a migration order that avoids changing the
  game DLL ABI while allowing internals to move toward `src/engine`.
  Evidence:
  `Documentation/codex/modern/engine/game-dll-bridge-boundary.md`.
- [x] `ENG-GAMEDLL-005` Run documentation validation and any focused adapter
  tests added by the audit.
  Evidence: `git diff --check` passed. No focused adapter tests were added in
  this audit-only phase; the test candidates are documented for the follow-up
  bridge slices.

## Phase 800: POSIX Console Backend Validation

- [ ] `ENG-POSIX-CON-001` Build on a POSIX/Linux target with the current
  backend wrapper present.
  Evidence: `Documentation/codex/todo/posix_console_backend_todo.md`.
- [ ] `ENG-POSIX-CON-002` Route `Platform_Input()` through the POSIX backend
  without changing `Host_GetCommands()` behavior.
  Evidence:
- [ ] `ENG-POSIX-CON-003` Route POSIX stdout output through the backend or
  document why stdout remains in `Sys_PrintStdout()` until the router phase.
  Evidence:
- [ ] `ENG-POSIX-CON-004` Validate dedicated stdin command input manually.
  Evidence:
- [ ] `ENG-POSIX-CON-005` Validate daemonize/no-stdin behavior manually.
  Evidence:
- [ ] `ENG-POSIX-CON-006` Run full tests and a POSIX runtime smoke test.
  Evidence:

## Phase 801: Non-Windows Console Output Validation

- [ ] `ENG-NONWIN-CON-001` Audit Android, iOS, Switch, Vita, and other
  non-Windows console/log output branches after the Windows backend route is
  stable.
  Evidence: `Documentation/codex/todo/non_windows_console_backend_todo.md`.
- [ ] `ENG-NONWIN-CON-002` Decide whether each platform should gain an explicit
  output-only backend or remain a direct `Sys_PrintStdout()` platform branch
  until the router phase.
  Evidence: moved from Phase 43 `ENG-LOG-010`.
- [ ] `ENG-NONWIN-CON-003` Build or cross-compile at least one non-Windows
  target where practical before moving any live platform branch.
  Evidence:
- [ ] `ENG-NONWIN-CON-004` Record manual validation expectations for platforms
  that cannot be built in the current Windows environment.
  Evidence:

## Phase 802: Non-Windows System Runtime Validation

- [ ] `ENG-NONWIN-SYS-001` Build a POSIX/Linux target and verify
  `Sys_GetCurrentUser` still returns the effective user name when available.
  Evidence: `Documentation/codex/todo/non_windows_system_runtime_todo.md`.
- [ ] `ENG-NONWIN-SYS-002` Validate the POSIX fallback to `Player` when the
  password database lookup fails or returns an empty name, using a safe test
  harness if practical.
  Evidence:
- [ ] `ENG-NONWIN-SYS-003` Validate Vita username lookup manually before moving
  it behind the modern current-user adapter.
  Evidence:
- [ ] `ENG-NONWIN-SYS-004` Validate Android `Sys_GetNativeObject` provider
  order: filesystem provider first, Android provider second.
  Evidence:
- [ ] `ENG-NONWIN-SYS-005` Revisit `Sys_CanRestart` and `Sys_NewInstance` only
  after the target can run restart/change-game smoke tests.
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

## Phase 1100: Licensing And Attribution Audit

- [ ] `LICENSE-001` Inventory repository license files, third-party notices,
  file headers, and copied/vendor-derived source islands.
  Evidence:
- [ ] `LICENSE-002` Build a per-area attribution map for GPL, BSD-style,
  Unlicense/public-domain, and other license families present in the tree.
  Evidence:
- [ ] `LICENSE-003` Verify migrated or rewritten files preserve required
  copyright notices, origin attribution, and license text from their source
  material.
  Evidence:
- [ ] `LICENSE-004` Check that new dependencies and modernization utilities
  are compatible with the project license and distribution model.
  Evidence:
- [ ] `LICENSE-005` Produce a release/PR checklist for license compliance
  before publishing modernization work beyond the fork.
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
| 2026-05-09 | DEC-043 | Use console/logging ownership as the next engine phase because it unlocks deferred filesystem logging cleanup, then use the launcher/platform lessons for a `system.c` facade audit. | `done/todo/engine_logging_todo.md`, `done/todo/engine_platform_todo.md`, `legacy/engine/common-audit.md` |
| 2026-05-10 | DEC-045 | Consolidate engine command-line lookup behind a stateless modern view and C adapter, while leaving startup parsing in the launcher and legacy copying/numeric parsing in `system.c`. | `done/todo/engine_command_line_todo.md`, `modern/engine/command-line-facade-plan.md`, `src/engine/platform/command_line.cpp` |
| 2026-05-10 | DEC-046 | Put a low-risk standalone straggler sweep at the top of the next roadmap, with CRC32 table/constant consolidation as the first target and broader CRT/platform work queued behind it. | `done/todo/low_risk_stragglers_todo.md`, `modern/engine/standalone-stragglers-roadmap.md`, `public/crclib.c`, `src/utilities/checksum.cpp` |
| 2026-05-10 | DEC-047 | Centralize CRC32 table ownership in `src/utilities/checksum.cpp` and expose a private C table adapter so public `CRC32_*` loops keep their ABI and table-lookup performance. | `src/include/utilities/compat/checksum_adapter.h`, `src/utilities/compat/checksum_adapter.cpp`, `public/crclib.c`, `public/tests/test_crclib.c` |
| 2026-05-10 | DEC-048 | Move public CRT numeric conversion implementations behind `src/utilities/conversion.*`, preserving the C ABI and legacy parsing quirks through a private compatibility export. | `done/todo/engine_crt_todo.md`, `modern/engine/public-crt-conversion-guide.md`, `src/utilities/compat/crtlib_conversion.cpp`, `public/tests/test_atoi.c` |
| 2026-05-10 | DEC-049 | Insert a public-folder relocation sweep before system runtime work, using the public-header/compat-adapter pattern to move remaining project-owned helper islands while deferring vendored, platform fallback, and broad math/parser surfaces. | `done/todo/public_folder_sweep_todo.md`, `modern/public/public-folder-sweep-roadmap.md`, `public/crclib.c`, `public/crtlib.c`, `public/atlas.c`, `public/build.c` |
| 2026-05-10 | DEC-050 | Move remaining MD5 and low-risk CRT text helper bodies into `src/utilities` with C compatibility exports, preserving public headers while adding bounded memory-line copy behavior for `Q_memfgets`. | `src/utilities/md5.cpp`, `src/utilities/text.cpp`, `src/utilities/compat/crclib_md5.cpp`, `src/utilities/compat/crtlib_text.cpp`, `public/tests/test_crclib.c`, `public/tests/test_strings.c` |
| 2026-05-10 | DEC-051 | Finish the public-folder sweep by moving atlas and pure build-number logic behind `src/utilities` while keeping UTF helpers in public until their new focused baseline can guide a later migration. | `done/todo/public_folder_sweep_todo.md`, `src/utilities/atlas.cpp`, `src/utilities/build_number.cpp`, `src/utilities/compat/atlas_adapter.cpp`, `src/utilities/compat/build_number_adapter.cpp`, `public/tests/test_utflib.c` |
| 2026-05-10 | DEC-052 | Route only the Windows-verifiable `Sys_GetCurrentUser` branch through a modern platform adapter, preserving POSIX/Vita/Android legacy behavior until non-Windows runtime validation exists. | `legacy/engine/system-user-runtime-audit.md`, `modern/engine/system-user-runtime-facade-plan.md`, `src/engine/platform/current_user.cpp`, `src/engine/platform/current_user_adapter.cpp`, `todo/non_windows_system_runtime_todo.md` |
| 2026-05-10 | DEC-053 | Extract only pure filesystem bridge mount flag policy into `src/engine/filesystem`, while keeping `fs_interface_t` logging callbacks and rendered-console routing at the legacy boundary until an engine router/sink phase exists. | `legacy/engine/filesystem-bridge-audit.md`, `modern/engine/filesystem-bridge-migration-guide.md`, `src/engine/filesystem/mount_flags.cpp`, `engine/common/filesystem_engine.c`, `deferred/todo/filesystem_logging_todo.md` |
| 2026-05-10 | DEC-054 | After the Phase 50 milestone, use server-side engine code as the next coherent migration lane, starting with `sv_filter.c` and then source-query response building, while deferring renderer, memory, savegame, and rendered-console work. | `modern/milestone-50-structure-audit.md` |
| 2026-05-10 | DEC-055 | Treat `server.h`, `SV_*`, `Log_*`, `sv`, `svs`, `svgame`, command/cvar names, save/config files, and packet payloads as server compatibility boundaries; pure modern server logic should live under `src/engine/server` and receive snapshots or plain values from legacy adapters. | `legacy/engine/server-boundary-audit.md`, `modern/engine/server-migration-guide.md` |
| 2026-05-10 | DEC-056 | For the first server filter migration, keep legacy linked lists, commands, file writes, client iteration, and `host.realtime` ownership in `sv_filter.c`; route only rule activity, ID prefix matching, IP matching, and IP removal-selector policy through modern server helpers. | `legacy/engine/server-filter-baseline.md`, `modern/engine/server-filter-migration.md`, `src/engine/server/server_filter.cpp`, `engine/server/server_filter_adapter.cpp` |
