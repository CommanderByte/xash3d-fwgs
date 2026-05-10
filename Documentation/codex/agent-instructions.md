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

## Automation Helpers

Prefer the local scripts for repeated phase mechanics:

- `scripts/scaffold-modern-helper.ps1`: create modern helper header/source,
  focused test, optional C adapter, and Waf test/adapter wiring.
- `scripts/new-phase.ps1`: insert a standard phase skeleton into
  `Documentation/codex/tasks.md` and optionally append to a TODO file.
- `scripts/phase-status.ps1`: summarize open phase items and missing evidence.
- `scripts/append-phase-evidence.ps1`: append a task evidence line and
  optionally mark the task done.
- `scripts/doc-todo-rollup.ps1`: count open/done checkbox items across active
  task and TODO documents.
- `scripts/legacy-modernization-scan.ps1`: compare a legacy tree with a modern
  tree and list likely unmigrated files by basename.
- `scripts/precommit-phase.ps1`: run `git diff --check` and optional phase
  validation before a manual commit.
- `scripts/refresh-runtime-binaries.ps1`: copy current build outputs into
  `run-win32` without running the full validation harness.
- `scripts/runtime-diagnose.ps1`: inspect runtime DLL freshness, stale
  `xash3d.exe` processes, `XASH3D_RODIR`, `gfx.wad`, and the latest smoke log.
- `scripts/run-phase-validation.ps1`: run focused validation, build `xash`,
  run full tests, refresh runtime DLLs, run the `+wait +wait` smoke, and print
  a copy-pasteable evidence line.
- `scripts/ask-local-model.ps1`: ask a local LM Studio model for read-only
  analysis and write the scratch report under `.codex-cache/local-agent/`.
- `scripts/start-local-model-ask.ps1`: launch the same helper asynchronously
  with a prompt file, request JSON, PID status, and stdout/stderr logs.
- `scripts/lmstudio-mcp-server.py`: expose the same local LM Studio helper as
  a read-only stdio MCP server for clients that can register local MCP tools.

Use the scripts as helpers, not as a substitute for judgment. Bugs, crashes,
ABI questions, and compatibility surprises still require a focused deep dive.
Do not use `-AllowSmokeNonZeroExit` as green evidence; it is for investigating
known post-first-frame shutdown flakes only.
Do not run helpers that mutate the same document, such as
`append-phase-evidence.ps1`, in parallel.
Use local LM Studio output as scratch analysis only; never treat it as test
evidence or a replacement for direct code inspection.
When LM Studio is running with a large context window, such as 64K, prefer using
it asynchronously for bounded sidecar scouting while continuing direct code
inspection locally. Keep prompts file-scoped, and do not put local-model output
on the critical path unless the task is explicitly exploratory. Use
`start-local-model-ask.ps1` for hidden/background jobs so argument quoting does
not split prompts into positional PowerShell parameters.

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
- Keep compiled filesystem tests under `tests/filesystem`, built by
  `filesystem/wscript`, until a shared root Waf test subproject is
  intentionally added.
- Use `tests/` for behavior inventories, fixture strategy, generated fixtures,
  and future shared test harness notes.

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
