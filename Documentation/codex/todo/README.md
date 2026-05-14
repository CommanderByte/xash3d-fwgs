# Codex TODO Notes

This folder contains active focused TODO lists derived from the modernization
docs.

Use these files for implementation checklists that are more detailed than the
main `tasks.md` phase tracker, but still useful for auditability.

Completed TODOs move to [../done/todo](../done/todo/README.md) after their
implementation evidence is recorded.

Active documents:

- [common_structure_modernization_todo.md](common_structure_modernization_todo.md)
  tracks the root `common/` header audit, layout-test, and private C++ wrapper
  lane.
- [debugging_todo.md](debugging_todo.md) tracks remaining work for the modern
  debugging utility layer.
- [engine_deferred_todo.md](engine_deferred_todo.md) tracks deferred
  engine/common candidates after the BaseCmd pilot.
- [engine_memory_todo.md](engine_memory_todo.md) tracks cautious memory pool and
  allocation modernization.
- [file_handle_todo.md](file_handle_todo.md) tracks future `file_t` operation
  migration.
- [non_windows_console_backend_todo.md](non_windows_console_backend_todo.md)
  tracks deferred non-Windows output backend decisions.
- [non_windows_system_runtime_todo.md](non_windows_system_runtime_todo.md)
  tracks deferred non-Windows `system.c` runtime validation.
- [posix_console_backend_todo.md](posix_console_backend_todo.md) tracks POSIX
  console backend validation.
- [platform_portability_todo.md](platform_portability_todo.md) tracks
  platform-profile, defaults, path, library-locator, and launcher handoff
  modernization.
- [server_migration_todo.md](server_migration_todo.md) tracks the active
  server-side migration lane opened by Phase 52.
