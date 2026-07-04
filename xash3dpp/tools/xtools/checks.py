"""Scanner implementations: compliance, limits, stubs, status, finish gate,
dependency edges. All functions return plain dicts (the CLI/MCP layers wrap
them in the envelope)."""

from __future__ import annotations

import re
from pathlib import Path

from . import DOCS, INCLUDE, LIMITS_HPP, PRIVATE, REPO, SRC, TESTS, resolve_scope
from .rules import JUDGMENT_CHECKS, MUTATOR_NAMES, RULES, STRUCTURED_CHECKS
from .scan import CPP_EXT, code_lines, subsystem_files

SEV_ORDER = {"blocker": 3, "warning": 2, "note": 1}

# Inline exemption for ABI-forced constructs the rules can't know about:
#   <flagged code>  // compliance-allow(<check-id>[, <check-id>]): <rationale>
# The marker must sit on the flagged line (comment side); every allow is
# reported in the scan result so pre-pr can audit the full list.
ALLOW_RE = re.compile(r"compliance-allow\(\s*([\w\-, ]+?)\s*\)")


def _rel(p: Path) -> str:
    try:
        return p.relative_to(REPO).as_posix()
    except ValueError:
        return p.as_posix()


def _violation(check, severity, path, line, excerpt, hint, rule_ref, candidate=False):
    return {
        "check": check,
        "severity": ("candidate-" + severity) if candidate else severity,
        "file": _rel(path),
        "line": line,
        "excerpt": excerpt.strip()[:200],
        "hint": hint,
        "rule_ref": rule_ref,
    }


def _scope_of(path: Path) -> str:
    s = str(path)
    if str(TESTS) in s:
        return "tests"
    if str(SRC) in s:
        return "src"
    return "include"


def _sub_of_path(path: Path) -> str:
    """Infer the owning subsystem from a repo-relative xash3dpp path (for
    per-subsystem rule exclusions in file-list scans)."""
    rel = _rel(path)
    for rx in (r"xash3dpp/src/([\w]+)/",
               r"xash3dpp/include/xash3dpp/private/([\w]+)/",
               r"xash3dpp/include/xash3dpp/([\w]+)/",
               r"xash3dpp/tests/([\w]+)/"):
        m = re.match(rx, rel)
        if m:
            return m.group(1)
    return "slice"


def _group_files(paths: list[str]) -> tuple[list[tuple[str, list[Path]]],
                                            list[str]]:
    """Group an explicit file list (repo-relative) by inferred subsystem
    (suffix set = scan.CPP_EXT, same as subsystem mode).  Anything outside
    xash3dpp/ (the legacy engine is reference-only, never scanned),
    non-C++ files and missing paths land in the returned `ignored` list —
    a gate must be able to see what it did NOT scan.  Duplicates are
    scanned once."""
    by_sub: dict[str, list[Path]] = {}
    ignored: list[str] = []
    seen: set[str] = set()
    for raw in paths:
        rel = raw.strip().replace("\\", "/")
        if rel.startswith("./"):
            rel = rel[2:]
        if not rel or rel in seen:
            continue
        seen.add(rel)
        p = REPO / rel
        if not rel.startswith("xash3dpp/") or p.suffix not in CPP_EXT \
                or not p.is_file():
            ignored.append(rel)
            continue
        by_sub.setdefault(_sub_of_path(p), []).append(p)
    return sorted(by_sub.items()), ignored


# --------------------------------------------------------------------------- #
# compliance_scan
# --------------------------------------------------------------------------- #

def compliance_scan(subsystem: str | None, checks: str = "all",
                    min_severity: str = "note",
                    files: list[str] | None = None) -> dict:
    """Run the [M] ruleset over one subsystem (or all).

    checks: "all" | "prepr" | "detail" | comma-list of check ids.
    files: explicit repo-relative paths (a slice_diff change set) —
    overrides subsystem discovery so gates cover exactly what a slice
    touched, wherever it lives (the S7a cvar_ops.cpp gap).
    """
    files_ignored: list[str] = []
    if files is not None:
        groups, files_ignored = _group_files(files)
        subs = [g[0] for g in groups]
    else:
        subs = resolve_scope(subsystem)
        groups = [(sub, subsystem_files(sub, tests=True)) for sub in subs]
    explicit = None
    if checks in ("all", "prepr", "detail"):
        active = [r for r in RULES if checks in r.sets]
        structured = list(STRUCTURED_CHECKS)
        if checks == "prepr":
            structured = ["hpp-under-src", "thread-assert", "test-macros"]
    else:
        explicit = {c.strip() for c in checks.split(",") if c.strip()}
        active = [r for r in RULES if r.check in explicit]
        structured = [c for c in STRUCTURED_CHECKS if c in explicit]

    violations: list[dict] = []
    allows: list[dict] = []
    files_scanned = 0
    for sub, sub_files in groups:
        files_scanned += len(sub_files)
        file_texts: dict[Path, str] = {}
        for path in sub_files:
            scope = _scope_of(path)
            lines = list(code_lines(path))
            file_texts[path] = "\n".join(code for _, code, _ in lines)
            has_extern_c = 'extern "C"' in file_texts[path]
            for rule in active:
                if scope not in rule.scopes:
                    continue
                if sub in rule.exclude_subsystems:
                    continue
                if rule.exclude_path_re and re.search(rule.exclude_path_re, str(path)):
                    continue
                if rule.check == "externc-stringview" and not has_extern_c:
                    continue
                rx = re.compile(rule.pattern)
                ex = re.compile(rule.exclude_line_re) if rule.exclude_line_re else None
                for lineno, code, raw in lines:
                    if not code.strip():
                        continue
                    if rx.search(code) and not (ex and ex.search(code)):
                        allow = ALLOW_RE.search(raw)
                        if allow and rule.check in {
                                c.strip() for c in allow.group(1).split(",")}:
                            allows.append({"check": rule.check,
                                           "file": _rel(path),
                                           "line": lineno,
                                           "excerpt": raw.strip()[:200]})
                            continue
                        violations.append(_violation(
                            rule.check, rule.severity, path, lineno, raw,
                            rule.hint, rule.source_ref, rule.candidate))
        # structured checks, per subsystem
        if "hpp-under-src" in structured:
            src_dir = SRC / sub
            if src_dir.is_dir():
                for p in src_dir.rglob("*.hpp"):
                    violations.append(_violation(
                        "hpp-under-src", "blocker", p, 0, p.name,
                        "no .hpp under src/ — move to include/xash3dpp/[private/]%s/" % sub,
                        "reviewer §3; detail CHECK-HEADERS"))
        if "naming-file-case" in structured:
            for p in sub_files:
                if not re.fullmatch(r"[a-z0-9_.]+", p.name):
                    violations.append(_violation(
                        "naming-file-case", "warning", p, 0, p.name,
                        "files are snake_case (reviewer §12)",
                        "reviewer §12; decisions-style"))
        if "naming-enum-kprefix" in structured:
            violations.extend(_scan_enum_kprefix(sub_files))
        if "nodiscard-missing" in structured:
            violations.extend(_scan_nodiscard(sub))
        if "thread-assert" in structured:
            violations.extend(_scan_thread_assert(sub))
        if "ns-qualify" in structured:
            violations.extend(_scan_ns_qualify(sub, sub_files, file_texts))
        if "test-macros" in structured:
            violations.extend(_scan_test_macros(sub))

    if files is not None:
        # File-list mode scopes EVERY check to the given set — the
        # directory-walking structured checks above would otherwise
        # report a touched subsystem's whole backlog (full-subsystem
        # coverage stays with subsystem mode, which pre-pr runs).
        in_slice = {_rel(p) for _, ps in groups for p in ps}
        violations = [v for v in violations if v["file"] in in_slice]

    threshold = SEV_ORDER.get(min_severity, 1)
    violations = [
        v for v in violations
        if SEV_ORDER[v["severity"].removeprefix("candidate-")] >= threshold
    ]
    violations.sort(key=lambda v: (-SEV_ORDER[v["severity"].removeprefix("candidate-")],
                                   v["file"], v["line"]))
    counts = {"blocker": 0, "warning": 0, "note": 0, "candidate": 0}
    for v in violations:
        sev = v["severity"]
        if sev.startswith("candidate-"):
            counts["candidate"] += 1
            sev = sev.removeprefix("candidate-")
        counts[sev] += 1
    out = {
        "subsystems": subs,
        "checks": checks,
        "files_scanned": files_scanned,
        "violations": violations,
        "counts": counts,
        "allows": allows,
        "judgment_checks_not_run": JUDGMENT_CHECKS,
    }
    if files is not None:
        out["files_ignored"] = files_ignored
    return out


def _scan_enum_kprefix(files: list[Path]) -> list[dict]:
    out = []
    for path in files:
        in_enum = False
        for lineno, code, raw in code_lines(path):
            if re.search(r"\benum\s+class\b", code):
                in_enum = True
            if in_enum:
                m = re.match(r"\s*(k[A-Z]\w*)\s*[,=}]", code)
                if m:
                    out.append(_violation(
                        "naming-enum-kprefix", "warning", path, lineno, raw,
                        "enum class values are PascalCase, no k prefix (QF)",
                        "reviewer §12; sweep NAMING_ENUM"))
                if "}" in code:
                    in_enum = False
    return out


_DECL_RX = re.compile(
    r"^\s*(?:virtual\s+)?"
    r"(?!.*\b(void|return|if|for|while|switch|else|using|typedef|namespace|template|class|struct|enum|friend|operator|static_assert|delete|default)\b)"
    r"(?:const\s+)?[A-Za-z_][\w:<>,*&\s]*?\s+&?(\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept[^;{]*)?;\s*$"
)


def _scan_nodiscard(sub: str) -> list[dict]:
    """Heuristic: non-void function declarations in headers lacking
    [[nodiscard]] on the same or previous line. Candidate findings only."""
    out = []
    for root in (INCLUDE / sub, PRIVATE / sub):
        if not root.is_dir():
            continue
        for path in sorted(root.rglob("*.hpp")):
            prev = ""
            for lineno, code, raw in code_lines(path):
                if _DECL_RX.match(code) and "[[nodiscard]]" not in code \
                        and "[[nodiscard]]" not in prev and "operator" not in code:
                    out.append(_violation(
                        "nodiscard-missing", "warning", path, lineno, raw,
                        "[[nodiscard]] is the default for non-void returns (QA)",
                        "reviewer §13; sweep NODISCARD", candidate=True))
                if code.strip():
                    prev = code
    return out


def _scan_thread_assert(sub: str) -> list[dict]:
    """Mutator method definitions whose first statements lack
    assert_thread_role. Skips pure-function subsystems by the caller's
    judgment — findings are candidates."""
    out = []
    rx = re.compile(r"^\s*void\s+\w[\w:]*::(%s)\s*\(" % MUTATOR_NAMES)
    src_dir = SRC / sub
    if not src_dir.is_dir():
        return out
    for path in sorted(src_dir.rglob("*.cpp")):
        lines = list(code_lines(path))
        for i, (lineno, code, raw) in enumerate(lines):
            if rx.match(code):
                lookahead = " ".join(c for _, c, _ in lines[i:i + 6])
                if "assert_thread_role" not in lookahead:
                    out.append(_violation(
                        "thread-assert", "warning", path, lineno, raw,
                        "main-thread mutator should open with assert_thread_role(ThreadRole::Main) (TH-Role)",
                        "reviewer §7; pre-pr Phase 2; sweep TH-Role",
                        candidate=True))
    return out


def _scan_ns_qualify(sub: str, files: list[Path],
                     file_texts: dict[Path, str]) -> list[dict]:
    out = []
    siblings = {"limits", "utilities", "memory", "platform", "core"} - {sub}
    rx = re.compile(r"(?<![:\w])(%s)::" % "|".join(sorted(siblings)))
    for path in files:
        text = file_texts.get(path, "")
        if "namespace xash::" not in text:
            continue
        for lineno, code, raw in code_lines(path):
            m = rx.search(code)
            if m and "::xash::" not in code[:m.start() + 2]:
                if re.search(r"namespace\s", code):
                    continue
                out.append(_violation(
                    "ns-qualify", "warning", path, lineno, raw,
                    "sibling namespace refs must be absolute ::xash::%s:: (NS_QUALIFY)" % m.group(1),
                    "sweep NS_QUALIFY; instructions", candidate=True))
    return out


def _scan_test_macros(sub: str) -> list[dict]:
    out = []
    tdir = TESTS / sub
    if not tdir.is_dir():
        return out
    for path in sorted(tdir.rglob("*.cpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        if re.search(r"#\s*define\s+(CHECK|REQUIRE)\b", text):
            out.append(_violation(
                "test-macros", "warning", path, 0, "#define CHECK/REQUIRE",
                "tests must use test_helpers.hpp macros, no local defines (QK)",
                "sweep TEST_MACROS; finish-subsystem §6"))
        is_test_exe = "int main" in text or re.search(r"\bCHECK(_EQ)?\s*\(", text)
        if "test_helpers.hpp" not in text and is_test_exe:
            out.append(_violation(
                "test-macros", "warning", path, 0, "missing include",
                'tests must #include "../test_helpers.hpp" (QK)',
                "sweep TEST_MACROS; finish-subsystem §6"))
    return out


# --------------------------------------------------------------------------- #
# limits_scan
# --------------------------------------------------------------------------- #

_LIMIT_BLOCK = re.compile(
    r"#ifndef\s+(XASH_LIMIT_\w+)\s*\n"
    r"inline constexpr\s+(\S+)\s+(\w+)\s*=\s*([^;]+);[^\n]*\n"
    r"#else\s*\n"
    r"inline constexpr\s+\S+\s+\w+\s*=\s*XASH_LIMIT_\w+;\s*\n"
    r"#endif",
    re.MULTILINE,
)
_GROUP_RX = re.compile(r"^//\s*(\w[\w /-]*?)\s+subsystem\s*$", re.MULTILINE)

_MAGIC_PATTERNS = [
    ("array-size", re.compile(r"\[\s*([0-9]{2,})\s*\]")),
    ("template-capacity", re.compile(r"<\s*[A-Za-z_][\w:]*\s*,\s*([0-9]{2,})\s*>")),
    ("constexpr-value", re.compile(r"constexpr[^=\n]*=\s*([0-9]{2,})\b")),
    ("reserve-call", re.compile(r"\.(reserve|resize)\s*\(\s*([0-9]{2,})\s*\)")),
]


def parse_limits() -> list[dict]:
    text = LIMITS_HPP.read_text(encoding="utf-8", errors="replace")
    groups = [(m.start(), m.group(1)) for m in _GROUP_RX.finditer(text)]
    limits = []
    for m in _LIMIT_BLOCK.finditer(text):
        group = ""
        for pos, name in groups:
            if pos < m.start():
                group = name
        limits.append({
            "macro": m.group(1),
            "type": m.group(2),
            "name": m.group(3),
            "default": m.group(4).strip(),
            "group": group,
            "line": text[:m.start()].count("\n") + 1,
        })
    return limits


def limits_scan(subsystem: str | None) -> dict:
    subs = resolve_scope(subsystem)
    limits = parse_limits()
    by_name = {l["name"]: l for l in limits}

    # usage census across ALL subsystems (dead-limit detection must be global)
    users: dict[str, list[str]] = {l["name"]: [] for l in limits}
    all_files = [f for s in resolve_scope(None) for f in subsystem_files(s, tests=True)]
    magic: list[dict] = []
    shadow: list[dict] = []
    scoped = set()
    for s in subs:
        scoped.update(subsystem_files(s, tests=False))
    value_index: dict[str, list[str]] = {}
    for l in limits:
        value_index.setdefault(l["default"], []).append(l["name"])

    for path in all_files:
        for lineno, code, raw in code_lines(path):
            for name in re.findall(r"limits::(\w+)", code):
                if name in users:
                    users[name].append("%s:%d" % (_rel(path), lineno))
            if path in scoped and path != LIMITS_HPP:
                for kind, rx in _MAGIC_PATTERNS:
                    for m in rx.finditer(code):
                        literal = m.group(m.lastindex or 1)
                        entry = {
                            "file": _rel(path), "line": lineno, "kind": kind,
                            "literal": literal, "context": raw.strip()[:160],
                        }
                        if literal in value_index and "limits::" not in code:
                            shadow.append({**entry,
                                           "matches_limit": value_index[literal]})
                        else:
                            magic.append(entry)

    out_limits = [{**l, "users": len(users[l["name"]])} for l in limits]
    unused = [l["name"] for l in limits if not users[l["name"]]]
    return {
        "subsystems": subs,
        "limits_total": len(limits),
        "limits": out_limits,
        "unused": unused,
        "magic": magic,
        "shadow": shadow,
        "note": "magic/shadow hits include non-tunable literals; classify per "
                "the limits-audit exemptions (math constants, wire-frozen "
                "discriminators, test files are already excluded).",
    }


# --------------------------------------------------------------------------- #
# stub_scan
# --------------------------------------------------------------------------- #

_FUNC_DEF = re.compile(r"^[\w:\[\]<>,*&~\s]+?\b([\w~]+(?:::[\w~]+)+|\w+)\s*\([^;{}]*\)?\s*(?:const)?\s*(?:noexcept)?\s*\{?\s*$")


def stub_scan(subsystem: str) -> dict:
    subs = resolve_scope(subsystem)
    stubs: list[dict] = []
    todo_count = 0
    for sub in subs:
        src_dir = SRC / sub
        if not src_dir.is_dir():
            continue
        for path in sorted(src_dir.rglob("*.cpp")):
            enclosing = "<file scope>"
            text = path.read_text(encoding="utf-8", errors="replace")
            for lineno, rawline in enumerate(text.splitlines(), start=1):
                m = _FUNC_DEF.match(rawline)
                if m and "(" in rawline:
                    enclosing = m.group(1)
                tm = re.search(r"//\s*(TODO|STUB|XASH3DPP-STUB)\b(.*)", rawline)
                if tm:
                    todo_count += 1
                    stubs.append({
                        "file": _rel(path), "line": lineno,
                        "symbol": enclosing, "marker": tm.group(0).strip()[:120],
                    })
    tests = _test_liveness(subs)
    return {"subsystems": subs, "todo_count": todo_count, "stubs": stubs,
            "tests": tests}


def _test_liveness(subs: list[str]) -> dict:
    files = live = stub = 0
    stub_files = []
    for sub in subs:
        tdir = TESTS / sub
        if not tdir.is_dir():
            continue
        for path in sorted(tdir.rglob("*.cpp")):
            files += 1
            text = "\n".join(c for _, c, _ in code_lines(path))
            checks = len(re.findall(r"\b(CHECK|CHECK_EQ|CHECK_NEAR|REQUIRE)\s*\(", text))
            if checks > 0:
                live += 1
            else:
                stub += 1
                stub_files.append(_rel(path))
    return {"files": files, "live": live, "stub": stub, "stub_files": stub_files}


# --------------------------------------------------------------------------- #
# status_table
# --------------------------------------------------------------------------- #

_STUB_MARKER_RX = re.compile(r"//\s*(TODO|STUB|XASH3DPP-STUB)\b")


def _stub_marker_count(src_files: list[Path]) -> int:
    count = 0
    for path in src_files:
        text = path.read_text(encoding="utf-8", errors="replace")
        count += sum(1 for line in text.splitlines()
                     if _STUB_MARKER_RX.search(line))
    return count


def status_table() -> dict:
    rows = []
    for sub in resolve_scope(None):
        src_files = [p for p in (SRC / sub).rglob("*.cpp")] if (SRC / sub).is_dir() else []
        inc = (INCLUDE / sub).is_dir() or (PRIVATE / sub).is_dir()
        tst = (TESTS / sub).is_dir() and any((TESTS / sub).rglob("*.cpp"))
        if src_files and tst:
            status = "Complete"
        elif src_files:
            status = "Partial"
        else:
            status = "Skeleton"
        rows.append({
            "subsystem": sub, "src_files": len(src_files),
            "include": inc, "tests": tst, "status": status,
            "stub_markers": _stub_marker_count(src_files),
        })
    # Structural completeness can hide unfinished work (the cvar_full_set
    # incident): surface every "Complete" subsystem still carrying
    # TODO/stub markers so consumers (whereami stub-debt, plan refresh)
    # see the debt without a per-subsystem stub_scan pass.
    complete_with_stubs = [
        {"subsystem": r["subsystem"], "stub_markers": r["stub_markers"]}
        for r in rows if r["status"] == "Complete" and r["stub_markers"] > 0
    ]
    return {"rows": rows, "complete_with_stubs": complete_with_stubs}


def status_markdown(data: dict) -> str:
    lines = ["| Subsystem | src/ files | include/ | tests/ | Status |",
             "|-----------|------------|----------|--------|--------|"]
    for r in data["rows"]:
        lines.append("| %s | %d | %s | %s | %s |" % (
            r["subsystem"], r["src_files"],
            "✓" if r["include"] else "—", "✓" if r["tests"] else "—",
            r["status"]))
    return "\n".join(lines)


def status_check(data: dict) -> list[str]:
    """Diff generated statuses against the implementation-plan status table."""
    plan = DOCS / "implementation-plan.md"
    text = plan.read_text(encoding="utf-8", errors="replace")
    # scope to the "## Status Table" section only (the Subsystem Scores
    # table reuses subsystem names in its first column)
    m = re.search(r"## Status Table(.*?)(?:\n## |\n_{10,})", text, re.DOTALL)
    if m:
        text = m.group(1)
    drift = []
    plan_status: dict[str, str] = {}
    for line in text.splitlines():
        m = re.match(r"\|\s*`?(\w+)`?\s*\|", line)
        if m and line.count("|") >= 3:
            cells = [c.strip().strip("*") for c in line.strip("|").split("|")]
            for cell in reversed(cells):
                if cell and re.search(r"[A-Za-z]", cell):
                    plan_status[m.group(1)] = cell
                    break
    for r in data["rows"]:
        planned = plan_status.get(r["subsystem"])
        if planned and planned.lower() not in (r["status"].lower(),):
            if not any(w in planned.lower() for w in
                       (r["status"].lower(), "done", "in progress")):
                drift.append("%s: tree says %s, plan table says %r"
                             % (r["subsystem"], r["status"], planned))
    return drift


# --------------------------------------------------------------------------- #
# finish_check
# --------------------------------------------------------------------------- #

_BOUNDARY_HEADINGS = ["Responsibility", "ABI", "Interface", "Dependencies",
                      "Owned state", "Quirks"]


def finish_check(subsystem: str, run_tests: bool = False) -> dict:
    [sub] = resolve_scope(subsystem)
    items: list[dict] = []

    def add(idx, name, status, evidence):
        items.append({"id": idx, "name": name, "status": status,
                      "evidence": evidence if isinstance(evidence, list) else [evidence]})

    # 1 boundary spec
    boundary = DOCS / "boundaries" / ("%s-boundary.md" % sub)
    if boundary.is_file():
        text = boundary.read_text(encoding="utf-8", errors="replace")
        missing = [h for h in _BOUNDARY_HEADINGS if h.lower() not in text.lower()]
        add(1, "Boundary spec", "pass" if not missing else "fail",
            "exists" if not missing else "missing sections: %s" % ", ".join(missing))
    else:
        add(1, "Boundary spec", "fail", "%s not found" % _rel(boundary))

    # 2 limits
    lim = limits_scan(sub)
    group_present = any(sub in l["group"] for l in lim["limits"])
    magic_n = len(lim["magic"]) + len(lim["shadow"])
    add(2, "limits.hpp",
        "pass" if magic_n == 0 else "needs-judgment",
        ["limits group present: %s" % group_present,
         "%d magic/shadow literal candidates (classify per exemptions)" % magic_n])

    # 3 stats
    sub_files = subsystem_files(sub)
    joined = "\n".join(p.read_text(encoding="utf-8", errors="replace")
                       for p in sub_files if p.suffix == ".hpp")
    has_stats = bool(re.search(r"struct\s+\w*Stats\b", joined)) and \
        "stats()" in joined
    exempt = any("stats exempt" in p.read_text(encoding="utf-8", errors="replace")
                 for p in sub_files)
    add(3, "STATS_TIERS", "pass" if (has_stats or exempt) else "fail",
        "Stats struct + accessor" if has_stats else
        ("exemption comment present" if exempt else
         "no Stats struct and no 'stats exempt' comment"))

    # 4 nodiscard
    nd = _scan_nodiscard(sub)
    add(4, "NODISCARD", "pass" if not nd else "needs-judgment",
        "%d candidate omissions" % len(nd))

    # 5 naming
    nm = _scan_enum_kprefix(subsystem_files(sub))
    add(5, "NAMING_FN / NAMING_ENUM", "pass" if not nm else "fail",
        "%d k-prefixed enum values" % len(nm))

    # 6 tests exist + macros
    tdir = TESTS / sub
    tfiles = list(tdir.rglob("*.cpp")) if tdir.is_dir() else []
    macro_issues = _scan_test_macros(sub)
    add(6, "Tests present + test_helpers.hpp",
        "pass" if tfiles and not macro_issues else "fail",
        ["%d test files" % len(tfiles),
         "%d macro-convention issues" % len(macro_issues)])

    # 7 architecture docs + threading
    arch = DOCS / "architecture" / sub
    arch_ok = (arch / "README.md").is_file() or any(arch.glob("*.md")) if arch.is_dir() else False
    thr = (DOCS / "threading-analysis" / ("%s-threading.md" % sub)).is_file()
    if not thr and arch.is_dir():
        thr = any("hreading" in p.read_text(encoding="utf-8", errors="replace")
                  for p in arch.glob("*.md"))
    add(7, "Architecture docs + threading",
        "pass" if arch_ok and thr else ("needs-judgment" if arch_ok else "fail"),
        ["architecture docs: %s" % arch_ok, "threading documented: %s" % thr])

    # 8 tests pass — filter by the subsystem's actual test names, parsed
    # from tests/<sub>/CMakeLists.txt (xash3dpp_add_test / add_test)
    if run_tests:
        from .buildtools import test as run_test
        names = _subsystem_test_names(sub)
        if names:
            rx = "^(%s)$" % "|".join(sorted(names))
            result = run_test(filter_regex=rx)
            ok = result["failed"] == 0 and result["total"] == len(names)
            add(8, "Tests pass", "pass" if ok else "fail",
                "%d/%d passed (%d registered)" % (
                    result["passed"], result["total"], len(names)))
        else:
            add(8, "Tests pass", "fail", "no test targets registered")
    else:
        add(8, "Tests pass", "needs-judgment", "not run (use --run-tests)")

    # 9 compat / satellite / sockets
    comp = compliance_scan(sub, checks="compat-global,os-socket")
    q11 = boundary.is_file() and "Q-11" in boundary.read_text(encoding="utf-8",
                                                              errors="replace")
    hard = [v for v in comp["violations"]
            if not v["severity"].startswith("candidate-")]
    add(9, "Compat & satellite (Q-11/Q-12)",
        "pass" if not hard and q11 else ("fail" if hard else "needs-judgment"),
        ["%d compat/socket violations" % len(hard),
         "Q-11 verdict in boundary spec: %s" % q11])

    passed = sum(1 for i in items if i["status"] == "pass")
    return {"subsystem": sub, "items": items,
            "summary": "%d/9 pass, %d need judgment, %d fail" % (
                passed,
                sum(1 for i in items if i["status"] == "needs-judgment"),
                sum(1 for i in items if i["status"] == "fail"))}


def _subsystem_test_names(sub: str) -> list[str]:
    cml = TESTS / sub / "CMakeLists.txt"
    if not cml.is_file():
        return []
    text = cml.read_text(encoding="utf-8", errors="replace")
    names = re.findall(r"xash3dpp_add_test\(\s*(\w+)", text)
    names += re.findall(r"add_test\(\s*NAME\s+(\w+)", text)
    # a foreach over a list variable registers each listed name
    m = re.search(r"set\(test_sources(.*?)\)", text, re.DOTALL)
    if m and "foreach" in text:
        names += re.findall(r"(test_\w+)", m.group(1))
    return sorted({n for n in names if not n.startswith("${")})


# --------------------------------------------------------------------------- #
# dep_scan
# --------------------------------------------------------------------------- #

def dep_scan() -> dict:
    """Dependency edges from InitParams fields and cross-namespace refs."""
    edges: set[tuple[str, str]] = set()
    subs = resolve_scope(None)
    ns_rx = re.compile(r"(?:::)?xash::(%s)::" % "|".join(subs))
    for sub in subs:
        for path in subsystem_files(sub):
            for _, code, _ in code_lines(path):
                for m in ns_rx.finditer(code):
                    target = m.group(1)
                    if target != sub:
                        edges.add((sub, target))
    inits = []
    for path in sorted(INCLUDE.rglob("*.hpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r"struct\s+(\w+InitParams)\b", text):
            inits.append({"struct": m.group(1), "file": _rel(path)})
    cycles = sorted(
        "%s <-> %s" % (a, b) for a, b in edges if (b, a) in edges and a < b
    )
    return {
        "edges": sorted(["%s -> %s" % e for e in edges]),
        "init_params": inits,
        "cycles": cycles,
    }
