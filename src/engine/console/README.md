# Engine Console

Private console/logging modernization helpers.

- `platform_console_backend.cpp` implements target-neutral background console
  backend helpers, the null backend, and the POSIX-style backend wrapper.
  The Win32-style wrapper is target-neutral and tested through injectable I/O;
  the live `Wcon_*` C surface is routed through
  `platform_console_backend_adapter.cpp`.
- `system_console_message.cpp` models the target-neutral filtering and bounded
  formatting result rules for the system console print wrappers. It does not
  own the rendered in-game console sink.

Filesystem diagnostics should still reach this area through engine-owned
adapters. The filesystem module should not depend directly on rendered console,
platform console, or log-file globals.
