# Game Hierarchy Builder

## Purpose

`GameHierarchyBuilder` is the target-neutral planning object for game
directory mount order. It does not touch `searchpath_t`, scan archives, or
mutate global filesystem state. Its job is to turn the already-parsed game
directory inputs into ordered mount requests that legacy `filesystem.c` can
apply later.

This keeps policy and mutation separate:

- The builder owns ordering, generated path names, flags, and whether a request
  temporarily enables direct paths.
- Legacy filesystem code still owns recursion through parsed `gameinfo.txt`,
  archive discovery, and `FS_AddGameDirectory` application for now.

## Current Order

For the active game directory, the request order is:

1. `rodir/game/` with direct paths enabled while mounting.
2. `game_downloads/`.
3. `game/`.
4. `game_hd/` when HD content is requested.
5. `game_addon/` when addon content is requested.
6. `game_lv/` when low-violence content is requested.
7. `game_language/` when localization is requested and the language begins
   with an ASCII alphabetic character.
8. `game/custom/`.

For base or fallback directories, downloads and custom are skipped because they
are game-directory-only mounts. Optional HD, addon, low-violence, and
localization requests still follow the main directory request, matching the
current legacy behavior.

## Request Shape

Each `GameHierarchyMountRequest` records:

- `kind`: stable debug category for diagnostics and future snapshots.
- `path`: generated mount path with legacy trailing slash conventions.
- `flags`: caller-provided flags to pass to `FS_AddGameDirectory`.
- `enableDirectPaths`: whether the legacy adapter must temporarily allow direct
  paths while applying the request.

The builder deliberately accepts already-derived flag values. It separates
read-only root flags, optional content flags, and game-custom flags because the
legacy code treats those buckets differently:

- `rodir/game/` gets read-only root flags and may gain `FS_GAMERODIR_PATH`.
- `game_hd/`, `game_addon/`, `game_lv/`, and `game_language/` keep the caller's
  mount flags plus nowrite/custom markers.
- `game_downloads/` and `game/custom/` use only the nowrite/custom markers.

Keeping that derivation outside the builder leaves it independent of
`filesystem.h` legacy bit constants and makes it reusable in tests, tools, and
future module code.

## Legacy Adapter

`filesystem/game_hierarchy_adapter.cpp` computes the legacy flag buckets, asks
`GameHierarchyBuilder` for requests, and applies each request through
`FS_AddGameDirectory`. The existing recursive `gameinfo.txt` handling stays in
`filesystem.c` until gameinfo parsing itself is extracted.

This means Phase 14 changes mount construction without changing the public
filesystem API or the legacy application point.
