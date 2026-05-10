# Game DLL String Pool Compatibility

Phase 94 models game-DLL-facing string-pool behavior without moving
`globalvars_t::pStringBase` or the live 64-bit string arenas out of
`sv_game.c`.

## Legacy Baseline

- `SV_ProcessString()` converts only `\n`, `\r`, and `\t` escape pairs to
  control characters. Other backslashes are copied through.
- `SV_AllocString()` runs that escape processing before storing the string.
- On 64-bit builds, the engine keeps a numeric `string_t` offset into
  `globalvars_t::pStringBase`; `SV_GetString()` returns
  `pStringBase + iString` with no validity check.
- Deduplication is enabled by default. `-str64dup` disables deduplication and
  allows duplicate copies.
- When the static arena overflows, the active arena rewinds to
  `pStringBase + 1`, old active entries become unreachable, and the overflow
  counter increments.
- Dynamic string mode switches the base to the lower array. Static mode uses
  the upper half of the 64-bit allocation so offsets are more likely to fit in
  `int` handles for external DLLs.
- `SV_MakeString()` normally returns the pointer difference from
  `pStringBase`; if that difference is outside the `int` range on 64-bit
  builds, it falls back to `SV_AllocString()`.
- Physics extension overrides for `AllocString`, `MakeString`, and `GetString`
  take precedence when present.

## Modern Boundary

`src/engine/server/game_dll_string_pool_compat.cpp` owns a fixture-safe model
for:

- escape normalization and required byte counts;
- string-pool override and make-string routing decisions;
- duplicate versus duplicate-allowed allocation behavior;
- static/dynamic arena accounting;
- overflow rewind accounting;
- active-handle lookup for unit tests.

The model deliberately returns `nullptr` for invalid handles. That is safer for
tests, but it is not how legacy `SV_GetString()` behaves; the live engine still
does raw pointer arithmetic through `globalvars_t::pStringBase`.

The legacy server still owns:

- the actual `globalvars_t::pStringBase` value;
- near-DLL 64-bit allocation and memory release;
- `-str64alloc`, `-str64dup`, and dynamic/static arena switches;
- physics string override callbacks;
- console string-pool statistics output;
- every live `SV_AllocString()`, `SV_MakeString()`, and `SV_GetString()` call.

Phase 94 therefore adds compatibility fixtures only. Routing should wait until
there are loaded-DLL fixtures or an explicit string-base ownership phase.
