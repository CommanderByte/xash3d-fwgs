# Filesystem Bridge Audit

## Scope

This audit covers the engine-side bridge in `engine/common/filesystem_engine.c`.
It does not cover the standalone `filesystem_stdio` implementation in
`filesystem/` or `src/filesystem/`.

The bridge currently owns:

- thin wrappers from engine callers to `g_fsapi`;
- VFS cvars and mount flag construction;
- VFS config loading and saving;
- filesystem DLL loading and `GetFSAPI`/`CreateInterface` resolution;
- root and read-only root directory selection;
- engine callback wiring through `fs_interface_t`;
- startup command registration for `fs_rescan`, `fs_path`, `fs_clearpaths`,
  and `fs_make_gameinfo`.

## Movable Now

The mount flag construction is pure policy. It only maps four engine cvar
selections to public filesystem mount bits:

- `fs_mount_hd` -> `FS_MOUNT_HD`
- `fs_mount_lv` -> `FS_MOUNT_LV`
- `fs_mount_addon` -> `FS_MOUNT_ADDON`
- `fs_mount_l10n` -> `FS_MOUNT_L10N`

That behavior can move into a modern helper because it has no dependency on
command buffers, file IO, DLL loading, rendered console state, or host
lifecycle.

## Not Movable Yet

### Logging Callbacks

`fs_interface_t` passes `Con_Printf`, `Con_DPrintf`, `Con_Reportf`, and
`Sys_Error` to the filesystem module. This remains a compatibility boundary.

The console/logging phase has platform console backends and message filtering,
but it does not yet own a full router for normal console output, engine log
output, rcon, rendered in-game console output, and fatal paths. Replacing the
callbacks before that router exists would either duplicate the future design or
silently change where filesystem messages appear.

### VFS Config Load/Save

`FS_LoadVFSConfig()` and `FS_SaveVFSConfig()` depend on cvars, command-buffer
execution, `Host_FinalizeConfig()`, and filesystem IO. A useful refactor here
would need a config serialization phase, not a filesystem bridge cleanup.

### DLL Loading

`FS_LoadProgs()` is tied to `COM_LoadLibrary`, `COM_GetProcAddress`,
`Sys_Error`, ABI version checks, and the engine startup fatal path. It should
stay in the legacy bridge until a host/library-loading phase is ready.

### Root Directory Selection

`FS_DetermineRootDirectory()` and `FS_DetermineReadOnlyRootDirectory()` are
platform sensitive. They touch environment variables, command-line parsing,
SDL base paths, iOS paths, Vita paths, `getcwd`, and fatal fallback behavior.
This needs non-Windows validation before the live branches move.

## Current Recommendation

Phase 50 should extract only mount flag construction and leave the logging
callback bridge intact. The next useful bridge work is either:

- a router-backed filesystem logging callback adapter after console routing
  exists; or
- a root-directory resolver phase after POSIX/mobile validation is available.
