# Public CRT Conversion Guide

## Ownership

The numeric conversion helpers are public CRT utilities, not engine policy:

- `public/crtlib.h` remains the C-compatible ABI.
- `src/utilities/conversion.*` owns the modern implementation.
- `src/utilities/compat/crtlib_conversion.cpp` exports the legacy C symbols.

## Migrated Helpers

Phase 47 moved:

- `Q_atoi_hex`
- `Q_atoi`
- `Q_atof`
- `Q_atov`

The public declarations did not change.

## Preserved Quirks

The modern helpers intentionally preserve these compatibility details:

- only literal space characters are stripped before parsing;
- tabs are not stripped;
- a leading `+` returns zero instead of accepting the number;
- `0x` and `0X` hex prefixes are accepted;
- character literals return the byte after the first `'`;
- float parsing is decimal-only and does not handle exponent notation;
- repeated decimal points reset the decimal position;
- vector parsing uses a single space as the separator and keeps the legacy
  repeated-space behavior.

## Deferred CRT Areas

Good later slices:

- `Q_strnlwr` and `Q_memfgets`, both covered by `test_strings`;
- wildcard/pattern helpers after dedicated golden cases;
- `COM_ParseFileSafe` as a parser phase;
- formatting helpers after console/fatal-path constraints are documented.
