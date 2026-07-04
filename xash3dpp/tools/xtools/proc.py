"""Subprocess helper shared by build/test/refresh tools."""

from __future__ import annotations

import subprocess
import time
from pathlib import Path


def run(
    cmd: list[str] | str,
    cwd: Path | None = None,
    timeout: float = 1800.0,
    shell: bool = False,
) -> tuple[int, list[str], float]:
    """Run a command, return (returncode, combined-output lines, seconds)."""
    start = time.monotonic()
    proc = subprocess.run(
        cmd,
        cwd=str(cwd) if cwd else None,
        shell=shell,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
    )
    duration = time.monotonic() - start
    out = (proc.stdout or "") + ("\n" + proc.stderr if proc.stderr else "")
    return proc.returncode, out.splitlines(), duration
