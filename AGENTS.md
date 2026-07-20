# Repo guide for coding agents

Read by Codex CLI, opencode, and VS Code Copilot (Claude Code primarily
reads `CLAUDE.md`). The SYNC-CORE blocks below are byte-identical between
this file and `CLAUDE.md` (enforced by `xash3dpp/tools/workflow_sync.py`).
Edit them in both files together; the tails are framework-specific.

## Repo truth

<!-- SYNC-CORE:BEGIN repo-truth -->
Fork of Xash3D FWGS. The **legacy C engine at the repository root is
reference-only** — never modify it; read it for behaviour and ABI contracts.
All new work happens in **`xash3dpp/`**, a self-contained C++23 rewrite (no
exceptions `/EHs-c-`, no RTTI `/GR-`). `xash3dpp/docs/implementation-plan.md`
is the authoritative chunk plan and status — if any other doc or comment
contradicts its numbering, the plan wins (chunks renumbered 2026-07-04;
numbers are frozen, Chunk 4 is a tombstone).
<!-- SYNC-CORE:END repo-truth -->

## Build & test

<!-- SYNC-CORE:BEGIN build-test -->
Build/test ladder (prefer the top):

1. **MCP**: if the host exposes the `xash-tools` MCP server, use its
   `build` / `test` / `refresh_compile_db` tools.
2. **CLI** (identical JSON):

   ```powershell
   & .venv\Scripts\python.exe xash3dpp\tools\build.py --json
   & .venv\Scripts\python.exe xash3dpp\tools\test.py --json
   ```

3. **Raw fallback**: the VS2022-bundled cmake/ctest (NOT on PATH) with
   presets `debug-msvc` / `debug`, run from `xash3dpp/`. Stale generator ⇒
   delete `xash3dpp/build/Debug` and reconfigure.

Paths, env overrides (`XASH_*`), and the full tool table:
`xash3dpp/tools/README.md`.
<!-- SYNC-CORE:END build-test -->

## Where the truth lives

<!-- SYNC-CORE:BEGIN where-truth-lives -->

- `.github/instructions/xash3dpp.instructions.md` — binding C++ conventions.
- `xash3dpp/docs/design/decisions-architecture.md` — Q-1..Q-24 register;
  §3a is the boundary-spec OQ crosswalk (blocks-scaffold rows gate work);
  §4.3 applies to all new code.
- `.github/WORKFLOW.md` + `.github/prompts/*.prompt.md` (23) +
  `.github/agents/*.agent.md` (4) — the single source of truth for the
  workflow. Adapters in `.claude/commands|agents/` and
  `.opencode/commands|agents/` are thin delegators — **edit the `.github/`
  originals, never the adapters**.
- `xash3dpp/tools/` — deterministic workflow tooling (see its README); MCP
  servers `xash-tools` + `cpp-lsp` registered in `.mcp.json`,
  `.vscode/mcp.json`, `opencode.json`, `.codex/config.toml`.
- **State**: run `whereami` (MCP tool or `xash3dpp/tools/whereami.py`,
  `--doctor` after dormancy) at session start; record a `checkpoint` at
  every commit, handoff, or interruption. `.agent-checkpoints.jsonl` is
  advisory — ground truth is always derived.
- `xash3dpp/docs/legacy-survey/deep-dive-*.md` — committed recon; read it
  instead of re-deriving. Boundary specs: `xash3dpp/docs/boundaries/`.
- Setup matrix (all frameworks + humans): `.github/AGENT-SETUP.md`.
- After ANY edit to the workflow surface, run
  `xash3dpp/tools/workflow_sync.py`.

<!-- SYNC-CORE:END where-truth-lives -->

## Doc-trust warnings

<!-- SYNC-CORE:BEGIN doc-trust -->
`Documentation/codex/**` describes an **abandoned** earlier rewrite effort
("xash-ng") — it is NOT related to the Codex CLI agent, its plans are
aspirational, and file paths cited there may not exist. Do not treat it as
current. Boundary specs and the decision register are current and binding.
<!-- SYNC-CORE:END doc-trust -->

## Commit style

<!-- SYNC-CORE:BEGIN commit-style -->
`tag: description` (e.g. `networking: ...`, `docs: ...`) — never
Conventional Commits. One green (build + tests) commit per stable state;
one session per commit (see `.github/MODEL-GUIDE.md`). Every agent commit,
from every framework, ends with a `Co-Authored-By` trailer naming the
agent/model; generate it with `xash3dpp/tools/agent_workflow.py coauthor
<framework> [model]` when unsure. Record a `checkpoint` after each commit.
<!-- SYNC-CORE:END commit-style -->

## Codex CLI notes

- Codex has no per-command mechanism: to run a workflow step, **open
  `.github/prompts/<name>.prompt.md`, read it, and follow it exactly**
  (same 22 steps the other frameworks expose as commands).
- To list prompts or print the exact framework-specific invocation, run
  `& .venv\Scripts\python.exe xash3dpp\tools\agent_workflow.py list` or
  `& .venv\Scripts\python.exe xash3dpp\tools\agent_workflow.py command codex <name> [args]`.
- MCP servers come from the committed `.codex/config.toml` after the
  one-time trust step (`.github/AGENT-SETUP.md` §Codex). Codex never reads
  `.github/` files automatically — only this AGENTS.md.
- Verify wiring with `codex mcp list`; `xash-tools` and `cpp-lsp` should be
  enabled, with checkpoints recorded as actor `codex`.
- Keep this file lean: Codex budgets ~32 KiB for combined AGENTS.md
  content.

## opencode notes

- The 22 workflow steps are `/`-commands from `.opencode/commands/`
  (same names as the prompt stems); subagents live in `.opencode/agents/`.
- `agent_workflow.py command opencode <name> [args]` prints the same
  invocation from the canonical `.github/prompts/` metadata.
- `opencode.json` registers both MCP servers and additionally loads
  `.github/copilot-instructions.md` via its `instructions` array
  (additive with this file).
- opencode auto-loads AGENTS.md in preference to CLAUDE.md — a no-op here
  because the core blocks are identical.
