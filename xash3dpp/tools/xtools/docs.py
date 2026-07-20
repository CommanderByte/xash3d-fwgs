"""Documentation-drift scanner.

The 2026-07-20 modernization audit found ~17 places where a doc asserted
something about the code that was not true -- three false rows in
server-boundary.md's own Extension-axes table, six "assert_main_thread is a
thin wrapper" claims describing a refactor that never happened, a compliance
rule Q-20 promises and that does not exist, and an HB-2 fence anchor pointing
at the wrong lines.  None of it was catchable, because prose assertions are
not checkable and so nothing ever failed.

The fix is not "keep the docs current" -- that is a per-commit tax nobody
pays.  It is to make BINDING claims checkable and let everything else be
explicitly advisory.  Two families:

  anchors  Every backticked `file.cpp:123` citation must still resolve, and
           must still point at the text it pointed at when it was blessed.
           The blessed text lives in a sidecar lockfile rather than in the
           971 prose sites, and pure line drift (code moved, text unchanged)
           is REPAIRED automatically -- that is what keeps the cost at zero.

  claims   A small predicate vocabulary in HTML comments, for the assertions
           a lockfile cannot express ("this compliance rule exists", "this
           symbol still has no caller").  Applied only to binding surfaces,
           so there are dozens of these, not thousands.  Every predicate is
           exact by construction -- see evaluate() for why `callers()` and
           `impls()` were drafted and then deliberately dropped.

Exit convention is the repo's: 0 clean, 1 findings, 2 execution error.
"""

from __future__ import annotations

import json
import re
from pathlib import Path

from . import DOCS, INCLUDE, PRIVATE, REPO, SRC, TESTS, subsystems
from .scan import code_lines

LOCKFILE = DOCS / ".doc-anchors.json"
LOCK_VERSION = 1

# A backticked code citation: `file.cpp:123`, `file.cpp:12-34`,
# `xash3dpp/src/host/host.cpp:90-91`.  Backticks matter -- they are the
# convention across the docs and they keep prose out of the match set.
ANCHOR_RX = re.compile(
    r"`([A-Za-z0-9_./\\-]+\.(?:cpp|hpp|h|c|py|txt|cmake|lst))"
    r"\s*:\s*(\d+)(?:\s*-\s*(\d+))?`")

# `<!-- verify: <predicate> -->`  (the docs' HTML-comment channel is
# otherwise unused apart from pymarkdown pragmas, so there is no collision).
CLAIM_RX = re.compile(r"<!--\s*verify:\s*(.+?)\s*-->")

# `<!-- verify-skip: <reason> -->` on the line above an anchor exempts it.
SKIP_RX = re.compile(r"<!--\s*verify-skip:\s*(.+?)\s*-->")

# Greedy `.*` so a regex ARGUMENT may itself contain parentheses -- the
# closing paren is the last one before an optional comparison.
_PRED_RX = re.compile(
    r"^(?P<fn>[a-z-]+)\s*\((?P<args>.*)\)\s*"
    r"(?:(?P<op>==|>=|<=|>|<)\s*(?P<want>\d+))?$")


# ---------------------------------------------------------------------------
# Pure parsers (unit-tested against inline snippets -- no filesystem)
# ---------------------------------------------------------------------------

def parse_anchors(text: str) -> list[dict]:
    """Every code citation in a doc body, with its 1-based doc line and the
    occurrence index of that exact citation within the doc (the lockfile key
    must survive unrelated edits above it, so it cannot be the line number)."""
    out: list[dict] = []
    # Occurrence is counted per CITED PATH, never per raw text: --repair
    # rewrites the line number inside the raw text, so a raw-text key would
    # change identity on every repair -- orphaning the old entry, silently
    # auto-blessing the new one, and resetting drift detection to zero.
    seen: dict[str, int] = {}
    lines = text.splitlines()
    for i, line in enumerate(lines, start=1):
        prev = lines[i - 2] if i >= 2 else ""
        skipped = bool(SKIP_RX.search(line) or SKIP_RX.search(prev))
        for m in ANCHOR_RX.finditer(line):
            raw = m.group(0)
            cited = m.group(1).replace("\\", "/")
            occ = seen.get(cited, 0)
            seen[cited] = occ + 1
            out.append({
                "raw": raw,
                "path": m.group(1).replace("\\", "/"),
                "line": int(m.group(2)),
                "end": int(m.group(3)) if m.group(3) else int(m.group(2)),
                "doc_line": i,
                "occurrence": occ,
                "skipped": skipped,
            })
    return out


def parse_claims(text: str) -> list[dict]:
    """Every `<!-- verify: ... -->` predicate with its doc line."""
    out = []
    for i, line in enumerate(text.splitlines(), start=1):
        for m in CLAIM_RX.finditer(line):
            out.append({"doc_line": i, "predicate": m.group(1).strip()})
    return out


def parse_predicate(pred: str) -> dict | None:
    """`callers(foo) >= 1` -> {fn, args, op, want}.  None if unparseable."""
    m = _PRED_RX.match(pred.strip())
    if not m:
        return None
    return {
        "fn": m.group("fn"),
        # Kept RAW: a regex argument may contain commas ("\w{1,3}"), so
        # splitting is the evaluator's job -- it knows each function's arity
        # and splits on the LAST comma for the two-argument forms.
        "argstr": m.group("args").strip(),
        "op": m.group("op") or ">=",
        "want": int(m.group("want")) if m.group("want") else 1,
    }


def compare(actual: int, op: str, want: int) -> bool:
    return {
        "==": actual == want, ">=": actual >= want, "<=": actual <= want,
        ">": actual > want, "<": actual < want,
    }[op]


def anchor_key(doc_rel: str, a: dict) -> str:
    """Stable identity for one citation: the doc, the cited PATH and which
    occurrence of that path it is.  Deliberately excludes the line number so
    that --repair does not change an anchor's identity."""
    return "%s|%s|%d" % (doc_rel, a["path"], a["occurrence"])


# ---------------------------------------------------------------------------
# Resolution
# ---------------------------------------------------------------------------

def _doc_subsystem(doc_rel: str) -> str | None:
    """The subsystem a doc is about, from its path/name, for scoped lookup."""
    known = set(subsystems())
    m = re.search(r"docs/boundaries/([\w]+)-boundary\.md$", doc_rel)
    if m and m.group(1) in known:
        return m.group(1)
    m = re.search(r"docs/modernization-opportunities/([\w]+)-modernization\.md$",
                  doc_rel)
    if m and m.group(1) in known:
        return m.group(1)
    m = re.search(r"docs/architecture/([\w]+)/", doc_rel)
    if m and m.group(1) in known:
        return m.group(1)
    m = re.search(r"docs/threading-analysis/([\w]+)-threading\.md$", doc_rel)
    if m and m.group(1) in known:
        return m.group(1)
    return None


def _legacy_roots() -> list[Path]:
    """The reference-only legacy engine tree (everything but xash3dpp/)."""
    out = []
    for d in ("engine", "common", "pm_shared", "public", "filesystem",
              "ref", "game_launch", "utils"):
        p = REPO / d
        if p.is_dir():
            out.append(p)
    return out


def resolve(path: str, doc_rel: str, index: dict) -> tuple[Path | None, str]:
    """Resolve a citation to a file.  Returns (path|None, status) where
    status is 'ok', 'ambiguous' or 'missing'."""
    # Full-ish path: try it verbatim under the repo root first.
    if "/" in path:
        for cand in (REPO / path, REPO / "xash3dpp" / path):
            if cand.is_file():
                return cand, "ok"

    base = path.rsplit("/", 1)[-1]
    hits = index.get(base, [])
    if not hits:
        return None, "missing"
    if len(hits) == 1:
        return hits[0], "ok"

    # A partial path (`win32/crash.cpp`, `private/sound/codec.hpp`) is not a
    # bare basename: keep only candidates whose path ends with it.
    if "/" in path:
        suffix = "/" + path.lstrip("./")
        narrowed = [h for h in hits if h.as_posix().endswith(suffix)]
        if len(narrowed) == 1:
            return narrowed[0], "ok"
        if narrowed:
            hits = narrowed

    # Several basenames match -- prefer the doc's own subsystem.
    sub = _doc_subsystem(doc_rel)
    if sub:
        scoped = [h for h in hits
                  if ("/%s/" % sub) in h.as_posix()]
        if len(scoped) == 1:
            return scoped[0], "ok"
    # legacy-survey docs cite the reference-only tree.
    if "legacy-survey" in doc_rel or "deep-dive" in doc_rel:
        legacy = [h for h in hits if "/xash3dpp/" not in h.as_posix()]
        if len(legacy) == 1:
            return legacy[0], "ok"
    return None, "ambiguous"


def build_index() -> dict[str, list[Path]]:
    """basename -> every matching source file, across xash3dpp AND the
    reference-only legacy tree (legacy-survey docs cite the latter)."""
    index: dict[str, list[Path]] = {}
    # tools/ is in scope too: the modernization reports cite the scanners.
    roots = [SRC, INCLUDE, TESTS, REPO / "xash3dpp" / "tools"] + _legacy_roots()
    exts = {".cpp", ".hpp", ".h", ".c", ".py", ".txt", ".cmake", ".lst"}
    for root in roots:
        if not root.is_dir():
            continue
        for p in root.rglob("*"):
            if p.suffix in exts and p.is_file():
                index.setdefault(p.name, []).append(p)
    return index


def snapshot_of(path: Path, start: int, end: int) -> list[str] | None:
    """The stripped text of the cited line range, or None if out of range."""
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return None
    if start < 1 or start > len(lines):
        return None
    end = min(max(end, start), len(lines))
    # Cap: a huge cited range would bloat the lockfile and is not a useful
    # fingerprint.  The first few lines identify the site well enough.
    return [ln.strip() for ln in lines[start - 1:min(end, start + 2)]]


def find_snapshot(path: Path, snap: list[str]) -> int | None:
    """The 1-based line where `snap` now begins, if it moved within the file."""
    if not snap:
        return None
    try:
        lines = [ln.strip() for ln in
                 path.read_text(encoding="utf-8", errors="replace").splitlines()]
    except OSError:
        return None
    n = len(snap)
    for i in range(len(lines) - n + 1):
        if lines[i:i + n] == snap:
            return i + 1
    return None


# ---------------------------------------------------------------------------
# Predicate evaluation
# ---------------------------------------------------------------------------

def _prod_files() -> list[Path]:
    out = []
    for root in (SRC, INCLUDE):
        if root.is_dir():
            out.extend(p for p in root.rglob("*")
                       if p.suffix in {".cpp", ".hpp"})
    return out


def _glob_files(glob: str) -> list[Path]:
    """Repo-relative glob -> files.  `**` spans directories."""
    import fnmatch
    pat = glob.lstrip("./")
    roots = [SRC, INCLUDE, TESTS, REPO / "xash3dpp" / "tools"]
    out = []
    for root in roots:
        if not root.is_dir():
            continue
        for p in root.rglob("*"):
            if not p.is_file():
                continue
            rel = p.relative_to(REPO).as_posix()
            if fnmatch.fnmatch(rel, pat):
                out.append(p)
    return sorted(set(out))


def _count_code(pattern: re.Pattern, files: list[Path]) -> int:
    n = 0
    for p in files:
        for _lineno, code, _raw in code_lines(p):
            n += len(pattern.findall(code))
    return n


def evaluate(pred: dict) -> tuple[int, str | None]:
    """Evaluate a parsed predicate -> (actual, error).  `actual` is the
    measured count; `error` is set for an unknown function or bad args.

    The vocabulary is deliberately small, and deliberately excludes
    `callers()` and `impls()`.  Both were drafted and then dropped: counting
    call sites or interface implementations correctly needs a C++ parser, and
    the regex versions got two of this audit's own headline facts backwards
    (they scored get_compat_policy as having 2 callers when it has none, and
    ITrustOracle as having an implementation when every one is test-only).  A
    predicate that reports a false PASS is worse than no predicate, so what
    survives is only what can be computed exactly.  Anything else is spelled
    out with grep-count, where the pattern is visible in the annotation and
    the reader can audit it.
    """
    fn = pred["fn"]
    argstr = pred["argstr"]

    def split2() -> list[str]:
        """Two args, split on the LAST comma (arg 2 -- a glob or a census key
        -- never contains one, arg 1 may)."""
        if "," not in argstr:
            return [argstr]
        a, b = argstr.rsplit(",", 1)
        return [a.strip(), b.strip()]

    args = split2() if argstr else []

    if fn == "grep-count":
        if len(args) != 2:
            return 0, "grep-count takes (regex, path-glob)"
        pattern, glob = args
        try:
            rx = re.compile(pattern)
        except re.error as exc:
            return 0, "bad regex %r: %s" % (pattern, exc)
        files = _glob_files(glob)
        if not files:
            return 0, "glob %r matched no files" % glob
        return _count_code(rx, files), None

    if fn == "symbol-exists":
        if len(args) != 1:
            return 0, "symbol-exists takes one symbol"
        sym = args[0].rsplit("::", 1)[-1]
        # A DEFINITION or declaration line: a return type (possibly ending in
        # * or &) then the name then an open paren, at statement start.
        rx = re.compile(r"^\s*(?:[\w:<>,\[\]]+[\s*&]+)+(?:\w+::)*%s\s*\("
                        % re.escape(sym))
        return _count_code(rx, _prod_files()), None

    if fn == "compliance-rule-exists":
        if len(args) != 1:
            return 0, "compliance-rule-exists takes one rule id"
        from .rules import RULES, STRUCTURED_CHECKS
        ids = {getattr(r, "check", None) for r in RULES}
        ids |= set(STRUCTURED_CHECKS)
        return (1 if args[0] in ids else 0), None

    if fn == "census":
        if len(args) != 2:
            return 0, "census takes (subsystem, key)"
        from .checks import census
        sub, key = args
        data = census(sub if sub != "all" else None)
        if sub == "all":
            val = data["totals"].get(key)
        else:
            entry = data["subsystems"].get(sub)
            if entry is None:
                return 0, "unknown subsystem %r" % sub
            val = entry.get(key)
            if isinstance(val, dict):
                val = val.get("sites", val.get("total"))
        if val is None:
            return 0, "unknown census key %r" % key
        return int(val), None

    return 0, "unknown predicate %r" % fn


# ---------------------------------------------------------------------------
# Scan
# ---------------------------------------------------------------------------

def _docs() -> list[Path]:
    return sorted(p for p in DOCS.rglob("*.md") if p.is_file())


def _rel(p: Path) -> str:
    return p.relative_to(REPO).as_posix()


def load_lock() -> dict:
    if not LOCKFILE.is_file():
        return {"version": LOCK_VERSION, "anchors": {}}
    try:
        return json.loads(LOCKFILE.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {"version": LOCK_VERSION, "anchors": {}}


def save_lock(lock: dict) -> None:
    LOCKFILE.write_text(json.dumps(lock, indent=1, sort_keys=True) + "\n",
                        encoding="utf-8")


def scan(*, checks: str = "all", bless: bool = False,
         repair: bool = False) -> dict:
    """Run the drift checks.  `bless` (re)records every resolvable anchor;
    `repair` rewrites doc line numbers whose blessed text merely moved."""
    do_anchors = checks in ("all", "anchors")
    do_claims = checks in ("all", "claims")

    index = build_index() if do_anchors else {}
    lock = load_lock()
    anchors = lock.setdefault("anchors", {})

    findings: list[dict] = []
    notes: list[dict] = []
    repaired: list[dict] = []
    counts = {"anchors": 0, "blessed": 0, "claims": 0, "skipped": 0}

    for doc in _docs():
        rel = _rel(doc)
        # newline="" keeps the file's own CRLF/LF: read_text() would
        # universal-newline them to LF, and writing that back during --repair
        # silently reformats the whole doc (git normalises the diff away, so
        # it shows up only as a phantom "modified" file).
        with open(doc, encoding="utf-8", errors="replace", newline="") as fh:
            text = fh.read()

        if do_anchors:
            edits: list[tuple[int, str, str]] = []   # doc_line, old, new
            for a in parse_anchors(text):
                counts["anchors"] += 1
                if a["skipped"]:
                    counts["skipped"] += 1
                    continue
                path, status = resolve(a["path"], rel, index)
                if status == "ambiguous":
                    notes.append({"doc": rel, "line": a["doc_line"],
                                  "kind": "ambiguous-basename",
                                  "detail": "%s matches several files"
                                            % a["path"]})
                    continue
                if path is None:
                    findings.append({
                        "doc": rel, "line": a["doc_line"],
                        "kind": "unresolved-anchor",
                        "detail": "%s does not resolve to a file" % a["raw"]})
                    continue

                snap = snapshot_of(path, a["line"], a["end"])
                if snap is None:
                    findings.append({
                        "doc": rel, "line": a["doc_line"],
                        "kind": "line-out-of-range",
                        "detail": "%s: %s has fewer lines"
                                  % (a["raw"], _rel(path))})
                    continue

                key = anchor_key(rel, a)
                if bless or key not in anchors:
                    anchors[key] = {"target": _rel(path), "line": a["line"],
                                    "snapshot": snap}
                    counts["blessed"] += 1
                    continue

                want = anchors[key]
                if want.get("snapshot") == snap:
                    continue

                moved = find_snapshot(path, want.get("snapshot") or [])
                if moved is not None:
                    if repair:
                        new_raw = a["raw"].replace(
                            ":%d" % a["line"], ":%d" % moved, 1)
                        edits.append((a["doc_line"], a["raw"], new_raw))
                        anchors[key]["line"] = moved
                        repaired.append({"doc": rel, "line": a["doc_line"],
                                         "from": a["line"], "to": moved})
                    else:
                        findings.append({
                            "doc": rel, "line": a["doc_line"],
                            "kind": "anchor-moved",
                            "detail": "%s: cited text is now at %s:%d "
                                      "(run --repair)"
                                      % (a["raw"], _rel(path), moved)})
                else:
                    findings.append({
                        "doc": rel, "line": a["doc_line"],
                        "kind": "anchor-drift",
                        "detail": "%s no longer points at the text it was "
                                  "blessed with, and that text is gone from "
                                  "%s" % (a["raw"], _rel(path))})

            if edits:
                lines = text.splitlines(keepends=True)
                for doc_line, old, new in edits:
                    i = doc_line - 1
                    lines[i] = lines[i].replace(old, new, 1)
                # newline="" so the file keeps its existing line endings:
                # a repair rewrites ONE number per edited line, and must not
                # flip the whole doc LF<->CRLF underneath the diff.
                with open(doc, "w", encoding="utf-8", newline="") as fh:
                    fh.write("".join(lines))

        if do_claims:
            for c in parse_claims(text):
                counts["claims"] += 1
                pred = parse_predicate(c["predicate"])
                if pred is None:
                    findings.append({
                        "doc": rel, "line": c["doc_line"],
                        "kind": "bad-predicate",
                        "detail": "cannot parse %r" % c["predicate"]})
                    continue
                actual, err = evaluate(pred)
                if err:
                    findings.append({
                        "doc": rel, "line": c["doc_line"],
                        "kind": "bad-predicate", "detail": err})
                    continue
                if not compare(actual, pred["op"], pred["want"]):
                    findings.append({
                        "doc": rel, "line": c["doc_line"],
                        "kind": "false-claim",
                        "detail": "%s -- actual %d" % (c["predicate"], actual)})

    # Persist whenever anything was recorded, not only under --bless: an
    # anchor seen for the first time is auto-recorded (that is what makes
    # adoption free), and forgetting to save meant every run re-blessed from
    # scratch and no drift could ever be detected.  Auto-recording is safe
    # because an anchor is only recorded after it RESOLVES and its range is
    # in bounds -- a broken new citation is still a finding.
    if do_anchors and (bless or repaired or counts["blessed"]):
        lock["version"] = LOCK_VERSION
        save_lock(lock)

    return {
        "checks": checks,
        "docs": len(_docs()),
        "counts": counts,
        "findings": findings,
        "notes": notes,
        "repaired": repaired,
        "lockfile": _rel(LOCKFILE),
    }
