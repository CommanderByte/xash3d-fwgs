# Server Event Log Formatter Migration

Phase 58 adds target-neutral helpers for server event log messages:

- `src/include/engine/server/server_event_log.hpp`
- `src/engine/server/server_event_log.cpp`

The helpers format timestamped lines and stock server-log messages. They do not
own sinks or server state.

## Extracted Formatting

The modern helper currently formats:

- `MM/DD/YYYY - HH:MM:SS: <message>` event log lines.
- server cvar section start/end rows.
- individual server cvar rows.
- log-file started and closed rows.

The legacy adapter converts `struct tm` fields and existing C strings into the
small C++ value types.

## Legacy Ownership Kept

`sv_log.c` still owns:

- deciding whether logging is active;
- opening, naming, writing, and closing files;
- console echo and UDP forwarding;
- local time retrieval;
- command parsing and cvar enumeration;
- the varargs surface of `Log_Printf()`.

This keeps the phase small and avoids binding server logging to the broader
console/log router before that router exists.
