# Windows `fs_path` Baseline

## Capture Context

Date captured: 2026-05-09.

Branch: `codex-repo-onboarding-modular-rewrite`.

Commit at capture time: `063cf9cd`.

Engine version reported by log:

```text
Xash3D FWGS 0.21 (4056, 843f6297, codex-repo-onboarding-modular-rewrite, win32-i386)
```

Runtime layout:

- Engine runtime: `C:\git\xash3d-fwgs\run-win32`
- Writable base directory: `C:\git\xash3d-fwgs\run-win32`
- Read-only asset directory: `C:\Program Files (x86)\Steam\steamapps\common\Half-Life`
- Runtime uses local FWGS-compatible `hlsdk-portable` `hl.dll` and
  `client.dll` with Steam assets mounted through `XASH3D_RODIR`.

Launch command:

```powershell
$env:XASH3D_BASEDIR = 'C:\git\xash3d-fwgs\run-win32'
$env:XASH3D_RODIR = 'C:\Program Files (x86)\Steam\steamapps\common\Half-Life'
Push-Location .\run-win32
.\xash3d.exe -dev 2 -log +fs_path +quit
Pop-Location
```

Result:

```text
Exited with code 0
Stopped with reason "command" at May09 2026 [12:00.32]
```

## Raw `fs_path` Output

The order below is the search order printed by the engine. Earlier entries have
lookup priority over later entries.

```text
Current search path:
valve/custom/ custom nowrite
valve/ gamedir
valve/extras.pk3 (120 files) gamedir
valve_downloads/ custom nowrite
C:/Program Files (x86)/Steam/steamapps/common/Half-Life/valve/ rodir nowrite
C:/Program Files (x86)/Steam/steamapps/common/Half-Life/valve/xeno.wad (264 files) rodir nowrite
C:/Program Files (x86)/Steam/steamapps/common/Half-Life/valve/tempdecal.wad (1 files) rodir nowrite
C:/Program Files (x86)/Steam/steamapps/common/Half-Life/valve/spraypaint.wad (14 files) rodir nowrite
C:/Program Files (x86)/Steam/steamapps/common/Half-Life/valve/liquids.wad (32 files) rodir nowrite
C:/Program Files (x86)/Steam/steamapps/common/Half-Life/valve/halflife.wad (3116 files) rodir nowrite
C:/Program Files (x86)/Steam/steamapps/common/Half-Life/valve/gfx.wad (7 files) rodir nowrite
C:/Program Files (x86)/Steam/steamapps/common/Half-Life/valve/fonts.wad (3 files) rodir nowrite
C:/Program Files (x86)/Steam/steamapps/common/Half-Life/valve/decals.wad (222 files) rodir nowrite
C:/Program Files (x86)/Steam/steamapps/common/Half-Life/valve/cached.wad (2 files) rodir nowrite
./ static
C:/Program Files (x86)/Steam/steamapps/common/Half-Life/ nowrite static
```

## Observations

- Local `valve/custom/` has the highest priority and is marked `custom
  nowrite`.
- Local `valve/` is the primary writable game directory and is marked
  `gamedir`.
- Local `valve/extras.pk3` is mounted after local loose `valve/` content but
  before `valve_downloads/`.
- Steam `valve/` content is mounted through `rodir nowrite`.
- Steam WADs are individually mounted after the Steam `valve/` directory.
- Static roots remain at the bottom: local `./` first, then Steam Half-Life as
  `nowrite static`.

## Why This Baseline Matters

Filesystem modernization must preserve this ordering unless a future design
decision explicitly changes compatibility behavior. This baseline is especially
useful when refactoring:

- `FS_AddGameHierarchy`
- `FS_AddGameDirectory`
- archive mount ordering
- `rodir` handling
- loose-file versus archive precedence
- debug output for future `fs_path_verbose` or `fs_why` tools
