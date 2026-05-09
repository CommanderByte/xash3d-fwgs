# Filesystem Logging TODO

## Purpose

Track migration away from scattered direct console output in legacy filesystem
code toward a small modern logging bridge. This is a prerequisite for making
filesystem handlers reusable, testable, and eventually friendlier to threaded
diagnostics.

The current code calls `Con_Printf`, `Con_DPrintf`, `Con_Reportf`, and
`Sys_Error` directly from many legacy paths. That is convenient, but it keeps
modern handlers tied to engine globals and makes structured diagnostics harder.

## Phase 30 Tasks

- [ ] `FS-LOG-001` Inventory filesystem logging and fatal-error call sites.
  Evidence:
  Notes: classify call sites as user-facing output, developer trace,
  mount/report status, warning, or fatal error.

- [ ] `FS-LOG-002` Define a filesystem logging facade backed by existing
  engine callbacks.
  Evidence:
  Notes: keep `fs_interface_t` callback behavior, but route modern code through
  a narrow interface instead of direct `Con_*` macros.

- [ ] `FS-LOG-003` Add structured log categories for filesystem runtime work.
  Evidence:
  Notes: likely categories are `mount`, `archive`, `path_policy`, `file_io`,
  `gameinfo`, `library_lookup`, and `compat`.

- [ ] `FS-LOG-004` Add tests for log capture where behavior depends on
  diagnostics.
  Evidence:
  Notes: use in-memory sinks for modern handlers; keep console output tests
  minimal to avoid brittle text snapshots.

- [ ] `FS-LOG-005` Route modern backend/handler code through the logging
  facade.
  Evidence:
  Notes: do this gradually. Avoid changing legacy console text unless a test
  or compatibility note says it is safe.

- [ ] `FS-LOG-006` Define release-build behavior for trace-heavy filesystem
  diagnostics.
  Evidence:
  Notes: default user-facing errors should remain available; verbose trace
  output should be gated by build flags or runtime cvars.

## Rules

- Do not remove current engine callback support.
- Do not throw C++ exceptions across C ABI or public facade boundaries.
- Preserve fatal error behavior until a deliberate error-policy phase changes
  it.
- Prefer structured capture in modern code and formatting at the boundary.
