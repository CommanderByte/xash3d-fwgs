# Command Buffer Migration Guide

## Shape

The command-buffer primitive lives in `src/engine/commands/command_buffer.cpp`
with declarations in `src/include/engine/commands/command_buffer.hpp`.

The legacy C surface remains in `engine/common/cmd.c`. A private bridge in
`engine/common/command_buffer_adapter.cpp` owns the runtime C++ buffers and
exposes C-callable helpers to `cmd.c`.

```text
common.h Cbuf_* declarations
        |
engine/common/cmd.c
        |
engine/common/command_buffer_adapter.cpp
        |
src/engine/commands/command_buffer.cpp
```

## Rules For Future Changes

- Keep `CommandBuffer` policy-free: it should not include `common.h`, cvars,
  console output, aliases, or command dispatch.
- Keep `wait`, privilege checks, filtered command policy, and `stuffcmds` in the
  command layer until those policies are migrated explicitly.
- Preserve splitter quirks until a deliberate script compatibility phase says
  otherwise.
- Add standalone tests in `tests/engine/command_buffer.cpp` for raw mechanics.
- Add `xash_tests` coverage when the public `Cbuf_*` behavior changes.
- Do not expose this primitive through game, client, renderer, filesystem, or
  plugin ABI surfaces.

## Follow-Up Candidates

Good later candidates are typed command-script execution plans, better
diagnostics for command-buffer overflow, and an explicit command source model.
Those should sit above the raw buffer primitive rather than making the buffer
itself aware of engine policy.
