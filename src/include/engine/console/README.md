# Engine Console Headers

Private console/logging modernization headers.

- `platform_console_backend.hpp` defines the internal background console
  backend contract, capability flags, default configuration, null backend, and
  POSIX/Win32-style backend wrappers.
- `platform_console_backend_adapter.h` exposes the C adapter used by legacy
  platform code while preserving the public C console surface.
- `system_console_message.hpp` defines the target-neutral system console
  message filter/format result seam used to test `Con_Printf`, `Con_DPrintf`,
  and `Con_Reportf` compatibility rules before routing legacy callers.
