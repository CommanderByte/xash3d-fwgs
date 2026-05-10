# Client Command Dispatch Migration

Phase 60 adds target-neutral helpers for server client command lookup:

- `src/include/engine/server/client_command_dispatch.hpp`
- `src/engine/server/client_command_dispatch.cpp`

The helper owns the built-in and enttools command name tables, exact
case-sensitive lookup, and the first routing decision. It does not tokenize raw
client strings or execute any command.

## Extracted Policy

The modern classifier returns one of these routes:

- `Builtin`: command matched the built-in client command table.
- `EntTools`: command matched the enttools table and the enttools gates passed.
- `FullUpdate`: active server fallback for unthrottled `fullupdate`.
- `GameDll`: active server fallback for unknown commands.
- `Ignore`: inactive unknown commands and throttled `fullupdate`.

The legacy adapter returns the route and command-table index. `sv_client.c`
keeps function pointer arrays in the same order as the modern name tables.

## Legacy Ownership Kept

`sv_client.c` still owns:

- `Cmd_TokenizeString()` and `Cmd_Argv()`;
- handler function pointers and all `sv_client_t` mutation;
- `Con_Printf()`, `Con_Reportf()`, and `Log_Printf()`;
- fullupdate restart effects and penalty updates;
- the game DLL `pfnClientCommand()` call.

This phase is intentionally a lookup/table extraction, not a declarative command
registration system. A later server-command registration phase can build on
this once more command surfaces are migrated.
