# Engine Command Buffer TODO

## Purpose

Plan extraction or rewrite of the command buffer primitive currently embedded
inside `engine/common/cmd.c`.

This is a good follow-up to BaseCmd but should stay separate from command/cvar
registration. The command buffer owns ordering, insertion, filtering, command
splitting, comments, quotes, and `wait`.

## Scope

Legacy surface:

- `Cbuf_Clear`
- `Cbuf_AddText`
- `Cbuf_AddTextf`
- `Cbuf_AddFilteredText`
- `Cbuf_InsertText`
- `Cbuf_ExecStuffCmds`
- `Cbuf_Execute`

## Method

- Continue using `xash_tests` for public behavior.
- Add standalone tests for buffer mechanics where possible.
- Use parallel comparison before replacing the runtime buffer.
- Keep command execution policy separate from raw buffer mechanics.

## Phase 41 Tasks: Command Buffer Primitive

- [ ] `ENG-CBUF-001` Audit current command buffer ownership, command splitting,
  quote/comment handling, insertion behavior, overflow behavior, filtered
  buffer behavior, and `wait` semantics.
  Evidence:

- [ ] `ENG-CBUF-002` Expand tests for semicolon and newline splitting inside
  and outside quotes, line comments, CRLF handling, and inserted alias text.
  Evidence:

- [ ] `ENG-CBUF-003` Add a modern command-buffer primitive under
  `src/engine/commands` without changing command dispatch.
  Evidence:

- [ ] `ENG-CBUF-004` Add shadow tests comparing legacy buffer mechanics and the
  modern primitive.
  Evidence:

- [ ] `ENG-CBUF-005` Route `Cbuf_*` mechanics through the modern primitive while
  keeping command dispatch and cvar policy in legacy code.
  Evidence:

- [ ] `ENG-CBUF-006` Run focused tests, `xash_tests`, `.\waf.bat build
  --alltests`, and a Windows runtime smoke.
  Evidence:
