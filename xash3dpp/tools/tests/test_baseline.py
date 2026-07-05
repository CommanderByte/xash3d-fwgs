"""Regression tests for compliance_scan's baseline-diff (introduced-only) mode.

Stdlib `unittest` only, filesystem-free — tests the pure set-diff helper
`_introduced_findings`.  The git-blob side (`_base_excerpts`) is exercised by
manual CLI smoke, keeping the suite off the filesystem.

Run:
    .venv\\Scripts\\python.exe -m unittest discover -s xash3dpp/tools/tests
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from xtools.checks import _introduced_findings  # noqa: E402


def _f(file: str, excerpt: str, check: str = "raw-cstring") -> dict:
    return {"check": check, "severity": "warning", "file": file, "line": 1,
            "excerpt": excerpt}


class IntroducedFindings(unittest.TestCase):
    def test_unchanged_line_suppressed(self):
        base = {"a.cpp": {"strlen( s );"}}
        self.assertEqual(
            _introduced_findings([_f("a.cpp", "strlen( s );")], base), [])

    def test_new_line_kept(self):
        base = {"a.cpp": {"other();"}}
        self.assertEqual(
            len(_introduced_findings([_f("a.cpp", "strlen( s );")], base)), 1)

    def test_added_file_all_kept(self):
        # file absent from base_excerpts (added by the slice) -> all new
        self.assertEqual(
            len(_introduced_findings([_f("new.cpp", "strlen( s );")], {})), 1)

    def test_mixed_keeps_only_new(self):
        base = {"a.cpp": {"old_flag();"}}
        kept = _introduced_findings(
            [_f("a.cpp", "old_flag();"), _f("a.cpp", "new_flag();")], base)
        self.assertEqual([f["excerpt"] for f in kept], ["new_flag();"])

    def test_matches_content_not_line_number(self):
        base = {"a.cpp": {"dup();"}}
        f = _f("a.cpp", "dup();")
        f["line"] = 999  # line number is irrelevant to content matching
        self.assertEqual(_introduced_findings([f], base), [])


if __name__ == "__main__":
    unittest.main()
