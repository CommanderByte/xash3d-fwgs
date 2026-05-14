# Module Cleanup Checkpoint

Phase 166 closes the cleanup lane that started after the Phase 160
domain-consolidation checkpoint. The goal was to reduce ceremony after the
server helper layer had accumulated many small files, not to hide legacy
ownership behind a broad facade.

## Count Snapshot

| Area | Phase 160 count | Phase 166 count | Delta |
| --- | ---: | ---: | ---: |
| Modern server `.cpp` files | 66 | 45 | -21 |
| Modern server `.hpp` files | 66 | 66 | 0 |
| Legacy server adapter `.cpp` files | 52 | 46 | -6 |
| Legacy server adapter headers | 52 | 52 | 0 |
| Engine test `.cpp` files | 86 | 72 | -14 |
| Full test target count | 141 | 127 | -14 |

The header counts intentionally did not collapse. Headers remain the
conceptual include map for adapters, tests, and future migration work. The
implementation files now carry the simpler domain shape.

## Current Domain Shape

| Domain | Modern `.cpp` files | Role |
| --- | ---: | --- |
| `client` | 10 | Admission, command routing, session slots, query responses, remote admin, timeout, and user-agent policy. |
| `game_dll` | 14 | Game DLL ABI metadata, entity/lifecycle parsing, callback-facing policies, message sessions, and user messages. |
| `messaging` | 11 | Wire payload helpers, multicast policy, frame/datagram helpers, voice/sound/static/spawn messages. |
| `resources` | 3 | Resource identity, consistency, and resource-flow policy. |
| `runtime` | 3 | Server filter, event-log formatting, and runtime/operator command planning. |
| `save` | 1 | Save/restore value and format policy. |
| `shared` | 2 | Limits and shared server rules. |
| `world` | 1 | World, physics, movement, PMove, and trace policy helpers. |

This shape is less noisy than the Phase 160 tree while still preserving the
important domain names.

## What Improved

- The modern source tree now communicates domains instead of migration seams.
- Test targets now match the grouped domain behavior where that is safe.
- Adapter implementation files now group only where a real modern domain
  exists: query responses, consistency, and resource flow.
- Narrow adapter headers still keep legacy call-site ownership visible.
- Validation time improved mostly through fewer test targets, while coverage
  stayed focused on the same behavior.

## What Still Looks Right To Keep Split

- Game DLL ABI/table-order tests and adapters.
- Packet byte layout tests for sound, static messages, userinfo, resources,
  customizations, datagrams, packet entities, and spawn handshakes.
- Save format/runtime/value tests.
- Fixture-heavy PMove/world trace tests.
- Legacy `sv_*.c` files that own live server state, side effects, callbacks,
  packet buffers, and save streams.

## Decision

Do not keep squeezing the server cleanup lane right now.

The remaining server structure is much less cluttered, and the next meaningful
server improvements need stronger fixtures around live state, exact packet
ownership, world traces, PMove, or save streams. Further file-count reduction
would probably become cosmetic and risk hiding useful compatibility seams.

Recommended next lane: switch to a deliberate client/render audit and fixture
planning pass before extracting more runtime behavior. Good candidates:

- client rendered console and platform/system console routing boundaries;
- client message/render/audio sink ownership;
- renderer-facing resource/model visibility fixtures;
- client-side command/cvar bridge surfaces;
- smoke-test scripts that exercise a rendered-frame path, not only first-frame
  launch.

Server work should continue later as fixture-building phases rather than more
adapter/source collapsing.

## Validation

Phase 166 ran full validation with runtime smoke:

```powershell
.\scripts\run-phase-validation.ps1 -SkipFocused -StopRunningXash -CopyLauncher
```

Result:

- `.\waf.bat build --targets=xash` passed.
- `.\waf.bat build --alltests` passed 127/127 tests.
- Runtime smoke reached first frame in 0.513 seconds and stopped with reason
  `command`.
- Report:
  `.codex-cache/validation-reports/phase-validation-20260511-182202.md`.
