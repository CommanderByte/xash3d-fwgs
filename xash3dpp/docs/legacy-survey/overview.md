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
1. **Function-pointer plugin ABIs.** Filesystem (`fs_api_t`), renderer (`ref_api.h`), game DLL (`enginefuncs_t`) all use large flat callback tables with no version negotiation.
1. **`#ifdef` platform soup.** `engine/platform/` should be an abstraction layer but most platform code is conditionally compiled inline across the codebase.
1. **Implicit ordering.** Cvar/command registration, frame-loop phase order, search-path mount order, edict free-list — all order-dependent with no explicit lifecycle model.
1. **Replication of physics.** `pm_shared/` is run on both client (prediction) and server (authority). Any divergence breaks netplay.
1. **Bug-compat behaviour.** GoldSrc quirks (lightmap math, rendering hacks, save format, WAD nesting) are deliberately preserved and undocumented in code.

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

## As-built synthesis (2026-07-06)

The section above surveys the **legacy** engine. This section synthesises the
**as-built rewrite** after the 13-subsystem documentation refresh
(utilities → memory → filesystem → platform → core → cmd_cvar → host → abi →
launcher → map_loader → networking → content → server). It collates the
cross-cutting flags each per-subsystem pass recorded into one map. Each row
points at the refreshed per-subsystem docs where the finding lives; candidate
follow-up work is enumerated (unscheduled) in the **Harmonization backlog** at
the end of `docs/implementation-plan.md`.

The themes fall into five classes: **SEC** (security), **INVARIANT**
(do-not-modernize), **P-8** (thread-role conformance), **DOOR** (extension
infrastructure per `design/extension-goals.md`), and **HOUSE** (housekeeping /
drift).

| # | Theme | Class | Surfaced in (subsystems) | Primary docs | Synthesis |
| --- | --- | --- | --- | --- | --- |
| T-1 | `string_view.data()` → C-string `strnicmp`/`strncmp` NUL-over-read | **SEC** | utilities (M-4), filesystem (M-7), cmd_cvar (M-5, 6 sites) — **verified absent** in the other 9 subsystems checked; memory is N/A (no string-compare sites) | `modernization-opportunities/{utilities,cmd_cvar}-modernization.md` + `filesystem-modernization.md` | Latent OWASP buffer-over-read (in-tree callers pass NUL-terminated literals). One bounded `ci_compare(string_view, string_view)` in `utilities`, adopted by all three call sites, closes the class. |
| T-2 | Tree-wide float/byte-**EXACT** "do-not-modernize" set | **INVARIANT** | map_loader (Q-18 trace/PVS/CRC), content (studio bone math), networking (wire bit-codec/delta widths/LZSS/OOB), server (rotated-brush ULP `clip.cpp:216`), utilities (double-precision studio math) | `modernization-opportunities/{map_loader,networking,content,server}-modernization.md`, deep-dive-{trace-pvs,delta-encoder,content}.md | Five subsystems independently flagged the same prohibition (no FMA/reassoc/ranges rewrite). Name the union in one authoritative place so no future "modernization" touches it. |
| T-3 | P-8 `assert_thread_role` under-guard gaps | **P-8** | host (`RunFrame`/`RequestShutdown`/`signal_frame_abort` unguarded vs header claim), map_loader (`new_game`/`change_level`/`clear_world` transitions) | `threading-analysis/{host,map_loader}-threading.md` | Transition/frame-entry points under-guarded — the exact spots where "marshal back to Main" is anchored. Server is the clean reference (92 uniform asserts). A P-8 conformance sweep closes it (Chunk 6B territory). |
| T-4 | Missing thread-spawn + `ThreadRole`-register primitive | **DOOR** (P-1) | core (owns `ThreadRole` + `assert_thread_role`), platform (hosts `thread_role.cpp`, no spawn), host (owns P-1 inbox-drain slot in `RunFrame`), launcher (pure Main provider) | `boundaries/{platform,core,host}-boundary.md`, `threading-analysis/host-threading.md` | Every north-star off-main thread (G-1 MCP listener, G-2 NetIO, G-3 debug thread, P-1 worker pool) needs this one primitive. Design the spawn + inbox as a single Chunk-7 P-1 unit; platform is the natural owner. |
| T-5 | P-2 published-snapshot pattern | **DOOR** (P-2) | map_loader (immutable `WorldData` = reference Safe-RO surface), content (model cache needs snapshot swap), server (needs G-1/G-3 snapshot publish), networking (publishes counters not state) | `boundaries/{map_loader,content,server,networking}-boundary.md` | map_loader's immutable snapshot is the zero-machinery reference. One shared P-2 snapshot/double-buffer idiom serves the rest. |
| T-6 | "One introspection layer" (P-4) | **DOOR** (P-4) | core (substrate: logs, `Clock::stats`, `error_code_name`), memory (`get_stats`/`for_each_pool`), content (`model_infos`/`StudioView`), map_loader (world queries), server (`EntityView`) | `boundaries/core-boundary.md` + each subsystem boundary | These are the channels of a single typed introspection layer feeding G-1/G-3/G-4. Name them as one layer so no frontend grows a private backdoor (P-4 door rule). |
| T-7 | Shared aligned-allocation door | **DOOR** (P-7/G-5) | memory (H-1: lift `alignof(T) ≤ 8`) | `modernization-opportunities/memory-modernization.md`, `boundaries/memory-boundary.md` | H-1 simultaneously serves P-7 over-aligned `pool_new`, the G-5 script-allocator bridge, and the future `IAllocatorBackend` seam (strategy fn-ptr triple). One design brief should cover all three. |
| T-8 | `xash3dpp_miniz` shared static target | **HOUSE** | filesystem + content (link it PRIVATE), utilities (does **not** own it) | `boundaries/{filesystem,content}-boundary.md`, deep-dive-{filesystem,content}.md | Built from `../public/miniz.c`; a cross-subsystem ownership fact recorded so it is not mis-attributed to utilities. |
| T-9 | Enforcement-free-by-role subsystems | **HOUSE** | abi (C-ABI shim, 0 asserts by design), launcher (pure Main provider, 0 asserts) | `threading-analysis/abi-threading.md`, `boundaries/{abi,launcher}-boundary.md` | The deliberate 0-`assert_thread_role` counterpoint to consumer subsystems — a P-8 sweep (T-3) must not misflag them. |
| T-10 | Status / plan drift | **HOUSE** | content (status_table Complete vs plan Partial), server (Chunk 6 Complete), core (cvar-name drift) | `boundaries/{content,core}-boundary.md`, `implementation-plan.md` | Plan status rows lag `status_table.py`. Core cvar drift (`host_maxfps` vs legacy `fps_max`, `host_sleeptime` vs `sleeptime`; `sys_timescale` FCVAR_CHEAT vs legacy FCVAR_FILTERABLE) is a GoldSrc config-compat gap. |
| T-11 | Unfinished code inside "Complete" subsystems | **HOUSE** | utilities (`gameinfo_parser` stubs, `swap_struct` undefined), server (`ITrustOracle`/`ICompatPolicy` Chunk 7, save paths Chunk 8), networking (DNS + bz2 deferred), cmd_cvar (`$`-subst / `if`-`else` / stuffcmd-filter / `base_cmd` sorted autocomplete), launcher (Optimus/PowerXpress `dllexport` port gap) | each subsystem boundary + `stub_scan.py` | Chunk-inherited deferrals inside structurally-Complete subsystems; each already carries an inline marker. Enumerated so no "Complete" label is read as "nothing left". |
| T-12 | RNG-unification stub | **HOUSE** | server (`s_rng_state`/`s_pm_rng`), plan (`COM_RandomLong`/`Float` xorshift stubs in `engine_table.cpp` + `init_client_move.cpp`) | `boundaries/server-boundary.md`, `implementation-plan.md` deferred inventory | Multiple deterministic-RNG stubs await one shared idtech RNG stream. |

**Reading pointers** — the refreshed per-subsystem material lives in four
folders: `boundaries/` (interface + owned-state + Q-21 extension axes),
`threading-analysis/` (assert-role sites + statics), `modernization-
opportunities/` (as-built Implemented/Remaining status), and this
`legacy-survey/` folder (`deep-dive-*.md` recon). The extension-door language
(G-/P- IDs) is defined in `design/extension-goals.md`; the decision register
(Q-IDs) in `design/decisions-architecture.md`.
