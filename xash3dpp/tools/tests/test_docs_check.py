"""Unit tests for the documentation-drift scanner (xtools/docs.py) and the
shared markdown helpers (xtools/md.py).

Stdlib `unittest` only -- the xash3dpp/tools CLI path is stdlib-only by
contract (see tools/README.md).  Filesystem-free: every parser under test is
pure over text, so it is exercised against inline snippets.  status_check is
the counter-example in this tree -- it reads implementation-plan.md from disk
and is therefore the one checker with no tests at all.
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from xtools.docs import (anchor_key, compare, parse_anchors, parse_claims,
                         parse_predicate)
from xtools.md import cells, is_separator, section, table_rows


class ParseAnchors(unittest.TestCase):
    def test_bare_basename_and_range(self):
        a = parse_anchors("see `clip.cpp:237-273` for the kernel")
        self.assertEqual(len(a), 1)
        self.assertEqual(a[0]["path"], "clip.cpp")
        self.assertEqual((a[0]["line"], a[0]["end"]), (237, 273))

    def test_full_path_form(self):
        a = parse_anchors("- **File(s)**: `xash3dpp/src/launcher/main.cpp:90-91`")
        self.assertEqual(a[0]["path"], "xash3dpp/src/launcher/main.cpp")
        self.assertEqual(a[0]["line"], 90)

    def test_single_line_sets_end_to_start(self):
        a = parse_anchors("`host.cpp:309`")
        self.assertEqual((a[0]["line"], a[0]["end"]), (309, 309))

    def test_unbackticked_prose_is_ignored(self):
        """Backticks are the convention; without them prose like a version
        string or a ratio would flood the match set."""
        self.assertEqual(parse_anchors("see clip.cpp:237 and 16/26 verbs"), [])

    def test_repeated_citations_get_distinct_keys(self):
        a = parse_anchors("`a.cpp:1` then\nmore\n`a.cpp:1` again")
        self.assertEqual([x["occurrence"] for x in a], [0, 1])
        self.assertNotEqual(anchor_key("d.md", a[0]), anchor_key("d.md", a[1]))

    def test_key_is_independent_of_the_line_number(self):
        """--repair rewrites the line number inside the anchor text.  If the
        lockfile key included it, every repair would orphan the old entry and
        auto-bless the new one -- silently resetting drift detection to zero
        for that anchor.  The key is (doc, cited path, occurrence)."""
        before = parse_anchors("`ctx.cpp:181`")[0]
        after = parse_anchors("`ctx.cpp:182`")[0]
        self.assertEqual(anchor_key("d.md", before), anchor_key("d.md", after))

    def test_occurrence_counts_per_path_not_per_raw_text(self):
        a = parse_anchors("`x.cpp:1` and `x.cpp:9` and `y.cpp:1`")
        self.assertEqual([(x["path"], x["occurrence"]) for x in a],
                         [("x.cpp", 0), ("x.cpp", 1), ("y.cpp", 0)])

    def test_doc_line_is_one_based(self):
        a = parse_anchors("intro\n\n`x.cpp:5`")
        self.assertEqual(a[0]["doc_line"], 3)

    def test_verify_skip_on_same_or_previous_line(self):
        same = parse_anchors("`x.cpp:5` <!-- verify-skip: illustrative -->")
        self.assertTrue(same[0]["skipped"])
        above = parse_anchors("<!-- verify-skip: illustrative -->\n`x.cpp:5`")
        self.assertTrue(above[0]["skipped"])
        self.assertFalse(parse_anchors("`x.cpp:5`")[0]["skipped"])


class RepairRange(unittest.TestCase):
    """--repair must shift BOTH ends of a range anchor by the same delta.
    Rewriting only the start produced `rules.py:367-354` -- an end before its
    start -- which then re-reported as drift forever."""

    @staticmethod
    def _repaired(path: str, line: int, end: int, moved: int) -> str:
        delta = moved - line
        return ("`%s:%d`" % (path, moved) if end == line
                else "`%s:%d-%d`" % (path, moved, end + delta))

    def test_single_line(self):
        self.assertEqual(self._repaired("a.cpp", 181, 181, 182), "`a.cpp:182`")

    def test_range_shifts_both_ends(self):
        self.assertEqual(self._repaired("rules.py", 348, 354, 367),
                         "`rules.py:367-373`")

    def test_range_shifting_backwards(self):
        self.assertEqual(self._repaired("clip.cpp", 237, 273, 211),
                         "`clip.cpp:211-247`")


class ParsePredicate(unittest.TestCase):
    def test_default_comparison_is_at_least_one(self):
        p = parse_predicate("compliance-rule-exists(entvars-confinement)")
        self.assertEqual((p["fn"], p["op"], p["want"]),
                         ("compliance-rule-exists", ">=", 1))

    def test_explicit_comparison(self):
        p = parse_predicate("census(server, src_tu_count) == 31")
        self.assertEqual((p["op"], p["want"]), ("==", 31))
        self.assertEqual(p["argstr"], "server, src_tu_count")

    def test_regex_argument_may_contain_parens(self):
        """The closing paren is the LAST one, so a regex argument keeps its
        own parentheses -- greedy match, not [^)]*."""
        p = parse_predicate(
            r"grep-count(:\s*(public\s+)?ITrustOracle\b, tests/**/*.hpp) == 0")
        self.assertIsNotNone(p)
        self.assertEqual(p["fn"], "grep-count")
        self.assertTrue(p["argstr"].endswith("tests/**/*.hpp"))
        self.assertEqual(p["want"], 0)

    def test_regex_argument_may_contain_commas(self):
        """Args are kept raw and split on the LAST comma by the evaluator,
        so a quantifier like \\w{1,3} survives."""
        p = parse_predicate(r"grep-count(\w{1,3}_test, src/**/*.cpp) == 2")
        self.assertEqual(p["argstr"], r"\w{1,3}_test, src/**/*.cpp")

    def test_rejects_garbage(self):
        for bad in ("not a predicate", "fn[x] == 1", ""):
            with self.subTest(bad=bad):
                self.assertIsNone(parse_predicate(bad))


class Compare(unittest.TestCase):
    def test_operators(self):
        self.assertTrue(compare(2, "==", 2))
        self.assertFalse(compare(2, "==", 3))
        self.assertTrue(compare(2, ">=", 1))
        self.assertTrue(compare(0, "<=", 0))
        self.assertTrue(compare(3, ">", 2))
        self.assertFalse(compare(2, "<", 2))


class ParseClaims(unittest.TestCase):
    def test_extracts_predicate_and_line(self):
        c = parse_claims("intro\n<!-- verify: symbol-exists(foo) -->\n")
        self.assertEqual(c, [{"doc_line": 2, "predicate": "symbol-exists(foo)"}])

    def test_table_cell_pipe_escape_is_undone(self):
        """Most binding claims live in boundary Extension-axes TABLE ROWS,
        where a literal `|` must be written `\|` or it splits the row."""
        c = parse_claims(r"| x | <!-- verify: grep-count(a\|b, s/**/*.cpp) == 2 --> |")
        self.assertEqual(c[0]["predicate"], "grep-count(a|b, s/**/*.cpp) == 2")

    def test_ignores_other_html_comments(self):
        """The docs' only existing HTML comments are pymarkdown pragmas."""
        self.assertEqual(parse_claims("<!-- pyml disable-next-line ol-prefix -->"),
                         [])


class MarkdownHelpers(unittest.TestCase):
    def test_section_body_stops_at_next_heading(self):
        body = section("## A\nalpha\n\n## B\nbeta\n", "A")
        self.assertIn("alpha", body)
        self.assertNotIn("beta", body)

    def test_section_body_stops_at_underscore_rule(self):
        """implementation-plan.md separates sections with a long rule."""
        body = section("## A\nalpha\n" + "_" * 40 + "\nbeta\n", "A")
        self.assertIn("alpha", body)
        self.assertNotIn("beta", body)

    def test_section_absent(self):
        self.assertIsNone(section("## A\nalpha\n", "Nope"))

    def test_cells_strips_outer_pipes(self):
        self.assertEqual(cells("| a | b | c |"), ["a", "b", "c"])

    def test_separator_row_detected(self):
        self.assertTrue(is_separator(["---", ":--:"]))
        self.assertFalse(is_separator(["a", "b"]))

    def test_table_rows_drops_separators_and_prose(self):
        text = ("| Goal | Verdict |\n|---|---|\n| **G-1** | yes |\n"
                "prose with a | pipe\n")
        rows = table_rows(text)
        self.assertEqual(rows, [["Goal", "Verdict"], ["**G-1**", "yes"]])

    def test_table_rows_keeps_emphasis(self):
        """q21 keys on the bold axis id, so markers must survive."""
        self.assertEqual(table_rows("| **P-3** | x |")[0][0], "**P-3**")


if __name__ == "__main__":
    unittest.main()
