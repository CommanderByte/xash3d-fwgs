# Phase 50 Structure Audit

This audit is a checkpoint after the first fifty modernization phases. Its
purpose is to reset direction: identify what is now genuinely modern, what is
still legacy-owned, and which next steps give the project a coherent migration
path instead of another set of disconnected helper extractions.

## Current Shape

The modern tree is real now, but it is not evenly distributed.

| Area | Files | Code files | Approximate lines | Status |
| --- | ---: | ---: | ---: | --- |
| `src/filesystem` | 40 | 39 | 9,370 | Most advanced migration lane; still has compatibility mass. |
| `src/engine` | 21 | 12 | 1,558 | Several tested islands, not yet a broad engine architecture. |
| `src/utilities` | 17 | 16 | 960 | Useful shared helpers for hash, checksum, paths, text, atlas, build numbers. |
| `src/debugging` | 5 | 4 | 722 | Usable first-pass debug/log/trace/snapshot utilities. |
| `src/launcher` | 11 | 10 | 919 | Mature modular launcher lane; old `game_launch/` wrapper removed. |
| `engine` | 282 | 269 | 117,904 | Still the main legacy body of the engine. |
| `filesystem` | 7 | 5 | 3,223 | Now mostly export/facade and temporary compatibility wrappers. |
| `public` | 31 | 30 | 12,010 | Public C ABI plus several remaining helper islands. |
| `ref` | 62 | 52 | 39,028 | Renderer work remains essentially untouched. |

The important interpretation is not just file count. The modern code has good
patterns now: private C++ implementations, C adapters where ABI requires them,
focused tests, and smoke tests after runtime-sensitive changes. The risk is
that continued work on small isolated helpers can make progress feel busy
without moving a whole subsystem closer to modern ownership.

## Modern Ownership

`src/filesystem` is the strongest proof that the migration strategy works.
Archive backends, path policy, filesystem state, game hierarchy, file handle
helpers, search result helpers, and bridge utilities are all represented in
the modern tree. The remaining legacy filesystem folder is no longer the
center of design, but it still owns exports and compatibility shims.

`src/launcher` is similarly clean: platform entry, environment, library loading,
settings, and optional config are in the modern layout, with assets moved under
`resources/launcher`.

`src/engine` has good first slices:

- `commands`: base command registry and command buffer.
- `console`: system console backend and message formatting.
- `filesystem`: engine-side mount flag policy.
- `network`: byte buffer primitives.
- `platform`: command-line and current-user helpers.

These are useful foundations, but they are not yet enough to make the engine
feel modular. The next phase should choose a single engine lane with visible
runtime behavior and carry it forward for several phases.

## Legacy Pressure Points

The largest remaining pressure is still `engine/`, especially:

| Legacy area | Approximate lines | Migration pressure |
| --- | ---: | --- |
| `engine/client` | 51,452 | High, but tied to rendering, UI console, client state, and assets. |
| `engine/common` | 35,353 | Central runtime services; good source of narrow candidates. |
| `engine/server` | 19,571 | Strong next lane: gameplay-visible, testable, less renderer-coupled. |
| `engine/platform` | 11,528 | Needs non-Windows validation phases before aggressive migration. |

The renderer under `ref/` is large enough to deserve a dedicated renderer
audit. It should not be casually mixed into engine/common cleanup.

The public folder has been improved, but it still contains ABI headers,
fallback code, and license-sensitive source islands. Further movement there
should wait for precise ownership and the licensing audit rather than a broad
cleanup sweep.

## Recommended Direction

The best next lane is server-side policy and response helpers, starting with
`engine/server/sv_filter.c`.

Reasons:

- It is smaller and more self-contained than host lifecycle, renderer, savegame,
  world physics, or client console work.
- It already has embedded behavior tests for address and ID filtering.
- It uses systems we recently improved: command handling, parsing utilities,
  network address formatting, and server logging boundaries.
- It has real user-facing behavior without forcing a renderer or game DLL
  rewrite.
- It can be migrated in the same shadow/adapter style as previous successful
  phases.

The second good candidate is `engine/server/sv_query.c`: source-query response
building can be tested with golden byte payloads and should benefit from the
modern network buffer primitives.

Server logging is tempting, but it should wait until console/log routing has a
clearer sink model. Save/restore, physics, world, and model loading are
important but too broad for the immediate next step.

## Suggested Phase Queue

1. Phase 51: milestone audit and roadmap reset.
2. Phase 52: server boundary audit.
3. Phase 53: server filter pilot.
4. Phase 54: server query response builder.
5. Phase 55: server logging boundary or rendered console sink, depending on
   what the server audit reveals.

## Not Next

Do not make these the next active lane unless a specific bug forces it:

- broad memory allocator replacement;
- renderer rewrite or GL/soft renderer refactor;
- savegame migration;
- rendered in-game console integration;
- host lifecycle rewrite;
- public-folder cleanup without license attribution work.

These are not bad targets. They are simply expensive enough to need their own
audit and acceptance criteria first.

## Test Strategy For The Next Lane

For server filters:

- extract pure address and ID policy into `src/engine/server`;
- preserve existing C command surfaces and save-file behavior through adapters;
- add focused unit tests under `tests/engine`;
- keep existing embedded legacy tests passing;
- run `.\waf.bat build --alltests`;
- smoke with a dedicated/server-style launch where practical.

For source-query responses:

- create golden payload fixtures for rules, players, info, and failure cases;
- verify byte-for-byte output where legacy behavior is stable;
- use modern network buffer helpers without changing the external query ABI.

## Documentation Hygiene Notes

`Documentation/codex/codebase-map.md` remains useful as an orientation map, but
its exact counts should be treated as historical. This audit is the current
Phase 50 source-structure snapshot.

The active TODO folder should stay small. Future-only items such as memory pool
replacement, POSIX validation, and license compliance are correctly parked in
high-numbered phases or dedicated TODO documents.
