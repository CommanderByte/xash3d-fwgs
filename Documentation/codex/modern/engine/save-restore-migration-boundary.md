# Save/Restore Migration Boundary

Phase: 83

## Decision

For now, save/restore modernization should focus on read-only compatibility
fixtures and format validation. Runtime save/load remains legacy-owned until we
have enough binary fixtures to prove field ordering, token tables, entity table
patches, landmark offsets, and bundled `HL?` files.

## Safe Now

- Parse `VALV` / `JSAV` headers.
- Validate version numbers and section counts.
- Parse token tables without mutating engine state.
- Parse synthetic field sections by token name.
- Parse bundled `.sav` file entries.
- Generate tiny fixtures in tests to pin down offsets and malformed inputs.

## Not Safe Yet

- Replacing `pfnSaveWriteFields()` or `pfnSaveReadFields()`.
- Changing token hashing or token slot allocation.
- Moving entity creation or `pfnRestore()` calls.
- Rewriting landmark offset handling.
- Changing `HL1` / `HL2` / `HL3` file ownership.
- Changing save slot aging or screenshot side effects.

## Proposed Migration Path

1. Keep `save_restore_format` as a read-only fixture parser.
2. Add malformed fixture coverage for `HL3` entity patch indexes before touching
   patch restore behavior.
3. Add real-save snapshot tests once we can safely generate minimal saves from
   a controlled test map or synthetic game DLL fixture.
4. Move pure policies next, such as save eligibility and save-slot naming.
5. Only after that, consider an adapter around field section read/write calls.

## Modern Module Role

`src/engine/server/save_restore_format.cpp` is not a replacement save system.
It is a compatibility microscope: it lets tests inspect the byte-level shape of
save files without invoking the game DLL, renderer, sound system, filesystem
mutators, or entity allocator.
