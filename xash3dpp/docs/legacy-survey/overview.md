# Legacy Engine — Cross-Cutting Overview

This document synthesises the per-area summaries into a single map of how the
legacy Xash3D FWGS engine fits together, what the **frozen boundaries** are,
and where the rewrite has the most freedom.

## Subsystem Dependency Graph

```text
                  ┌─────────────────────┐
                  │   game_launch/      │  (loads engine binary)
                  │   android/          │
                  └──────────┬──────────┘
                             │
                  ┌──────────▼──────────┐
                  │   engine/common     │  host loop, cmd/cvar,
                  │   + platform/       │  net stack, mem, OS abstraction
                  └─┬──────┬──────────┬─┘
                    │      │          │
         ┌──────────┘      │          └─────────┐
         │                 │                    │
  ┌──────▼──────┐   ┌──────▼──────┐    ┌────────▼────────┐
  │ engine/     │   │ engine/     │    │   filesystem/   │ (plugin)
  │ client      │◄─▶│ server      │    │   ref/          │ (plugin)
  └──────┬──────┘   └──────┬──────┘    └─────────────────┘
         │                 │
         │ cdll_int.h      │ eiface.h
         │ cdll_exp.h      │ edict.h
         ▼                 ▼
  ┌──────────────┐  ┌──────────────┐
  │  Client DLL  │  │  Game DLL    │   ← external SDK consumers
  │  (e.g. hl)   │  │  (e.g. hl)   │     FROZEN ABI
  └──────────────┘  └──────────────┘

  Shared by everyone (headers-only):
    common/      SDK structures (FROZEN)
    pm_shared/   player movement (FROZEN client⇄server)
    public/      utility library (free to rewrite behind C facade)
```

## ABI Boundaries

| Boundary | Headers | Status |
| --- | --- | --- |
| **Game DLL ABI** | `engine/eiface.h`, `engine/edict.h`, `common/entity_state.h`, `common/const.h`, `pm_shared/*` | **FROZEN** — external SDK |
| **Client DLL ABI** | `engine/cdll_int.h`, `engine/cdll_exp.h`, `common/cl_entity.h`, `common/event_args.h`, `common/ref_params.h` | **FROZEN** — external SDK |
| Renderer plugin | `engine/ref_api.h` | Internal — free to redesign |
| Filesystem plugin | `filesystem/filesystem.h` (`fs_api_t`) | Internal — free to redesign |
| Engine ↔ MainUI/VGUI | `engine/menu_int.h`, VGUI bridge | Internal — free to redesign |

## Where Global State Lives

A short census of the heaviest globals — these are the chokepoints any rewrite has to deal with explicitly:

| Owner | Subsystem | Holds |
| --- | --- | --- |
| `host` (`host_parm_t`) | engine/common | timing, game state, mempools, feature flags, config, frame count |
| `cl`, `cls`, `clgame`, `gameui` | engine/client | per-level state, connection, demo, client DLL, MainUI |
| `sv`, `svs`, `svgame` | engine/server | frame state, persistent slots, game DLL handle + edict array |
| `tr`, `glState`, `RI` | ref/ | render globals, GL context, per-frame state |
| Filesystem search paths | filesystem/ | mounted archives, search order |

## Cross-Cutting Pain Points

The following themes appear in **most** subsystem summaries — they are the things the rewrite must address structurally rather than locally.

1. **Monolithic singletons.** `host`, `cl/cls/clgame`, `sv/svs/svgame`, `tr/glState/RI` — each is a god-object that every part of its subsystem touches.
2. **Function-pointer plugin ABIs.** Filesystem (`fs_api_t`), renderer (`ref_api.h`), game DLL (`enginefuncs_t`) all use large flat callback tables with no version negotiation.
3. **`#ifdef` platform soup.** `engine/platform/` should be an abstraction layer but most platform code is conditionally compiled inline across the codebase.
4. **Implicit ordering.** Cvar/command registration, frame-loop phase order, search-path mount order, edict free-list — all order-dependent with no explicit lifecycle model.
5. **Replication of physics.** `pm_shared/` is run on both client (prediction) and server (authority). Any divergence breaks netplay.
6. **Bug-compat behaviour.** GoldSrc quirks (lightmap math, rendering hacks, save format, WAD nesting) are deliberately preserved and undocumented in code.

## Strategic Boundaries for the Rewrite

These are observations from the survey — not architectural commitments. They are the natural seams visible in the legacy code.

- **Filesystem** is the most isolated subsystem (no engine dependencies; clean plugin entry). Best candidate for an early standalone target if a pilot is desired.
- **`public/`** can be rewritten freely in C++ behind a C-shaped facade because the SDK ABI does not touch its implementation.
- **`pm_shared/`** has a hard determinism requirement that should drive an early decision about float vs. fixed-point.
- **Renderer ABI** is fully internal — the rewrite can replace `ref_api.h` with whatever shape suits a modern backend (Vulkan/Metal/D3D12), provided the on-screen output for GoldSrc content matches.
- **Server-side `entvars_t` and the `enginefuncs_t` table** are the hardest external constraint: the rewrite must produce a memory layout and call table that existing game DLLs accept unchanged.
- **Engine/common's host singleton** has no external visibility — internally it can be split into separate services (timing, frame scheduler, cvar registry, command bus, memory) however the rewrite likes.
- **Client and Server** share substantial physics and entity-state code via `pm_shared/` + `common/` headers; the rewrite needs a clear story for where the shared simulation lives.

## Read Next

- Per-area summaries: see each `*.md` in this folder
- For deeper analysis of a specific subsystem, run `/analyse-subsystem <name>` to produce a boundary spec
