# Filesystem Export And Dependency Audit

## Purpose

This note records what `filesystem_stdio` must provide to the engine and which
repo callers still depend on filesystem implementation details. The goal is to
separate hard ABI contracts from internal code that can be trimmed or moved
behind modern runtime adapters.

## Shared Library Contract

The built Windows DLL currently exports exactly two symbols:

| Export | Required By | Contract |
| --- | --- | --- |
| `GetFSAPI` | `engine/common/filesystem_engine.c`, `utils/xar/xar.c`, tests | Primary C ABI loader. Returns `FS_API_VERSION`, fills `fs_api_t`, and returns `fs_globals_t *FI`. |
| `CreateInterface` | `engine/common/filesystem_engine.c`, tests, external Valve-style consumers | Secondary factory. Returns `VFileSystem009` for `FILESYSTEM_INTERFACE_VERSION` and a copied `fs_api_t` for `FS_API_CREATEINTERFACE_TAG`. |

Verification on 2026-05-09 used a PE export table read of
`build/filesystem/filesystem_stdio.dll` and found:

```text
CreateInterface
GetFSAPI
```

`filesystem/exports.txt` now lists both symbols so relocatable/static helper
builds match the normal DLL contract.

## Engine Loader Dependencies

`engine/common/filesystem_engine.c` is the primary in-repo loader:

- loads `filesystem_stdio` with `COM_LoadLibrary`
- resolves `GetFSAPI`
- calls `GetFSAPI(FS_API_VERSION, &g_fsapi, &FI, &fs_memfuncs)`
- resolves `CreateInterface`
- stores it for `FS_GetNativeObject`

That means both exports are mandatory unless the engine loader contract changes.

## Public Header Dependencies

`filesystem/filesystem.h` is included outside the filesystem module by:

- `engine/ref_api.h`
- platform loaders such as `engine/platform/posix/lib_posix.c` and
  `engine/platform/android/lib_android.c`
- `engine/common/lib_common.c`
- a few platform/video helpers
- `utils/xar/xar.c`
- filesystem tests

`filesystem/fscallback.h` is included from `engine/platform/platform.h`, which
widens the effective public surface because it exposes `g_fsapi` as `FS_*`
macros across the engine.

`filesystem/VFileSystem009.h` is only included by the facade implementation and
the interface regression test in this repo, but it remains a compatibility ABI
for external callers.

`filesystem/filesystem_internal.h` is now only included by filesystem facade,
adapter, and legacy backend files. That is good: it is private, but it is still
too broad.

## Public Data Shape That Cannot Be Trimmed Yet

These types are part of the current ABI and should remain stable until an
explicit filesystem API version bump:

- `fs_api_t`
- `fs_globals_t`
- `gameinfo_t`
- `search_t`
- opaque `file_t`
- opaque `searchpath_t`
- `fs_dllinfo_t`

The biggest compatibility anchors are `fs_api_t` and `fs_globals_t`. Engine UI
and host code read `FI->numgames`, `FI->games`, and `FI->GameInfo` directly, so
gameinfo ownership cannot fully disappear behind `FilesystemRuntime` until
those callers move to a safer snapshot/query API.

## High-Value In-Repo Callers

| Caller | Dependency | Trim Impact |
| --- | --- | --- |
| `engine/common/filesystem_engine.c` | loader, wrapper functions, command hooks | Defines the current hard module boundary. |
| `engine/platform/platform.h` | includes `fscallback.h` | Keeps many `FS_*` macros visible across engine code. |
| `engine/common/lib_common.c` and platform loaders | `FindLibrary`, `fs_dllinfo_t` | Keeps DLL lookup behavior in the public table. |
| `engine/common/mod_bmodel.c` | archive lookup/open/load functions | Keeps `searchpath_t *` archive handles public for now. |
| UI/game switching code | `FI->games`, `gameinfo_t` | Blocks hiding gameinfo state immediately. |
| `utils/xar/xar.c` | `GetFSAPI`, archive mount/search/read | Keeps command-line archive tooling as a real external client. |
| `tests/filesystem/*` | broad API coverage | Useful safety net; not a trimming blocker. |

## Dormant Or Weakly Used ABI Slots

These fields are candidates for cleanup only in a future `FS_API_VERSION` bump
or compatibility shim because the struct layout is public:

- `ArchivePath`: currently `NULL` in `g_api`; no in-repo caller found.
- `CreateInterface(FS_API_CREATEINTERFACE_TAG)`: returns a copied `fs_api_t`;
  tests cover it, and external callers may rely on it.
- `VFileSystem009` methods that are stubs today, such as resource preloading
  and warning hooks. They are vtable slots and cannot be removed safely.

## Trim Candidates Without ABI Breakage

1. Split `filesystem_internal.h`.
   Keep legacy layout types in one private compatibility header and move
   adapter function declarations into focused headers. This reduces accidental
   coupling without changing exports.

2. Move `VFileSystem009.cpp` off direct private internals.
   It still includes `filesystem_internal.h` for `FS_*`, `Mem_Free`, and
   `Con_DPrintf`. Add a small C facade adapter for the exact operations the
   Valve wrapper needs, then remove the private internal include from
   `VFileSystem009.cpp`.

3. Introduce runtime-owned mount/search path records.
   Current backend `.c` files still allocate `searchpath_t`, set callback
   tables, and own payload structs. Move allocation/registration into
   `FilesystemRuntime`, leaving `searchpath_t` as a compatibility view.

4. Extract `FS_Search` result assembly.
   `search_t` must remain public, but sorting, duplicate handling, and packed
   result allocation can move to modern code behind an adapter.

5. Extract file handle operations.
   `file_t` is opaque publicly, so the internals can migrate more aggressively
   than `search_t` or `gameinfo_t`. Existing `file_handle_ops` helpers are the
   starting point.

6. Move gameinfo state behind snapshots/query APIs.
   This is high value but higher risk because engine callers read `FI`
   directly. Keep `FI` as a compatibility mirror until engine call sites stop
   indexing `FI->games` directly.

These completed candidates are archived in
`Documentation/codex/done/todo/modern_filesystem_handlers_todo.md`. Logging
cleanup is deferred in `Documentation/codex/deferred/todo/filesystem_logging_todo.md`
because direct `Con_Printf`, `Con_DPrintf`, `Con_Reportf`, and `Sys_Error`
usage is engine console behavior, and should resume after that ownership pass.

## Things Not To Trim Yet

- Do not remove `CreateInterface`; the engine currently requires it at load.
- Do not reorder or shrink `fs_api_t`.
- Do not change `VFileSystem009.h`.
- Do not remove archive-specific public functions while `mod_bmodel.c` uses
  `GetArchiveByName`, `FindFileInArchive`, and `LoadFileFromArchive`.
- Do not hide `FI` until UI/game-switching callers stop reading it directly.
