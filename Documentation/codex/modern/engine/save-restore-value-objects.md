# Save/Restore Value Objects

Phase: 141

## Purpose

This phase starts save/restore modernization with pure values. It does not move
`SAVERESTOREDATA`, game DLL field callbacks, file extraction, entity restore,
landmark transition mutation, or `.HL?` filesystem effects.

The goal is to make small decision trees testable before touching runtime save
streams.

## Implemented Values

`SaveAdmissionSnapshot` captures the facts read by legacy `IsValidSave()`:
server activity, background/credits suppression, physics-extension veto,
client activity, intermission, maxclient count, spawned-client availability,
player edict availability, dead flag, and health.

`BuildSaveAdmissionPlan()` preserves the legacy decision order and stores the
legacy console message as data. Background-map and credits suppression remain
silent, matching the current C path.

`BuildSaveGameCommentHeaderPlan()` models the header/version gates from
`SV_GetSaveComment()`:

- missing file clears the comment;
- invalid magic uses `<corrupted>`;
- version `0x0065` uses the old unsupported Xash3D FWGS text;
- older versions use `<old version>`;
- newer versions use `<invalid version>`;
- only the current save-game version proceeds to field parsing.

`BuildSaveCommentFallbackPlan()` models the source-selection order used by
`SaveBuildComment()` after the legacy adapter has already read strings or
called the optional game DLL callback:

1. game DLL override, if the callback exists;
2. pre-resolved hardcoded title alias;
3. worldspawn message, if a world message index existed;
4. map name fallback.

The helper also preserves the legacy minute/second truncation from `sv.time`.

## Route-Through Decision

No legacy call site was routed in this phase. That is intentional.

`IsValidSave()` still owns callback reads and console printing. Routing it now
would require a careful adapter around `svgame.physFuncs.SV_AllowSaveGame()`,
`CL_Active()`, `CL_IsIntermission()`, client slot state, player edict state,
and exact `Con_Printf()` ordering. The value object tests prove the pure
decision order first.

`SV_GetSaveComment()` still owns filesystem reads, heap allocation, token table
rebasing, raw field parsing, and output buffer mutation. The header/version
plan can be routed later after the parser and fallback tests are paired with a
real save-file fixture.

`SaveBuildComment()` still owns the optional game DLL callback and string-table
lookups. The fallback helper only accepts already-read plain strings.

## Validation Target

The focused test target is `test_engine_save_restore_values`.
