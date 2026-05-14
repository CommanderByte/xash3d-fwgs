# Server Event Log Baseline

This note captures the `engine/server/sv_log.c` formatting behavior before
Phase 58 routing. The server event log is separate from `engine.log` and from
the rendered in-game console.

## Log Line Shape

`Log_Printf()` returns early unless either UDP event logging or normal server
logging is active. When active, it reads local time and prefixes the formatted
message with:

```text
MM/DD/YYYY - HH:MM:SS: 
```

The caller-supplied message follows immediately after the colon-space prefix.
Callers normally include their own trailing newline. The completed string is
then reused for every enabled sink:

- UDP `log %s` packets when `svs.log.net_log` is enabled.
- console echo when normal logging is active, multiplayer or
  `sv_log_singleplayer` allows it, and `mp_logecho` is enabled.
- log file output when a log file exists and `mp_logfile` is enabled.

Sink selection, `time()`/`localtime()`, and `Q_vsnprintf()` varargs formatting
are still legacy-owned after Phase 58.

## Stock Messages

`Log_Open()` writes:

```text
Log file started (file "<file>") (game "<gamedir>") (version "<protocol>/<XASH_VERSION>/<build>")
```

`Log_Close()` writes:

```text
Log file closed
```

`Log_PrintServerVars()` writes `Server cvars start`, one row per server cvar,
and `Server cvars end`. Each cvar row is:

```text
Server cvar "<name>" = "<value>"
```

All of those messages are passed through `Log_Printf()`, so the timestamp
prefix is added later.

## Migration Boundary

Phase 58 may move target-neutral string construction into `src/engine/server`,
but these remain legacy-owned:

- `FS_*` file opening, probing, writing, and closing.
- `Con_Printf()` console output.
- `Netchan_OutOfBandPrint()` UDP log forwarding.
- `Cvar_LookupVars()` and command parsing in `SV_ServerLog_f()` /
  `SV_SetLogAddress_f()`.
- local time retrieval and varargs formatting for arbitrary `Log_Printf()`
  callers.
