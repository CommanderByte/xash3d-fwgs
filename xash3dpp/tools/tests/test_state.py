"""Regression tests for the plan/chunk/ladder parsers in xtools.state.

Stdlib `unittest` only, filesystem-free: the parsers under test are the pure
text functions (`_parse_chunks`, `_ladder_from_text`, `_scoped_ladder`)
introduced with the Chunk 6B suffix support (2026-07-06 wave).

Run:
    .venv\\Scripts\\python.exe -m unittest discover -s xash3dpp/tools/tests
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

# Make the sibling `xtools` package importable (tools/ is tests/'s parent).
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from xtools import state  # noqa: E402
from xtools.state import (_filter_checkpoints, _ladder_from_text,  # noqa: E402
                          _parse_chunks, _scoped_ladder)

# A minimal plan shaped like the real one at the 6B stage: Chunk 6 DONE (and
# still carrying its historical ladder), Chunk 6B IN PROGRESS with its own
# ladder, Chunk 7 todo.
PLAN_6B = """\
*Generated: 2026-05-15 — Updated: 2026-07-06*

### Chunk 6 — server ✅ DONE

**Subsystems**: `server`
**Session ladder** (one green commit per step): S1 scaffold ✅ → S2 plan ✅

### Chunk 6B — hardening retrofit (Q-22 + QN) IN PROGRESS

**Subsystems**: `server`, `networking`, `utilities`
**Session ladder** (one commit per step): S1 utilities+memory ✅ → S2 platform → S9 server

### Chunk 7 — content pipeline

**Subsystems**: `content`
"""


class ParseChunks(unittest.TestCase):
    def test_suffix_and_label(self):
        chunks = _parse_chunks(PLAN_6B)
        self.assertEqual([c["label"] for c in chunks], ["6", "6B", "7"])
        self.assertEqual(chunks[1]["num"], 6)
        self.assertEqual(chunks[1]["suffix"], "B")

    def test_statuses(self):
        chunks = _parse_chunks(PLAN_6B)
        self.assertEqual([c["status"] for c in chunks],
                         ["done", "in-progress", "todo"])

    def test_sort_order_6_6b_7(self):
        chunks = _parse_chunks(PLAN_6B)
        ordered = sorted(chunks, key=lambda c: (c["num"], c["suffix"]))
        self.assertEqual([c["label"] for c in ordered], ["6", "6B", "7"])

    def test_subsystems_primary_and_full_set(self):
        chunks = _parse_chunks(PLAN_6B)
        self.assertEqual(chunks[1]["subsystem"], "server")
        self.assertEqual(chunks[1]["subsystems"],
                         ["server", "networking", "utilities"])

    def test_plain_integer_chunks_unaffected(self):
        chunks = _parse_chunks("### Chunk 7 — content pipeline\n")
        self.assertEqual(chunks[0]["suffix"], "")
        self.assertEqual(chunks[0]["label"], "7")


class LadderScoping(unittest.TestCase):
    def test_in_progress_chunk_wins_over_first_in_file(self):
        # Chunk 6 (done) has S2 ✅; 6B (in progress) has only S1 ✅.  The old
        # first-in-file match would report S2 — scoping must pick 6B's.
        ladder = _scoped_ladder(PLAN_6B)
        self.assertIsNotNone(ladder)
        self.assertEqual(ladder["highest_done"], "S1")
        self.assertEqual([s["id"] for s in ladder["steps"]],
                         ["S1", "S2", "S9"])

    def test_todo_chunk_used_when_nothing_in_progress(self):
        text = PLAN_6B.replace(" IN PROGRESS", " ✅ DONE").replace(
            "### Chunk 7 — content pipeline",
            "### Chunk 7 — content pipeline\n\n"
            "**Session ladder**: S1 recon → S2 scaffold")
        ladder = _scoped_ladder(text)
        self.assertEqual([s["id"] for s in ladder["steps"]], ["S1", "S2"])
        self.assertIsNone(ladder["highest_done"])

    def test_fallback_to_file_when_no_active_chunk_has_ladder(self):
        # Everything done → no in-progress/todo ladder; the whole-file
        # fallback preserves the pre-6B behavior.
        text = PLAN_6B.replace(" IN PROGRESS", " ✅ DONE").replace(
            "### Chunk 7 — content pipeline", "### Chunk 7 — content ✅ DONE")
        ladder = _scoped_ladder(text)
        self.assertIsNotNone(ladder)
        self.assertEqual(ladder["highest_done"], "S2")  # chunk 6's, first in file

    def test_ladder_preamble_split(self):
        # Step ids inside the parenthetical preamble must not become steps.
        ladder = _ladder_from_text(
            "**Session ladder** (refined by S2 plan-implementation): "
            "S1 a ✅ → S2 b")
        self.assertEqual([s["id"] for s in ladder["steps"]], ["S1", "S2"])
        self.assertEqual(ladder["highest_done"], "S1")


class FilterCheckpoints(unittest.TestCase):
    """`_filter_checkpoints` (T7): pure search over the advisory log."""

    ENTRIES = [
        {"chunk": "6-server", "step": "commit", "note": "S8 sweep done",
         "actor": "claude", "session": "s1", "branch": "b"},
        {"chunk": "7-content", "step": "handoff", "note": "bone solver",
         "actor": "codex", "session": "s2", "branch": "b"},
        {"chunk": "6-server", "step": "pre-pr", "note": "SHIP verdict",
         "actor": "claude", "session": "s3", "branch": "b"},
    ]

    def test_grep_is_case_insensitive_regex(self):
        out = _filter_checkpoints(self.ENTRIES, grep="ship|SOLVER")
        self.assertEqual([e["step"] for e in out], ["handoff", "pre-pr"])

    def test_chunk_exact_match(self):
        out = _filter_checkpoints(self.ENTRIES, chunk="6-server")
        self.assertEqual(len(out), 2)
        self.assertTrue(all(e["chunk"] == "6-server" for e in out))
        # exact, not prefix: "6" matches nothing
        self.assertEqual(_filter_checkpoints(self.ENTRIES, chunk="6"), [])

    def test_limit_keeps_newest_tail(self):
        out = _filter_checkpoints(self.ENTRIES, limit=2)
        self.assertEqual([e["step"] for e in out], ["handoff", "pre-pr"])
        self.assertEqual(len(_filter_checkpoints(self.ENTRIES, limit=0)), 3)


class BlockingOqs(unittest.TestCase):
    def test_decided_scaffold_rows_do_not_block(self):
        register = """\
## 3a. Boundary-spec OQ crosswalk

| Doc | OQs | Status | Blocks |
|-----|-----|--------|--------|
| `physics-boundary` | PHY-OQ-1 (real blocker) | open | scaffold |
| `sound-boundary` | SND-OQ-1 (historical gate) | **resolved**: done | scaffold |
| `sound-boundary` | SND-OQ-2 (historical gate) | ✅ **decided**: done | scaffold |

______________________________________________________________________
"""
        original = state._REGISTER
        try:
            class FakeRegister:
                @staticmethod
                def read_text(**_kwargs):
                    return register

            state._REGISTER = FakeRegister()
            out = state.blocking_oqs()
        finally:
            state._REGISTER = original
        self.assertEqual([row["oq"] for row in out], ["PHY-OQ-1"])


if __name__ == "__main__":
    unittest.main()
