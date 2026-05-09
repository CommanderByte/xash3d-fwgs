# Filesystem State Ownership

## Purpose

This note records the current `filesystem.c` global state before extracting a
future `FilesystemState` object.

## Current Globals

| State | Current Owner | Role |
| --- | --- | --- |
| `FI` | `filesystem.c` | Public `fs_globals_t` view of game info and discovered games. |
| `fs_mempool` | `filesystem.c` | Filesystem allocation pool used by search paths, files, and lists. |
| `fs_rootdir` | `filesystem.c` | Current root directory for direct paths and full disk paths. |
| `fs_writepath` | `filesystem.c` | Writable search path used for writes, renames, and deletes. |
| `fs_searchpaths` | `filesystem.c` | Ordered linked list of mounted search paths. |
| `fs_basedir` | `filesystem.c` | Base game directory name. |
| `fs_gamedir` | `filesystem.c` | Current game directory name. |
| `fs_rodir` | `filesystem.c` | Optional read-only external content root. |
| `fs_language` | `filesystem.c` | Current localization suffix for hierarchy mounting. |
| `fs_ext_path` | `filesystem.c` | Direct-path bypass flag for compatibility-sensitive lookups. |
| `g_engfuncs` | `filesystem.c` | Engine callback table installed by `GetFSAPI`. |
| `g_api` | `filesystem.c` | Exported filesystem API function table. |

## Extraction Candidates

- `FilesystemState`: own root dirs, search path list, write path, language, and
  direct-path flag.
- `SearchPathList`: append, clear, duplicate-mount checks, and snapshot capture.
- `GameInfoStore`: own `FI.games`, selected `FI.GameInfo`, and parsing state.
- `FilesystemCallbacks`: wrap `g_engfuncs` and default no-init callback stubs.

## Migration Rule

Do not move state until tests cover the behavior attached to that state. The
first state extraction should be a read-only snapshot helper or a tiny mutation
helper with no search order changes.
