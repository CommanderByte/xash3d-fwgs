# Command Buffer Baseline

## Scope

This document captures the legacy command-buffer behavior currently exposed by
`engine/common/cmd.c`.

Public C surface:

- `Cbuf_Clear`
- `Cbuf_AddText`
- `Cbuf_AddTextf`
- `Cbuf_AddFilteredText`
- `Cbuf_InsertText`
- `Cbuf_ExecStuffCmds`
- `Cbuf_Execute`

The command buffer is not just a queue. It also owns script splitting rules,
insert-front behavior used by aliases and `exec`, filtered command storage, and
the `wait` frame break.

## Legacy Ownership

Before Phase 41, `cmd.c` owned two raw byte buffers:

- `cmd_text`: privileged command text.
- `filteredcmd_text`: command text that may be restricted when it came from
  filtered or server-provided sources.

Both buffers used a 32768-byte fixed capacity. The command splitter copied one
line into a 2048-byte stack buffer before dispatching it through
`Cmd_ExecuteStringWithPrivilegeCheck`.

## Behavior To Preserve

- `Cbuf_Execute` drains privileged text first, then filtered text.
- Filtered text is only treated as privileged in the existing singleplayer
  server condition.
- `wait` decrements once per `Cbuf_ExecuteCommandsFromBuffer` entry and breaks
  execution for that frame.
- `Cbuf_AddText` and `Cbuf_AddFilteredText` append text and reject overflow
  non-fatally.
- `Cbuf_InsertText` inserts text before the remaining buffered text.
- `exec` may reserve one extra byte of capacity when adding a final newline,
  even though that byte is inserted separately.
- Semicolons split commands only outside quotes and outside comments.
- `\n` and `\r` split commands everywhere, including inside comments.
- Quotes toggle on `"`; inside quotes, escaped `\"` and `\\` skip the escaped
  character.
- `//` starts a comment only at the beginning of a command line or when the
  previous byte is whitespace.
- Comment text is stripped from the executed command, but the buffer consumes
  through the next newline or carriage return.
- Command lines with delimiter index `>= 2047` log the existing overflow
  warning and execute as an empty line.

## Migration Boundary

Phase 41 moves raw buffer mechanics into a private modern primitive while
leaving command dispatch policy in `cmd.c`.

The modern primitive owns:

- fixed-capacity byte storage;
- append and insert-front mechanics;
- overflow detection;
- command-line splitting;
- quote/comment compatibility.

The legacy layer keeps:

- public C ABI;
- command/cvar lookup;
- privilege and filtering policy;
- aliases;
- `wait`;
- `stuffcmds`;
- console output and warning text.

This keeps the migration narrow enough to test without changing user-visible
command policy.
