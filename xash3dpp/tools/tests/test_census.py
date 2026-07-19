"""Regression tests for the census tool's pure helpers.

Stdlib `unittest` only and filesystem-free (contract: tools/README.md):
`_tally_allows` is pure text->tally; the assert-call regex is exercised
against snippet lines the same way the compliance snippet tests do.

Run:
    .venv\\Scripts\\python.exe -m unittest discover -s xash3dpp/tools/tests
or from xash3dpp/tools/:
    python -m unittest tests.test_census
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

# Make the sibling `xtools` package importable (tools/ is tests/'s parent).
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from xtools.checks import _THREAD_ASSERT_CALL_RX, _tally_allows  # noqa: E402


class TallyAllows(unittest.TestCase):
    def test_counts_by_check_id(self):
        text = ("// compliance-allow(thread-assert): reason one\n"
                "code();\n"
                "// compliance-allow(thread-assert): reason two\n"
                "// compliance-allow(int-width): abi\n")
        self.assertEqual(_tally_allows(text),
                         {"thread-assert": 2, "int-width": 1})

    def test_comma_list_counts_each_check(self):
        text = "// compliance-allow(int-width, mutable-global): shared\n"
        self.assertEqual(_tally_allows(text),
                         {"int-width": 1, "mutable-global": 1})

    def test_no_markers(self):
        self.assertEqual(_tally_allows("plain code();\n"), {})


class ThreadAssertCallRx(unittest.TestCase):
    def test_call_sites_counted_not_mentions(self):
        self.assertTrue(_THREAD_ASSERT_CALL_RX.search(
            "    ::xash::core::assert_thread_role( ThreadRole::Main );"))
        self.assertTrue(_THREAD_ASSERT_CALL_RX.search(
            "    assert_main_thread();"))
        # a name mention without a call paren is not a site
        self.assertFalse(_THREAD_ASSERT_CALL_RX.search(
            "    // assert_thread_role coverage discussed here"))

    def test_two_calls_on_one_line_count_twice(self):
        line = "assert_thread_role(a); assert_thread_role(b);"
        self.assertEqual(len(_THREAD_ASSERT_CALL_RX.findall(line)), 2)


if __name__ == "__main__":
    unittest.main()
