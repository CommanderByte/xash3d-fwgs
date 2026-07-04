#!/usr/bin/env python
"""xash-tools — project MCP server (stdio, FastMCP).

Thin wrappers over the same xtools functions the CLI scripts use, so
MCP-capable agent frameworks (Claude Code, VS Code Copilot, opencode) get
the deterministic workflow tooling without shell round-trips. Registered in
.mcp.json, .vscode/mcp.json, opencode.json, and .codex/config.toml (trusted
projects); see .github/AGENT-SETUP.md.

Requires the `mcp` package (see root requirements.txt); everything else in
tools/ is stdlib-only.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from mcp.server.fastmcp import FastMCP  # noqa: E402

from xtools import REPO, venv_python  # noqa: E402
from xtools import buildtools, checks, state  # noqa: E402
from xtools import sync as xsync  # noqa: E402

mcp = FastMCP("xash-tools")


@mcp.tool()
def build(preset: str = "debug", configure: bool = False,
          target: str = "") -> dict:
    """Build xash3dpp via the VS2022-bundled cmake. Returns parsed errors,
    counts, and a log tail."""
    return buildtools.build(preset=preset, configure=configure,
                            target=target or None)


@mcp.tool()
def test(filter: str = "", preset: str = "debug") -> dict:
    """Run ctest (optionally filtered with -R `filter`). Returns pass/fail
    breakdown and failed-test tails."""
    return buildtools.test(filter_regex=filter, preset=preset)


@mcp.tool()
def refresh_compile_db() -> dict:
    """Regenerate build/clangd/compile_commands.json (the cpp-lsp/clangd
    database). Run after adding files or targets."""
    return buildtools.refresh_compile_db()


@mcp.tool()
def compliance_scan(subsystem: str, checks_set: str = "all",
                    min_severity: str = "note") -> dict:
    """Mechanical convention scan (reviewer [M] checks). checks_set: all |
    prepr | detail | comma-list of check ids. candidate-* findings need
    judgment."""
    return checks.compliance_scan(subsystem, checks=checks_set,
                                  min_severity=min_severity)


@mcp.tool()
def status(check: bool = False) -> dict:
    """Per-subsystem implementation status derived from the tree (src file
    counts, include/tests presence). check=True also diffs against the
    implementation-plan status table (drift list)."""
    data = checks.status_table()
    if check:
        data["drift"] = checks.status_check(data)
    return data


@mcp.tool()
def whereami(doctor: bool = False) -> dict:
    """Ground-truth session brief: git state, plan/chunk status, sync gates,
    blocking OQs, recent checkpoints (with staleness/concurrency flags), and
    a suggested next action. Run at session start and after dormancy.
    doctor=True adds environment checks."""
    return state.whereami(doctor_requested=doctor)


@mcp.tool()
def checkpoint(chunk: str, step: str, note: str, actor: str = "",
               session: str = "") -> dict:
    """Append an advisory checkpoint (intent record) to
    .agent-checkpoints.jsonl. Record at every commit, handoff, or
    interruption. Ground truth is always derived — checkpoints only aid
    resumption."""
    return state.append_checkpoint(chunk, step, note, actor or None,
                                   session or None)


@mcp.tool()
def workflow_sync(stage: int = 2) -> dict:
    """Drift check over the agent-workflow surface (.github originals vs
    adapters, model dialects, twin entry files, MCP registrations, doc
    counters). stage 1 = tooling subset, stage 2 (default) = full gate."""
    return xsync.workflow_sync(stage=stage)


@mcp.tool()
def finish_check(subsystem: str, run_tests: bool = False) -> dict:
    """The 9-section finish-subsystem done checklist as
    pass/fail/needs-judgment items."""
    return checks.finish_check(subsystem, run_tests=run_tests)


@mcp.tool()
def stub_scan(subsystem: str) -> dict:
    """TODO/stub markers with enclosing symbols plus a live-vs-stub test
    tally for a subsystem."""
    return checks.stub_scan(subsystem)


@mcp.tool()
def limits_scan(subsystem: str = "") -> dict:
    """limits.hpp audit: parsed XASH_LIMIT_* entries, dead limits, magic
    numbers and shadow literals in scope."""
    return checks.limits_scan(subsystem or None)


@mcp.tool()
def markdown_lint(paths: list[str]) -> dict:
    """Lint markdown files with the repo's pymarkdownlnt config. Paths are
    repo-relative."""
    abs_paths = [str(REPO / p) for p in paths]
    proc = subprocess.run(
        [str(venv_python()), "-m", "pymarkdown", "scan", *abs_paths],
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        cwd=str(REPO), timeout=300,
    )
    issues = [l for l in (proc.stdout or "").splitlines() if l.strip()]
    return {"exit_code": proc.returncode, "issues": issues[:200],
            "issue_count": len(issues)}


if __name__ == "__main__":
    mcp.run()
