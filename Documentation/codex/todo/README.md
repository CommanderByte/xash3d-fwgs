# Codex TODO Notes

This folder contains active focused TODO lists derived from the modernization
docs.

Use these files for implementation checklists that are more detailed than the
main `tasks.md` phase tracker, but still useful for auditability.

Completed TODOs move to [../done/todo](../done/todo/README.md) after their
implementation evidence is recorded.

Active documents:

- [debugging_todo.md](debugging_todo.md) tracks remaining work for the modern
  debugging utility layer.
- [engine_deferred_todo.md](engine_deferred_todo.md) tracks deferred
  engine/common candidates after the BaseCmd pilot.
- [engine_infostring_todo.md](engine_infostring_todo.md) tracks the proposed
  low-level info-string rewrite.
- [engine_hash_todo.md](engine_hash_todo.md) tracks hash/checksum helper
  modernization.
- [engine_string_path_todo.md](engine_string_path_todo.md) tracks shared
  string/path helper modernization.
- [engine_command_buffer_todo.md](engine_command_buffer_todo.md) tracks command
  buffer primitive extraction.
- [engine_memory_todo.md](engine_memory_todo.md) tracks cautious memory pool and
  allocation modernization.
- [engine_netbuffer_todo.md](engine_netbuffer_todo.md) tracks network/message
  buffer modernization.
- [file_handle_todo.md](file_handle_todo.md) tracks future `file_t` operation
  migration.
- [game_launch_todo.md](game_launch_todo.md) tracks the launcher
  modularization pilot.
- [modern_filesystem_handlers_todo.md](modern_filesystem_handlers_todo.md)
  tracks migration of legacy-facing behavior into modern handlers and a future
  `src/filesystem/legacy_adapter.cpp` boundary.
