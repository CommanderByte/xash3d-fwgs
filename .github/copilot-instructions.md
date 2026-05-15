# xash3dpp Rewrite — Standing Agent Instructions

This repository contains a legacy C engine codebase and a new modular C++ rewrite.
Conventions and mandatory patterns for `xash3dpp/` work are in
`.github/instructions/xash3dpp.instructions.md` (auto-injected when editing those files).
Session workflow, prompt ordering, and commit discipline are in `.github/WORKFLOW.md`.

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

Only the SDK-facing contracts that game DLLs and client DLLs depend on are
fixed. Renderer, filesystem, and other internal plugin interfaces will be
redesigned as part of the rewrite.

| Surface | Headers |
|---------|---------|
| Game DLL | `engine/eiface.h`, `engine/edict.h` |
| Client DLL | `engine/cdll_int.h`, `engine/cdll_exp.h` |
| Shared SDK structures | `common/`, `pm_shared/`, `engine/*.h` |

## Commit Convention

`tag: short description` — tag is a subsystem, feature, or filename without extension.
See [CONTRIBUTING.md](../CONTRIBUTING.md) for full code style rules.
