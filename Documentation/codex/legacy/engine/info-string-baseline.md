# Info String Baseline

## Purpose

This note captures the legacy `Info_*` behavior migrated in Phase 38. Info
strings are compact `\key\value` records used for userinfo, serverinfo, master
queries, and command/config plumbing. The external C API remains the contract:
callers still include `common.h` and call `Info_ValueForKey`,
`Info_SetValueForKey`, `Info_SetValueForStarKey`, `Info_RemoveKey`,
`Info_RemovePrefixedKeys`, `Info_IsValid`, `Info_Print`, and `Info_WriteVars`.

## Format Rules

- A string is a sequence of `\key\value` pairs. A leading slash is optional for
  parsing, but generated strings always append pairs in slash-delimited form.
- Keys and values are copied through fixed 128-byte parsing buffers, preserving
  the legacy truncation boundary.
- `Info_IsValid` rejects a dangling key and rejects empty values. An empty
  whole string is valid.
- `Info_ValueForKey` returns an empty string for missing keys and uses the
  legacy four-slot static buffer pattern so callers may compare several lookup
  results in one expression.

## Set And Remove Quirks

- `Info_SetValueForKey` rejects keys beginning with `*`; callers that need
  star-prefixed keys must call `Info_SetValueForStarKey`.
- Keys or values containing `\`, `"`, or `..` are rejected. Backslash, quote,
  and star-key failures still report through the legacy console messages.
- Values longer than the 127-byte key/value buffer are rejected.
- Setting an empty or null value removes the key and returns success.
- Values for the `team` key are lowercased while being written.
- Control bytes `<= 13` are skipped while appending a new pair.
- `Info_RemoveKey` uses the legacy prefix-length comparison. Removing `foo`
  from `\foo2\bad\foo\good` removes `foo2` first, leaving `\foo\good`.
- `Info_RemovePrefixedKeys` repeatedly scans from the start after removing a
  matching key.

## Overflow Policy

If appending a pair would exceed `maxsize`, non-important keys are silently
ignored and the function still returns success. Important keys may evict the
largest non-important key until the pending pair fits or no removable key
remains.

Important keys are:

- star-prefixed keys
- `name`
- `model`
- `rate`
- `topcolor`
- `bottomcolor`
- `cl_updaterate`
- `cl_lw`
- `cl_lc`
- `cl_nopred`

## Phase 38 Migration

The old `engine/common/infostring.c` implementation was replaced with
`engine/common/infostring.cpp`, which exports the same C symbols and delegates
to `src/engine/info_string.cpp`. The modern implementation keeps the C ABI out
of `src/engine` and avoids engine globals, allocation, and console dependencies.

Coverage added:

- `tests/engine/info_string.cpp` exercises the modern implementation directly.
- `Test_InfoStrings` in `engine/common/common.c` exercises the routed legacy
  `Info_*` API through `xash_tests`.

The compatibility focus is exact enough for current callers, while still
leaving room to build cleaner typed userinfo/serverinfo helpers above the C
surface later.
