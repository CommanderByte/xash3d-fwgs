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
