# Info String Migration Guide

## Shape

Info strings are now implemented as a low-level modern engine primitive in
`src/engine/info_string.cpp` with public C++ declarations in
`src/include/engine/info_string.hpp`. The C API remains in
`engine/common/infostring.cpp`.

This is intentionally not an adapter-heavy migration. Info strings are a small,
low-level primitive with no useful legacy ownership boundary, so the cleaner
shape is:

```text
common.h Info_* declarations
        |
engine/common/infostring.cpp
        |
src/engine/info_string.cpp
```

## Rules For Future Changes

- Keep `common.h` free of C++ types.
- Keep `src/engine/info_string.*` free of `common.h`, console output, cvars,
  and engine memory allocation.
- Preserve generated string format and return semantics until all callers are
  migrated away from raw `Info_*` functions.
- Add new behavior as typed helpers above this layer rather than changing the
  raw parser implicitly.
- Extend `tests/engine/info_string.cpp` for modern behavior and `xash_tests`
  coverage when the legacy C surface changes.

## Future Candidates

Good follow-up work would be typed userinfo/serverinfo views that validate
specific fields before writing to a raw info string. That should happen above
this primitive and should not make `Info_*` depend on cvars, commands, logging,
or network state.
