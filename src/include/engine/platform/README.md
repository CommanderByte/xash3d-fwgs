# Engine Platform Headers

Reserved for private platform facade headers behind existing `Sys_*` contracts.

- `command_line.hpp` defines the modern target-neutral command-line helper.
- `command_line_adapter.h` exposes the C adapter used by `engine/common/system.c`.
- `current_user.hpp` defines the target-neutral fallback policy for
  `Sys_GetCurrentUser`.
- `current_user_adapter.h` exposes the C adapter used by the Windows branch in
  `engine/common/system.c`.
