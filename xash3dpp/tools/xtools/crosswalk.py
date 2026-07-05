"""Legacy C engine <-> xash3dpp C++ symbol crosswalk.

Indexes the inline "port annotations" the rewrite carries — mapping a legacy
engine symbol (SV_Multicast) or a legacy file:line (sv_game.c:4026) to the C++
that ports it — plus the deep-dive recon docs as a second, legacy-side source.

Coverage is uneven by design of the codebase (function-level in server/, only
file-level in networking/map_loader), so every hit reports its `source` and
`confidence` rather than pretending uniform coverage.  Annotation grammars
(verified across src/): a symbol + `(file.c:line)` directly above the porting
function (Style A, the gold source); a `// Legacy reference:` file header
(Style B, file-level); `// Legacy <Symbol>` with no line (Style D, networking);
and a bare `// SV_Foo` symbol opening a doc / section-divider comment directly
above the porting definition (Style E — the form that resolves class-method
ports like SV_LinkEdict -> WorldLinks::link_edict, which A/B/D miss).

A Style-A/D/E comment is treated as a *port* when the next substantive line is a
function signature (definition or declaration), and as a mid-body *citation*
(confidence "ref") otherwise — this is namespace-agnostic, unlike brace depth.
"""

from __future__ import annotations

import re
from pathlib import Path

from . import DOCS, REPO, subsystems
from .checks import _FUNC_DEF
from .scan import code_lines, subsystem_files


# --- annotation grammars ---------------------------------------------------- #

# Style A (gold): legacy symbol + (file.c:line[-range]) in a // comment.
_STYLE_A = re.compile(
    r"//.*?\b([A-Za-z_]\w+)\s*\(\s*([\w./]+\.(?:c|cpp|h))\s*:\s*(\d+)"
    r"(?:\s*-\s*(\d+))?\s*\)")
# Style B: file-header "Legacy reference:" banner (file-level).
_STYLE_B = re.compile(r"//\s*Legacy(?:\s+reference)?:\s*(.+)")
# Style D (networking): "// Legacy <Symbol>" — symbol, no file/line. The
# lookahead keeps "Legacy reference:" (Style B) out; captured symbols are then
# filtered through _looks_legacy so prose words ("Legacy behavior") drop.
_STYLE_D = re.compile(r"//\s*Legacy\s+(?!reference\b)([A-Za-z_]\w+)")
# Style E: a bare legacy symbol as the FIRST token of a comment-first line,
# with no file:line and no "Legacy" keyword — the "// SV_LinkEdict" doc /
# section-divider form that sits directly above the porting definition (often a
# class method, which Style A/B/D never resolve).  It only fires when _next_def
# finds a real signature below, so prose that merely opens with a symbol
# ("// SV_Foo returns false here") is rejected as a citation, not a port.
_STYLE_E = re.compile(r"^\s*//+\s*([A-Za-z_]\w*)")
# Deep-dive prose: Symbol (file.c:line[-range]) anywhere in a recon doc.
_DEEP_DIVE = re.compile(
    r"\b([A-Za-z_]\w+)\s*\(\s*([\w./]+\.c)\s*:\s*(\d+)(?:\s*-\s*(\d+))?\s*\)")
# A legacy engine file path inside a header/prose blob.
_LEGACY_FILE = re.compile(r"([\w./]+\.(?:c|cpp|h))\b")

# A C++ function-signature opener: a return type (>=1 leading token) then the
# name then '('.  The >=1-token requirement excludes bare calls `foo(`; the
# statement-keyword guard in _next_def excludes `return foo(` etc.
_SIG_OPEN = re.compile(
    r"^\s*(?:[\w:\[\]<>,*&~]+\s+)+([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*\(")
_STMT_KW = {"return", "if", "for", "while", "switch", "else", "do", "case",
            "delete", "throw", "co_return", "co_await", "sizeof", "static_cast"}

# Legacy engine symbols look like these; used to filter the constant/macro
# tokens (MAX_CLIENTS, FATPHS_RADIUS) a bare word regex sweeps up in the fuzzy
# (header / deep-dive prose) sources.  Style A is structured enough to skip it.
_LEGACY_PREFIXES = (
    "SV_", "Mod_", "Netchan_", "Delta_", "MSG_", "PM_", "pfn", "Host_",
    "COM_", "CL_", "NET_", "Cvar_", "Cmd_", "Con_", "Sys_", "Info_", "Cbuf_",
    "SCR_", "Mem_", "CRC", "GL_", "R_", "S_", "V_", "Q_", "World",
)

_CONF_ORDER = {"high": 0, "symbol": 1, "file": 2, "ref": 3, "doc": 4}


def _rel(p: Path) -> str:
    try:
        return p.relative_to(REPO).as_posix()
    except ValueError:
        return p.as_posix()


def _looks_legacy(sym: str) -> bool:
    """Filter for the fuzzy sources: drop ALL_CAPS constant/macro tokens, keep
    prefixed engine symbols."""
    if sym.isupper():
        return False
    return sym.startswith(_LEGACY_PREFIXES)


def _entry(legacy_symbol, legacy_file, legacy_line, cpp_file, cpp_symbol,
           cpp_line, source, confidence, legacy_line_end=None) -> dict:
    return {
        "legacy_symbol": legacy_symbol,
        "legacy_file": legacy_file,
        "legacy_line": legacy_line,
        "legacy_line_end": int(legacy_line_end) if legacy_line_end else None,
        "cpp_file": cpp_file,
        "cpp_symbol": cpp_symbol,
        "cpp_line": cpp_line,
        "source": source,
        "confidence": confidence,
        "ported": source in ("code-styleA", "code-networking", "code-symbol")
        and cpp_symbol not in (None, "<file scope>"),
    }


def _next_def(rows, i, window: int = 12):
    """Classify what follows the annotation at row i.  Returns (cpp_symbol,
    cpp_line, confidence): the first substantive line after the comment is a
    clean definition -> "high"; a declaration / multi-line signature opener ->
    "symbol"; anything else (a statement — i.e. a mid-body citation) -> "ref"."""
    for j in range(i + 1, min(i + 1 + window, len(rows))):
        _, code, _ = rows[j]
        s = code.strip()
        if not s or s.startswith("#"):
            continue  # blank, comment-only (banner), or preprocessor line
        first = s.split(None, 1)[0].rstrip("(")
        if first in _STMT_KW:
            return None, None, "ref"
        md = _FUNC_DEF.match(code)
        if md and "(" in code:
            return md.group(1), rows[j][0], "high"
        ms = _SIG_OPEN.match(code)
        if ms:
            return ms.group(1), rows[j][0], "symbol"
        return None, None, "ref"  # first real line isn't a signature -> citation
    return None, None, "ref"


def _index_code_file(path: Path) -> list[dict]:
    return _index_rows(list(code_lines(path)), _rel(path))  # (lineno, code, raw)


def _index_rows(rows: list, rel: str) -> list[dict]:
    """Extract every port annotation from a file's rows (pure; the disk read is
    _index_code_file's job).  Style A wins and short-circuits; then D; then the
    bare-symbol Style E, which only indexes when a real signature follows."""
    entries: list[dict] = []
    for i, (lineno, _code, raw) in enumerate(rows):
        if lineno <= 8:  # Style B file-header banner
            mb = _STYLE_B.search(raw)
            if mb:
                entries.extend(_parse_header(mb.group(1), rel, lineno))
        ma = _STYLE_A.search(raw)
        if ma:
            sym, lf, l1, l2 = ma.group(1), ma.group(2), ma.group(3), ma.group(4)
            cpp_sym, cpp_line, conf = _next_def(rows, i)
            entries.append(_entry(sym, lf, int(l1), rel,
                                  cpp_sym or "<file scope>", cpp_line or lineno,
                                  "code-styleA", conf, l2))
            continue
        md = _STYLE_D.search(raw)
        if md and _looks_legacy(md.group(1)):
            cpp_sym, cpp_line, _conf = _next_def(rows, i)
            # attaches to a signature -> symbol-level; otherwise still a
            # file-level pointer at the TU that ports it.
            entries.append(_entry(md.group(1), None, None, rel,
                                  cpp_sym or "<file scope>", cpp_line or lineno,
                                  "code-networking",
                                  "symbol" if cpp_sym else "file"))
            continue
        me = _STYLE_E.search(raw)
        if me and _looks_legacy(me.group(1)):
            # Only a real definition below turns a leading-symbol comment into a
            # port; a statement/citation leaves it out of the index.
            cpp_sym, cpp_line, conf = _next_def(rows, i)
            if cpp_sym and conf in ("high", "symbol"):
                entries.append(_entry(me.group(1), None, None, rel, cpp_sym,
                                      cpp_line, "code-symbol", "symbol"))
    return entries


def _parse_header(text: str, rel: str, lineno: int) -> list[dict]:
    """A Style-B `Legacy reference:` header: file-level mapping(s) plus any
    prefixed symbols listed in it."""
    files = _LEGACY_FILE.findall(text)
    entries = [_entry(None, lf, None, rel, "<file scope>", lineno,
                      "code-header", "file") for lf in files]
    primary = files[0] if files else None
    for sym in re.findall(r"\b([A-Za-z_]\w+)\b", text):
        if _looks_legacy(sym):
            entries.append(_entry(sym, primary, None, rel, "<file scope>",
                                  lineno, "code-header", "file"))
    return entries


def _index_deep_dive(path: Path) -> list[dict]:
    rel = _rel(path)
    entries: list[dict] = []
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return entries
    for lineno, line in enumerate(text.splitlines(), start=1):
        for sym, lf, l1, l2 in _DEEP_DIVE.findall(line):
            if _looks_legacy(sym):
                entries.append(_entry(sym, lf, int(l1), rel, None, lineno,
                                      "deep-dive", "doc", l2))
    return entries


def build_index() -> list[dict]:
    entries: list[dict] = []
    for sub in subsystems():
        for path in subsystem_files(sub, src=True, include=True):
            if path.suffix in (".cpp", ".hpp", ".h"):
                entries.extend(_index_code_file(path))
    for doc in sorted((DOCS / "legacy-survey").glob("deep-dive-*.md")):
        entries.extend(_index_deep_dive(doc))
    for doc in sorted((DOCS / "boundaries").glob("*.md")):
        entries.extend(_index_deep_dive(doc))
    return entries


def _rank(matches: list[dict]) -> list[dict]:
    return sorted(matches, key=lambda e: (
        _CONF_ORDER.get(e["confidence"], 9), not e["ported"],
        e["cpp_file"] or ""))


def _match_symbol(entries: list[dict], sym: str) -> list[dict]:
    low = sym.lower()
    exact = [e for e in entries
             if e["legacy_symbol"] and e["legacy_symbol"].lower() == low]
    if exact:
        return exact
    return [e for e in entries
            if e["legacy_symbol"] and low in e["legacy_symbol"].lower()]


def _match_fileline(entries: list[dict], legacy_file: str,
                    line: int) -> list[dict]:
    """Precise (line-covering) hits win; file-level header hits are returned
    only as a fallback when nothing pins the exact line."""
    base = legacy_file.rsplit("/", 1)[-1].lower()
    precise, filelevel = [], []
    for e in entries:
        lf = e["legacy_file"]
        if not lf or lf.rsplit("/", 1)[-1].lower() != base:
            continue
        l1 = e["legacy_line"]
        if l1 is None:
            filelevel.append(e)
        elif l1 <= line <= (e["legacy_line_end"] or l1):
            precise.append(e)
    return precise if precise else filelevel


def _missing(entries: list[dict]) -> dict:
    """Legacy symbols documented in the recon deep-dives with no mention in any
    code annotation — the "still to port" set (serves the pmove/parity phases).
    Heuristic: a deep-dive symbol whose name appears in no code-* entry.  It can
    over-report a symbol ported without any port comment, so treat it as a
    recon hint, not a gate."""
    referenced = {e["legacy_symbol"].lower() for e in entries
                  if e["legacy_symbol"] and e["source"].startswith("code-")}
    seen: dict[str, dict] = {}
    for e in entries:
        s = e["legacy_symbol"]
        if not s or e["source"] != "deep-dive" or s.lower() in referenced:
            continue
        seen.setdefault(s, e)
    out = sorted(seen.values(), key=lambda e: e["legacy_symbol"])
    return {"missing": out, "count": len(out)}


def crosswalk(query: str = "", kind: str = "auto",
              missing: bool = False) -> dict:
    """Resolve a legacy symbol or file:line to its xash3dpp port(s).

    query: a legacy symbol (SV_Multicast) or file:line (sv_game.c:4026).
    kind:  auto | symbol | fileline.
    missing=True ignores `query` and lists deep-dive-documented symbols with no
    code-level mention yet.
    """
    entries = build_index()
    if missing:
        return _missing(entries)
    q = (query or "").strip()
    if not q:
        return {"query": q, "count": 0, "matches": [],
                "note": "empty query (pass a legacy symbol or file.c:line)"}
    fl = re.match(r"(.+\.(?:c|cpp|h)):(\d+)$", q)
    if kind == "fileline" or (kind == "auto" and fl):
        if not fl:
            return {"query": q, "count": 0, "matches": [],
                    "note": "kind=fileline needs a file.c:line query"}
        matches = _match_fileline(entries, fl.group(1), int(fl.group(2)))
    else:
        matches = _match_symbol(entries, q)
    matches = _rank(matches)
    return {"query": q, "count": len(matches), "matches": matches}
