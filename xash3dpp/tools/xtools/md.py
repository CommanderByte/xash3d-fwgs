"""Shared markdown parsing helpers.

Four scanners grew their own "markdown table -> rows" loop
(q21.parse_boundary_axes, checks.status_check, sync.canonical_models,
state._ladder_from_text).  These are the primitives they have in common,
kept pure over text so they unit-test against inline snippets.
"""

from __future__ import annotations

import re

# Docs separate sections with a long underscore rule as well as `## `, so a
# section body ends at either (see implementation-plan.md).
_SECTION_END = r"(?=\n## |\n_{10,}|\Z)"


def section(text: str, heading: str) -> str | None:
    """The body of the `## <heading>` section, or None if absent.

    Matching is on the exact heading text after the `##` marker; the body
    runs to the next `## ` heading, the next long underscore rule, or EOF.
    """
    rx = re.compile(r"^##+\s+" + re.escape(heading) + r"\s*$(.*?)" + _SECTION_END,
                    re.MULTILINE | re.DOTALL)
    m = rx.search(text)
    return m.group(1) if m else None


def cells(line: str) -> list[str]:
    """Split one markdown table row into stripped cells.

    Leading/trailing pipes are dropped; emphasis markers are NOT stripped
    (callers that key on a bold id need them).
    """
    return [c.strip() for c in line.strip().strip("|").split("|")]


def is_separator(row: list[str]) -> bool:
    """True for the `|---|:--:|` alignment row under a table header."""
    return bool(row) and all(
        c and set(c) <= {"-", ":", " "} for c in row)


def table_rows(text: str, *, min_cells: int = 2) -> list[list[str]]:
    """Every markdown table row in `text` as cell lists.

    Separator rows are dropped; header rows are NOT (the caller knows its
    own header shape).  A line only counts as a row when it starts with `|`
    and yields at least `min_cells` cells, so prose containing a pipe is
    ignored.
    """
    out: list[list[str]] = []
    for line in text.splitlines():
        s = line.strip()
        if not s.startswith("|"):
            continue
        row = cells(s)
        if len(row) < min_cells or is_separator(row):
            continue
        out.append(row)
    return out
