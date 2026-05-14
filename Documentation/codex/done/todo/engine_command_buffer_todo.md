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

- [x] `ENG-CBUF-001` Audit current command buffer ownership, command splitting,
  quote/comment handling, insertion behavior, overflow behavior, filtered
  buffer behavior, and `wait` semantics.
  Evidence: `Documentation/codex/legacy/engine/command-buffer-baseline.md`.

- [x] `ENG-CBUF-002` Expand tests for semicolon and newline splitting inside
  and outside quotes, line comments, CRLF handling, and inserted alias text.
  Evidence: `engine/common/cmd.c` `Test_RunCommandBufferPolicy` now covers
  CRLF, quoted semicolons, line-comment stripping, alias insertion, `wait`,
  filtered order, and overflow rejection.

- [x] `ENG-CBUF-003` Add a modern command-buffer primitive under
  `src/engine/commands` without changing command dispatch.
  Evidence: `src/include/engine/commands/command_buffer.hpp` and
  `src/engine/commands/command_buffer.cpp`.

- [x] `ENG-CBUF-004` Add shadow tests comparing legacy buffer mechanics and the
  modern primitive.
  Evidence: `tests/engine/command_buffer.cpp` mirrors the splitter, insertion,
  overflow, quote, escape, comment, and CR/LF cases now pinned in `xash_tests`.

- [x] `ENG-CBUF-005` Route `Cbuf_*` mechanics through the modern primitive while
  keeping command dispatch and cvar policy in legacy code.
  Evidence: `engine/common/command_buffer_adapter.cpp` owns the two runtime C++
  buffers; `engine/common/cmd.c` still owns command dispatch, alias expansion,
  filtered privilege policy, `stuffcmds`, and `wait`.

- [x] `ENG-CBUF-006` Run focused tests, `xash_tests`, `.\waf.bat build
  --alltests`, and a Windows runtime smoke.
  Evidence: `.\waf.bat build --targets=test_engine_command_buffer`,
  `.\waf.bat build --targets=xash_tests`, and `.\waf.bat build --alltests`
  passed 52/52. Runtime smoke command
  `.\xash3d.exe -dev 2 -log +wait +wait +quit` from `run-win32` exited 0,
  reached `Time to first frame: 0.534 seconds`, and stopped with reason
  `command`.
