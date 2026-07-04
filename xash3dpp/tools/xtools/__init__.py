"""xtools — shared library for the xash3dpp agent-workflow tools.

Stdlib-only. CLI wrappers in the parent directory and mcp_server.py both
import from here so behaviour is defined exactly once.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

TOOL_VERSION = 1

# Windows consoles default to a legacy codepage; the tables/status glyphs are
# UTF-8. Reconfigure once for every CLI/MCP entry point that imports xtools.
for _stream in (sys.stdout, sys.stderr):
    try:
        _stream.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass


def find_repo_root(start: Path | None = None) -> Path:
    """Walk upward from `start` (default: this file) until a .git dir/file."""
    p = (start or Path(__file__)).resolve()
    for candidate in [p, *p.parents]:
        if (candidate / ".git").exists():
            return candidate
    raise RuntimeError("repo root not found (no .git above %s)" % p)


REPO = find_repo_root()
XPP = REPO / "xash3dpp"
SRC = XPP / "src"
INCLUDE = XPP / "include" / "xash3dpp"
PRIVATE = INCLUDE / "private"
TESTS = XPP / "tests"
DOCS = XPP / "docs"
LIMITS_HPP = INCLUDE / "limits.hpp"
GITHUB = REPO / ".github"


def subsystems() -> list[str]:
    """Subsystem names = directories under xash3dpp/src (sorted)."""
    if not SRC.is_dir():
        return []
    return sorted(d.name for d in SRC.iterdir() if d.is_dir())


def resolve_scope(subsystem: str | None) -> list[str]:
    """Validate a subsystem argument; None/'all' means every subsystem."""
    subs = subsystems()
    if subsystem in (None, "", "all"):
        return subs
    if subsystem not in subs:
        raise ValueError(
            "unknown subsystem %r (known: %s)" % (subsystem, ", ".join(subs))
        )
    return [subsystem]


def venv_python() -> Path:
    return REPO / ".venv" / "Scripts" / "python.exe"


def env_path(name: str) -> Path | None:
    v = os.environ.get(name)
    return Path(v) if v else None
