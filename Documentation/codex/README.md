# Codex Onboarding Notes

This folder is a working documentation layer for this fork. It is meant for
repo orientation, modernization planning, and design notes that may not belong
in upstream-facing user documentation yet.

## Current Documents

- [tasks.md](tasks.md) is the living task list for the modularization effort,
  including test work and audit notes.
- [restart-branch-guide.md](restart-branch-guide.md) explains how this
  documentation-only restart branch was extracted from the larger modular
  rewrite branch and which references are the safest starting points.
- [agent-instructions.md](agent-instructions.md) defines standing instructions
  for Codex-driven modernization work in this fork.
- [agent-profiles.md](agent-profiles.md) describes reusable working profiles
  for documentation, testing, implementation, review, and release hygiene.
- [filesystem-glossary.md](filesystem-glossary.md) defines recurring
  filesystem terms used in the legacy and modernization docs.
- [../../tests/README.md](../../tests/README.md) describes the root test
  strategy and fixture policy for behavior-preserving modernization.
- [codebase-map.md](codebase-map.md) maps the repository layout, build targets,
  major subsystems, and important source files.
- [modular-cpp-rewrite-feasibility.md](modular-cpp-rewrite-feasibility.md)
  evaluates whether a more modular C++ style rewrite is feasible and proposes
  an incremental migration path.
- [windows-build-run-notes.md](windows-build-run-notes.md) records the current
  Windows build, SDL2, Steam asset, and HLSDK runtime smoke-test notes.
- [modularization-plan/](modularization-plan/README.md) contains the working
  plan for migrating internals toward a more modular C++ architecture.
- [legacy/engine/](legacy/engine/README.md) audits current engine/common
  architecture and coupling before engine-side modularization.
- [legacy/filesystem/](legacy/filesystem/README.md) documents the current
  filesystem architecture with Mermaid diagrams.
- [modern/](modern/README.md) documents intended modernized internal
  architecture and shared utility contracts for the rewrite.
- [modern/milestone-50-structure-audit.md](modern/milestone-50-structure-audit.md)
  is the current Phase 50 structure checkpoint and recommended next-roadmap
  reset.
- [todo/](todo/README.md) contains focused implementation checklists derived
  from the modernization docs.
- [deferred/](deferred/README.md) contains paused work that waits on broader
  subsystem ownership decisions.

## Repository At A Glance

Xash3D FWGS is a portable Half-Life/GoldSrc-compatible engine. The codebase is
mostly C with a few C++ compatibility and launcher surfaces. It builds through
Waf, with Gradle/Ninja integration for Android and CI scripts for many target
platforms.

The first-party engine surface is concentrated in:

- `engine/`: host loop, client, server, platform backends, DLL interfaces.
- `filesystem/`: virtual filesystem module and archive/search path handling.
- `ref/`: renderer modules, including OpenGL/GLES-compatible and software
  renderers.
- `public/`, `common/`, `pm_shared/`: shared SDK-style headers and utility
  libraries consumed by the engine, renderers, and game/client DLL interfaces.
- `src/launcher/`: modern launcher implementation, platform entry shell, and
  native launcher executable wiring through `src/wscript`.
- `resources/`: first-party product/build assets such as launcher icons and
  platform resource files.

Large dependency and packaging surfaces live in:

- `3rdparty/`: bundled libraries and optional client-side modules.
- `android/`: Android app, Gradle configuration, Kotlin/Java UI, and native
  build bridge.
- `scripts/`: platform build, release, packaging, and Waf helper scripts.

## Suggested Next Onboarding Steps

1. Build once with tests enabled on the fork's primary development platform.
2. Read the Phase 50 structure audit to see which migration lanes are mature
   and which legacy areas still need ownership decisions.
3. Use the server-side engine lane as the next coherent pilot, starting with a
   boundary audit and then the filter/query helpers.
4. Document every compatibility boundary before refactoring it: engine to game
   DLL, engine to client DLL, engine to renderer DLL, and filesystem module ABI.
