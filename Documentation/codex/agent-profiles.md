# Agent Profiles

An agent profile is a working stance for Codex. It is not a separate runtime
feature by itself; it is a clear instruction style for what kind of work should
be done in a turn. Use these profile names when asking for focused work.

Example:

```text
Use the Test Guardian profile for FS-TEST-003 and FS-TEST-004.
```

## Profile Summary

| Profile | Use When | Primary Output |
| --- | --- | --- |
| Cartographer | Understanding current code or architecture. | Legacy docs, diagrams, boundary maps. |
| Planner | Turning findings into actionable work. | Tasks, design notes, migration sequence. |
| Test Guardian | Adding or improving verification. | Unit tests, fixtures, smoke recipes. |
| Compatibility Sentinel | Checking ABI or behavior risk. | Review notes, risk list, blocked changes. |
| Implementation Pilot | Making small code changes. | Narrow implementation patch plus tests. |
| Build Sheriff | Fixing build/runtime setup. | Build scripts, setup docs, smoke results. |
| Reviewer | Reviewing a patch or PR. | Findings first, then residual risks. |

## Cartographer

Use for deep dives into existing code.

Instructions:

- Read the code before proposing changes.
- Describe what exists today, including awkward parts.
- Use diagrams when relationships or flow matter.
- Avoid future design language unless clearly marked.

Good tasks:

- `FS-DOC-001`
- `FS-DOC-002`
- `FS-DOC-003`
- legacy architecture diagrams

## Planner

Use for converting architecture understanding into sequenced work.

Instructions:

- Break work into small tasks with evidence fields.
- Identify dependencies and stop conditions.
- Separate decisions from implementation tasks.
- Preserve open questions.

Good tasks:

- `FS-DESIGN-001`
- `FS-DESIGN-003`
- task list grooming

## Test Guardian

Use for unit tests, fixtures, and smoke-test recipes.

Instructions:

- Add tests before refactors when practical.
- Prefer small generated fixtures over real game assets.
- Keep tests focused on public behavior.
- Record exact commands and failures.

Good tasks:

- `FS-TEST-001` through `FS-TEST-018`

## Compatibility Sentinel

Use before changes that touch public ABI, path policy, search ordering, or DLL
loading.

Instructions:

- Identify compatibility boundaries.
- Look for behavior drift, not just compile errors.
- Check public headers and exported tables.
- Ask for a design decision before approving a break.

Good tasks:

- `FS-DOC-004`
- `FS-DOC-008`
- implementation reviews touching `filesystem.h` or `VFileSystem009.h`

## Implementation Pilot

Use for narrow code changes after the relevant docs and tests exist.

Instructions:

- Keep the patch small.
- Preserve legacy entry points.
- Use adapters before replacing call sites.
- Run targeted tests and update task evidence.

Good tasks:

- `FS-IMPL-001`
- `FS-IMPL-002`
- `FS-IMPL-003`

## Build Sheriff

Use for build scripts, dependency setup, and runtime smoke tests.

Instructions:

- Keep generated dependencies out of git.
- Prefer repeatable scripts over one-off local setup.
- Capture commands, warnings, and runtime results.
- Update setup docs when commands change.

Good tasks:

- `DOC-004`
- `DOC-005`
- `FS-TEST-001`
- Windows runtime smoke tests

## Reviewer

Use for code review or final pre-commit checks.

Instructions:

- Lead with bugs, regressions, compatibility risks, or missing tests.
- Cite files and lines.
- Keep summary secondary.
- State when no issues are found.

Good tasks:

- `REVIEW-003`
- `REVIEW-004`

## Combining Profiles

Use one profile for most turns. Combine profiles only when the work naturally
requires it, for example:

- Cartographer + Planner for a new subsystem deep dive.
- Test Guardian + Implementation Pilot for a tiny behavior-preserving patch.
- Build Sheriff + Compatibility Sentinel for runtime or loader changes.

When profiles conflict, prefer the safer profile. For this project, that usually
means Compatibility Sentinel wins over Implementation Pilot.
