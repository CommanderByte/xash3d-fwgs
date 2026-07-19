"""Markdown lint wrapper around pymarkdownlnt with the repo config.

Shared by the `markdown_lint.py` CLI twin and the MCP `markdown_lint` tool
(whose former inline body moved here, 2026-07-19 tooling wave T6)."""

from __future__ import annotations

import subprocess

from . import REPO, venv_python

CONFIG = REPO / "xash3dpp" / ".pymarkdown.json"


def _build_cmd(python: str, config: str, paths: list[str]) -> list[str]:
    """Pure: the pymarkdown invocation line (unit-tested)."""
    return [python, "-m", "pymarkdown", "--config", config, "scan", *paths]


def lint(paths: list[str], timeout: int = 300) -> dict:
    """Lint repo-relative markdown paths with xash3dpp/.pymarkdown.json.

    --config must be explicit: pymarkdown's discovery is cwd-relative and
    cwd is the REPO ROOT, so the xash3dpp config was silently ignored
    (md013 etc. fired despite being disabled) — fixed 2026-07-06 (B2)."""
    abs_paths = [str(REPO / p) for p in paths]
    cmd = _build_cmd(str(venv_python()), str(CONFIG), abs_paths)
    proc = subprocess.run(
        cmd,
        stdin=subprocess.DEVNULL,  # never inherit the MCP stdio pipe
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        cwd=str(REPO), timeout=timeout,
    )
    issues = [ln for ln in (proc.stdout or "").splitlines() if ln.strip()]
    return {"exit_code": proc.returncode, "issues": issues[:200],
            "issue_count": len(issues), "config": str(CONFIG)}
