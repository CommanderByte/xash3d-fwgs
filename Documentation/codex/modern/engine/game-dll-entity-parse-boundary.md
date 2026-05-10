# Game DLL Entity Parse Boundary

Phase 96 models the `SV_ParseEdict()` and `SV_LoadFromFile()` decisions that
can be tested without invoking a real game DLL. The live parser, key-value
callbacks, spawn callbacks, edict allocation, and map text cursor remain in
`sv_game.c`.

## Legacy Baseline

- `SV_LoadFromFile()` lets `svgame.physFuncs.SV_LoadEntities()` replace the
  text parser only when that callback exists and returns true. Missing or
  false-returning overrides fall back to text parsing.
- Text entity parsing expects each entity to begin with `{`; malformed input
  remains a legacy fatal error path.
- The first parsed entity reuses world edict `0`; later entities allocate
  through `SV_AllocEdict()`.
- Empty keys, empty values, and the exact key `wad` are skipped before any
  trailing-space trimming.
- Leading-underscore keys are skipped only when `FWORLD_SKYSPHERE` is set.
- `classname` is special: it is sent to `pfnKeyValue()` immediately and must
  be handled by the game DLL. Duplicate `classname` keys are ignored.
- Non-classname keys are deferred until after private data allocation. Their
  trailing spaces are trimmed after the `classname` path has already been
  handled.
- `SV_AllocPrivateData()` may use a named export, physics custom entity
  creation, or the `custom` export fallback. If the `custom` fallback is used,
  the parser sends `customclass=<original classname>` with class name `custom`
  and does not require the game DLL to mark it handled.
- If allocation leaves the edict invalid or marked `FL_KILLME`, deferred
  key-values are freed and parsing returns false.
- Deferred key `angle` is rewritten to `angles`. Positive yaw becomes
  `<current pitch> <yaw> <current roll>`, `-1` becomes `-90 0 0`, `-2`
  becomes `90 0 0`, and other negative values become `0 0 0`.
- If `pfnSpawn()` returns `-1` and the entity is not marked `FL_KILLME`, the
  edict is freed and the inhibited count increments. If `FL_KILLME` is already
  set, the parser leaves cleanup to the existing entity-removal path.
- After loading, world origin and angles are cleared.

## Modern Boundary

`src/engine/server/game_dll_entity_parse.cpp` owns pure plans for:

- skip versus handle-classname versus deferred key-value storage;
- duplicate `classname` rejection;
- deferred key trailing-space trimming;
- `angle` to `angles` rewrite values;
- custom entity `customclass` key-value planning;
- parse continuation/rejection after classname and allocation checks;
- physics override versus text parsing admission;
- spawn accepted/free-and-inhibit/leave-killme decisions.

The legacy server still owns:

- `COM_ParseFile()` and map entity text lifetime;
- `KeyValueData` allocation and freeing;
- `pfnKeyValue()` and `pfnSpawn()` callback invocation;
- `SV_AllocPrivateData()`, `SV_AllocEdict()`, and `SV_FreeEdict()`;
- `FWORLD_SKYSPHERE`, `FL_KILLME`, and any mod-specific hacks;
- all fatal parse errors and console output.

No route-through is added in Phase 96. The helper exists to pin compatibility
decisions before any future loaded-DLL fixture or parser adapter takes
ownership of the call ordering.
