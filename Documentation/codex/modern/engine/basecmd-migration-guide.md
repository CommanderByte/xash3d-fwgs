# BaseCmd Migration Guide

## Goal

Move the shared command/cvar/alias registry behavior out of
`engine/common/base_cmd.c` into modern C++ internals while keeping the legacy C
functions stable:

- `BaseCmd_Init`
- `BaseCmd_Shutdown`
- `BaseCmd_Find`
- `BaseCmd_FindAll`
- `BaseCmd_Insert`
- `BaseCmd_Remove`
- `BaseCmd_Stats_f`
- `BaseCmd_Test_f`

This guide is intentionally narrow. `Cmd_*`, `Cbuf_*`, and `Cvar_*` have more
policy and should migrate only after their behavior tests are stronger.

## Current First Slice

The first modern helper is:

- `src/include/engine/commands/base_command_registry.hpp`
- `src/engine/commands/base_command_registry.cpp`
- `tests/engine/base_command_registry.cpp`
- `engine/common/base_cmd_adapter.h`
- `engine/common/base_cmd_adapter.cpp`

It is build-wired as `modern_engine`, and legacy `base_cmd.c` now routes
through the private adapter while keeping the public `BaseCmd_*` surface
unchanged.

## Behavior To Preserve

`BaseCommandRegistry` mirrors the behavior-sensitive parts of legacy
`base_cmd.c`:

- 64 buckets, using the same case-insensitive hash shape as `COM_HashKey`.
- Case-insensitive lookup.
- Case-insensitive alphabetical ordering within a bucket.
- A shared name space across command, alias, and cvar entries.
- Typed lookup through `find`.
- Multi-type lookup through `findAll`.
- Duplicate insertion is allowed at this layer.
- A duplicate with the same type/name shadows older entries in typed `find`.
- `findAll` preserves the legacy traversal quirk where duplicate same-type
  entries leave the older entry as the final match.
- Removal deletes the first matching typed entry.

Higher-level duplicate policy still belongs to `Cmd_AddCommandEx`,
`Cvar_RegisterVariable`, and alias creation logic.

## Migration Pattern

Use this pattern for BaseCmd and nearby command/cvar code:

1. Capture the legacy behavior in standalone tests when possible.
2. Add a modern helper under `src/engine/commands/` without exposing it to game,
   client, renderer, or filesystem ABI users.
3. Run the legacy implementation and modern helper in parallel where practical,
   comparing behavior before replacing the legacy data path.
4. Keep the public C functions and legacy data ownership intact until tests
   cover the behavior being moved.
5. Add a small adapter only after the helper is tested.
6. Route one legacy function group through the adapter at a time.
7. Run the focused unit test, then `.\waf.bat build --alltests`.
8. Run a Windows startup smoke test when legacy command execution, startup
   scripts, cvar registration, or command buffers are touched.

## Parallel Verification

For the BaseCmd registry, prefer a temporary shadow-verification step before
replacement:

- Keep `engine/common/base_cmd.c` as the authoritative runtime table.
- Create a private adapter that can mirror inserts and removes into
  `BaseCommandRegistry`.
- Add test-only or debug-only comparison helpers for `Find`, `FindAll`, stats,
  insertion order, and removal behavior.
- Fail fast in unit tests when modern and legacy results diverge.
- Avoid enabling runtime shadow checks in release builds unless the checks are
  explicitly behind a debug/developer gate.

This makes the replacement less dramatic: first the modern registry proves it
can follow the legacy table, then the adapter can become authoritative.

Current status: the adapter is now authoritative for `BaseCmd_*`; the shadow
comparison remains in `tests/engine/base_command_registry.cpp` as the lower
level behavior mirror, and `xash_tests` covers the legacy command/cvar public
behavior above it.

## Good-Enough Migration Bar

Do not keep adding BaseCmd-only tests forever. Treat BaseCmd as migrateable when
all of these are true:

- direct unit tests cover typed lookup, shared command/alias/cvar names,
  duplicate same-type quirks, removal, clear, invalid modern-helper inputs, and
  bucket stats;
- a shadow operation sequence compares modern and reference behavior after every
  insert and remove;
- the focused BaseCmd registry test passes;
- `.\waf.bat build --alltests` passes;
- no public `BaseCmd_*`, `Cmd_*`, `Cbuf_*`, or `Cvar_*` signatures have changed;
- higher-level command/cvar duplicate policy, alias collision behavior,
  privileged/filterable behavior, and command-buffer ordering have current
  legacy test coverage.

Once those conditions are true, further work should move up one layer to
`Cmd_*`, aliases, command buffers, or `Cvar_*` policy tests instead of adding
more registry-only cases.

## Adapter Shape

The adapter is private to the engine implementation and does not expose C++
types through legacy headers.

Current shape:

```cpp
// modern helper
xash::engine::commands::BaseCommandRegistry
```

```c
// stable legacy surface in base_cmd.c
BaseCmd_Find(...)
BaseCmd_FindAll(...)
BaseCmd_Insert(...)
BaseCmd_Remove(...)
```

`engine/common/base_cmd_adapter.cpp` translates `HM_CMD`, `HM_CMDALIAS`, and
`HM_CVAR` to `BaseCommandType`, then forwards into the registry.

## Do Not Migrate Yet

Leave these for later phases:

- command buffer execution order;
- filtered versus privileged command policy;
- cvar mutation permissions;
- console completion formatting;
- `Con_Printf` and logging ownership;
- plugin-style command registration.

Those areas need broader tests and are more coupled to host, client, server,
and console state.
