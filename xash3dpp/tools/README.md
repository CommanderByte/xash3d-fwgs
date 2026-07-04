# xash3dpp/tools — deterministic agent-workflow tooling

Python scripts that do the mechanical parts of the `.github/` workflow
(grep sweeps, checklists, build/test invocation, drift checking) so agents
spend tokens on judgment, not plumbing. The workflow prompts invoke these as
their primary path; each prompt keeps a short manual fallback.

## Invocation

All scripts run with the repo venv (Python 3.14, no install step needed —
the `xtools/` library resolves via `sys.path[0]`):

```powershell
c:\git\xash3d-fwgs\.venv\Scripts\python.exe xash3dpp\tools\<script>.py [args] --json
```

Prompts abbreviate this to `python xash3dpp/tools/<script>.py`; `python`
always means the repo venv interpreter. The CLI scripts are **stdlib-only**;
only `mcp_server.py` needs the `mcp` package (see root `requirements.txt`).

## Output envelope and exit codes

Every CLI prints (with `--json`, always machine-readable):

```json
{ "tool": "<name>", "version": 1, "ok": true, "data": { ... }, "errors": [] }
```

Exit codes: `0` clean · `1` findings/failures present · `2` execution error.

## Scripts

| Script | Purpose | Consumed by |
|--------|---------|-------------|
| `build.py` | Configure/build via the VS2022-bundled cmake (presets `debug-msvc`/`debug`); parsed `error C…` list | sweep-module, implement-audit, retriever, bisect, write-unit-tests, pre-pr |
| `test.py` | `ctest --preset debug` (optional `-R` filter); pass/fail breakdown | same set + `finish_check.py` |
| `refresh_compile_db.py` | `VsDevCmd -arch=x64 && cmake --preset clangd` → regenerates `build/clangd/compile_commands.json` for the cpp-lsp/clangd MCP server | manual, after adding files/targets |
| `compliance_scan.py` | The reviewer charter's [M] checks + pre-pr/sweep/detail grep sweeps as JSON violations (`--checks all\|prepr\|detail\|id,…`) | pre-pr Phase 2, sweep-module Step 2, detail-audit, reviewer pre-pass |
| `limits_scan.py` | Parses `limits.hpp` `XASH_LIMIT_*` blocks; magic-number/shadow/dead-limit report | limits-audit, detail-audit, finish_check |
| `stub_scan.py` | TODO/stub markers with enclosing symbol; live-vs-stub test tally | plan-implementation, status-and-next |
| `status_table.py` | Regenerates the subsystem status table from the tree; `--check` diffs vs implementation-plan.md | status-and-next; plan refresh |
| `finish_check.py` | The 9-section done checklist as pass/fail/needs-judgment JSON — single source for finish-subsystem AND pre-pr Phase 1 | finish-subsystem, pre-pr |
| `dep_scan.py` | Dependency edges (cross-namespace refs) + InitParams inventory + cycle check | dependency-graph |
| `workflow_sync.py` | Drift checker: frontmatter schema, model dialects vs MODEL-GUIDE canonical table, adapter parity, twin-entry-file SYNC-CORE blocks, ABI single-source, MCP registrations, doc counters. `--stage 1` = tooling-session subset | run after ANY workflow-surface edit |
| `cpp_lsp_launcher.py` | Portable launcher for the `cpp-lsp` MCP server: resolves clangd + mcp-language-server via vswhere/PATH/env instead of hardcoded machine paths | `.mcp.json` / `.vscode/mcp.json` |
| `mcp_server.py` | `xash-tools` FastMCP server (stdio) exposing build/test/refresh_compile_db/compliance_scan/status/finish_check/stub_scan/limits_scan/markdown_lint | Claude Code, VS Code, opencode MCP configs |

## Path resolution / env overrides

`xtools/vsenv.py` finds tooling via (in order): env override → vswhere →
PATH. Overrides: `XASH_CMAKE`, `XASH_CTEST`, `XASH_VSDEVCMD`, `XASH_CLANGD`,
and `XASH_MCP_LANGUAGE_SERVER` (cpp_lsp_launcher only).

## Codex CLI note

Codex reads the root `AGENTS.md`. To give Codex the `xash-tools` MCP server,
add to your **user-global** `~/.codex/config.toml` (not committed — Codex
has no in-repo config):

```toml
[mcp_servers.xash-tools]
command = "c:/git/xash3d-fwgs/.venv/Scripts/python.exe"
args = ["c:/git/xash3d-fwgs/xash3dpp/tools/mcp_server.py"]
```

## Library layout

`xtools/` — shared implementation (`rules.py` is the data-only ruleset with
`source_ref` back to the charter/prompt each rule came from; `checks.py`
scanners; `buildtools.py` build/test/refresh; `sync.py` drift invariants;
`vsenv.py` tool discovery; `scan.py` comment-aware C++ line iteration;
`report.py` envelope/exit codes). CLIs and the MCP server are thin wrappers
so behaviour is defined exactly once.
