# Save Runtime Fixture Expansion

Phase 158 adds a fixture-level save runtime view. It is not a replacement for
`sv_save.c`; it is a plain-value harness for decisions around save admission,
comment/version gates, entity patch plans, and bundled save manifests.

## Added Runtime Values

`save_restore_runtime` adds:

- `SaveEntityPatchWritePlan`, which selects removed entity indexes from plain
  entity-table row snapshots;
- `SaveEntityPatchApplyPlan`, which wraps the existing `.HL3` parser into a
  plan for which entity-table rows would be marked removed and which indexes
  are malformed;
- `SaveArchiveManifest`, which walks bundled `.HL?` entries in a `.sav` file
  and records names, sizes, byte ranges, and parse status.

The aggregate test composes these values with the existing save admission and
comment/version helpers. That gives us one focused save-domain fixture without
moving the live runtime.

## Still Legacy-Owned

The phase keeps these in `sv_save.c`:

- `SAVERESTOREDATA` allocation, pointer rebasing, and publication through the
  game DLL globals;
- `pfnSave*` and `pfnRestore*` field callbacks;
- filesystem writes, deletes, renames, archive extraction, and quick/autosave
  aging;
- renderer, sound, decal, and signon restore state;
- console output, `Host_Error()`, and warning behavior.

## Aggregate Readiness

`test_engine_save_restore_runtime` is ready as a fixture aggregate. It is not a
facade and should not become the route-through layer until real save files or a
controlled game DLL field-serialization harness prove callback ordering.

## Validation

- `.\waf.bat build --targets=test_engine_save_restore_runtime,test_engine_save_restore_values,test_engine_save_restore_format` passed.
- `.\waf.bat build --alltests` passed 141/141 tests.
- `.\scripts\run-phase-validation.ps1 -FocusedTarget test_engine_save_restore_runtime -SkipFullTests -AllowSmokeNonZeroExit -StopRunningXash` passed; first frame was 0.535 seconds.
