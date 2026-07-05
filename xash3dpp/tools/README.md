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
| `build.py` | Configure/build via the VS2022-bundled cmake; parsed `error C…` list. `--arch x64` (default) or `--arch x86` selects the width — x86 drives the 32-bit chain (configure `debug-msvc-x86` → `build/Debug-x86`) for the retail 32-bit GoldSrc dlls/hl.dll (S15). `--preset` names the configuration (`debug`/`release`) orthogonally | sweep-module, implement-audit, retriever, bisect, write-unit-tests, pre-pr |
| `test.py` | `ctest --output-on-failure` (optional `-R` filter); `--arch x64`(default)`/x86` picks the suite (x86 = test preset `debug-x86`; build that arch first); pass/fail breakdown; failed tests carry their output block (LastTest.log fallback for crashed children), an `assert_tail` (the assertion/abort message window — the XASH_ASSERT/REQUIRE/CHECK line, so exit-3 aborts are diagnosable without a re-run), and a decoded exit code (STATUS_BREAKPOINT/ACCESS_VIOLATION/…) | same set + `finish_check.py` |
| `refresh_compile_db.py` | `VsDevCmd -arch=x64 && cmake --preset clangd` → regenerates `build/clangd/compile_commands.json` for the cpp-lsp/clangd MCP server | manual, after adding files/targets |
| `compliance_scan.py` | The reviewer charter's [M] checks + pre-pr/sweep/detail grep sweeps as JSON violations (`--checks all\|prepr\|detail\|id,…`). Scope: a subsystem, `--files a,b,…`, or `--slice` (the slice_diff change set — slices cross subsystem boundaries; gates scan what changed). ABI-forced constructs a rule can't know about carry an inline `// compliance-allow(<check-id>): <rationale>` on the flagged line; every allow is echoed in the result's `allows` list for pre-pr audit. Q-22/QN checks (2026-07-06): `class-operator-new`, `make-unique-outside-pimpl`, `operator-delete-pairing` (structured), `post-annotation-retired`, `unsafe-cast-safety-comment` + `lifetime-annotation` (detail-set candidates). `--checks annotation-coverage` returns the QN coverage report instead — per-subsystem denominators (`required`/`annotated`/`exempt` per marker, honoring `@annotation-exempt:`), the Chunk 6B backfill measure. Suppression markers (`@pre-reserved:`, `@lifetime:`, `SAFETY:`) match the RAW line — they live in comments | pre-pr Phase 2, sweep-module Step 2, detail-audit, reviewer pre-pass; per-slice gates; 6B coverage |
| `limits_scan.py` | Parses `limits.hpp` `XASH_LIMIT_*` blocks; magic-number/shadow/dead-limit report | limits-audit, detail-audit, finish_check |
| `stub_scan.py` | TODO/stub markers with enclosing symbol; live-vs-stub test tally; `by_tag` counts markers by their parenthesized tag (chunk6/chunk6-S9/S8-seam/…); `--delta` adds the per-tag net change vs HEAD~1 (surfaces net-zero marker churn) | plan-implementation, status-and-next |
| `crosswalk.py` | Legacy↔xash3dpp symbol index: resolve a legacy engine symbol (`SV_Multicast`) or file:line (`sv_game.c:4026`) to its C++ port, from the inline port annotations + the deep-dive recon docs; every hit carries `source` + `confidence` (server resolves function-level, networking/map_loader file-level). `--missing` lists deep-dive-documented symbols with no code port yet | recon during implement / parity; analyse-subsystem, retriever |
| `status_table.py` | Regenerates the subsystem status table from the tree (rows carry stub-marker counts; `complete_with_stubs` lists structurally-Complete subsystems still holding TODO/stub markers — structural completeness can hide unfinished work); `--check` diffs vs implementation-plan.md | status-and-next; plan refresh |
| `finish_check.py` | The 10-section done checklist as pass/fail/needs-judgment JSON — single source for finish-subsystem AND pre-pr Phase 1; `--run-tests` runs the subsystem's ctest set (honors `--arch x64\|x86`). Item 2 is QO-classified (wire/ABI-frozen `k_*`/abi-path literals machine-exempt); item 10 = Q-22/QN lifecycle + annotation-coverage gate | finish-subsystem, pre-pr |
| `dep_scan.py` | Dependency edges (cross-namespace refs) + InitParams inventory + cycle check | dependency-graph |
| `slice_diff.py` | Change inventory since a base ref (default: HEAD when the tree is dirty at a checkpointed commit — the slice is the uncommitted work — else the last differing checkpoint head): files + add/delete counts + untracked, optional capped patch — brief gate agents from ground truth, not hand-typed file lists | abi-watchdog / reviewer / parity-auditor invocations; `compliance_scan --slice` |
| `workflow_sync.py` | Drift checker: frontmatter schema, model dialects vs MODEL-GUIDE canonical table, adapter parity, twin-entry-file SYNC-CORE blocks, ABI single-source, MCP registrations, doc counters. `--stage 1` = tooling-session subset | run after ANY workflow-surface edit |
| `whereami.py` | Ground-truth session brief (git, plan/chunk status incl. stub debt in Complete subsystems, gates, blocking OQs, checkpoints with staleness/concurrency flags, suggested next action); `--doctor` adds environment checks. Chunk headings may carry a letter suffix (`### Chunk 6B — …`, label `6B`, sorted 6 < 6B < 7); the **Session ladder** parse is scoped to the active (in-progress, else first todo) chunk so multiple ladder lines don't mis-attribute | session start, dormancy recovery |
| `checkpoint.py` | Append an advisory checkpoint (intent record) to `.agent-checkpoints.jsonl` | every commit / handoff / interruption |
| `cpp_lsp_launcher.py` | Portable launcher for the `cpp-lsp` MCP server: resolves clangd + mcp-language-server via vswhere/PATH/env instead of hardcoded machine paths | `.mcp.json` / `.vscode/mcp.json` |
| `mcp_server.py` | `xash-tools` FastMCP server (stdio) exposing build/test/refresh_compile_db/compliance_scan/status/finish_check/stub_scan/crosswalk/limits_scan/slice_diff/markdown_lint/workflow_sync/whereami/checkpoint. Hot-reloads the xtools modules when their sources change on disk (tool registrations still need a session restart) | Claude Code, VS Code, opencode, Codex MCP configs |

## State & checkpoints

`.agent-checkpoints.jsonl` (repo root, **gitignored**) is an append-only
JSONL of advisory checkpoints:
`{ts, actor, session, branch, head, chunk, step, note, dirty}`.
Checkpoints record **intent** for resumption — they are never
authoritative. `whereami` derives ground truth (git / implementation-plan /
sync gates / OQ crosswalk) every time and flags a checkpoint as **stale**
when its recorded `head` no longer matches HEAD, and warns when ≥2 distinct
sessions checkpointed within 24 h (concurrent-session detection).
Defaults: `actor` = `XASH_CHECKPOINT_ACTOR` env or `agent`; `session` =
`<actor>-<utc-date>` — pass explicit `--session` ids when running multiple
concurrent sessions. The committed MCP configs pin the actor per
framework (`.mcp.json` → claude, `.vscode/mcp.json` → copilot,
`opencode.json` → opencode, `.codex/config.toml` → codex) so a server
restart cannot silently fall back to `agent` and split one real session
into two ids for the concurrency detector. Convention: run `whereami` at
session start; record a `checkpoint` at every commit, handoff, or
interruption.

## Path resolution / env overrides

`xtools/vsenv.py` finds tooling via (in order): env override → vswhere →
PATH. Overrides: `XASH_CMAKE`, `XASH_CTEST`, `XASH_VSDEVCMD`, `XASH_CLANGD`,
`XASH_MCP_LANGUAGE_SERVER` (cpp_lsp_launcher only), and
`XASH_CHECKPOINT_ACTOR` (default checkpoint actor label).

## Codex CLI note

Codex reads the root `AGENTS.md` natively, and the committed
`.codex/config.toml` registers both MCP servers once you trust the project
in your user-global `~/.codex/config.toml`:

```toml
[projects."c:\\git\\xash3d-fwgs"]
trust_level = "trusted"
```

Fallback (if your Codex build does not resolve the committed repo-relative
commands): add the servers to `~/.codex/config.toml` with absolute paths:

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

## Tests

`tests/` holds stdlib-`unittest` regression coverage for the check ruleset
(filesystem-free: regex rules are exercised against snippet lines, the
`compliance-allow` hatch against `_filter_allows`). Run after any edit to
`rules.py` / `checks.py`:

```powershell
c:\git\xash3d-fwgs\.venv\Scripts\python.exe -m unittest discover -s xash3dpp\tools\tests
```

Stdlib-only, no pip step — the same contract as the CLI scripts. The
`compliance-allow(<check-id>)` marker is honored by every `[M]` check
(regex rules and the structured heuristics alike), either on the flagged
line itself **or** on the contiguous `//` comment block directly above it —
so the natural "document the exemption above the line" form works, not only
a trailing comment. (A finding on an inner line of a multi-line statement
has code, not its comment block, directly above it; there the marker must
sit on the flagged line.)
