"""Regression tests for the Q-21 axis-coverage parser.

Stdlib `unittest` only and filesystem-free: `parse_goal_axes` /
`parse_boundary_axes` / `_row_axes` are pure functions over snippet text
(contract: tools/README.md). The snippet shapes mirror the real first-cell
variants inventoried across the 13 boundary docs during the 2026-07 audit.

Run:
    .venv\\Scripts\\python.exe -m unittest discover -s xash3dpp/tools/tests
or from xash3dpp/tools/:
    python -m unittest tests.test_q21
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

# Make the sibling `xtools` package importable (tools/ is tests/'s parent).
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from xtools.q21 import (_row_axes, parse_boundary_axes,  # noqa: E402
                        parse_goal_axes)


GOALS = """# Extension Goals — North-Star Requirements

## 2. The goals

### G-1 — In-engine MCP service
body

### G-2 — Game ABI v2
body

## 3. Shared primitives and door rules

### P-1 — Main-thread service inbox (MPSC)
body

### P-2 — Published-snapshot reads
body

### P-1 — duplicate heading is deduped
"""


def _doc(rows: str) -> str:
    return ("# X Boundary\n\n## Extension axes (Q-21)\n\npreamble\n\n"
            "| Goal / primitive | Applies? | Verdict |\n"
            "|------------------|----------|---------|\n"
            + rows + "\n\n## Next section\n")


class ParseGoalAxes(unittest.TestCase):
    def test_ordered_and_deduped(self):
        self.assertEqual(parse_goal_axes(GOALS),
                         ["G-1", "G-2", "P-1", "P-2"])


class RowAxes(unittest.TestCase):
    """One test per real first-cell variant (audit inventory)."""

    def test_single_bold_with_desc(self):
        self.assertEqual(_row_axes("**P-3** context-first, no new state"),
                         {"P-3"})

    def test_bundled_slash_list(self):
        self.assertEqual(_row_axes("**G-1 / G-3 / G-4 / G-5**"),
                         {"G-1", "G-3", "G-4", "G-5"})

    def test_multi_span_row(self):
        cell = ("**P-1** main-thread inbox / **P-2** snapshots / "
                "**P-3** context-first / **P-5** narrowest-state")
        self.assertEqual(_row_axes(cell), {"P-1", "P-2", "P-3", "P-5"})

    def test_paren_fallback_when_bold_has_no_axis(self):
        # map_loader's load-bearing shape: the ONLY G-3 coverage.
        self.assertEqual(_row_axes("**off-main read** (G-3)"), {"G-3"})

    def test_q_tokens_ignored(self):
        self.assertEqual(_row_axes("**Q-20** edict store confinement"), set())
        self.assertEqual(_row_axes("**Q-11** satellite / **Q-12** compat"),
                         set())

    def test_paren_without_axis_yields_nothing(self):
        self.assertEqual(_row_axes("**Enforcement primitive** (cross-cutting)"),
                         set())


class ParseBoundaryAxes(unittest.TestCase):
    def test_missing_and_unknown_diff(self):
        doc = _doc("| **G-1** mcp | Yes | door |\n"
                   "| **P-9** future axis | Yes | door |")
        covered, has = parse_boundary_axes(doc)
        self.assertTrue(has)
        self.assertEqual(covered, {"G-1", "P-9"})
        # the scan()-level diff: vs {G-1,G-2,P-1,P-2} G-1 is covered,
        # P-9 is unknown — asserted here at set level.
        axis_set = set(parse_goal_axes(GOALS))
        self.assertEqual(sorted(axis_set - covered), ["G-2", "P-1", "P-2"])
        self.assertEqual(sorted(covered - axis_set), ["P-9"])

    def test_no_section(self):
        covered, has = parse_boundary_axes("# Doc\n\n## Something else\n")
        self.assertFalse(has)
        self.assertEqual(covered, set())

    def test_header_and_separator_rows_skipped(self):
        doc = _doc("| **P-1** inbox | Yes | door |")
        covered, _ = parse_boundary_axes(doc)
        self.assertEqual(covered, {"P-1"})

    def test_section_ends_at_next_heading(self):
        doc = _doc("| **P-1** inbox | Yes | door |") + \
            "\n| **G-2** after the section | trap | no |\n"
        covered, _ = parse_boundary_axes(doc)
        self.assertNotIn("G-2", covered)


class PlanDenominator(unittest.TestCase):
    """The denominator is the subsystems the PLAN names, not the docs that
    already exist (which cannot detect silence) and not the src/ directory
    listing (which re-couples the metric to directories and counts ones no
    chunk plans)."""

    PLAN = """## Status Table

### Chunk 6 — server ✅ DONE
**Subsystems**: `server`, `world`\
**Status**: done

### Chunk 11 — physics (pm_shared)
**Subsystems**: `physics`\
**Status**: todo

### Chunk 12 — client
**Subsystems**: `client`, `demo` stub, `ui` stub\
**Status**: todo
"""

    def test_planned_set_is_the_union_of_subsystems_lines(self):
        from xtools.q21 import plan_denominator
        planned, owners, status = plan_denominator(self.PLAN)
        self.assertEqual(planned,
                         ["client", "demo", "physics", "server", "ui", "world"])
        self.assertEqual(owners["world"], ["6"])
        self.assertEqual(status["6"], "done")

    def test_owning_labels_are_deduped(self):
        """A chunk naming a subsystem twice must not report it twice."""
        from xtools.q21 import plan_denominator
        plan = ("### Chunk 6 — server\n"
                "**Subsystems**: `world`, `world`\n"
                "**Status**: done\n")
        _, owners, _ = plan_denominator(plan)
        self.assertEqual(owners["world"], ["6"])


if __name__ == "__main__":
    unittest.main()
