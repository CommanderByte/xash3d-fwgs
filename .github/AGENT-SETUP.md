# Agent & Human Setup

Per-framework wiring for this repo's agent workflow. The `.github/` files
(WORKFLOW.md, prompts, agent charters, instructions) are the single source
of truth; every framework below consumes them through thin adapters and
the same MCP servers. If you change any of that surface, run
`& .venv\Scripts\python.exe xash3dpp\tools\workflow_sync.py` before
committing.

## Framework / entry-file matrix

| Framework | Entry file | Commands | Subagents | MCP registration | Setup needed |
|-----------|-----------|----------|-----------|------------------|--------------|
| Claude Code | `CLAUDE.md` | `.claude/commands/` (21) | `.claude/agents/` (3) | `.mcp.json` | none |
| VS Code Copilot | `AGENTS.md` + `.github/copilot-instructions.md` | `.github/prompts/` (native) | `.github/agents/` (native) | `.vscode/mcp.json` | none (settings committed) |
| opencode | `AGENTS.md` (+ `instructions` array) | `.opencode/commands/` (21) | `.opencode/agents/` (3) | `opencode.json` | none |
| Codex CLI | `AGENTS.md` | read the prompt file directly | — | `.codex/config.toml` | one-time trust step |

## Claude Code

Zero setup — `CLAUDE.md`, the adapters, `.claude/settings.json`
(shared permission allowlist), and `.mcp.json` are committed. Personal
permission overrides go in `.claude/settings.local.json` (gitignored).
**After editing `.mcp.json`, restart the session** for MCP changes to
apply.

## VS Code Copilot

`.vscode/settings.json` commits the discovery toggles explicitly
(`chat.promptFiles`, `chat.useAgentsMdFile`, `chat.useNestedAgentsMdFiles`,
`github.copilot.chat.codeGeneration.useInstructionFiles`) because their
defaults vary by VS Code version. `.vscode/mcp.json` starts both servers
via `${workspaceFolder}` paths. Prompts appear as `/`-commands from
`.github/prompts/`; instructions auto-apply from
`.github/instructions/xash3dpp.instructions.md`.

## opencode

`opencode.json` registers both MCP servers and loads
`.github/copilot-instructions.md` via its `instructions` array; the root
`AGENTS.md` is auto-loaded (it wins over `CLAUDE.md`, which is a no-op
here — the core blocks are byte-identical twins). Commands live in
`.opencode/commands/` and subagents in `.opencode/agents/` (plural dirs
are opencode-canonical).

## Codex CLI

Codex reads **only** the root `AGENTS.md` automatically (never `.github/`
files) — to run a workflow step, open `.github/prompts/<name>.prompt.md`,
read it, and follow it exactly. The committed `.codex/config.toml`
registers both MCP servers once you trust the project in your
**user-global** `~/.codex/config.toml`:

```toml
[projects."c:\\git\\xash3d-fwgs"]
trust_level = "trusted"
```

If your Codex build does not resolve the committed repo-relative commands
against the project root, register the servers user-globally with
absolute paths instead:

```toml
[mcp_servers.xash-tools]
command = "c:/git/xash3d-fwgs/.venv/Scripts/python.exe"
args = ["c:/git/xash3d-fwgs/xash3dpp/tools/mcp_server.py"]

[mcp_servers.cpp-lsp]
command = "c:/git/xash3d-fwgs/.venv/Scripts/python.exe"
args = ["c:/git/xash3d-fwgs/xash3dpp/tools/cpp_lsp_launcher.py"]
```

## Human quickstart

This repo rewrites the Xash3D FWGS engine as `xash3dpp/` (C++23), one
scoped chunk at a time, with agents doing the work and deterministic
tools doing the checking. You drive it from any framework above.

**The one command that always tells you where things stand:**

```powershell
& .venv\Scripts\python.exe xash3dpp\tools\whereami.py --doctor
```

(or the `whereami` MCP tool from inside any agent session). It reports
git state, chunk statuses, gate results, blocking open questions, recent
checkpoints, and a suggested next action.

| You are… | Do this |
|----------|---------|
| Brand new here | Read `WORKFLOW.md`, then run `whereami --doctor`, then the `init` command in your framework |
| Back after weeks/months (dormancy) | `whereami --doctor` → fix any failing env checks → `refresh_compile_db.py` → `build.py` + `test.py` → read the plan's "Tooling / automation follow-ups" |
| Resuming yesterday's work | `whereami` — the last checkpoint note says where you left off |
| About to start new engine work | Follow `whereami`'s `suggested_next` (it enforces the recon → decisions → scaffold gates) |
| Seeing weird workflow behaviour | `workflow_sync.py --json` — drift findings name the broken invariant |

Convention: agents (and you) record a `checkpoint` at every commit,
handoff, or interruption — that note is what the next session sees first.

## Environment overrides

All tooling resolves paths via env override → vswhere → PATH:

| Variable | Overrides |
|----------|-----------|
| `XASH_CMAKE` / `XASH_CTEST` | VS2022-bundled cmake/ctest |
| `XASH_VSDEVCMD` | `VsDevCmd.bat` (compile-DB refresh) |
| `XASH_CLANGD` | clangd used by the `cpp-lsp` server |
| `XASH_MCP_LANGUAGE_SERVER` | `mcp-language-server` binary |
| `XASH_CHECKPOINT_ACTOR` | default actor label in checkpoints |

## Name collision warning

`Documentation/codex/**` is the **abandoned** "xash-ng" rewrite effort —
it has nothing to do with the Codex CLI agent or `.codex/config.toml`.
Do not treat its plans or paths as current.
