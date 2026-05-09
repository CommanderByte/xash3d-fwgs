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
- [file_handle_todo.md](file_handle_todo.md) tracks future `file_t` operation
  migration.
- [filesystem_logging_todo.md](filesystem_logging_todo.md) tracks filesystem
  console-output and structured logging migration.
- [modern_filesystem_handlers_todo.md](modern_filesystem_handlers_todo.md)
  tracks migration of legacy-facing behavior into modern handlers and a future
  `src/filesystem/legacy_adapter.cpp` boundary.
