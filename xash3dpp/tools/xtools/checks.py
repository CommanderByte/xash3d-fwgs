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
# The marker is honored either on the flagged line itself OR on the contiguous
# block of `//` comment lines directly above it (the doc-comment attached to
# the construct) — so annotating above a single-line finding works, not only
# the trailing-comment form.  (A finding on an inner line of a multi-line
# statement is not preceded by its comment block, so there the marker must sit
# on the flagged line.)  Every allow is reported in the scan result so pre-pr
# can audit the full list.
ALLOW_RE = re.compile(r"compliance-allow\(\s*([\w\-, ]+?)\s*\)")

# g_* file-scope definition shape (mirrors di-global-ref's definition
# exclusion) with the name captured — used to propagate a definition-line
# compliance-allow to every use of that global (6B S2 refinement).
_G_DEF_RX = re.compile(
    r"^(?:static\s+)?[\w:<>*&\s]+[\s*&](g_\w+)\s*(?:\[[^\]]*\]\s*)*(?:=|;|\{)")

# unique_ptr<T> inner type (first template arg, default deleter) — used to
# check T against the pool-owned set for the unique-ptr-nonpimpl suppression.
_UNIQUE_PTR_T_RX = re.compile(
    r"std::unique_ptr\s*<\s*(?:class\s+|struct\s+)?([\w:]+?)\s*>")
# `class`/`struct Name` opener and a class-scoped operator-delete declaration.
_CLASS_OPEN_RX = re.compile(r"\b(?:class|struct)\s+(\w+)")
_OP_DELETE_DECL_RX = re.compile(r"\boperator\s+delete\b")


def _collect_pool_owned_types(paths: "list[Path]") -> "set[str]":
    """Names of classes/structs that declare a class-scoped `operator delete`
    (pool-owned classes — their default-deleter unique_ptr frees through
    mem_free per Q-22).  A light per-file scan: a class name is pool-owned if
    an `operator delete` appears in its body before the next top-level
    class/struct opener.  Header-scoped only (declarations live there).  Pure
    over code_lines — unit-tested."""
    owned: set[str] = set()
    for path in paths:
        if path.suffix != ".hpp":
            continue
        current: str | None = None
        for _lineno, code, _raw in code_lines(path):
            opener = _CLASS_OPEN_RX.search(code)
            if opener:
                # A forward decl (`class Foo;`) opens no body — ignore it.
                current = opener.group(1) if "{" in code or ";" not in code \
                    else current
            if current and _OP_DELETE_DECL_RX.search(code):
                owned.add(current)
    return owned


def _rel(p: Path) -> str:
    try:
        return p.relative_to(REPO).as_posix()
    except ValueError:
        return p.as_posix()


def _allow_context(lines: list[tuple[int, str, str]], idx: int) -> str:
    """The text a compliance-allow marker may live in to exempt the finding on
    lines[idx]: the flagged line's raw text plus the contiguous block of `//`
    comment lines immediately above it.  code_lines / code_lines_from_text yield
    every physical line (comments blanked from `code` but kept in `raw`), so
    list-index adjacency is exact.  Pure — unit-tested."""
    parts = [lines[idx][2]]
    j = idx - 1
    while j >= 0 and lines[j][2].lstrip().startswith("//"):
        parts.append(lines[j][2])
        j -= 1
    return "\n".join(parts)


# T9b: transient DI param structs (`struct FooInitParams` / bare
# `struct InitParams`) — the same name shape dep_scan keys on.
_INITPARAMS_RX = re.compile(r"\bstruct\s+\w*InitParams\b")


def _initparams_flags(lines: list[tuple[int, str, str]]) -> list[bool]:
    """Per-line True while inside a `struct \\w*InitParams` body (open line
    and closing `};` line included).  Depth-naive: DI param structs are flat
    by convention (P-3); a nested type inside one would end the span early.
    Pure — unit-tested."""
    flags: list[bool] = []
    inside = False
    for _, code, _ in lines:
        if not inside and _INITPARAMS_RX.search(code):
            flags.append(True)
            inside = "};" not in code  # one-line struct opens AND closes here
            continue
        flags.append(inside)
        if inside and "};" in code:
            inside = False
    return flags


def _stmt_start_idx(lines: list[tuple[int, str, str]], idx: int,
                    max_back: int = 5) -> int:
    """Index of the physical line that STARTS the statement containing
    lines[idx]: walk back while the previous line neither terminates a
    statement/block (code ending ';', '{' or '}') nor is blank/comment-only
    (code blanked to ""), bounded by max_back.  Used so a `// SAFETY:`
    comment above a multi-line cast statement is credited when the cast
    token sits on a continuation line (annotation-coverage T9a).  Pure —
    unit-tested."""
    j = idx
    while j > 0 and idx - j < max_back:
        prev_code = lines[j - 1][1].strip()
        if not prev_code:                      # blank or comment-only line
            break
        if prev_code.endswith((";", "{", "}")):
            break
        j -= 1
    return j


def _violation(check, severity, path, line, excerpt, hint, rule_ref,
               candidate=False, allow_ctx=None):
    return {
        "check": check,
        "severity": ("candidate-" + severity) if candidate else severity,
        "file": _rel(path),
        "line": line,
        "excerpt": excerpt.strip()[:200],
        "hint": hint,
        "rule_ref": rule_ref,
        # Text searched for a compliance-allow marker (_filter_allows); the
        # flagged line by default, or the flagged line + its preceding comment
        # block when the caller passes allow_ctx.  Stripped from the envelope.
        "_raw": allow_ctx if allow_ctx is not None else excerpt,
    }


def _filter_allows(violations: list[dict]) -> tuple[list[dict], list[dict]]:
    """Honor `// compliance-allow(<check>)` markers uniformly: a violation whose
    stashed `_raw` (the flagged line, plus its preceding comment block when the
    producer supplied one) carries a marker naming its check is moved to the
    allows list.  `_raw` is stripped from every kept violation so it never leaks
    into the envelope; the allow entry reports the clean flagged-line excerpt."""
    kept: list[dict] = []
    allowed: list[dict] = []
    for v in violations:
        raw = v.pop("_raw", "")
        m = ALLOW_RE.search(raw)
        if m and v["check"] in {c.strip() for c in m.group(1).split(",")}:
            allowed.append({"check": v["check"], "file": v["file"],
                            "line": v["line"], "excerpt": v["excerpt"]})
        else:
            kept.append(v)
    return kept, allowed


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


def _base_excerpts(rels: list[str], base_ref: str) -> dict[str, set[str]]:
    """{repo-rel path: set of stripped[:200] lines} for each file's base-commit
    version.  Files absent at base (added by the slice) are omitted, so all of
    their findings count as introduced."""
    from . import state  # git plumbing lives in the state layer
    out: dict[str, set[str]] = {}
    for rel in rels:
        rc, lines = state._git(["show", "%s:%s" % (base_ref, rel)])
        if rc != 0:
            continue
        out[rel] = {ln.strip()[:200] for ln in lines if ln.strip()}
    return out


def _introduced_findings(findings: list[dict],
                         base_excerpts: dict[str, set[str]]) -> list[dict]:
    """Keep only findings whose flagged line text is absent from the file's base
    version — i.e. the change introduced or modified that line.  Matched by line
    CONTENT (the `excerpt`, == `_violation`'s raw.strip()[:200]), not line
    number, so an unrelated edit that merely shifts a pre-existing finding does
    not resurface it.  Findings whose excerpt is a file name, not a code line
    (hpp-under-src, naming-file-case), are always kept — a line set cannot
    confirm those pre-existed."""
    out: list[dict] = []
    for f in findings:
        base = base_excerpts.get(f["file"])
        if base is not None and f.get("excerpt", "") in base:
            continue
        out.append(f)
    return out


# --------------------------------------------------------------------------- #
# compliance_scan
# --------------------------------------------------------------------------- #

def compliance_scan(subsystem: str | None, checks: str = "all",
                    min_severity: str = "note",
                    files: list[str] | None = None,
                    baseline_base: str = "") -> dict:
    """Run the [M] ruleset over one subsystem (or all).

    checks: "all" | "prepr" | "detail" | comma-list of check ids.
    files: explicit repo-relative paths (a slice_diff change set) —
    overrides subsystem discovery so gates cover exactly what a slice
    touched, wherever it lives (the S7a cvar_ops.cpp gap).
    baseline_base: a git ref (with `files`) — keep only findings the change
    INTRODUCED, dropping ones whose flagged line is unchanged from that base.
    Removes the "pre-existing finding surfaced only because my edit pulled the
    file into the slice scan" noise.

    checks="annotation-coverage" returns the QN coverage report (per-subsystem
    denominators) instead of a violation list — the 6B backfill measure.
    """
    if checks == "annotation-coverage":
        return annotation_coverage(subsystem)
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
            structured = ["hpp-under-src", "thread-assert", "test-macros",
                          "operator-delete-pairing"]
    else:
        explicit = {c.strip() for c in checks.split(",") if c.strip()}
        active = [r for r in RULES if r.check in explicit]
        structured = [c for c in STRUCTURED_CHECKS if c in explicit]

    violations: list[dict] = []
    allows: list[dict] = []
    files_scanned = 0
    for sub, sub_files in groups:
        files_scanned += len(sub_files)
        # Pool-owned class names (types declaring an `operator delete`): a
        # std::unique_ptr<T> with the DEFAULT deleter over such a T frees
        # through the class operator delete → mem_free, which IS the Q-22
        # canonical idiom (File / ISearchBackend precedents) — not a
        # unique-ptr-nonpimpl finding.  Collected group-wide so a header
        # declaring the class suppresses uses in sibling headers (6B S4).
        pool_owned = _collect_pool_owned_types(sub_files)
        file_texts: dict[Path, str] = {}
        for path in sub_files:
            scope = _scope_of(path)
            lines = list(code_lines(path))
            file_texts[path] = "\n".join(code for _, code, _ in lines)
            has_extern_c = 'extern "C"' in file_texts[path]
            # di-global-ref: a g_* DEFINITION carrying a compliance-allow
            # marker sanctions every reference to that name in the file —
            # the definition is the adjudication point; per-use markers
            # would be pure noise (6B S2 refinement).  Map name → def raw
            # line so the use-site's allow-context inherits the marker.
            g_def_allows: dict[str, str] = {}
            for _dl, _dc, _dr in lines:
                dm = _G_DEF_RX.match(_dc)
                if dm and ALLOW_RE.search(_dr):
                    g_def_allows[dm.group(1)] = _dr
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
                for idx, (lineno, code, raw) in enumerate(lines):
                    if not (code.strip() or rule.match_raw):
                        continue
                    # match_raw rules target comment content; exclusions always
                    # run on the RAW line — suppression markers (@pre-reserved:,
                    # @lifetime:, SAFETY:) live in comments the code view blanks.
                    subject = raw if rule.match_raw else code
                    m = rx.search(subject)
                    if m and not (ex and ex.search(raw)):
                        # unique_ptr<T> over a pool-owned T is the sanctioned
                        # idiom, not a finding (Q-22) — skip silently, no marker
                        # needed at the use site.
                        if rule.check == "unique-ptr-nonpimpl":
                            tm = _UNIQUE_PTR_T_RX.search(code)
                            if tm and tm.group(1) in pool_owned:
                                continue
                        # Stash the allow-context (flagged line + its preceding
                        # comment block); _filter_allows does the marker match
                        # uniformly with the structured checks.
                        ctx = _allow_context(lines, idx)
                        if rule.check == "di-global-ref" \
                                and m.group(0) in g_def_allows:
                            ctx = ctx + "\n" + g_def_allows[m.group(0)]
                        violations.append(_violation(
                            rule.check, rule.severity, path, lineno, raw,
                            rule.hint, rule.source_ref, rule.candidate,
                            allow_ctx=ctx))
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
        if "operator-delete-pairing" in structured:
            violations.extend(_scan_operator_delete_pairing(sub_files))

    # Inline compliance-allow markers for the structured checks (and a no-op
    # re-check + _raw strip for the regex-rule hits, which filtered inline).
    violations, structured_allows = _filter_allows(violations)
    allows.extend(structured_allows)

    if files is not None:
        # File-list mode scopes EVERY check to the given set — the
        # directory-walking structured checks above would otherwise
        # report a touched subsystem's whole backlog (full-subsystem
        # coverage stays with subsystem mode, which pre-pr runs).
        in_slice = {_rel(p) for _, ps in groups for p in ps}
        violations = [v for v in violations if v["file"] in in_slice]

    baseline_suppressed = 0
    if files is not None and baseline_base:
        rels = sorted({_rel(p) for _, ps in groups for p in ps})
        base_ex = _base_excerpts(rels, baseline_base)
        kept_v = _introduced_findings(violations, base_ex)
        kept_a = _introduced_findings(allows, base_ex)
        baseline_suppressed = (len(violations) - len(kept_v)) \
            + (len(allows) - len(kept_a))
        violations, allows = kept_v, kept_a

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
    if baseline_base:
        out["baseline_base"] = baseline_base
        out["baseline_suppressed"] = baseline_suppressed
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
    r"(?!.*\b(void|return|if|for|while|switch|else|using|typedef|namespace|template|class|struct|enum|friend|operator|static_assert|delete|default|explicit)\b)"
    r"(?:const\s+)?[A-Za-z_][\w:<>,*&\s]*?\s+&?(\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept[^;{]*)?;\s*$"
)


def _decl_args_are_bare_ids(code: str) -> bool:
    """True when every parenthesized argument is a bare snake_case identifier —
    a variable initializer (`std::string result( s );`) inside an inline body,
    not a declaration (C++ parameters carry a type; repo naming makes types
    PascalCase, so lowercase-only args can't be unnamed-param types).  Empty
    parens stay a declaration.  6B S1 false-positive fix."""
    m = re.search(r"\(([^()]*)\)", code)
    if not m or not m.group(1).strip():
        return False
    parts = [p.strip() for p in m.group(1).split(",")]
    return all(re.fullmatch(r"[a-z_]\w*", p) for p in parts)


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
                        and "[[nodiscard]]" not in prev and "operator" not in code \
                        and not _decl_args_are_bare_ids(code):
                    out.append(_violation(
                        "nodiscard-missing", "warning", path, lineno, raw,
                        "[[nodiscard]] is the default for non-void returns (QA)",
                        "reviewer §13; sweep NODISCARD", candidate=True))
                if code.strip():
                    prev = code
    return out


# Mutator DEFINITIONS at statement start: return-type token(s) then an
# optionally class-qualified mutator name.  The leading-type requirement keeps
# indented CALLS (no type prefix) out; control-flow keywords are excluded
# explicitly.  Broadened from void-only methods (QN wave, 2026-07-06): any
# return type, and free-function mutators too — the server's public entries
# are free functions over ServerRuntime.  Findings stay candidates.
_MUTATOR_DEF_RX = re.compile(
    r"^\s*(?!return\b|else\b|case\b|if\b|while\b|for\b|switch\b)"
    r"(?:[\w:<>*&\[\],]+\s+)+"
    r"((?:\w[\w:]*::)?(?:%s))\s*\(" % MUTATOR_NAMES)

# A trailing `;` means the line is a declaration or a statement, never a
# definition: `virtual void set_current_map(...) = 0;`, a forward decl, a
# ternary continuation `: clear_trace();`, or a variable declaration whose
# TYPE ends in a mutator verb (`LevelStateLoader loader( *buf_, *table_ );`).
_MUTATOR_NOT_DEF_RX = re.compile(r";\s*$")
# A const-qualified member function cannot mutate, so it is not a mutator no
# matter how its name reads (`Netchan::connect_time() const noexcept`).  The
# `\)` anchor keeps `const` parameters (`void f( const T &x )`) out of scope.
_MUTATOR_CONST_RX = re.compile(r"\)\s*const\b")


def _is_mutator_def(code: str) -> bool:
    """A mutator DEFINITION line, with the two structural non-definitions
    filtered out.  Added by the 2026-07-20 audit follow-up alongside the
    MUTATOR_NAMES suffix fix: broadening the verb set surfaced real mutators
    but also these two artifact classes, and annotating source with
    `compliance-allow` to silence a scanner artifact is the wrong fix."""
    if not _MUTATOR_DEF_RX.match(code):
        return False
    return not (_MUTATOR_NOT_DEF_RX.search(code)
                or _MUTATOR_CONST_RX.search(code))


def _scan_thread_assert(sub: str) -> list[dict]:
    """Mutator definitions whose first statements lack assert_thread_role.
    Skips pure-function subsystems by the caller's judgment — findings are
    candidates."""
    out = []
    src_dir = SRC / sub
    if not src_dir.is_dir():
        return out
    for path in sorted(src_dir.rglob("*.cpp")):
        lines = list(code_lines(path))
        for i, (lineno, code, raw) in enumerate(lines):
            if _is_mutator_def(code):
                lookahead = " ".join(c for _, c, _ in lines[i:i + 6])
                if "assert_thread_role" not in lookahead \
                        and "assert_main_thread" not in lookahead:
                    out.append(_violation(
                        "thread-assert", "warning", path, lineno, raw,
                        "main-thread mutator should open with assert_thread_role(ThreadRole::Main) (TH-Role)",
                        "reviewer §7; pre-pr Phase 2; sweep TH-Role",
                        candidate=True, allow_ctx=_allow_context(lines, i)))
    return out


_OP_DELETE_RX = re.compile(r"\boperator\s+delete\s*\(([^)]*)\)")


def _operator_delete_pairing_issues(
        lines: list[tuple[int, str, str]]) -> list[tuple[int, int, str]]:
    """(index, lineno, raw) for the first `operator delete` in a file that
    declares one overload form but not both (unsized `void*` + sized
    `void*, std::size_t`).  File-granular — headers here declare at most one
    pool-owned class.  Pure over code_lines tuples — unit-tested."""
    unsized = sized = 0
    first: tuple[int, int, str] | None = None
    for idx, (lineno, code, raw) in enumerate(lines):
        m = _OP_DELETE_RX.search(code)
        if not m:
            continue
        if first is None:
            first = (idx, lineno, raw)
        if "size_t" in m.group(1):
            sized += 1
        else:
            unsized += 1
    if first is not None and (unsized == 0 or sized == 0):
        return [first]
    return []


def _scan_operator_delete_pairing(files: list[Path]) -> list[dict]:
    out = []
    for path in files:
        lines = list(code_lines(path))
        for idx, lineno, raw in _operator_delete_pairing_issues(lines):
            out.append(_violation(
                "operator-delete-pairing", "warning", path, lineno, raw,
                "pool-owned classes override BOTH operator delete overloads "
                "(unsized + sized) so the default unique_ptr deleter is "
                "correct (Q-22)",
                "Q-22 LIFECYCLE_MODEL; detail CHECK-LIFECYCLE",
                allow_ctx=_allow_context(lines, idx)))
    return out


def _rule_pattern(check: str) -> str:
    for r in RULES:
        if r.check == check:
            return r.pattern
    raise KeyError(check)


_ANNOT_EXEMPT_RX = re.compile(r"@annotation-exempt:")


def annotation_coverage(subsystem: str | None) -> dict:
    """Per-subsystem QN annotation coverage WITH DENOMINATORS — the Chunk 6B
    "backfill complete" measure and finish_check item-10 data source.

    required-counts are heuristic (the same patterns as the candidate rules);
    a site counts as satisfied when annotated OR carrying an
    `@annotation-exempt:` marker (line, preceding comment block, or anywhere
    file-scope).  `// Pre:` is reported as raw usage — no denominator is
    derivable for preconditions."""
    subs = resolve_scope(subsystem)
    life_rx = re.compile(_rule_pattern("lifetime-annotation"))
    vec_rx = re.compile(_rule_pattern("prereserve-annotation"))
    cast_rx = re.compile(r"\breinterpret_cast\s*<|\bconst_cast\s*<")
    out: dict[str, dict] = {}
    for sub in subs:
        cnt: dict[str, dict] = {m: {"required": 0, "annotated": 0, "exempt": 0}
                                for m in ("lifetime", "pre_reserved", "safety",
                                          "thread_assert", "thread_safety")}
        pre_uses = 0
        files = subsystem_files(sub)
        for path in files:
            scope = _scope_of(path)
            lines = list(code_lines(path))
            file_exempt = any(_ANNOT_EXEMPT_RX.search(r) for _, _, r in lines)

            def _mark(slot: dict, tag: str, idx: int,
                      idx2: int | None = None) -> None:
                slot["required"] += 1
                ctx = _allow_context(lines, idx)
                if idx2 is not None and idx2 != idx:
                    # union with the statement-start context (multi-line
                    # statements: the annotation sits above the statement,
                    # the flagged token on a continuation line — T9a).
                    ctx = ctx + "\n" + _allow_context(lines, idx2)
                if tag in ctx:
                    slot["annotated"] += 1
                elif file_exempt or _ANNOT_EXEMPT_RX.search(ctx):
                    slot["exempt"] += 1

            ip_flags = _initparams_flags(lines) if scope == "include" else None
            for idx, (lineno, code, raw) in enumerate(lines):
                if "// Pre:" in raw:
                    pre_uses += 1
                if scope == "include":
                    if life_rx.search(code):
                        if ip_flags[idx]:
                            # Transient DI param struct (P-3): its raw
                            # pointers/refs are copied into the real
                            # state-bearing Impl at init, so they leave the
                            # REQUIRED denominator (T9b).  An @lifetime: tag
                            # present anyway stays creditable.
                            if "@lifetime:" in _allow_context(lines, idx):
                                cnt["lifetime"]["required"] += 1
                                cnt["lifetime"]["annotated"] += 1
                        else:
                            _mark(cnt["lifetime"], "@lifetime:", idx)
                    if vec_rx.search(code):
                        _mark(cnt["pre_reserved"], "@pre-reserved:", idx)
                if scope == "src":
                    if cast_rx.search(code):
                        _mark(cnt["safety"], "SAFETY:", idx,
                              _stmt_start_idx(lines, idx))
                    if _is_mutator_def(code):
                        slot = cnt["thread_assert"]
                        slot["required"] += 1
                        lookahead = " ".join(c for _, c, _ in lines[idx:idx + 6])
                        ctx = _allow_context(lines, idx)
                        if "assert_thread_role" in lookahead \
                                or "assert_main_thread" in lookahead:
                            slot["annotated"] += 1
                        elif file_exempt or _ANNOT_EXEMPT_RX.search(ctx) \
                                or "compliance-allow(thread-assert" in ctx:
                            slot["exempt"] += 1
        # @thread-safety: is a per-PUBLIC-header contract, not per-line
        ts = cnt["thread_safety"]
        for path in files:
            if path.suffix != ".hpp" or str(PRIVATE) in str(path):
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            ts["required"] += 1
            if "@thread-safety:" in text:
                ts["annotated"] += 1
            elif _ANNOT_EXEMPT_RX.search(text):
                ts["exempt"] += 1
        for slot in cnt.values():
            req = slot["required"]
            slot["coverage_pct"] = (
                round(100.0 * (slot["annotated"] + slot["exempt"]) / req, 1)
                if req else 100.0)
        cnt["pre_uses"] = pre_uses
        out[sub] = cnt
    return {"subsystems": subs, "coverage": out,
            "note": "denominators are heuristic (candidate-rule patterns); "
                    "@annotation-exempt: counts as satisfied-by-exemption; "
                    "// Pre: is raw usage (no denominator derivable)."}


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

# A stub/TODO marker, capturing the optional parenthesized tag right after the
# keyword: `// XASH3DPP-STUB(chunk6-S9) ...` -> tag "chunk6-S9"; a bare
# `// TODO: ...` leaves the tag group empty.
_STUB_RX = re.compile(r"//\s*(TODO|STUB|XASH3DPP-STUB)\b(?:\(([^)]*)\))?(.*)")


def _norm_tag(tag: str | None) -> str:
    """`by_tag` bucket key: fold whitespace + case so the noisy TODO variants
    ('Chunk 7', 'chunk7', 'Chunk 7 ') collapse to one bucket; untagged markers
    bucket together."""
    if not tag or not tag.strip():
        return "(untagged)"
    return re.sub(r"\s+", "", tag).lower()


def _scan_stub_text(text: str, rel: str) -> list[dict]:
    """Stub/TODO markers in one file's raw text, each with its enclosing symbol
    and the parenthesized marker tag.  Pure over text so it runs identically on
    a working-tree file and a `git show` blob (the HEAD~1 delta)."""
    stubs: list[dict] = []
    enclosing = "<file scope>"
    for lineno, rawline in enumerate(text.splitlines(), start=1):
        m = _FUNC_DEF.match(rawline)
        if m and "(" in rawline:
            enclosing = m.group(1)
        tm = _STUB_RX.search(rawline)
        if tm:
            stubs.append({
                "file": rel, "line": lineno, "symbol": enclosing,
                "tag": (tm.group(2) or "").strip(),
                "marker": tm.group(0).strip()[:120],
            })
    return stubs


def _count_by_tag(stubs: list[dict]) -> dict:
    counts: dict[str, int] = {}
    for s in stubs:
        key = _norm_tag(s.get("tag"))
        counts[key] = counts.get(key, 0) + 1
    return dict(sorted(counts.items(), key=lambda kv: (-kv[1], kv[0])))


def stub_scan(subsystem: str, delta: bool = False) -> dict:
    subs = resolve_scope(subsystem)
    stubs: list[dict] = []
    for sub in subs:
        src_dir = SRC / sub
        if not src_dir.is_dir():
            continue
        for path in sorted(src_dir.rglob("*.cpp")):
            text = path.read_text(encoding="utf-8", errors="replace")
            stubs.extend(_scan_stub_text(text, _rel(path)))
    by_tag = _count_by_tag(stubs)
    result = {"subsystems": subs, "todo_count": len(stubs), "stubs": stubs,
              "by_tag": by_tag, "tests": _test_liveness(subs)}
    if delta:
        result["delta_by_tag"] = _stub_delta_by_tag(subs, by_tag)
    return result


def _stub_delta_by_tag(subs: list[str], now_by_tag: dict) -> dict:
    """`by_tag`(worktree) - `by_tag`(HEAD~1): a net-zero refactor (retire one
    tag, add another) shows as +1/-1 instead of a silently-unchanged total.
    Walks current files only, so a wholesale file deletion is not reflected."""
    from . import state  # git plumbing lives in the state layer
    prev: list[dict] = []
    for sub in subs:
        src_dir = SRC / sub
        if not src_dir.is_dir():
            continue
        for path in sorted(src_dir.rglob("*.cpp")):
            rel = _rel(path)
            rc, lines = state._git(["show", "HEAD~1:%s" % rel])
            if rc != 0:
                continue  # absent at HEAD~1 (added since) -> no prior markers
            prev.extend(_scan_stub_text("\n".join(lines), rel))
    prev_by_tag = _count_by_tag(prev)
    tags = set(now_by_tag) | set(prev_by_tag)
    delta = {t: now_by_tag.get(t, 0) - prev_by_tag.get(t, 0) for t in sorted(tags)}
    return {t: d for t, d in delta.items() if d != 0}


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


# --------------------------------------------------------------------------- #
# census — per-subsystem ground-truth numbers (T2, 2026-07 tooling wave)
# --------------------------------------------------------------------------- #

_THREAD_ASSERT_CALL_RX = re.compile(
    r"\bassert_thread_role\s*\(|\bassert_main_thread\s*\(")


def _thread_assert_sites(sub: str) -> dict:
    """Plain assert-CALL census over the subsystem's src TUs (comments
    blanked) — the number boundary docs quote (e.g. server '92 sites / 26
    files'), NOT the mutator-gap heuristic `_scan_thread_assert` uses."""
    sites = 0
    files: dict[str, int] = {}
    for path in subsystem_files(sub):
        if _scope_of(path) != "src":
            continue
        n = 0
        for _, code, _ in code_lines(path):
            n += len(_THREAD_ASSERT_CALL_RX.findall(code))
        if n:
            files[_rel(path)] = n
            sites += n
    return {"sites": sites, "files": files}


def _tally_allows(text: str) -> dict[str, int]:
    """Pure: count `compliance-allow(...)` markers by check id; a marker
    naming several checks (comma list) counts once per named check."""
    out: dict[str, int] = {}
    for m in ALLOW_RE.finditer(text):
        for check in m.group(1).split(","):
            c = check.strip()
            if c:
                out[c] = out.get(c, 0) + 1
    return out


def census(subsystem: str | None = None) -> dict:
    """Per-subsystem ground-truth numbers the boundary/threading docs keep
    quoting (and hand-counting, and getting stale — the 2026-07 audit's D1
    findings were overwhelmingly numbers): src TU count, thread-assert call
    sites/files, compliance-allow tallies by rule, stub markers, test
    liveness.  Informational — always 'clean'.  Doc refreshes paste from
    this; audits diff quoted-vs-actual."""
    subs = resolve_scope(subsystem)
    out: dict[str, dict] = {}
    totals = {"src_tu_count": 0, "assert_sites": 0, "allows": 0,
              "stub_markers": 0, "test_files": 0, "live_tests": 0}
    for sub in subs:
        src_dir = SRC / sub
        srcs = sorted(src_dir.rglob("*.cpp")) if src_dir.is_dir() else []
        ta = _thread_assert_sites(sub)
        allows: dict[str, int] = {}
        for path in subsystem_files(sub):
            text = path.read_text(encoding="utf-8", errors="replace")
            for check, n in _tally_allows(text).items():
                allows[check] = allows.get(check, 0) + n
        st = stub_scan(sub)
        out[sub] = {
            "src_tu_count": len(srcs),
            "assert_thread_role": ta,
            "compliance_allows_by_rule": dict(sorted(allows.items())),
            "stub_markers": {"total": st["todo_count"],
                             "by_tag": st.get("by_tag", {})},
            "tests": st["tests"],
        }
        totals["src_tu_count"] += len(srcs)
        totals["assert_sites"] += ta["sites"]
        totals["allows"] += sum(allows.values())
        totals["stub_markers"] += st["todo_count"]
        totals["test_files"] += st["tests"]["files"]
        totals["live_tests"] += st["tests"]["live"]
    return {"subsystems": out, "totals": totals}


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


def finish_check(subsystem: str, run_tests: bool = False,
                 arch: str = "x64") -> dict:
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

    # 2 limits — QO-classified: wire/ABI-frozen literals (lines referencing a
    # vendored k_* constant, or abi/ paths) are machine-exempt per
    # CONSTANT_PLACEMENT (QO); only unclassified literals need judgment.
    lim = limits_scan(sub)
    group_present = any(sub in l["group"] for l in lim["limits"])
    hits = lim["magic"] + lim["shadow"]
    frozen = [h for h in hits
              if re.search(r"\bk_\w+", h["context"]) or "/abi/" in h["file"]]
    unclassified = [h for h in hits if h not in frozen]
    add(2, "limits.hpp (QO)",
        "pass" if not unclassified else "needs-judgment",
        ["limits group present: %s" % group_present,
         "%d QO-frozen literals (machine-exempt: k_* refs / abi/ paths)"
         % len(frozen),
         "%d unclassified magic/shadow candidates (classify per QO: "
         "limits / cvar / frozen)" % len(unclassified)])

    # 3 stats
    sub_files = subsystem_files(sub)
    joined = "\n".join(p.read_text(encoding="utf-8", errors="replace")
                       for p in sub_files if p.suffix == ".hpp")
    # Accessor spelling varies (`stats()`, `get_stats(handle)`) — any
    # stats-returning call surface counts (6B S1 quirk fix: memory spells it
    # get_stats and was failing the literal "stats()" probe).
    has_stats = bool(re.search(r"struct\s+\w*Stats\b", joined)) and \
        bool(re.search(r"stats\s*\(", joined))
    exempt = any("stats exempt" in p.read_text(encoding="utf-8", errors="replace")
                 for p in sub_files)
    add(3, "STATS_TIERS", "pass" if (has_stats or exempt) else "fail",
        "Stats struct + accessor" if has_stats else
        ("exemption comment present" if exempt else
         "no Stats struct and no 'stats exempt' comment"))

    # 4 nodiscard — honor compliance-allow markers like compliance_scan does
    # (adjudicated lines must not resurface as perpetual needs-judgment).
    nd, nd_allowed = _filter_allows(_scan_nodiscard(sub))
    add(4, "NODISCARD", "pass" if not nd else "needs-judgment",
        ["%d candidate omissions" % len(nd)] +
        (["%d compliance-allowed" % len(nd_allowed)] if nd_allowed else []))

    # 5 naming
    nm = _scan_enum_kprefix(subsystem_files(sub))
    add(5, "NAMING_FN / NAMING_ENUM", "pass" if not nm else "fail",
        "%d k-prefixed enum values" % len(nm))

    # 6 tests exist + macros.  Most subsystems keep tests under tests/<sub>/,
    # but the abi layout/ABI-pin tests live under tests/server/abi/ (they were
    # authored alongside the server and exercise the vendored struct layouts at
    # both pointer widths) — count that location too so abi isn't a false fail.
    _TEST_DIR_ALIASES = {"abi": [TESTS / "server" / "abi"]}
    tdirs = [TESTS / sub, *_TEST_DIR_ALIASES.get(sub, [])]
    tfiles = [p for d in tdirs if d.is_dir() for p in d.rglob("*.cpp")]
    macro_issues = _scan_test_macros(sub)
    add(6, "Tests present + test_helpers.hpp",
        "pass" if tfiles and not macro_issues else "fail",
        ["%d test files" % len(tfiles),
         "%d macro-convention issues" % len(macro_issues)])

    # 7 architecture docs + threading
    arch_dir = DOCS / "architecture" / sub
    arch_ok = (arch_dir / "README.md").is_file() or any(arch_dir.glob("*.md")) \
        if arch_dir.is_dir() else False
    thr = (DOCS / "threading-analysis" / ("%s-threading.md" % sub)).is_file()
    if not thr and arch_dir.is_dir():
        thr = any("hreading" in p.read_text(encoding="utf-8", errors="replace")
                  for p in arch_dir.glob("*.md"))
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
            result = run_test(filter_regex=rx, arch=arch)
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

    # 10 lifecycle & annotation discipline (Q-22/QN)
    q22 = compliance_scan(sub, checks="class-operator-new,"
                                      "operator-delete-pairing,"
                                      "make-unique-outside-pimpl,thread-assert")
    hard10 = [v for v in q22["violations"]
              if not v["severity"].startswith("candidate-")]
    cand10 = len(q22["violations"]) - len(hard10)
    cov = annotation_coverage(sub)["coverage"][sub]
    low = ["%s %.0f%%" % (m, cov[m]["coverage_pct"])
           for m in ("lifetime", "thread_safety", "pre_reserved", "safety",
                     "thread_assert")
           if cov[m]["coverage_pct"] < 100.0]
    add(10, "Lifecycle & annotation discipline (Q-22/QN)",
        "fail" if hard10 else ("needs-judgment" if (cand10 or low) else "pass"),
        ["%d hard violations, %d candidates" % (len(hard10), cand10),
         "coverage below 100%%: %s" % (", ".join(low) if low else "none")])

    passed = sum(1 for i in items if i["status"] == "pass")
    return {"subsystem": sub, "items": items,
            "summary": "%d/10 pass, %d need judgment, %d fail" % (
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
    """Dependency edges from InitParams fields and cross-namespace refs,
    plus (D-1, 2026-07-06) the CMake target link graph and the
    cross-subsystem include-direction census.  Include direction is the
    layer-model acceptance criterion — CMake tolerates static-lib link
    cycles, so link edges alone under-report (see
    docs/design/layer-model.md; note the diagnostics headers are core-pathed
    but hosted in platform, so a platform -> core include edge is expected
    and documented)."""
    edges: set[tuple[str, str]] = set()
    subs = resolve_scope(None)
    ns_rx = re.compile(r"(?:::)?xash::(%s)::" % "|".join(subs))
    inc_rx = re.compile(r"\s*#\s*include\s*<xash3dpp/(?:private/)?(\w+)/")
    inc_edges: set[tuple[str, str]] = set()
    for sub in subs:
        for path in subsystem_files(sub):
            for _, code, _ in code_lines(path):
                for m in ns_rx.finditer(code):
                    target = m.group(1)
                    if target != sub:
                        edges.add((sub, target))
                im = inc_rx.match(code)
                if im and im.group(1) in subs and im.group(1) != sub:
                    inc_edges.add((sub, im.group(1)))
    inits = []
    for path in sorted(INCLUDE.rglob("*.hpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r"struct\s+(\w+InitParams)\b", text):
            inits.append({"struct": m.group(1), "file": _rel(path)})
    cycles = sorted(
        "%s <-> %s" % (a, b) for a, b in edges if (b, a) in edges and a < b
    )
    # Target-level link edges from src/*/CMakeLists.txt (comments stripped).
    link_edges: set[tuple[str, str]] = set()
    for cml in sorted(SRC.glob("*/CMakeLists.txt")):
        text = re.sub(r"#[^\n]*", "",
                      cml.read_text(encoding="utf-8", errors="replace"))
        for m in re.finditer(
                r"target_link_libraries\s*\(\s*(xash3dpp_\w+)([^)]*)\)", text):
            src_target = m.group(1)
            for dep in re.findall(r"xash3dpp_\w+", m.group(2)):
                if dep != src_target:
                    link_edges.add((src_target, dep))
    link_cycles = sorted(
        "%s <-> %s" % (a, b) for a, b in link_edges
        if (b, a) in link_edges and a < b
    )
    include_cycles = sorted(
        "%s <-> %s" % (a, b) for a, b in inc_edges
        if (b, a) in inc_edges and a < b
    )
    return {
        "edges": sorted(["%s -> %s" % e for e in edges]),
        "init_params": inits,
        "cycles": cycles,
        "link_edges": sorted(["%s -> %s" % e for e in link_edges]),
        "link_cycles": link_cycles,
        "include_edges": sorted(["%s -> %s" % e for e in inc_edges]),
        "include_cycles": include_cycles,
    }
