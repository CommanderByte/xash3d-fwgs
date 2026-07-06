# Prompt Authoring Guide

Standards for writing and maintaining `.prompt.md` and `.agent.md` files in `.github/`.

---

## Frontmatter spec

Every `.prompt.md` file must have these fields in this order:

```yaml
---
name: "Short display name (≤ 60 chars)"
description: "One sentence: what it does, when to invoke it, what it produces."
argument-hint: "what $ARGUMENTS expects"   # omit only if the prompt takes no argument
agent: agent
tools: [read, search]                       # extend as needed — see Tools policy below
model: claude-sonnet-4-6                    # see MODEL-GUIDE.md for tier recommendations
---
```

**Note**: `agent: agent` is the documented frontmatter field (VS Code docs). `mode: agent`
is not a documented field — do not use it.

If `tools:` is specified and `agent:` is omitted, VS Code defaults to agent mode automatically.
The `agent: agent` line is therefore redundant but is kept for clarity.

Every `.agent.md` file uses:

```yaml
---
name: "Display name"
description: "One sentence: role, scope, what it reports."
tools: [read, search]
model: claude-sonnet-4-6
---
```

Agents never need `mode:` or `argument-hint:`.

---

## Tools policy

### Built-in tools

| Tool name | What it does |
|---|---|
| `read` | Read files in the workspace |
| `search` | Search files in the workspace |
| `edit` | Edit files in the workspace |
| `execute` | Execute code and applications (terminal commands) |
| `todo` | Manage and track todo items for task planning |
| `agent` | Delegate tasks to other agents |
| `browser` | Open and interact with integrated browser pages |
| `vscode` | Use VS Code features |
| `web` | Fetch information from the web |

### Repo MCP servers (symbol nav, build/test, scans)

Symbol navigation and build/test come from this repo's **MCP servers**, not
from the CMake Tools / C-C++ extension language-model tools. The pinned
extensions expose no usable `*_CMakeTools` / `Get*_CppTools` LM tools —
referencing those names silently drops them. Grant the server wildcard instead:

| Server wildcard | What it provides |
|---|---|
| `cpp-lsp/*` | clangd-backed definition / references / hover for symbol nav |
| `xash-tools/*` | build, test, refresh_compile_db, compliance_scan, finish_check, limits_scan, status, whereami, checkpoint, … (same JSON as the `xash3dpp/tools/*.py` CLIs) |

Both are registered in `.vscode/mcp.json`; the CLI (`execute` + the venv
python) is the identical-JSON fallback when a server is unavailable.

### Access tiers

| Access level | `tools:` value | Use for |
|---|---|---|
| Read-only | `[read, search]` | Analysis, audits that produce reports only |
| Read + symbol nav | `[read, search, cpp-lsp/*]` | Audits and analyses that trace types/usages |
| Docs-write | `[read, search, edit]` | Prompts that write docs but not source code |
| Implementation | `[read, search, edit, execute, todo, xash-tools/*]` | Prompts that modify source, build, and test |
| Debug/bisect | `[read, search, execute, xash-tools/*]` | Regression hunting, build verification |

**Rule**: use `execute` for terminal commands — not `run` or `terminal` (those are not valid tool names).
**Rule**: symbol nav is `cpp-lsp/*`; build/test/scans are `xash-tools/*` — never the
extension `*_CMakeTools` / `*_CppTools` names (they do not resolve in the pinned extensions).
**Rule**: grant the minimum access needed. Analysis prompts that do not modify source
files must not include `edit` in their tools list.

---

## Adapters and tool invocation

### Adapters (other agent frameworks)

The `.github/` files are the single source of truth. Other frameworks get
**thin delegator adapters** that must never restate the workflow logic:

- `.claude/commands/<stem>.md` — Claude Code slash commands: frontmatter is a
  single `description:` (copied **verbatim** from the origin prompt's
  `description`), body is the standard "Read `.github/prompts/<stem>.prompt.md`
  and execute it exactly as written" delegation with `$ARGUMENTS` pass-through.
- `.opencode/commands/<stem>.md` — opencode commands (plural directory is
  opencode-canonical): same shape.
- `.claude/agents/<stem>.md` — Claude subagents: Claude dialect frontmatter
  (`tools: Read, Grep, Glob`; `model:` alias per the MODEL-GUIDE canonical
  table) + a body that defers to the `.github/agents/` charter.
- `.opencode/agents/<stem>.md` — opencode subagents: `mode: subagent`,
  `model:` per the MODEL-GUIDE opencode column, read-only lockdown
  (`tools: {write: false, edit: false}`, `permission: {bash: deny}`), body
  defers to the `.github/agents/` charter.
- Codex CLI has no repo slash-command adapter surface. It is first-class by
  reading the canonical `.github/prompts/<stem>.prompt.md` directly; use
  `xash3dpp/tools/agent_workflow.py command codex <stem> [args]` to print
  the exact `codex exec` invocation.

Edit the `.github/` originals, never the adapters; `xash3dpp/tools/
workflow_sync.py` enforces existence, origin-path pointers, description
parity, model-tier parity across dialects, MCP registration parity, and the
Codex direct-read helper contract.

### Tool invocation from prompts

Mechanical work (grep sweeps, checklists, build/test, limits parsing) is
done by the `xash3dpp/tools/` scripts — prompts invoke them and interpret
the JSON instead of embedding recipes:

- Invocation line: `& .venv\Scripts\python.exe xash3dpp\tools\<script>.py … --json`
  (`python` = the repo venv; scripts are stdlib-only).
- **MCP preference**: when the host exposes the `xash-tools` MCP server,
  call the MCP tool of the same name instead of shelling out; the CLI is
  the fallback and returns identical JSON `data`. Exposed tools: build,
  test, refresh_compile_db, compliance_scan, status (= status_table),
  finish_check, stub_scan, crosswalk, limits_scan, slice_diff,
  markdown_lint, workflow_sync, whereami, checkpoint. Exception: `dep_scan`
  is CLI-only by design.
- A prompt that invokes a tool needs `execute` in its `tools:` list.
- Keep a short `### Manual fallback (no Python available)` appendix with the
  condensed recipe whenever the tool replaces one.
- Findings marked `candidate-*` are heuristics: the prompt must tell the
  agent to confirm or discard them, never to relay them blindly.
- Never re-embed a recipe a tool covers — extend the tool
  (`xash3dpp/tools/xtools/`) instead, and keep `tools/README.md` in sync.

---

## Required sections by prompt type

| Section | Analysis prompt | Implementation prompt | Scaffolding prompt |
|---|---|---|---|
| **Opening paragraph** — scope + `$ARGUMENTS` usage | ✓ required | ✓ required | ✓ required |
| **Guardrails** — explicit "do not modify X" or safety constraints | one-liner: "Do not modify source files." | full block | minimal |
| **Reference Documents** | if ≥ 2 external docs needed | always | if ≥ 2 external docs needed |
| **Step/Check structure** | ✓ required | ✓ required | ✓ required |
| **Done Condition** | optional — add when the definition of "done" is non-obvious | ✓ required | ✓ required |
| **Commit step** | never — analysis prompts do not commit | ✓ required | if files are created |

---

## $ARGUMENTS conventions

| Convention | Value | Examples |
|---|---|---|
| **Subsystem name** (default) | snake_case directory name under `xash3dpp/src/` | `networking`, `cmd_cvar`, `utilities` |
| **Folder path** | repo-relative path when a folder is the subject | `xash3dpp/src/filesystem` |
| **Rule token** | `RULE_NAME: description` colon-separated pair | `NODISCARD: add [[nodiscard]] to all Result<T> returns` |

The `argument-hint` field must state which convention the prompt uses.
Omit `argument-hint` only when the prompt takes no argument at all (e.g. `init`, `status-and-next`).

---

## Commit message format

All prompts that produce a commit must use the project convention:

```text
tag: short description

- one line per logical change
- group by check category or subsystem

Co-Authored-By: <agent/model> <noreply@provider>
```

Where `tag` is the subsystem name or a short file-based tag (e.g. `networking`, `limits`,
`prompts`). **Never** use Conventional Commits format (`feat(scope):`, `refactor(scope):` etc.).
The `Co-Authored-By` trailer is mandatory for every agent commit regardless of
framework. Generate it with
`xash3dpp/tools/agent_workflow.py coauthor <framework> [model]`, or write the
same trailer manually when the model identity is clearer from the active
session.

---

## Structural patterns

Use **numbered steps** (`## Step 1 — Name`) for linear workflows.  
Use **named checks** (`### CHECK-NAME — description`) for audit/compliance passes.  
Use **phases** (`## Phase N — Name`) only for prompts with genuinely distinct macro-stages
(e.g. analysis → gate decision → apply).

Do not mix patterns within one prompt.

---

## Model selection

See [MODEL-GUIDE.md](MODEL-GUIDE.md) for the authoritative tier recommendations per prompt.
The `model:` field in frontmatter is the default; the user can override at invocation time.

Defaults:

- Analysis prompts: `claude-haiku-4-5-20251001` for mechanical checks, `claude-sonnet-4-6` for
  reasoning-heavy analysis, `claude-opus-4-7` for deep architectural or concurrency work.
- Implementation prompts: `claude-sonnet-4-6`.
- Scaffolding prompts: `claude-sonnet-4-6`.
