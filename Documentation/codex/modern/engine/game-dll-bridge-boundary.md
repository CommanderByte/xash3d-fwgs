# Game DLL Bridge Boundary

Phase 86 plans the long-term `sv_game.c` split while preserving the public game
DLL ABI.

## Direction

The game DLL bridge should become a compatibility shell around modern server
internals, not a new public C++ plugin API. Existing GoldSrc/Xash DLLs should
still see:

- the same `enginefuncs_t` layout and callback order;
- the same `DLL_FUNCTIONS` and `NEW_DLL_FUNCTIONS` handshakes;
- the same `globalvars_t`, `edict_t`, `entvars_t`, `string_t`, and
  `SAVERESTOREDATA` shapes;
- the same calling conventions and failure behavior.

Modern code may improve implementation structure behind those callbacks, but
must not leak exceptions, C++ containers, references, virtual interfaces, or
ownership assumptions across the C ABI.

## Scope Start And End

Start with table metadata, message-session state, user-message registry policy,
small command/output decisions, and resource/precache admission rules. These
areas are visible enough to matter, but can be tested through snapshots and
mock buffers without loading a real game DLL.

Stop the first bridge lane before replacing:

- `COM_LoadLibrary()` / `COM_UnloadLibrary()` lifetime ownership;
- `GiveFnptrsToDll()` and concrete callback table publication;
- edict array allocation and `pvPrivateData` storage;
- `globalvars_t::pStringBase` and 64-bit near-DLL string storage;
- runtime save/restore game-field callback ordering;
- trace, visibility, world collision, and movement behavior.

Those are legitimate modernization targets, but they need deeper fixtures or
broader subsystem migration before route-through.

## Proposed Module Split

| Modern area | Suggested files | Responsibility | Legacy-owned until later |
| --- | --- | --- | --- |
| Load handshake planning | `game_dll_load_plan.*` | Decide required/optional exports, version outcomes, and failure reasons from adapter-supplied symbols. | `COM_LoadLibrary`, `COM_GetProcAddress`, actual DLL lifetime. |
| Engine function table metadata | `game_dll_enginefuncs.*` | Categorize callbacks, document required table slots, and support tests for table-construction invariants. | The concrete `enginefuncs_t` ABI object. |
| Message session facade | `game_dll_message_session.*` | Model begin/write/end state, size patching, fixed-size checks, and rewrite admission with mockable buffers. | `sv.multicast`, `SV_Multicast`, live user-message registry. |
| Entity lifecycle policy | `game_dll_entity_lifecycle.*` | Small decisions around entity admission, private-data free ordering, and compatibility flags. | `edict_t` array allocation and game DLL constructors/destructors. |
| String pool policy | `game_dll_string_pool.*` | Numeric string-handle behavior, duplication rules, and static/dynamic mode decisions. | `globalvars_t::pStringBase` and physics extension overrides. |
| Save bridge policy | `game_dll_save_bridge.*` | Safe wrapper decisions for game field callbacks once Phase 84 fixtures are hardened. | Runtime save/load streams and game field serialization. |
| Physics extension bridge | `game_dll_physics_bridge.*` | Optional-callback presence checks and extension feature reporting. | Collision, trace, and movement behavior. |

The names are intentionally provisional. Keep the first implementation small
and driven by tests, as with prior server phases.

## Adapter Pattern

Use the existing migration split:

| Layer | Owns |
| --- | --- |
| `src/engine/server` | Target-neutral state machines, planners, byte builders, and value objects. |
| `src/include/engine/server` | C++ contracts for tests and adapters. |
| `engine/server/*_adapter.cpp` | Translation from `server.h`, `svgame`, `sv`, `svs`, `Cmd_*`, `Cvar_*`, `MSG_*`, and `COM_*`. |
| `engine/server/sv_game.c` | Public C callback names and lifecycle until the bridge itself is explicitly replaced. |

The first callback routes should be leaf routes. Avoid routes that require
moving edict layout, global vars, string base ownership, or actual DLL loading.

## Recommended Migration Order

1. **Audit and fixtures**
   - Complete Phase 84 save/restore hardening first.
   - Keep this Phase 86 audit as the map for later bridge work.

2. **Engine callback table metadata**
   - Add tests that ensure required callback slots are intentionally assigned.
   - Classify callbacks by module so future phases can move one domain at a
     time.
   - Do not alter `enginefuncs_t` order.

3. **Message session facade**
   - Supersede the earlier Phase 79 placeholder with Phase 88.
   - Extract begin/write/end state transitions behind mockable buffer writers.
   - Preserve system-message rewrites and multicast ownership in the adapter.

4. **String pool policy**
   - Extract pure string-handle decisions and stats reporting where possible.
   - Keep physics extension override calls in the adapter.

5. **Entity private-data lifecycle**
   - Start with free-order and validity decisions only.
   - Keep actual `edict_t` allocation and `pvPrivateData` storage in legacy
     code until there are game DLL fixtures.

6. **Load/unload facade**
   - Move last. It is tempting, but it owns DLL lifetime, command/cvar unlinking,
     edict allocation, globals, physics initialization, and save/restore
     initialization in one place.

## Test Strategy

Do not require a real game DLL for the first tests. Use fake symbol tables,
snapshots, and mock writers.

Useful tests:

- load-plan cases for missing `GetEntityAPI`, missing `GiveFnptrsToDll`,
  optional `GetNewDLLFunctions`, version mismatch, and legacy fallback;
- callback-table metadata tests that verify slot names and categories;
- message-session tests for double begin, end without begin, unregistered user
  message, fixed-size mismatch, variable-size patching, overflow clearing, and
  rewrite-admission decisions;
- string-handle tests for duplicate strings, empty string, static mode, and
  invalid handles;
- private-data lifecycle tests for optional destructor ordering and invalid
  edicts.

Runtime validation should still include full tests plus the usual
`+wait +wait` smoke after any route-through.

## Validation Practice

Bridge validation is a standing practice, not its own implementation phase:

- after each routed bridge slice, run focused tests, full tests, and
  `scripts/run-phase-validation.ps1`;
- record `+wait +wait` first-frame timing in `Documentation/codex/tasks.md`;
- every few routed slices, run `scripts/run-game.ps1`, manually start a new
  game, and record the result in the relevant phase evidence;
- record mod-specific, game DLL, asset-loading, or runtime compatibility
  failures before continuing.

## Compatibility Rules

- Keep `engine/eiface.h` ABI definitions stable.
- Keep all game DLL callbacks callable through the current C function pointer
  tables.
- Never throw across a game DLL callback.
- Never store references into transient game DLL inputs unless legacy code
  already does so.
- Keep `svgame`, `sv`, `svs`, and `globalvars_t` adapter-owned until a later
  architecture phase explicitly replaces them.
- Treat save/restore and edict/private-data behavior as gameplay
  compatibility, not cleanup.

## Immediate Follow-Up

The safest bridge follow-up is Phase 87 metadata first, then Phase 88 as a
concrete `GameDllMessageSession` slice. The metadata phase prevents table-slot
drift before code moves; the message phase has a clear state machine,
already-adjacent message payload helpers, and fewer dependencies than loader,
edict, physics, or save/restore ownership.

## Post-Audit Phase Lane

The Phase 86 deep audit turns into this implementation sequence:

| Phase | Slice | Scope |
| --- | --- | --- |
| 87 | Enginefuncs metadata | Inventory and table-slot tests only; no ABI changes. |
| 88 | Message session facade | Begin/write/end state, size accounting, fixed-size checks, overflow, and rewrite admission. |
| 89 | User message registry policy | Duplicate/invalid registration decisions and active-server resend planning. |
| 90 | Text, command, and alert output policy | Validation and output classification while sinks stay legacy-owned. |
| 91 | Resource and precache callback policy | Optional resource flags, path normalization, and lookup/admission decisions. |
| 92 | Sound, decal, and static payload bridge | Reuse existing payload helpers from server-message phases where possible. |
| 93 | Client info-key and query callback policy | Info buffer selection, key-value mutation decisions, cvar-query fallback results, and game-dir behavior. |
| 94 | String pool fixtures | Escape normalization, dedup, invalid handles, overflow, and 64-bit mode documentation before routing. |
| 95 | Entity handle and private-data policy | Index admission, bugcompat pointer behavior, private-data rounding, and destructor ordering plans. |
| 96 | Entity parse and spawn boundary | Classname/keyvalue/angle parsing plans while game callbacks remain legacy-owned. |
| 97 | Changelevel and save/restore bridge policy | Duplicate suppression, landmark handling, and save patch planning. |
| 98 | Visibility and trace boundary | Audit and pure admission/result conversion only after world fixtures exist. |
| 99 | Movement and fake-client boundary | Audit and pure policy only after movement fixtures exist. |
| 100 | DLL load/unload facade plan | Fake-symbol load planner only; real DLL lifetime remains last. |

The detailed checklist for this lane is
[`Documentation/codex/todo/game_dll_bridge_todo.md`](../../todo/game_dll_bridge_todo.md).
