"""Build / test / compile-DB wrappers around the VS2022-bundled cmake/ctest.

These replace the absolute-path PowerShell snippets that used to be pasted
into the workflow prompts (sweep-module, pre-pr, bisect, retriever,
implement-audit)."""

from __future__ import annotations

import json
import re
from pathlib import Path

from . import XPP
from .proc import run
from .vsenv import cmake_path, ctest_path, vsdevcmd_path

_ERROR_RX = re.compile(r"error C\d+|(?<!\w)error:|fatal error", re.IGNORECASE)
_MSVC_ERR = re.compile(r"^(.*?)\((\d+)(?:,\d+)?\)\s*:\s*(?:fatal )?error (C\d+)\s*:\s*(.*)$")
_WARN_RX = re.compile(r"warning C\d+|(?<!\w)warning:")


def build(preset: str = "debug", configure: bool = False,
          target: str | None = None) -> dict:
    cmake = cmake_path()
    configured = False
    log: list[str] = []
    if configure or not (XPP / "build" / "Debug" / "CMakeCache.txt").is_file():
        rc, lines, _ = run([str(cmake), "--preset", "debug-msvc"], cwd=XPP)
        log += lines
        configured = True
        if rc != 0:
            return _build_result(preset, configured, rc, log, 0.0)
    cmd = [str(cmake), "--build", "--preset", preset]
    if target:
        cmd += ["--target", target]
    rc, lines, duration = run(cmd, cwd=XPP)
    log += lines
    return _build_result(preset, configured, rc, log, duration)


def _build_result(preset, configured, rc, log, duration) -> dict:
    errors = []
    for line in log:
        if _ERROR_RX.search(line):
            m = _MSVC_ERR.match(line.strip())
            if m:
                errors.append({"file": m.group(1), "line": int(m.group(2)),
                               "code": m.group(3), "text": m.group(4)[:200]})
            else:
                errors.append({"file": "", "line": 0, "code": "",
                               "text": line.strip()[:200]})
    return {
        "preset": preset,
        "configured": configured,
        "exit_code": rc,
        "errors": errors[:50],
        "error_count": len(errors),
        "warning_count": sum(1 for l in log if _WARN_RX.search(l)),
        "duration_s": round(duration, 1),
        "log_tail": log[-15:],
    }


def test(filter_regex: str = "", preset: str = "debug") -> dict:
    ctest = ctest_path()
    cmd = [str(ctest), "--preset", preset]
    if filter_regex:
        cmd += ["-R", filter_regex]
    rc, lines, duration = run(cmd, cwd=XPP)
    total = passed = failed = skipped = 0
    failed_tests: list[dict] = []
    for line in lines:
        m = re.search(r"(\d+)% tests passed, (\d+) tests? failed out of (\d+)", line)
        if m:
            failed = int(m.group(2))
            total = int(m.group(3))
            passed = total - failed
        m = re.search(r"^\s*\d+\s*-\s*(\S+)\s*\((Failed|Timeout|Exception|Subprocess aborted)", line)
        if m:
            failed_tests.append({"name": m.group(1), "reason": m.group(2)})
        m = re.search(r"(\d+) tests? skipped", line)
        if m:
            skipped = int(m.group(1))
    for ft in failed_tests:
        ft["tail"] = [l for l in lines if ft["name"] in l][:8]
    return {
        "preset": preset, "filter": filter_regex,
        "exit_code": rc, "total": total, "passed": passed,
        "failed": failed, "skipped": skipped,
        "failed_tests": failed_tests, "duration_s": round(duration, 1),
        "log_tail": lines[-10:],
    }


def refresh_compile_db() -> dict:
    """Re-run the clangd Ninja preset (needs the VS x64 dev environment) to
    regenerate xash3dpp/build/clangd/compile_commands.json."""
    vsdevcmd = vsdevcmd_path()
    cmake = cmake_path()
    db = XPP / "build" / "clangd" / "compile_commands.json"
    before = db.stat().st_mtime if db.is_file() else 0.0
    cmdline = 'call "%s" -arch=x64 -no_logo && "%s" --preset clangd' % (vsdevcmd, cmake)
    rc, lines, duration = run('cmd /s /c "%s"' % cmdline, cwd=XPP, shell=True)
    refreshed = db.is_file() and db.stat().st_mtime > before
    entry_count = 0
    if db.is_file():
        try:
            entry_count = len(json.loads(db.read_text(encoding="utf-8")))
        except (ValueError, OSError):
            entry_count = -1
    return {
        "exit_code": rc,
        "compile_commands": str(db),
        "entry_count": entry_count,
        "refreshed": refreshed,
        "duration_s": round(duration, 1),
        "log_tail": lines[-10:],
    }
