# Modular C++ Rewrite Feasibility

## Short Answer

A full rewrite to a modern modular C++ architecture is technically possible, but
a big-bang rewrite is not a good first move. The safer path is an incremental
internal modernization that preserves the C ABI surfaces used by games,
clients, renderers, filesystem plugins, tooling, and supported platforms.

The repo already has useful module boundaries at build and DLL level. The main
maintenance problem is not absence of modules; it is that most module internals
are C-style singletons with broad shared headers, mutable global state, macro
feature switches, and cross-subsystem function access.

Recommended strategy: keep external headers and binary contracts C-compatible,
then migrate internals subsystem by subsystem to C++ translation units,
namespaces, small owning types, RAII cleanup, typed handles, and explicit
dependency objects.

## Feasibility Rating

| Scope | Feasibility | Risk | Recommendation |
| --- | --- | --- | --- |
| Internal C++ wrappers around selected subsystems | High | Low to Medium | Start here. |
| Filesystem backend modernization | High | Medium | Good pilot project. |
| Public utility library cleanup | High | Low | Good supporting work, but keep C APIs. |
| Renderer internal modularization | Medium | Medium to High | Do after test coverage and GL state mapping. |
| Client/server state encapsulation | Medium | High | Incremental only. |
| Full engine rewrite with new architecture | Low to Medium | Very High | Avoid until compatibility suite exists. |
| Public ABI redesign | Low | Very High | Avoid unless this fork intentionally drops compatibility. |

## Why A Full Rewrite Is Risky

### Compatibility Is The Product

This engine exists to run Half-Life/GoldSrc-compatible games and mods. The
important behavior is not just visible gameplay; it includes quirks, protocol
details, structure layouts, path lookup order, DLL loading names, cvar behavior,
network message formatting, save/restore expectations, and bug compatibility
flags.

### ABI Surfaces Are Broad

Stable C-style boundaries include:

- Game DLL exports and engine callbacks.
- Client DLL exports and engine callbacks.
- Renderer DLL API.
- Filesystem module API and Valve filesystem compatibility.
- Shared SDK structures in `common/`, `pm_shared/`, and `engine/*.h`.

C++ can improve internals, but changing these contracts would break the main
reason the engine is useful.

### State Is Global And Interdependent

Examples:

- `host` owns global frame timing, game state, command-line state, memory pools,
  feature flags, and paths.
- `cl` and `cls` span networking, prediction, rendering, downloads, DLL
  callbacks, screen state, and resources.
- `sv`, `svs`, and `svgame` span map lifecycle, clients, game DLL state,
  edicts, physics, resources, networking, and save/restore.
- Renderer and filesystem modules have their own global state and callback
  tables.

Moving this state all at once would create a long period where bugs are hard to
classify as behavior regressions, ABI breaks, or refactor mistakes.

### Platform Coverage Is Wide

The code supports desktop, mobile, console-like homebrew targets, and legacy
platforms. Build scripts and platform backends are part of the architecture.
Every modernization choice must account for compiler support, exception policy,
RTTI policy, dynamic library behavior, and C++ standard library availability.

## What C++ Can Improve

C++ is still useful here if applied as an internal implementation language:

- RAII for memory pools, files, dynamic libraries, renderer resources, and
  temporary state pushes.
- Namespaces to reduce prefix-only organization.
- Small classes around existing state blocks without changing the C ABI.
- Stronger typed handles for textures, models, sounds, files, entities, and
  network channels.
- Containers where fixed arrays are not ABI-visible.
- Explicit dependency injection for tests, especially filesystem, console,
  clock, and platform services.
- Backend interfaces matching existing function-table patterns.
- Better compile-time separation between public ABI headers and internal
  implementation headers.

## Best Pilot: Filesystem

The filesystem is the best first modernization target because it already has a
backend object shape:

- `searchpath_t` stores backend data plus function pointers.
- Concrete backends live in `dir.c`, `pak.c`, `wad.c`, `zip.c`, and
  `android.c`.
- `filesystem.c` centralizes search paths, path safety, gameinfo parsing, and
  file operations.
- `VFileSystem009.cpp` is already C++.

Possible pilot shape:

1. Keep `filesystem.h`, `filesystem_internal.h`, and exported C symbols stable.
2. Convert one backend implementation to C++ internally, for example directory
   search paths.
3. Introduce an internal `SearchPath` interface or tagged wrapper that maps back
   to existing `searchpath_t` callbacks.
4. Add focused tests around case-insensitive lookup, archive precedence, path
   normalization, direct path permission, and game hierarchy scanning.
5. Repeat for `pak`, `wad`, and `zip` only after behavior stays identical.

Success criteria:

- Existing filesystem tests pass.
- Engine starts and loads game data identically.
- Search path printout and lookup ordering remain stable.
- No public header requires C++ to include.

## Second Pilot: Public Utilities

`public/` is a good supporting target, especially string/path parsing and math
helpers. Keep exported C functions and tests, but internally add:

- More unit tests before refactor.
- Safer internal helper functions.
- Optional C++ test harnesses if the Waf setup allows it cleanly.

Avoid turning public headers into C++ headers. They are SDK-style dependencies.

## Later Target: Renderer Internals

Renderer modularization can pay off, but it is riskier:

- `gl_local.h` centralizes a large amount of renderer state.
- GL state caching and compatibility shims make behavior sensitive.
- Renderer APIs are externally consumed through C-style tables.

Possible C++ direction:

- Keep `ref_api.h` and exported renderer entry points stable.
- Encapsulate GL state cache in a `GlState` object.
- Encapsulate texture registry in a `TextureManager`.
- Encapsulate frame/view state in a `RenderFrame` or `ViewPass` object.
- Leave model and BSP structures C-compatible until test coverage is stronger.

Do this after a screenshot/rendering smoke-test harness exists.

## Later Target: Host, Client, And Server

These are high value but high risk. A sensible path is to wrap state before
moving behavior:

1. Define internal C++ facade types around existing globals:
   `HostContext`, `ClientContext`, `ServerContext`, `ServerGameContext`.
2. Move only initialization/shutdown ownership first.
3. Convert local helpers and private file-scope functions before exported or
   cross-module functions.
4. Keep old `CL_`, `SV_`, `Host_`, `Cmd_`, `Cvar_` entry points as wrappers.
5. Gradually reduce direct global access by passing context references through
   new internal code.

Do not start by changing protocol structs, edict layout, client frame layout, or
game DLL callback structures.

## Proposed Internal Module Model

External ABI remains C. Internal modules can evolve toward:

| Module | Internal C++ Shape | Existing Surface To Preserve |
| --- | --- | --- |
| Host | `HostContext`, `FrameClock`, `CommandLine`, `GameStateMachine` | `Host_*`, `host`, public callbacks. |
| Commands/Cvars | `CommandRegistry`, `CvarRegistry`, `CommandBuffer` | `Cmd_*`, `Cvar_*`, console behavior. |
| Memory | `MemoryPool`, `PoolAllocation` helpers | `Mem_*` macros/functions and pool handles. |
| Filesystem | `Filesystem`, `SearchPath`, backend classes | `FS_*`, filesystem API table, `VFileSystem009`. |
| Networking | `NetAddress`, `NetChannel`, `PacketBuffer` | Protocol structs and `NET_*`/`Netchan_*`. |
| Client | `ClientContext`, `Prediction`, `DownloadManager` | Client DLL ABI and `CL_*`. |
| Server | `ServerContext`, `ClientSession`, `GameDll` | Game DLL ABI and `SV_*`. |
| Renderer | `Renderer`, `GlState`, `TextureManager`, `RenderFrame` | Renderer DLL exports and ref API tables. |
| Platform | `PlatformServices`, per-backend implementations | Backend macros and existing platform entry points. |

## Migration Sequence

### Phase 0: Baseline

- Build the current branch with tests.
- Generate compile database.
- Add smoke-test instructions for engine startup and at least one known game
  data set if available.
- Capture current `waf configure` options for target platforms.

### Phase 1: Documentation And Tests

- Expand this folder with subsystem-specific notes.
- Add more filesystem and public utility tests before changing internals.
- Identify functions with heavy TODO/FIXME/HACK compatibility behavior and mark
  them as behavior-sensitive.

### Phase 2: C++ Build Hygiene

- Confirm C++ standard and exception/RTTI policy per platform.
- Add a tiny first-party C++ internal target or file where it already makes
  sense.
- Make sure C headers remain includable from C and C++.

### Phase 3: Filesystem Pilot

- Convert one backend behind stable callbacks.
- Keep all exported symbols and behavior stable.
- Add regression tests for lookup, ordering, and path safety.

### Phase 4: Context Facades

- Add C++ facades around existing globals without moving storage yet.
- Introduce internal helper APIs that accept context references.
- Convert low-risk files that already have narrow responsibilities.

### Phase 5: Renderer And Runtime State

- Encapsulate GL state and texture registry.
- Add render smoke tests or golden screenshot checks where possible.
- Begin host/client/server state movement only after smaller pilots prove the
  build and testing workflow.

## Guardrails

- No public ABI break unless the fork explicitly decides to drop compatibility.
- No C++ exceptions across C ABI boundaries.
- No STL types in public C headers.
- No behavior-preserving refactor without tests or a manual verification recipe.
- No vendored dependency cleanup unless it removes an integration bug.
- Keep Waf and Android/CI integration green at every phase.
- Prefer small PRs by subsystem over cross-cutting style rewrites.

## Early Wins

- Add subsystem ownership docs under `Documentation/codex/`.
- Generate and commit a module dependency graph from Waf target relationships.
- Add more tests around `public/crtlib.c` path handling and `filesystem/`
  search-path behavior.
- Convert local-only helper structs to explicit internal types where no ABI is
  exposed.
- Wrap dynamic library handles and file handles with cleanup helpers inside
  implementation files.

## Decision

Proceed with incremental modularization, not a full rewrite. C++ should be used
as a tool for internal clarity and ownership while the engine's C-compatible ABI
continues to define the project boundary.
