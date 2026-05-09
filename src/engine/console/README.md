# Engine Console

Private console/logging modernization helpers.

- `platform_console_backend.cpp` implements target-neutral background console
  backend helpers and the null backend.

Filesystem diagnostics should still reach this area through engine-owned
adapters. The filesystem module should not depend directly on rendered console,
platform console, or log-file globals.
