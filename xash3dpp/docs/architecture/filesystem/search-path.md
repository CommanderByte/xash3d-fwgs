# Search Path

> **Defined in**: `xash3dpp/include/xash3dpp/filesystem/search_path_flags.hpp`,
> `xash3dpp/include/xash3dpp/private/filesystem/search_path.hpp`,
> `xash3dpp/src/filesystem/filesystem.cpp`\
> **Namespace**: `xash::filesystem`

## Overview

A *search path* is a single mounted source of game assets — a directory, an
archive, or a device-specific backend. The `Filesystem` maintains an ordered
`std::deque<SearchPath>`, iterating it from back to front (highest priority
first) to resolve each file operation.

`SearchPathFlags` is a bitmask that records both static metadata about a path
(can it be written to? is it part of the active game?) and mount-time
configuration options (should HD variants be included?).

## `SearchPath` struct

| Field | Type | Role |
|-------|------|------|
| `backend` | `std::unique_ptr<ISearchBackend>` | The mounted backend; destroyed when the `SearchPath` is erased |
| `source_path` | `std::string` | The disk path that was mounted (for debug / info output) |
| `flags` | `SearchPathFlags` | Bitmask copied from the `add_game_directory` call |

`SearchPath` is a move-only value type stored directly in the deque. Moving it
transfers ownership of `backend`.

## `SearchPathFlags` enum

| Flag | Value | Meaning |
|------|-------|---------|
| `None` | `0` | No special attributes |
| `Static` | `1<<0` | Survives `clear_paths()`; used for engine root paths |
| `NoWrite` | `1<<1` | Never selected as the write destination |
| `GameDir` | `1<<2` | Part of the active game hierarchy |
| `Exec` | `1<<3` | May serve native library files (`.so` / `.dll`) |
| `Custom` | `1<<4` | Injected outside the normal game hierarchy |
| `SkipWads` | `1<<5` | Do not auto-mount WADs found inside this archive |
| `MountHD` | `1<<6` | Include `_hd` variant directories during hierarchy mount |
| `MountLV` | `1<<7` | Include `_lv` (low-violence) directories |
| `MountAddon` | `1<<8` | Include `addon` directories (TODO: not yet implemented) |
| `MountL10n` | `1<<9` | Include localisation directories (TODO: not yet implemented) |

All bitwise operators (`|`, `&`, `~`, `|=`, `&=`) and the `any()` helper are
provided as `constexpr` free functions in the same header.

## Layered lookup semantics

The `search_paths` deque is traversed **back-to-front**; the last-added entry has
the highest priority. This mirrors the legacy `searchpath_t` linked list which
was prepended on each `add_game_directory` call.

Typical mount order after `init()` + `activate_game()`:

```text
front (lowest priority)
  [0] basedir plain-dir    (Static)
  [1] basedir pak0.pak     (Static)
  …
  [n] gamedir pak0.pak     (GameDir)
  [n+1] gamedir plain-dir  (GameDir)  ← highest priority
back
```

A query iterates from `n+1` down to `0`; the first backend that returns
non-null/non-empty wins.

## `add_game_directory(dir, flags)`

1. Acquires `unique_lock(paths_mutex)`.
1. Calls `collect_paths_for_dir(pool, dir, flags, search_paths)`:
   a. Lists the directory with `platform::list_directory`.
   b. Sorts entries alphabetically (so `pak0.pak` before `pak1.pak`).
   c. For each archive type in `k_archive_types` order (PAK → PK3 → PK3DIR →
   WAD): for each matching entry, calls the type's `BackendFactory` and
   pushes the result onto `out`.
   d. Pushes a `DirBackend` for `dir` itself (always last → highest priority
   within this call).
1. Appends all newly created `SearchPath` entries to `search_paths`.

**Mount order within a directory** (lowest to highest priority within that call):
`pak0.pak` → `pak1.pak` → … → `maps/foo.pk3` → … → `dir_itself`

## `add_game_hierarchy(dir, flags)`

Calls `add_game_directory` for `dir`, optionally for `dir_hd` (`MountHD`),
and `dir_lv` (`MountLV`). Each call appends to `search_paths` in that order;
the plain dir is added last in each group, so `dir_hd/somefile` beats `dir/somefile`.

## `clear_paths()`

Acquires `unique_lock(paths_mutex)`. Erases every `SearchPath` whose `flags`
does not include `SearchPathFlags::Static`. Backends are destroyed (and their
pool memory reclaimed) as they fall out of the deque.

**TOCTOU gap**: after `clear_paths` releases the lock and before `add_game_directory`
re-acquires it during `rescan`, the deque may contain only `Static` entries (or be
empty). Concurrent readers will observe this partial state.

## `allow_direct_paths(enable)`

Atomically sets `allow_direct_paths`. When `true`, `open()` and related methods
accept paths that start with `/`, `C:\`, or contain `../`. Callers must always
restore to `false` after the operation that required it.

## Threading model

- `search_paths` is protected by `std::shared_mutex paths_mutex`.
  - `shared_lock`: `open`, `load_file`, `file_exists`, `file_size`, `file_time`,
    `disk_path`, `search`, `crc32_file`, `md5_file`, `find_library`.
  - `unique_lock`: `add_game_directory`, `clear_paths`, `mount_archive`.
- `allow_direct_paths` is `std::atomic<bool>`; reads and writes are lock-free
  with default `memory_order_seq_cst`.

## See also

- [backend-interface.md](./backend-interface.md) — `ISearchBackend` interface dispatched per `SearchPath`
- [filesystem-facade.md](./filesystem-facade.md) — `Filesystem::Impl` structure and threading model
- [archive-backends.md](./archive-backends.md) — concrete backend implementations
