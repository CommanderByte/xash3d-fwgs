"""Build / test / compile-DB wrappers around the VS2022-bundled cmake/ctest.

These replace the absolute-path PowerShell snippets that used to be pasted
into the workflow prompts (sweep-module, pre-pr, bisect, retriever,
implement-audit)."""

from __future__ import annotations

import json
import re
from pathlib import Path

from . import REPO, XPP
from .proc import run
from .vsenv import cmake_path, ctest_path, vsdevcmd_path

_ERROR_RX = re.compile(r"error C\d+|(?<!\w)error:|fatal error", re.IGNORECASE)
_MSVC_ERR = re.compile(r"^(.*?)\((\d+)(?:,\d+)?\)\s*:\s*(?:fatal )?error (C\d+)\s*:\s*(.*)$")
_WARN_RX = re.compile(r"warning C\d+|(?<!\w)warning:")


# (configuration, architecture) -> (configure preset, build/test preset,
# binaryDir under build/).  The retail GoldSrc dlls (hl.dll) are 32-bit, so
# the S15 milestone smoke needs the x86 chain; the presets mirror
# CMakePresets.json.  `preset` names the configuration axis (debug/release),
# `arch` the width axis — orthogonal, matching CMake's own model.
_BUILD_MATRIX = {
    ("debug",   "x64"): ("debug-msvc",     "debug",     "Debug"),
    ("debug",   "x86"): ("debug-msvc-x86", "debug-x86", "Debug-x86"),
    ("release", "x64"): ("release-msvc",   "release",   "Release"),
}
_ARCHES = {"x64": "x64", "amd64": "x64", "x86": "x86", "win32": "x86"}


def _resolve(preset: str, arch: str) -> tuple[str, str, str]:
    """Map a (configuration, architecture) pair to
    (configure preset, build/test preset, binaryDir stem under build/).

    `preset` names the configuration (``debug``/``release``) but also tolerates
    a full build-preset name carrying an ``-x86`` suffix (e.g. ``debug-x86``),
    from which the architecture is inferred — so the older
    ``--preset debug-x86`` form keeps working.  Unknown pairs fall back to the
    x64 variant of the configuration, else ``debug`` x64."""
    cfg = (preset or "debug").strip().lower()
    a = _ARCHES.get((arch or "x64").strip().lower(), (arch or "x64").strip().lower())
    if cfg.endswith("-x86"):
        cfg, a = cfg[:-4], "x86"
    for key in ((cfg, a), (cfg, "x64"), ("debug", "x64")):
        if key in _BUILD_MATRIX:
            return _BUILD_MATRIX[key]
    return _BUILD_MATRIX[("debug", "x64")]


def compile_db_status() -> dict:
    """Staleness of build/clangd/compile_commands.json vs the newest
    non-build CMakeLists.txt.  Shared by whereami --doctor and
    build(refresh_db="auto") so both use ONE predicate.  A missing DB
    reports stale=True (auto-refresh should create it)."""
    db = XPP / "build" / "clangd" / "compile_commands.json"
    newest, newest_name = 0.0, ""
    for cml in XPP.rglob("CMakeLists.txt"):
        if "build" in cml.parts:
            continue
        mt = cml.stat().st_mtime
        if mt > newest:
            newest, newest_name = mt, cml.relative_to(REPO).as_posix()
    exists = db.is_file()
    stale = (not exists) or db.stat().st_mtime < newest
    return {"exists": exists, "stale": stale,
            "db": db.relative_to(REPO).as_posix(),
            "newest_cmakelists": newest_name}


def _refresh_decision(mode: str, stale: bool) -> bool:
    """Pure: should build() refresh the compile DB?  on -> always,
    off -> never, auto (default/unknown) -> only when stale."""
    m = (mode or "auto").strip().lower()
    if m == "on":
        return True
    if m == "off":
        return False
    return stale


def build(preset: str = "debug", configure: bool = False,
          target: str | None = None, arch: str = "x64",
          refresh_db: str = "auto") -> dict:
    cmake = cmake_path()
    cfg_preset, build_preset, bdir = _resolve(preset, arch)
    width = "x86" if bdir.endswith("-x86") else "x64"
    configured = False
    log: list[str] = []
    if configure or not (XPP / "build" / bdir / "CMakeCache.txt").is_file():
        rc, lines, _ = run([str(cmake), "--preset", cfg_preset], cwd=XPP)
        log += lines
        configured = True
        if rc != 0:
            return _build_result(build_preset, configured, rc, log, 0.0, width)
    cmd = [str(cmake), "--build", "--preset", build_preset]
    if target:
        cmd += ["--target", target]
    rc, lines, duration = run(cmd, cwd=XPP)
    log += lines
    result = _build_result(build_preset, configured, rc, log, duration, width)
    # T5: keep the clangd compile DB fresh without paying the VsDevCmd +
    # Ninja-configure cost on every build — auto fires only when stale
    # (doctor's predicate) and only after a SUCCESSFUL build.
    status = compile_db_status()
    ran = rc == 0 and _refresh_decision(refresh_db, status["stale"])
    result["refresh_db"] = {
        "mode": (refresh_db or "auto").strip().lower(),
        "stale": status["stale"],
        "ran": ran,
        "result": refresh_compile_db() if ran else None,
    }
    return result


def _build_result(preset, configured, rc, log, duration, arch="x64") -> dict:
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
        "arch": arch,
        "configured": configured,
        "exit_code": rc,
        "errors": errors[:50],
        "error_count": len(errors),
        "warning_count": sum(1 for l in log if _WARN_RX.search(l)),
        "duration_s": round(duration, 1),
        "log_tail": log[-15:],
    }


# Windows NTSTATUS / CRT exit codes a crashing test child commonly dies
# with — the decoded name is usually the whole diagnosis (a bare "Failed"
# with exit 0x80000003 once cost a four-round-trip bisect).
_EXIT_DECODE = {
    0x80000003: "STATUS_BREAKPOINT — __debugbreak / MSVC debug assert "
                "(debug-heap leak check, _ASSERTE)",
    0xC0000005: "STATUS_ACCESS_VIOLATION — segfault / null deref",
    0xC0000409: "STATUS_STACK_BUFFER_OVERRUN — fail-fast (/GS, "
                "std::terminate on MSVC)",
    0xC00000FD: "STATUS_STACK_OVERFLOW",
    0xC0000135: "STATUS_DLL_NOT_FOUND — missing dependent DLL",
    3: "MSVC CRT abort()",
}


def _decode_exit(text: str) -> str | None:
    """Map any recognizable exit code in `text` to its decoded name."""
    for m in re.finditer(r"0x[0-9A-Fa-f]{8}|-?\d{9,10}|\bcode 3\b", text):
        token = m.group(0)
        if token == "code 3":
            token = "3"
        try:
            value = int(token, 0) & 0xFFFFFFFF
        except ValueError:
            continue
        if value in _EXIT_DECODE:
            return "%s (%s)" % (_EXIT_DECODE[value], m.group(0))
    return None


_FAIL_LINE_RX = re.compile(
    r"^\s*\d+/\d+\s+Test\s+#\d+:\s+(\S+)\s+\.*\*\*\*(\S[\w ]*)")
_BLOCK_END_RX = re.compile(
    r"^\s*(Start\s+\d+:|\d+/\d+\s+Test\s+#|\d+% tests passed)")


def _failed_output_blocks(lines: list[str]) -> dict[str, list[str]]:
    """Per-failed-test inline output (present with --output-on-failure)."""
    blocks: dict[str, list[str]] = {}
    current: str | None = None
    for line in lines:
        fm = _FAIL_LINE_RX.match(line)
        if fm:
            current = fm.group(1)
            blocks[current] = []
            continue
        if current is not None:
            if _BLOCK_END_RX.match(line):
                current = None
                continue
            if len(blocks[current]) < 40:
                blocks[current].append(line)
    return blocks


def _last_test_log_section(lines: list[str], name: str) -> list[str]:
    """Fallback: the test's section from Testing/Temporary/LastTest.log —
    ctest records output there even when the child crashed before its
    stdout reached the console."""
    build_dir = None
    for line in lines:
        m = re.search(r"Test project (.+)", line)
        if m:
            build_dir = Path(m.group(1).strip())
            break
    if build_dir is None:
        return []
    log = build_dir / "Testing" / "Temporary" / "LastTest.log"
    if not log.is_file():
        return []
    try:
        log_lines = log.read_text(encoding="utf-8",
                                  errors="replace").splitlines()
    except OSError:
        return []
    section: list[str] = []
    in_section = False
    for line in log_lines:
        if re.search(r"Testing:\s+%s\s*$" % re.escape(name), line):
            in_section = True
            continue
        if in_section and re.search(r"Testing:\s+\S+\s*$", line):
            break
        if in_section:
            section.append(line)
    return section[-40:]


# Signatures that mark the diagnostic line in a failed test's output.  The
# in-tree harness (tests/test_helpers.hpp) prints "FATAL [file:line]" /
# "FAIL [file:line]" on REQUIRE/CHECK failure; CRT / XASH_ASSERT aborts print an
# assertion or abort message just before dying (exit 3).
_ASSERT_RX = re.compile(
    r"FATAL \[|FAIL \[|\[FAIL\]|assert|abort|terminate|"
    r"unhandled exception|sanitizer|runtime error|panic|fatal error",
    re.IGNORECASE)


def _assert_tail(output: list[str], limit: int = 8) -> list[str]:
    """The most diagnostic slice of a failed test's output: a window at the
    first assertion/abort signature if present, else the last few non-empty
    lines.  Surfaces the actual message (the XASH_ASSERT / REQUIRE / CHECK
    text) so a bare exit-3 abort no longer needs a manual unpiped re-run."""
    nonempty = [ln for ln in output if ln.strip()]
    if not nonempty:
        return []
    for i, line in enumerate(nonempty):
        if _ASSERT_RX.search(line):
            start = max(0, i - 2)  # a little lead-in for context
            return nonempty[start:start + limit]
    return nonempty[-limit:]


def test(filter_regex: str = "", preset: str = "debug",
         arch: str = "x64") -> dict:
    ctest = ctest_path()
    _, test_preset, bdir = _resolve(preset, arch)
    width = "x86" if bdir.endswith("-x86") else "x64"
    cmd = [str(ctest), "--preset", test_preset, "--output-on-failure"]
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
    blocks = _failed_output_blocks(lines) if failed_tests else {}
    for ft in failed_tests:
        output = blocks.get(ft["name"]) or []
        if not any(l.strip() for l in output):
            output = _last_test_log_section(lines, ft["name"])
        ft["output"] = output
        tail = _assert_tail(output)
        if tail:
            ft["assert_tail"] = tail
        decoded = _decode_exit("\n".join(output + [ft["reason"]]))
        if decoded:
            ft["exit_decode"] = decoded
    return {
        "preset": test_preset, "arch": width, "filter": filter_regex,
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
