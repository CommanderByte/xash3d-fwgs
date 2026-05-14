# Platform Portability Architecture

This note audits `engine/platform/` and proposes a common structure for future
platform modernization. The goal is to reduce porting burden by giving the
engine one stable platform layer, while each build supplies the appropriate
compile-time platform implementation.

## Current Shape

`engine/platform/` currently mixes three kinds of code:

| Layer | Current folders | Role |
| --- | --- | --- |
| OS/platform ports | `win32`, `posix`, `linux`, `android`, `ios`, `psvita`, `nswitch`, `dos`, `irix` | Native process, filesystem, dynamic library, console, crash, path, and platform-specific hooks. |
| Framework backends | `sdl1`, `sdl2`, `sdl3` | Window, input, clipboard, joystick, timing, audio, and SDL event handling. |
| Support/fallback code | `misc`, `stub` | Static library loader, custom allocator/sbrk support, network/sound stubs. |

`engine/platform/platform.h` is the central legacy C surface. It declares
`Platform_*`, `SDLash_*`, `Win32_*`, `Posix_*`, `Linux_*`, `Android_*`,
`PSVita_*`, and similar functions, then uses inline preprocessor dispatch for
init, shutdown, sleep, input, library existence, crash handler availability,
and optional input/window helpers.

Waf already performs coarse platform selection:

- `engine/wscript` adds `platform/<DEST_OS>/*.c`;
- POSIX targets add `platform/posix/*.c`;
- iOS adds `.c` and `.m` files from `platform/ios`;
- SDL targets add `platform/sdlN/*.c`;
- custom swap and static-library modes add files from `platform/misc`.

That means future modernization must respect compile-time source selection.
The win is not runtime switchability; the win is that every compiled platform
implementation provides the same documented platform hooks from one platform area,
so the rest of the engine can rely on one shape.

## Platform Layer Goal

The engine should eventually depend on one platform layer, not on scattered
knowledge of Win32, POSIX, SDL, Android, Vita, Switch, DOS, or custom allocator
details.

That layer should be:

- compile-time selected by Waf and target defines;
- physically grouped under the platform source area;
- exposed to the engine through one stable C-compatible facade during
  migration;
- backed by small C++ helpers where target-neutral choices are easy to test;
- free of SDK-specific types in generic headers.

This is explicitly not a runtime plugin system. There is no need to switch
from Win32 to SDL to Vita at runtime. The platform layer exists so a port
author knows which services to implement and the engine knows which services it
can call.

## Critical Take

A single inheritance-heavy `IPlatform` would be too broad. It would collect
timers, message boxes, crash handlers, native objects, console input, SDL
events, joysticks, clipboard, shell execution, dynamic libraries, app paths,
and restart behavior into one object. That would be easy to name and painful
to maintain.

Use small game-engine pieces under one platform layer instead:

- target-neutral helpers for defaults and startup choices;
- narrow backends for runtime hooks;
- C adapters where legacy engine code still calls `Platform_*` or `Sys_*`;
- fake implementations only for unit tests, not for shipping platform swaps.

## Proposed Pieces

### `PlatformProfile`

Plain build/runtime facts used by default and startup helpers:

- OS family: Windows, POSIX, Linux, Android, iOS, Vita, Switch, DOS, PSP, etc.
- framework: SDL none/1/2/3;
- build mode: client, dedicated, static libs, internal game libs;
- feature flags: mobile, low-memory, custom swap, ffmpeg, curl, crash handler;
- target quirks: no zip, no touch, reduced file descriptors, no IPv6 resolve.

This is highly testable. Unit tests can feed synthetic profiles without
compiling each platform.

### `PlatformDefaults`

Pure functions that answer what `common/defaults.h` currently answers through
macros:

- video backend;
- input backend;
- sound backend;
- timer backend;
- message box backend;
- dynamic library backend;
- default fullscreen/touch/mouse/renderer cvars;
- low-memory and reduced-file-descriptor defaults.

The public macros should remain authoritative until the helper matches them in
tests. The first route-through can be a static-assert or test-only comparison,
not a production rewrite.

### `AppEnvironment`

Paths and process environment:

- executable path and directory;
- current working directory fallback;
- base/read-only data directory;
- user/write directory;
- platform document or preference directory;
- restart command-line reconstruction;
- native object lookup order.

This is one of the most useful porting targets because asset lookup failures
are common and painful. It also connects cleanly to the modern launcher, which
already owns startup argument and engine-library loading.

### `EngineLibraryLocator`

Dynamic library naming and search behavior:

- prefix/suffix rules: `lib`, `.dll`, `.so`, `.dylib`, `.prx`;
- engine, filesystem, renderer, menu, client, and game DLL categories;
- static/internal game library behavior;
- Android custom loader limitations;
- architecture/platform suffix decisions.

This should complement existing filesystem/library work rather than create a
second unrelated loader.

### `PlatformRuntimeHooks`

Runtime hooks that genuinely need OS/framework code:

- init/shutdown sequencing;
- time/sleep/nanosleep;
- message box/shell open/status;
- system console input/output;
- crash handler setup/restore;
- event pump and text input;
- mouse/clipboard/joystick/haptics/gyro;
- display orientation;
- restart/new instance.

Do not try to make every method mandatory. Use capability queries or small
backend groups so dedicated/server-only builds do not pretend to support
clipboard, joystick, or rendered window APIs. From the engine's perspective,
these groups should still live in the same platform layer.

## Launcher Handoff

The modern launcher can help by creating a small startup record before handing
control to `Host_Main`:

```text
LauncherStartupContext
  argc/argv
  executable path
  launcher config path
  requested game dir
  engine library path
  platform profile snapshot
```

For now this should remain a private launcher/engine bridge concept. The legacy
entry point can still pass `argc`, `argv`, default game directory, and
`Sys_ChangeGame`. Later, the engine can query a C adapter for the startup
context instead of rediscovering base paths and platform defaults in several
places.

## Suggested Source Layout

```text
src/include/engine/platform/
  platform_profile.hpp          pure build/runtime profile values
  platform_defaults.hpp         backend/default selections
  app_environment.hpp           path/native-object/restart records
  engine_library_locator.hpp    library name/search rules
  platform_capabilities.hpp     capability flags and service grouping
  platform_startup.hpp          launcher-to-engine startup snapshot
  compat/
    platform_profile_adapter.h
    app_environment_adapter.h

src/engine/platform/
  platform_profile.cpp
  platform_defaults.cpp
  app_environment.cpp
  engine_library_locator.cpp
  platform_startup.cpp
  compat/
```

Keep `engine/platform/platform.h` as the legacy C facade during migration.
Move pure decisions into `src/engine/platform/`; leave OS calls in
`engine/platform/<target>/` until a target can be validated. The long-term
physical layout can be consolidated later, but platform-specific code should
remain in the platform source area rather than leaking into unrelated engine
domains.

## Migration Sequence

1. Audit `engine/platform/`, `common/defaults.h`, `common/port.h`,
   `engine/common/system.c`, and `engine/common/filesystem_engine.c`.
2. Add `PlatformProfile` and `PlatformDefaults` with synthetic tests for
   Windows, SDL2/SDL3, Linux fbdev, dedicated, Android, Vita, Switch, DOS, and
   static-library profiles.
3. Add `AppEnvironment` path planning and compare it to current
   `FS_Engine_GetBaseDir`, `FS_Engine_GetRoDir`, and launcher behavior.
4. Add `EngineLibraryLocator` and compare it to `Sys_LoadLibrary`,
   platform-specific library loaders, filesystem library lookup, and launcher
   engine-library lookup.
5. Introduce a small launcher startup snapshot only after the default/path
   helpers are tested.
6. Route one low-risk legacy decision at a time.

## Testing Strategy

Most of this can be tested without smoke-testing every phase:

- default-selection tests use synthetic `PlatformProfile` values;
- path tests use fake executable/config/current-directory inputs;
- library locator tests use fake filesystem probes;
- startup handoff tests use a fake launcher context;
- platform OS calls stay behind the compile-time platform implementation until
  a target is available.

Smoke tests still matter after route-through changes, but the majority of
portability decisions can be covered with fast unit tests.

## Things To Avoid

- Do not make platform implementations runtime plugins.
- Do not put SDL, Win32, Android, Vita, or Switch SDK types in private generic
  headers.
- Do not expose C++ classes across `Host_Main`, game DLL, renderer, filesystem,
  or menu ABI boundaries.
- Do not migrate fatal error, restart, crash handler, or process replacement
  paths before there is explicit validation coverage.
- Do not duplicate library lookup rules between launcher, filesystem, and
  engine; define one locator helper and let each legacy facade adapt to it.
