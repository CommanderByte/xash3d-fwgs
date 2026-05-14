# Server Client Command Dispatch Baseline

This note captures the `SV_ExecuteClientCommand()` lookup behavior in
`engine/server/sv_client.c` before Phase 60 routing.

## Parsing

`SV_ExecuteClientCommand()` receives the raw string command from a client
packet and immediately calls `Cmd_TokenizeString()`. Dispatch is based on
`Cmd_Argv(0)`.

Command matching uses exact `Q_strcmp()` comparisons. Matching is
case-sensitive: `begin` is a built-in command, but `Begin` is not.

## Built-In Commands

The built-in `ucmds[]` table is checked first, before `sv.state` is considered.
That means built-ins are still found when the server is not active; individual
handler functions decide whether the command is valid for the current client
state.

Current built-ins, in lookup order:

- `_sv_build_info`
- `begin`
- `disconnect`
- `dlfile`
- `god`
- `info`
- `kill`
- `new`
- `noclip`
- `notarget`
- `pause`
- `sendres`
- `setinfo`
- `spawn`
- `status`

If the handler returns false, the legacy code prints:

```text
'<command>' is not valid from the console
```

If the handler succeeds, it reports:

```text
ucmd-><command>()
```

## Active Server Fallback

Commands that are not built-ins are ignored unless `sv.state == ss_active`.

When the server is active, enttools commands are checked before `fullupdate` and
the game DLL fallback, but only if:

- the client state is `cs_spawned`;
- `sv_enttools_enable` is greater than zero;
- the server is not running a background map.

Current enttools commands, in lookup order:

- `ent_create`
- `ent_fire`
- `ent_getvars`
- `ent_info`
- `ent_list`

Enttools commands report and log the raw command string, then invoke their
handler. The handler return value is ignored.

## Fullupdate And Game DLL Fallback

After enttools lookup, `fullupdate` is checked by exact name. If
`sv_fullupdate_penalty_time` is non-zero and `host.realtime` is still before
`cl->fullupdate_next_calltime`, the command is ignored.

Otherwise, `fullupdate` is forwarded to `svgame.dllFuncs.pfnClientCommand()`
just like other active unknown commands. After the game DLL callback returns,
`fullupdate` also restarts ambient sounds, decals, static entities, and the
client view, then updates the penalty time.

Any other active unknown command is forwarded to the game DLL.

## Migration Boundary

Phase 60 may move the target-neutral command lookup and routing decision into
`src/engine/server`, but these remain legacy-owned:

- `Cmd_TokenizeString()` and command argument storage;
- built-in and enttools handler functions;
- `sv_client_t`, `sv`, `svgame`, and cvar state;
- logging, reporting, and fullupdate side effects;
- the game DLL callback invocation.
