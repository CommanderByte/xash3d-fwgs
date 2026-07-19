"""Q-21 axis-coverage scanner: boundary "Extension axes" sections vs the
CURRENT north-star set (T1, 2026-07 tooling wave).

The 2026-07 consolidation audit found 56 missing axis rows across the 13
boundary docs — the drift class this tool now catches mechanically (the
axis set in extension-goals.md is additive; sections written against an
older set silently under-cover).  The judgment half — claims-vs-code and
door-debt honesty — stays with the extension-door-auditor agent; this is
its axis-completeness pre-pass."""

from __future__ import annotations

import re

from . import DOCS, REPO

GOALS_DOC = DOCS / "design" / "extension-goals.md"
BOUNDARIES_DIR = DOCS / "boundaries"

# extension-goals.md axis headings: `### G-1 — In-engine MCP service`
_GOAL_HEADING_RX = re.compile(r"^### ([GP]-\d+)\b", re.MULTILINE)
_AXIS_RX = re.compile(r"\b([GP]-\d+)\b")
_BOLD_RX = re.compile(r"\*\*(.+?)\*\*")
_SECTION_RX = re.compile(
    r"^## Extension axes \(Q-21\)\s*$(.*?)(?=^## |\Z)",
    re.MULTILINE | re.DOTALL)
_TRAILING_PAREN_RX = re.compile(r"\(([^()]*)\)\s*$")


def parse_goal_axes(text: str) -> list[str]:
    """Ordered, deduped axis ids from extension-goals.md's `###` headings —
    always the CURRENT set (never hardcoded; G-5/P-7/P-8 arrived later than
    the original G-1..G-4/P-1..P-6 and more may follow)."""
    seen: list[str] = []
    for m in _GOAL_HEADING_RX.finditer(text):
        if m.group(1) not in seen:
            seen.append(m.group(1))
    return seen


def _row_axes(cell: str) -> set[str]:
    """Axis ids covered by one table row's FIRST cell.

    Rule (verified against every first-cell variant in the 13 boundary
    docs): collect `[GP]-N` tokens from the `**bold**` spans — handles the
    single (`**P-3** desc`), bundled (`**G-1 / G-3 / G-4 / G-5**`) and
    multi-span (`**P-1** … / **P-2** …`) shapes.  When the bold spans carry
    NO axis token, fall back to tokens inside a trailing `(...)` — the only
    way to credit map_loader's `**off-main read** (G-3)` row.  `Q-N` tokens
    are a different register and never counted."""
    out: set[str] = set()
    for bold in _BOLD_RX.findall(cell):
        out.update(_AXIS_RX.findall(bold))
    if not out:
        m = _TRAILING_PAREN_RX.search(cell.strip())
        if m:
            out.update(_AXIS_RX.findall(m.group(1)))
    return out


def parse_boundary_axes(text: str) -> tuple[set[str], bool]:
    """(covered axis ids, has_q21_section) for one boundary doc's text."""
    m = _SECTION_RX.search(text)
    if not m:
        return set(), False
    covered: set[str] = set()
    for line in m.group(1).splitlines():
        s = line.strip()
        if not s.startswith("|"):
            continue
        cells = s.split("|")
        if len(cells) < 3:
            continue
        cell1 = cells[1].strip()
        if not cell1 or set(cell1) <= {"-", ":", " "}:
            continue  # separator row
        if cell1.lower().startswith("goal / primitive"):
            continue  # header row
        covered.update(_row_axes(cell1))
    return covered, True


def scan() -> dict:
    """Diff every boundary doc's Extension-axes coverage against the current
    goal set.  Findings (missing-axis / no-q21-section) mean exit 1 at the
    CLI; `unknown_axes` (axis ids present in a doc but absent from
    extension-goals.md) is informational future-proofing, not a finding."""
    axes = parse_goal_axes(
        GOALS_DOC.read_text(encoding="utf-8", errors="replace"))
    axis_set = set(axes)
    boundaries: dict[str, dict] = {}
    findings: list[dict] = []
    for doc in sorted(BOUNDARIES_DIR.glob("*-boundary.md")):
        sub = doc.name[:-len("-boundary.md")]
        rel = doc.relative_to(REPO).as_posix()
        covered, has_section = parse_boundary_axes(
            doc.read_text(encoding="utf-8", errors="replace"))
        missing = sorted(axis_set - covered) if has_section else sorted(axis_set)
        boundaries[sub] = {
            "file": rel,
            "has_section": has_section,
            "covered": sorted(covered & axis_set),
            "missing": missing,
            "unknown_axes": sorted(covered - axis_set),
        }
        if not has_section:
            findings.append({
                "doc": sub, "kind": "no-q21-section", "axis": "",
                "detail": "%s has no '## Extension axes (Q-21)' section" % rel})
        else:
            for ax in missing:
                findings.append({
                    "doc": sub, "kind": "missing-axis", "axis": ax,
                    "detail": "%s: no row/verdict for %s" % (rel, ax)})
    return {
        "axes": axes,
        "boundaries": boundaries,
        "findings": findings,
        "summary": {
            "docs": len(boundaries),
            "docs_clean": sum(1 for b in boundaries.values()
                              if b["has_section"] and not b["missing"]),
            "missing_total": sum(len(b["missing"])
                                 for b in boundaries.values()),
            "unknown_total": sum(len(b["unknown_axes"])
                                 for b in boundaries.values()),
        },
    }
