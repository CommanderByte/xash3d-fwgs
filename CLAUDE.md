# Repo guide for Claude Code

Fork of Xash3D FWGS. The **legacy C engine at the repository root is
reference-only** — do not modify it. All new work happens in **`xash3dpp/`**,
a self-contained C++23 rewrite (no exceptions, no RTTI).

## Build & test (xash3dpp)

`cmake`/`ctest` are **not on PATH**. Use the VS2022 Community bundled ones:

```sh
CMAKE="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
CTEST="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"

cd xash3dpp
"$CMAKE" --preset debug-msvc        # configure (VS 17 2022, x64 → build/Debug)
"$CMAKE" --build --preset debug     # build
"$CTEST" --preset debug             # run tests (from xash3dpp/)
```

If configure fails with a generator/platform mismatch, delete the stale
`xash3dpp/build/Debug` directory and reconfigure (build/ is gitignored).

## Where the truth lives

- `xash3dpp/docs/implementation-plan.md` — **authoritative** chunk plan and
  status (5 = map_loader ✅, 6 = server, 7 = content, 9 = sound, 12 =
  client, 13 = renderer). Older docs/comments that used a pre-map_loader
  numbering were renumbered to this scheme on 2026-07-04 — if you spot a
  chunk reference that contradicts the plan, the plan wins.
- `.github/instructions/xash3dpp.instructions.md` — binding conventions
  (naming, error handling, memory, limits, tests). Follow exactly.
- `xash3dpp/docs/design/decisions-architecture.md` — Q-1..Q-18 decision
  register; §4.3 lists what applies to all new code.
- `.github/WORKFLOW.md` + `.github/prompts/*.prompt.md` — the pipeline for
  implementing a subsystem; `.github/agents/*.agent.md` — verification
  subagent roles (reviewer, ABI watchdog, legacy parity auditor).
- **Claude Code adapters** (thin wrappers; the `.github/` files stay the
  single source of truth): `.claude/agents/` exposes the three verification
  roles as subagent types (`xash3dpp-reviewer`, `abi-watchdog`,
  `legacy-parity-auditor`); `.claude/commands/` exposes the recurring
  workflow prompts as slash commands (`/status-and-next`,
  `/analyse-subsystem`, `/plan-implementation`, `/write-unit-tests`,
  `/sweep-module`, `/analyse-threading`, `/document-architecture`,
  `/finish-subsystem`, `/pre-pr`, `/assess-impact`, `/detail-audit`,
  `/limits-audit`). Edit the `.github/` originals, not the adapters.
- `.mcp.json` wires a `cpp-lsp` server (mcp-language-server + VS clangd)
  for symbol navigation over `xash3dpp/`; refresh its compile database from
  a VS x64 dev prompt with `cmake --preset clangd` (see `xash3dpp/.clangd`).
- `xash3dpp/docs/legacy-survey/` — behavioural reference notes on the legacy
  engine. **Convention:** recon agents targeting a chunk commit their brief
  here as a `deep-dive-*.md` before implementation starts, so future
  sessions read instead of re-deriving.

## Doc-trust warnings

- `Documentation/codex/**` describes an **abandoned** earlier rewrite effort
  ("xash-ng"; renamed/superseded by xash3dpp). Its plans are aspirational —
  file paths cited there may not exist. Do not treat it as current.
- Boundary specs under `xash3dpp/docs/boundaries/` and the decision register
  are current and binding.

## Commit style

`tag: description` (e.g. `networking: ...`, `build: ...`, `docs: ...`), one
green (build + tests pass) commit per state. Trailer:
`Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.
