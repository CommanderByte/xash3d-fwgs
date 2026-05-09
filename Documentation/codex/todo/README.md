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
- [engine_logging_todo.md](engine_logging_todo.md) tracks console/logging
  ownership and the path back to deferred filesystem logging cleanup.
- [engine_memory_todo.md](engine_memory_todo.md) tracks cautious memory pool and
  allocation modernization.
- [engine_platform_todo.md](engine_platform_todo.md) tracks the `system.c` and
  platform facade audit.
- [file_handle_todo.md](file_handle_todo.md) tracks future `file_t` operation
  migration.
