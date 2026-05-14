# xash3dpp Rewrite — Standing Agent Instructions

This repository contains a legacy C engine codebase and a new modular C++ rewrite.

**Run `/init` at the start of every session** to get the full context on the
repository layout, the legacy/rewrite boundary, and the working approach.

## Short Version

- All new work goes in `xash3dpp/`.
- Everything else in this repository (engine, filesystem, public, ref, common,
  pm_shared, android, 3rdparty, game_launch, scripts, src, tests, utils,
  Documentation/codex) is **legacy**. Read it for behaviour and ABI contracts;
  do not copy its structure or patterns into the rewrite.
- Agent customization files (`.github/`) are the only repo-root artefacts that
  are not legacy.

## Legacy Build (for verification only)

```powershell
# Dedicated server (no SDL2 required)
.\waf.bat configure --dedicated && .\waf.bat build

# Windows client (SDL2 required at 3rdparty/SDL2_VC)
.\waf.bat configure --sdl2=C:\git\xash3d-fwgs\3rdparty\SDL2_VC && .\waf.bat build
.\waf.bat install --destdir=C:\git\xash3d-fwgs\run-win32
```

See [Documentation/codex/windows-build-run-notes.md](../Documentation/codex/windows-build-run-notes.md)
for the full Windows smoke-test setup including SDL2 download and HLSDK.

## ABI Surfaces — Do Not Break

These C-compatible contracts must be preserved by the rewrite:

| Surface | Headers |
|---------|---------|
| Game DLL | `engine/eiface.h`, `engine/edict.h` |
| Client DLL | `engine/cdll_int.h`, `engine/cdll_exp.h` |
| Renderer | `engine/ref_api.h` |
| Filesystem plugin | `filesystem/filesystem.h` |
| Shared SDK structures | `common/`, `pm_shared/`, `engine/*.h` |

## Commit Convention

`tag: short description` — tag is a subsystem, feature, or filename without extension.
See [CONTRIBUTING.md](../CONTRIBUTING.md) for full code style rules.
