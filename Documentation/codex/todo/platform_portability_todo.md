# Platform Portability TODO

Goal: reduce porting burden by turning scattered platform/default/path/library
decisions into one compile-time selected platform layer with small tested
helpers and documented runtime hook groups.

Reference plan:
`Documentation/codex/modern/engine/platform-portability-architecture.md`.

## Phase 168: Platform Layer Architecture

- [x] Scan `engine/platform/` folder structure and Waf source selection.
- [x] Identify OS/platform folders, framework backend folders, and support
  folders.
- [x] Define layered platform hooks under one platform layer instead of one giant
  runtime-swappable platform interface.
- [x] Record launcher handoff opportunities and migration constraints.

Phase 168 evidence:
`Documentation/codex/modern/engine/platform-portability-architecture.md`.

## Phase 169: Platform Targets And Defaults

- [ ] Add private `PlatformProfile` and `PlatformDefaults` records.
- [ ] Cover Windows, SDL2, SDL3, Linux fbdev, dedicated, Android, Vita, Switch,
  DOS, static-library, and low-memory synthetic profiles.
- [ ] Compare default selections against `common/defaults.h` and
  `common/backends.h`.
- [ ] Do not route production defaults until tests prove parity.

## Phase 170: Game Folder And Base Path Setup

- [ ] Audit launcher startup, `FS_Engine_GetBaseDir`, `FS_Engine_GetRoDir`,
  user/write path behavior, and native object lookup.
- [ ] Add target-neutral game/base path records and tests.
- [ ] Preserve current SDL/Win32/POSIX/Android/Vita/iOS/Switch behavior through
  legacy adapters.
- [ ] Route only one low-risk path decision after parity tests.

## Phase 171: Engine Library Loading

- [ ] Audit `Sys_LoadLibrary`, platform library loaders, launcher
  `EngineLibrary`, filesystem library lookup, renderer/client/menu/game DLL
  lookup, and static/internal game-library behavior.
- [ ] Add private engine-library name/search helper with fake filesystem probes.
- [ ] Cover `.dll`, `.so`, `.dylib`, `.prx`, `lib` prefix, Android custom
  loader, and static-library cases.
- [ ] Avoid changing loader lifetime or unload behavior until smoke-tested.

## Phase 172: Launcher Startup Handoff

- [ ] Define a private launcher startup snapshot with argc/argv, executable
  path, config path, requested game dir, engine library path, and platform
  profile.
- [ ] Add tests around snapshot construction without starting the engine.
- [ ] Decide how the engine can query the snapshot through a C-compatible
  adapter.
- [ ] Keep `Host_Main` and legacy entry signatures stable.

## Phase 173: Platform Runtime Hooks

- [ ] Split runtime hooks into small groups behind one platform layer:
  time/sleep, console, window, input, clipboard, haptics, crash, native objects,
  restart.
- [ ] Document which hooks are mandatory, optional, client-only, or
  dedicated-safe.
- [ ] Avoid SDK types in generic headers.
- [ ] Defer fatal/restart/crash route-through until target validation exists.

## Phase 174: Platform Layer Checkpoint

- [ ] Compare default/path/library test coverage, platform folder clarity, and
  smoke-test needs after phases 168-173.
- [ ] Decide whether to keep working platform portability or return to
  client/render fixtures.
- [ ] Move completed platform TODOs to `done/` where sensible.
- [ ] Run full validation and runtime smoke timing after any production
  route-through.
