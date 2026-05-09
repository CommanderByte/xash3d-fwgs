# Tests

This root folder is the coordination point for behavior tests, fixtures, and
test strategy across the fork.

The current build system already has module-local Waf tests, especially:

- `public/tests`
- `filesystem/tests`

For now, new compiled filesystem tests should continue to live in
`filesystem/tests` so they work with the existing `--enable-tests` Waf flow.
This root folder should hold cross-cutting test documentation, shared fixture
plans, baseline captures, and future harness notes. If we later add a root test
subproject, this folder can become build-integrated without moving the behavior
inventory.

## Goals

- Preserve compatibility quirks before refactoring them.
- Make behavior changes auditable.
- Prefer tiny generated fixtures over real game assets.
- Keep tests runnable without a full Half-Life install whenever possible.
- Separate unit tests from manual runtime smoke tests.

## Existing Test Entry Points

Configure with tests enabled:

```powershell
.\waf.bat configure --enable-tests --sdl2=C:\git\xash3d-fwgs\3rdparty\SDL2_VC
.\waf.bat build
```

The top-level Waf script enables standalone tests with `--enable-tests` and
registers Waf unit-test summary hooks. Individual modules add their own test
programs when `bld.env.TESTS` is true.

## Test Categories

| Category | Location | Purpose |
| --- | --- | --- |
| Public utility unit tests | `public/tests` | Exercise low-level string, parsing, math, and helper behavior. |
| Filesystem unit tests | `filesystem/tests` | Exercise `filesystem_stdio` through public module APIs. |
| Behavior inventory | `tests/*` | Describe quirks that need coverage before modernization. |
| Runtime smoke tests | `Documentation/codex/*` | Verify engine launch with real assets where needed. |

## Fixture Policy

- Generate files, folders, PAKs, ZIPs, and WADs during the test when practical.
- Keep binary fixtures tiny if they must be committed.
- Do not depend on Steam assets for unit tests.
- Use Steam assets only for documented manual smoke tests.
- Clean up temporary files created by tests.

## Audit Policy

When a behavior is captured by a test, update
`Documentation/codex/tasks.md` with:

- test file path
- command used
- commit hash when available
- any behavior notes needed for future refactors
