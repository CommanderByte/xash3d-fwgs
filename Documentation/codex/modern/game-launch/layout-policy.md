# Game Launcher Layout Policy

## Goal

Keep launcher behavior modular while respecting that executable entry points
and platform resource files are build-system concerns.

The launcher has no upper dependency inside this repository, so it can set a
clean convention for future top-level executable modules.

## Proposed Repository Shape

```text
src/
  include/launcher/        private launcher contracts
  include/launcher/platform/
                            private platform bridge contracts
  launcher/                shared launcher implementation
  launcher/platform/       executable entry points and process glue

tests/
  launcher/                target-neutral launcher unit tests

resources/
  launcher/
    windows/
      game.rc
      icon-xash-material.ico
    source/
      icon-xash-material.png
```

Launcher resources live in the top-level `resources/` tree because they are
product/build assets rather than launcher implementation code.

## Ownership Rules

- `src/launcher/` owns reusable launcher behavior.
- `src/launcher/platform/` owns platform entry-point signatures and
  process-level error presentation, dynamic-library calls, and platform
  environment defaults.
- `src/wscript` owns executable target wiring.
- Launcher resources live under `resources/launcher/`.
- Do not put platform packaging assets under `src/`; `src/` is for compiled
  implementation and private headers.

## Platform Resource Rules

- Use `resources/launcher/<platform>/` for platform-specific generated or
  compiled launcher resources.
- Use `resources/launcher/source/` for editable source assets used to produce
  platform resources.
- Keep filenames stable unless packaging scripts are updated in the same
  change.
- Keep large or generated assets out of git unless they are required to build
  the launcher.

## Near-Term Plan

1. Move the thin entry shell to `src/launcher/platform/entry.cpp`.
2. Move Windows resource files into `resources/launcher/windows/`.
3. Move the editable PNG into `resources/launcher/source/`.
4. Update `src/wscript` and `game.rc` paths.
5. Move dynamic-library and environment platform calls under
   `src/launcher/platform/`.
6. Add optional runtime config overrides backed by compiled defaults.
7. Move launcher executable target ownership into `src/wscript` and remove the
   obsolete `game_launch/` subproject.
8. Rebuild `xash3d` and run the Windows launcher smoke.

## Non-Goals

- Moving launcher resources into `src/` or back into implementation folders.
- Replacing Waf resource compilation in this phase.
- Designing Android/iOS packaging layout before auditing those targets.
- Renaming the executable target.
