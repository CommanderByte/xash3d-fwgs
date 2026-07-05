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

import importlib
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from mcp.server.fastmcp import FastMCP  # noqa: E402

import xtools  # noqa: E402
from xtools import REPO, venv_python  # noqa: E402
from xtools import buildtools, checks, proc, report, rules, scan, state  # noqa: E402
from xtools import crosswalk as crosswalk_mod  # noqa: E402
from xtools import sync as xsync  # noqa: E402
from xtools import vsenv  # noqa: E402

mcp = FastMCP("xash-tools")

# ---------------------------------------------------------------------------
# Hot reload: the server process is long-lived, but the xtools modules get
# edited mid-session (a stale compliance ruleset once kept reporting a fixed
# blocker).  Before each tool call, reload every xtools module in dependency
# order when any source file changed on disk.  Tool REGISTRATIONS in this
# file still need a session restart — only module bodies hot-reload.
# ---------------------------------------------------------------------------

_XTOOLS_DIR = Path(__file__).resolve().parent / "xtools"
_RELOAD_ORDER = [xtools, proc, report, vsenv, rules, scan,
                 buildtools, checks, crosswalk_mod, state, xsync]


def _xtools_mtimes() -> dict[str, float]:
    return {p.name: p.stat().st_mtime for p in _XTOOLS_DIR.glob("*.py")}


_mtimes = _xtools_mtimes()


def _maybe_reload() -> None:
    global _mtimes
    now = _xtools_mtimes()
    if now == _mtimes:
        return
    _mtimes = now
    for module in _RELOAD_ORDER:
        importlib.reload(module)


@mcp.tool()
def build(preset: str = "debug", configure: bool = False,
          target: str = "") -> dict:
    """Build xash3dpp via the VS2022-bundled cmake. Returns parsed errors,
    counts, and a log tail."""
    _maybe_reload()
    return buildtools.build(preset=preset, configure=configure,
                            target=target or None)


@mcp.tool()
def test(filter: str = "", preset: str = "debug") -> dict:
    """Run ctest (optionally filtered with -R `filter`). Returns pass/fail
    breakdown; failed tests carry their output block, an `assert_tail` (the
    focused assertion/abort message window — the XASH_ASSERT / REQUIRE / CHECK
    line, so exit-3 aborts are diagnosable without a re-run), and a decoded
    exit code (STATUS_BREAKPOINT, ACCESS_VIOLATION, ...) when recognizable."""
    _maybe_reload()
    return buildtools.test(filter_regex=filter, preset=preset)


@mcp.tool()
def refresh_compile_db() -> dict:
    """Regenerate build/clangd/compile_commands.json (the cpp-lsp/clangd
    database). Run after adding files or targets."""
    _maybe_reload()
    return buildtools.refresh_compile_db()


@mcp.tool()
def compliance_scan(subsystem: str = "", checks_set: str = "all",
                    min_severity: str = "note", slice: bool = False,
                    files: str = "", baseline: bool = False) -> dict:
    """Mechanical convention scan (reviewer [M] checks). checks_set: all |
    prepr | detail | comma-list of check ids. candidate-* findings need
    judgment. ABI-forced constructs carry inline compliance-allow markers,
    echoed in the result's `allows` list. slice=True scans the current
    change set (slice_diff default base) instead of a subsystem — slices
    cross subsystem boundaries; `files` (comma-list of repo-relative
    paths) scans exactly those. baseline=True (implies the slice change set)
    keeps only findings the change INTRODUCED — drops pre-existing findings in
    files merely pulled into the scan; adds `baseline_suppressed` to the
    result."""
    _maybe_reload()
    file_list = None
    baseline_base = ""
    if slice or baseline:
        diff = state.slice_diff()
        if "error" in diff:
            return {"error": diff["error"]}
        file_list = [f["path"] for f in diff["files"]] + diff["untracked"]
        if baseline:
            baseline_base = diff["base"]
    elif files:
        file_list = [f for f in files.split(",") if f.strip()]
    elif not subsystem:
        return {"error": "give a subsystem, files, or slice=True"}
    return checks.compliance_scan(subsystem or None, checks=checks_set,
                                  min_severity=min_severity, files=file_list,
                                  baseline_base=baseline_base)


@mcp.tool()
def status(check: bool = False) -> dict:
    """Per-subsystem implementation status derived from the tree (src file
    counts, include/tests presence, stub-marker counts; the
    complete_with_stubs list surfaces structurally-Complete subsystems
    still carrying TODO/stub markers). check=True also diffs against the
    implementation-plan status table (drift list)."""
    _maybe_reload()
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
    _maybe_reload()
    return state.whereami(doctor_requested=doctor)


@mcp.tool()
def checkpoint(chunk: str, step: str, note: str, actor: str = "",
               session: str = "") -> dict:
    """Append an advisory checkpoint (intent record) to
    .agent-checkpoints.jsonl. Record at every commit, handoff, or
    interruption. Ground truth is always derived — checkpoints only aid
    resumption."""
    _maybe_reload()
    return state.append_checkpoint(chunk, step, note, actor or None,
                                   session or None)


@mcp.tool()
def workflow_sync(stage: int = 2) -> dict:
    """Drift check over the agent-workflow surface (.github originals vs
    adapters, model dialects, twin entry files, MCP registrations, doc
    counters). stage 1 = tooling subset, stage 2 (default) = full gate."""
    _maybe_reload()
    return xsync.workflow_sync(stage=stage)


@mcp.tool()
def finish_check(subsystem: str, run_tests: bool = False) -> dict:
    """The 9-section finish-subsystem done checklist as
    pass/fail/needs-judgment items."""
    _maybe_reload()
    return checks.finish_check(subsystem, run_tests=run_tests)


@mcp.tool()
def stub_scan(subsystem: str, delta: bool = False) -> dict:
    """TODO/stub markers with enclosing symbols plus a live-vs-stub test
    tally for a subsystem. `by_tag` counts markers by their parenthesized
    tag (chunk6, chunk6-S9, S8-seam, ...); delta=True adds `delta_by_tag`,
    the net change per tag vs HEAD~1 (surfaces net-zero marker churn — a
    retired stub offset by a new one)."""
    _maybe_reload()
    return checks.stub_scan(subsystem, delta=delta)


@mcp.tool()
def crosswalk(query: str = "", kind: str = "auto",
              missing: bool = False) -> dict:
    """Resolve a legacy C engine symbol or file:line to its xash3dpp C++ port.
    `query` is a legacy symbol (SV_Multicast) or file:line (sv_game.c:4026).
    Indexes the inline port annotations + the deep-dive recon docs; each hit
    carries `source` + `confidence` (server resolves function-level; networking
    / map_loader are file-level, so a hit there points at the porting TU).
    missing=True lists deep-dive-documented symbols with no code port yet — the
    still-to-port set (serves the pmove / parity phases)."""
    _maybe_reload()
    return crosswalk_mod.crosswalk(query, kind=kind, missing=missing)


@mcp.tool()
def limits_scan(subsystem: str = "") -> dict:
    """limits.hpp audit: parsed XASH_LIMIT_* entries, dead limits, magic
    numbers and shadow literals in scope."""
    _maybe_reload()
    return checks.limits_scan(subsystem or None)


@mcp.tool()
def slice_diff(base: str = "", include_patch: bool = False,
               max_patch_lines: int = 400) -> dict:
    """Change inventory since `base` (default: HEAD when the tree is dirty
    at a checkpointed commit — the slice is the uncommitted work — else
    the newest checkpoint head differing from HEAD, else HEAD~1): files
    with add/delete counts + untracked list, optional capped patch. Use it
    to brief gate agents (abi-watchdog / reviewer) from ground truth
    instead of a hand-typed file list."""
    _maybe_reload()
    return state.slice_diff(base=base, include_patch=include_patch,
                            max_patch_lines=max_patch_lines)


@mcp.tool()
def markdown_lint(paths: list[str]) -> dict:
    """Lint markdown files with the repo's pymarkdownlnt config. Paths are
    repo-relative."""
    abs_paths = [str(REPO / p) for p in paths]
    proc = subprocess.run(
        [str(venv_python()), "-m", "pymarkdown", "scan", *abs_paths],
        stdin=subprocess.DEVNULL,  # never inherit the MCP stdio pipe
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        cwd=str(REPO), timeout=300,
    )
    issues = [l for l in (proc.stdout or "").splitlines() if l.strip()]
    return {"exit_code": proc.returncode, "issues": issues[:200],
            "issue_count": len(issues)}


if __name__ == "__main__":
    mcp.run()
