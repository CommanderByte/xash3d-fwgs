# Codebase Map

This document maps the repository as inspected on the
`codex-repo-onboarding-modular-rewrite` branch. It focuses on first-party
structure and modernization-relevant boundaries, not a line-by-line inventory of
vendored dependency internals.

## Scale And Composition

Top-level file counts from `rg --files`:

| Path | Files | Role |
| --- | ---: | --- |
| `3rdparty/` | 2065 | Bundled dependency and optional module sources. |
| `engine/` | 276 | Main engine, host, client, server, platform code, ABI headers. |
| `android/` | 83 | Android app wrapper and native build integration. |
| `ref/` | 62 | Renderer implementations and shared renderer support. |
| `scripts/` | 53 | CI, release, cross-compile, packaging, Waf helpers. |
| `Documentation/` | 43 | User, development, protocol, extension docs. |
| `common/` | 39 | Public SDK/game-facing data structures. |
| `public/` | 29 | Portable utility library, math/string/CRC/miniz/build metadata. |
| `utils/` | 18 | Developer utilities and fuzz runner. |
| `filesystem/` | 16 | Virtual filesystem module. |
| `game_launch/` | 5 | Native launcher. |
| `pm_shared/` | 2 | Shared player movement definitions. |

First-party source type counts excluding `3rdparty/` are mostly C:

| Extension | Files |
| --- | ---: |
| `.c` | 256 |
| `.h` | 154 |
| `.kt` | 16 |
| `.py` | 15 |
| `.cpp` | 3 |
| `.java` | 3 |

The engine is therefore not just "C flavored"; its central architecture is C:
shared headers, global subsystem state, function tables, macro-configured
platform paths, and dynamically loaded DLL-style modules.

## Build System

The root `wscript` is the build orchestrator. It registers subprojects, global
feature flags, renderers, utility builds, tests, platform options, and bundled
dependency choices.

Important build facts:

- Waf is the native build system.
- `public/` and `filesystem/` are always configured and built.
- `engine/` is built last among first-party core modules because it links the
  configured static support libraries and optional renderer/client pieces.
- Renderer DLLs are selected through root-level renderer options and `RefDll`
  entries: software, OpenGL, GLES1, GLES2, GL4ES, GLES3 compatibility, null.
- Client builds pull in `engine/client/**/*.c`, SDL platform code, audio codec
  libraries, renderers, and optional UI support.
- Dedicated builds use common, server, and platform code without client modules.
- Tests are enabled with `--enable-tests` and run with `./waf --alltests`.

Key build outputs:

| Target | Source | Meaning |
| --- | --- | --- |
| `xash` | `engine/wscript` | Main engine executable or shared library, depending on platform and launcher options. |
| `xash_tests` | `engine/wscript` | Client-capable engine test binary when tests are enabled. |
| `xash_tests_dedicated` | `engine/wscript` | Dedicated-server engine test binary when tests are enabled. |
| `filesystem_stdio` | `filesystem/wscript` | Filesystem shared library/module. |
| `public` | `public/wscript` | Static utility library. |
| `build_vcs` | `public/wscript` | Build metadata static library. |
| `ref_common` | `ref/common/wscript` | Shared renderer support library. |
| `ref_gl`, `ref_gles1`, `ref_gles2`, `ref_gl4es`, `ref_gles3compat` | `ref/gl/wscript` | GL-family renderer shared libraries. |
| `ref_soft` | `ref/soft/wscript` | Software renderer shared library. |
| `ref_null` | `ref/null/wscript` | Null renderer shared library. |
| `game_launch` executable | `game_launch/wscript` | Launcher when engine is built as a library. |

Android uses `android/app/build.gradle.kts` to call a native Ninja/Waf bridge via
`scripts/configure-ninja.py`. CI lives in `.github/workflows/c-cpp.yml` and
builds Linux, Android, Nintendo Switch, PSVita, Windows, macOS, iOS, and Flatpak
artifacts through `scripts/gha/*`.

## Top-Level Directory Roles

### `engine/`

Main first-party engine implementation.

Approximate line count by subarea:

| Area | Files | Lines | Role |
| --- | ---: | ---: | --- |
| `engine/client` | 91 | 51452 | Client networking, prediction, screen/UI, audio, renderer bridge, client DLL interface. |
| `engine/common` | 72 | 35365 | Host loop, command/cvar system, memory pools, filesystem bridge, networking, models, protocol, common services. |
| `engine/server` | 16 | 19571 | Server lifecycle, game DLL integration, physics, networking, saves, entity/world management. |
| `engine/platform` | 62 | 11495 | OS, video, input, sound, timing, dynamic library, crash handling, console backends. |

Important headers:

- `engine/common/common.h`: central engine header, `host_parm_t`, global host
  state, memory macros, command/cvar declarations, filesystem bridge, model and
  image interfaces.
- `engine/client/client.h`: central client state, `client_t`,
  `client_static_t`, client DLL bridge, renderer/audio interfaces, prediction,
  screen and UI state.
- `engine/server/server.h`: server state, `server_t`, `server_static_t`,
  `sv_client_t`, `svgame_static_t`, game DLL and physics interfaces.
- `engine/ref_api.h`: engine to renderer ABI.
- `engine/eiface.h`, `engine/cdll_int.h`, `engine/menu_int.h`,
  `engine/physint.h`: engine-facing ABI contracts for game/client/menu/physics
  modules.

The engine relies on global singleton-like state:

- `host` in `engine/common/host.c`.
- `cl` and `cls` in `engine/client/cl_main.c`.
- `sv`, `svs`, and `svgame` in `engine/server/sv_init.c`.
- `g_fsapi` and filesystem globals in `engine/common/filesystem_engine.c`.

### `engine/common/`

Shared host/client/server services.

Notable files:

- `host.c`, `host_state.c`: global host lifecycle and game state transitions.
- `common.c`: common engine services and initialization glue.
- `cmd.c`, `base_cmd.c`, `cfgscript.c`: command buffer, console commands, config
  script execution.
- `cvar.c`: console variable registry and mutation.
- `zone.c`: engine memory pool allocator and allocation diagnostics.
- `filesystem_engine.c`: bridge from engine memory/logging/system callbacks to
  the filesystem module.
- `net_ws.c`, `net_chan.c`, `net_buffer.c`, `net_encode.c`: networking,
  channels, packet buffers, and protocol encoding.
- `model.c`, `mod_bmodel.c`, `mod_studio.c`, `mod_sprite.c`, `mod_alias.c`:
  model loading and in-memory model representations.
- `world.c`, `pm_trace.c`, `pm_surface.c`: collision/world queries and movement
  support.
- `system.c`, `sys_con.c`, `lib_common.c`: common platform abstraction pieces.
- `imagelib/`, `soundlib/`, `http/`: shared image, sound, and HTTP helpers.

### `engine/client/`

Client-side runtime, local prediction, renderer interaction, UI, and client DLL
integration.

Notable files:

- `cl_main.c`: primary client lifecycle and global client state.
- `cl_frame.c`: server snapshot/frame handling.
- `cl_cmds.c`: client commands.
- `cl_demo.c`: demo recording/playback.
- `cl_events.c`, `cl_efx.c`, `cl_tent.c`: event and temporary entity systems.
- `cl_view.c`, `cl_scrn.c`, `console.c`, `titles.c`: screen, console, text, and
  view state.
- `vid_common.c`: video mode and renderer coordination.
- `sound/`, `soundlib/`: client audio backend and decoding/mixing support.
- `input/`: input binding and user command creation.
- `dll_int/`: client DLL, renderer, menu, and exported callback integration.
- `vgui/`: VGUI support integration.
- `avi/`: video/cinematic support.
- `parse/`: client-side parsers.

### `engine/server/`

Server runtime and game DLL integration.

Notable files:

- `sv_init.c`: server startup/shutdown and persistent server globals.
- `sv_main.c`: main server frame/network logic and operator commands.
- `sv_frame.c`: per-frame server simulation.
- `sv_client.c`: client connection, command, and state handling.
- `sv_game.c`: game DLL load/export bridge.
- `sv_phys.c`, `sv_move.c`, `sv_pmove.c`, `sv_world.c`: physics, movement, and
  world/entity queries.
- `sv_save.c`: save/restore.
- `sv_query.c`: server query responses and discovery.
- `sv_log.c`, `sv_filter.c`, `sv_custom.c`, `sv_cmds.c`: logging, filtering,
  customization, and server commands.

### `engine/platform/`

Platform-specific implementations selected by Waf and compile-time macros.

Subdirectories include `win32`, `posix`, `linux`, `android`, `sdl1`, `sdl2`,
`sdl3`, `ios`, `psvita`, `nswitch`, `dos`, `irix`, `misc`, and `stub`.

Expected responsibilities:

- OS process and system lifecycle.
- Dynamic library loading.
- Console and crash handling.
- Video, input, sound, joystick, and sensor backends.
- Platform-specific filesystem/native object hooks.

### `filesystem/`

Virtual filesystem module built as `filesystem_stdio`.

Key characteristics:

- `filesystem.c` is the largest file and handles initialization, gameinfo
  parsing, search path management, path safety, file operations, memory setup,
  and the exported interface.
- `filesystem_internal.h` defines the internal object model: `file_t`,
  `searchpath_t`, `dir_t`, `pack_t`, `wfile_t`, `zip_t`, and Android assets.
- `dir.c`, `pak.c`, `wad.c`, `zip.c`, `android.c` implement concrete search
  path/archive backends through function pointers stored in `searchpath_t`.
- `VFileSystem009.cpp` and `.h` provide a compatibility C++ surface for Valve
  style filesystem APIs.

The module is already closer to an object model than much of the engine because
`searchpath_t` dispatches operations through backend-specific function tables.
That makes it a good modernization candidate.

### `ref/`

Renderer modules and shared renderer support.

Approximate line count:

| Area | Files | Lines | Role |
| --- | ---: | ---: | --- |
| `ref/gl` | 24 | 21571 | GL-family renderers and GL/GLES abstraction. |
| `ref/soft` | 22 | 16511 | Software renderer. |
| `ref/common` | 5 | 946 | Shared renderer math/light/context utilities. |
| `ref/null` | 1 | 346 | Null renderer. |

Important GL files:

- `gl_local.h`: central renderer state and declarations.
- `gl_context.c`: renderer API table fill, map/model texture lifecycle, current
  entity/model state, display transform.
- `gl_backend.c`: GL state management, texture units, screenshots, timing.
- `gl_opengl.c`: GL loading/configuration globals.
- `gl_rmain.c`, `gl_rsurf.c`, `gl_studio.c`, `gl_alias.c`, `gl_sprite.c`:
  world, surface, studio model, alias model, and sprite rendering.
- `gl_image.c`: texture/image upload and management.
- `gl_decals.c`, `gl_beams.c`, `gl_rpart.c`, `gl_warp.c`: special effects.
- `gl2_shim/`, `vgl_shim/`: compatibility shims for specific GL targets.

### `public/`

Portable utility library and shared helpers.

Notable files:

- `crtlib.c/.h`: project string/path/parse utility functions.
- `xash3d_mathlib.c/.h`, `matrixlib.c`: math utilities.
- `crclib.c/.h`: CRC and hashing support.
- `miniz.c/.h`: bundled compression implementation.
- `utflib.c/.h`: UTF helpers.
- `build.c`, `build_vcs.c`: build metadata.
- `dllhelpers.c`, `getopt.c`: dynamic library/argument helpers.
- `tests/`: standalone unit tests for public helpers.

### `common/` And `pm_shared/`

These are mostly shared SDK-style headers. They define stable data contracts
used between engine, renderer, game DLL, client DLL, movement code, and tooling:
entities, protocol structures, player movement, rendering APIs, sound APIs,
model formats, cvars, and build/platform types.

Any rewrite must treat this area as an ABI compatibility zone.

### `3rdparty/`

Bundled dependencies and optional modules. Examples include audio codecs,
renderer shims, VGUI support, `mainui`, `libbacktrace`, `library_suffix`,
`gl4es`, `nanogl`, `gl-wes-v2`, `bzip2`, and extras/assets.

Modernization should avoid refactoring vendored code unless there is a narrow
integration reason. Prefer wrapping dependency boundaries instead.

### `android/`

Android app wrapper and Gradle project.

Responsibilities:

- App UI in Kotlin/Java.
- Game/library selection and settings screens.
- Native engine build bridge through Gradle external native build properties.
- APK packaging and native library handling.

### `scripts/`

Build, release, packaging, and helper scripts.

Important clusters:

- `scripts/gha/`: CI dependency install and per-platform build scripts.
- `scripts/waifulib/`: Waf extension/helper modules.
- `scripts/configure-ninja.py`, `scripts/build-ninja.py`: Ninja/native build
  integration.
- `scripts/flatpak/`, `scripts/ios/`, `scripts/sailfish/`: platform packaging.

## Runtime Boundary Sketch

The common runtime shape is:

1. Platform entry point starts the engine or launcher.
2. Host initialization creates memory pools, cvars, commands, filesystem, and
   platform services.
3. The filesystem module resolves game directory, base directory, archives, and
   library paths.
4. Server and client subsystems load game/client/menu DLLs through ABI tables.
5. Renderer DLL is loaded and receives engine-provided API structures.
6. The host frame runs input, networking, server simulation, client prediction,
   audio, rendering, and command/cvar processing.
7. Shutdown unwinds DLLs, renderer, filesystem, memory pools, and platform
   services.

Key compatibility boundaries:

- Engine to game DLL: `eiface.h`, `progdefs.h`, `edict.h`, `physint.h`.
- Engine to client DLL: `cdll_int.h`, `cdll_exp.h`, client callback tables.
- Engine to renderer DLL: `ref_api.h`, `render_api.h`, `triangleapi.h`.
- Engine to filesystem module: `filesystem/filesystem.h`,
  `filesystem/VFileSystem009.h`, engine callback tables.
- Engine to platform backend: `engine/platform/platform.h`, backend macros, SDL
  or OS-specific implementations.

## Modernization-Relevant Observations

- The code already has module boundaries at dynamic-library and Waf target
  level, but the inside of each module is largely global-state C.
- The most consequential globals are `host`, `cl`, `cls`, `sv`, `svs`,
  `svgame`, renderer globals, and filesystem globals.
- Many source files are organized by subsystem prefix (`CL_`, `SV_`, `FS_`,
  `R_`, `GL_`, `Cmd_`, `Cvar_`, `NET_`), which gives a natural map for
  extracting namespaces or class facades.
- ABI headers in `common/`, `engine/*.h`, and `filesystem/*.h` should remain C
  compatible even if internals move to C++.
- The filesystem backend dispatch table is already a strong candidate for C++
  interface-style modernization because `searchpath_t` acts like a vtable.
- Renderer code has large shared mutable state and GL state caching. It is a
  possible C++ target, but it should come after lower-risk state encapsulation.
- Server/client state is tightly coupled to protocol compatibility, prediction,
  and external DLL contracts. Big-bang rewrites here are high risk.

## Test And CI Surface

Known local test path:

- Configure with `--enable-tests`.
- Run `./waf --alltests`.

Test targets include:

- Public utility tests in `public/tests/`.
- Filesystem tests in `filesystem/tests/`.
- Engine test binaries `xash_tests` and `xash_tests_dedicated`.

CI uses `.github/workflows/c-cpp.yml` and platform scripts in `scripts/gha/`.
The CI matrix is broad and should be considered part of the compatibility
contract for any refactor.
