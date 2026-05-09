# Agent Instructions

These standing instructions apply to Codex-driven modernization work in this
fork. They are meant to reduce ambiguity, preserve auditability, and keep
changes compatible with existing Half-Life and GoldSrc behavior.

## Primary Rule

Modernize inward, preserve outward.

Internal implementation can move toward modular C++ over time. Public and
legacy-facing behavior must remain stable unless a task explicitly says a
compatibility break is allowed.

## Compatibility Boundaries

Do not change these without an explicit design task and review:

- `GetFSAPI`
- `fs_api_t`
- `fs_globals_t`
- `fs_interface_t`
- `CreateInterface`
- `VFileSystem009`
- game DLL and client DLL ABI surfaces
- renderer DLL API surfaces
- protocol, edict, save/restore, and demo formats
- allocator ownership rules for public APIs

## Task Discipline

- Start from `Documentation/codex/tasks.md`.
- Work on one task or a small related task group at a time.
- Update task evidence when work is complete.
- Do not delete completed tasks.
- Preserve notes about rejected approaches when they explain a decision.
- Prefer small commits with a clear behavior or documentation boundary.

## Filesystem Modernization Rules

- Treat `searchpath_t` as the legacy adapter while modernizing internals.
- Keep C callbacks callable while introducing private C++ implementation.
- Add or expand tests before moving behavior.
- Preserve search path ordering.
- Preserve loose-file versus archive precedence.
- Preserve `rodir` behavior.
- Preserve direct path quirks until tests and design notes say otherwise.
- Keep `VFileSystem009` vtable order stable.

## Testing Expectations

For documentation-only changes:

- Run `git diff --check`.

For filesystem test changes:

- Build with tests enabled.
- Run the affected filesystem tests.
- Run the no-init/interface tests when touching exported interfaces.
- Keep compiled filesystem tests under `filesystem/tests` until a root Waf
  test subproject is intentionally added.
- Use `tests/` for behavior inventories, fixture strategy, and future shared
  test harness notes.

For filesystem implementation changes:

- Run filesystem unit tests.
- Run the Windows runtime smoke test when feasible.
- Capture any relevant `fs_path` or log output in task evidence.

## Audit Trail Format

When completing a task, add evidence such as:

```text
Evidence: commit <hash>, command `<command>`, doc `<path>`, or manual smoke note.
```

For partial work, add a note:

```text
Note 2026-05-09: Found that <behavior> depends on <file/function>; leaving this
task open until <missing test/design decision>.
```

## Implementation Style

- Prefer adapters over rewrites.
- Prefer composition over broad inheritance.
- Keep public C headers C-compatible.
- Do not expose STL types through C ABI boundaries.
- Do not use exceptions across C ABI boundaries.
- Add comments only where behavior is compatibility-sensitive or non-obvious.

## Stop Conditions

Stop and ask for direction when:

- a change would break a documented compatibility boundary
- a required test cannot be run and there is no reasonable substitute
- generated or third-party files would need to be committed
- a task requires choosing between incompatible architecture directions
