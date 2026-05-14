# Legacy Engine Survey

A wide-and-shallow first pass over the legacy Xash3D FWGS codebase, produced to
inform the `xash3dpp/` architecture decisions. Each file in this folder is a
1-page summary of one area: what it contains, what it owns, what it depends on,
and where the redesign boundaries are likely to be.

These notes describe the **legacy** code at the repository root. They are a
behavioural reference, not a design constraint for the rewrite.

## Subsystem Summaries

- [engine-common-and-platform.md](engine-common-and-platform.md) — host loop, cmd/cvar, common services, OS abstraction
- [engine-client.md](engine-client.md) — `cl_*`, parse, sound, input
- [engine-server.md](engine-server.md) — `sv_*`, game DLL bridge
- [filesystem.md](filesystem.md) — virtual filesystem, archive backends
- [renderers.md](renderers.md) — `ref/` GL/GLES/software renderers
- [public-common-sdk.md](public-common-sdk.md) — `public/`, `common/`, `pm_shared/` utilities and SDK headers
- [launcher-and-android.md](launcher-and-android.md) — `game_launch/` and `android/` wrappers

## Top-level reference

- [overview.md](overview.md) — cross-cutting map: how the subsystems plug together at the engine level
