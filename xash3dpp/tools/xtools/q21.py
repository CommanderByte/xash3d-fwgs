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
from . import md
from .state import _parse_chunks

GOALS_DOC = DOCS / "design" / "extension-goals.md"
PLAN_DOC = DOCS / "implementation-plan.md"
BOUNDARIES_DIR = DOCS / "boundaries"

# extension-goals.md axis headings: `### G-1 — In-engine MCP service`
_GOAL_HEADING_RX = re.compile(r"^### ([GP]-\d+)\b", re.MULTILINE)
_AXIS_RX = re.compile(r"\b([GP]-\d+)\b")
_BOLD_RX = re.compile(r"\*\*(.+?)\*\*")
_SECTION_RX = re.compile(
    r"^## Extension axes \(Q-21\)\s*$(.*?)(?=^## |\Z)",
    re.MULTILINE | re.DOTALL)
_TRAILING_PAREN_RX = re.compile(r"\(([^()]*)\)\s*$")
# Q-25: the '## Role & parity' section must declare a Role: line.
_ROLE_PARITY_LINE_RX = re.compile(r"(?i)\bRole\s*:")


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
    for row in md.table_rows(m.group(1)):
        cell1 = row[0]
        if not cell1:
            continue
        if cell1.lower().startswith("goal / primitive"):
            continue  # header row
        covered.update(_row_axes(cell1))
    return covered, True


# The denominator is the set of subsystems the PLAN names, not the set of docs
# that already exist and not the `src/` directory listing.
#
# Globbing existing docs cannot detect silence: a subsystem with no boundary
# spec contributes no slot, so "16/16 clean" read as full coverage while six
# subsystems had never been asked (2026-07 audit).  Taking `xtools.subsystems()`
# instead would fix that but re-couple the metric to directories, so deleting an
# unbuilt skeleton would silently shrink the denominator again -- and it counts
# directories no chunk plans.  implementation-plan.md is the authority CLAUDE.md
# names, it is delete-proof, and a subsystem enters the denominator exactly when
# the plan starts claiming it.  `state._parse_chunks` already parses the
# `**Subsystems**:` lines and each chunk's status, so this reuses it rather than
# growing a second parser for the same doc.


def plan_denominator(text: str) -> tuple[list[str], dict[str, list[str]],
                                         dict[str, str]]:
    """(planned subsystems, subsystem -> owning chunk labels, label -> status)"""
    owners: dict[str, list[str]] = {}
    status: dict[str, str] = {}
    for c in _parse_chunks(text):
        label = c.get("label", "?")
        status[label] = c.get("status", "")
        for sub in c.get("subsystems") or []:
            labels = owners.setdefault(sub, [])
            if label not in labels:      # a chunk may name a subsystem twice
                labels.append(label)
    return sorted(owners), owners, status


def scan() -> dict:
    """Diff every boundary doc's Extension-axes coverage against the current
    goal set.  Findings (missing-axis / no-q21-section) mean exit 1 at the
    CLI; `unknown_axes` (axis ids present in a doc but absent from
    extension-goals.md) is informational future-proofing, not a finding."""
    axes = parse_goal_axes(
        GOALS_DOC.read_text(encoding="utf-8", errors="replace"))
    axis_set = set(axes)
    plan_text = PLAN_DOC.read_text(encoding="utf-8", errors="replace")
    planned, owners, chunk_status = plan_denominator(plan_text)
    boundaries: dict[str, dict] = {}
    findings: list[dict] = []
    pending: list[dict] = []
    have = {d.name[:-len("-boundary.md")]: d
            for d in sorted(BOUNDARIES_DIR.glob("*-boundary.md"))}

    for sub in sorted(set(planned) | set(have)):
        doc = have.get(sub)
        if doc is None:
            # No spec.  Whether that is a DEFECT depends on the owning chunk:
            # a subsystem whose chunk has started owes its spec now (the entry
            # gates say so); one whose chunk is still `todo` does not, and
            # reporting it as a finding would paint the gate permanently red on
            # work nobody can action -- which is how the stale reports became
            # invisible in the first place.
            labels = owners.get(sub, [])
            started = [l for l in labels
                       if chunk_status.get(l) in ("done", "in-progress")]
            entry = {"subsystem": sub, "chunks": labels,
                     "detail": "no %s-boundary.md" % sub}
            if started:
                findings.append({
                    "doc": sub, "kind": "no-boundary-doc", "axis": "",
                    "detail": "%s has no boundary spec and its chunk(s) %s "
                              "have started" % (sub, ", ".join(started))})
            else:
                pending.append(entry)
            continue
        rel = doc.relative_to(REPO).as_posix()
        text = doc.read_text(encoding="utf-8", errors="replace")
        covered, has_section = parse_boundary_axes(text)
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
        # Role & parity required-section check (Q-25): a subsystem whose chunk
        # has started must carry a '## Role & parity' section with a Role: line.
        # Not gating for still-todo chunks, matching the boundary-doc rule.
        started = [l for l in owners.get(sub, [])
                   if chunk_status.get(l) in ("done", "in-progress")]
        rp = md.section(text, "Role & parity")
        if started and not (rp and _ROLE_PARITY_LINE_RX.search(rp)):
            findings.append({
                "doc": sub, "kind": "no-role-parity", "axis": "",
                "detail": "%s has no '## Role & parity' section with a Role: "
                          "line (Q-25); chunk(s) %s have started"
                          % (rel, ", ".join(started))})
    return {
        "axes": axes,
        "planned": planned,
        "boundaries": boundaries,
        "findings": findings,
        "pending_specs": pending,
        "summary": {
            "docs": len(boundaries),
            "planned": len(planned),
            "pending": len(pending),
            "docs_clean": sum(1 for b in boundaries.values()
                              if b["has_section"] and not b["missing"]),
            "missing_total": sum(len(b["missing"])
                                 for b in boundaries.values()),
            "unknown_total": sum(len(b["unknown_axes"])
                                 for b in boundaries.values()),
        },
    }
