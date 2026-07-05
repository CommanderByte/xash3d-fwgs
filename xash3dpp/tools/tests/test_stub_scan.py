"""Regression tests for stub_scan's marker parsing + tag grouping.

Stdlib `unittest` only, filesystem-free — pure functions over synthetic text.

Run:
    .venv\\Scripts\\python.exe -m unittest discover -s xash3dpp/tools/tests
or from xash3dpp/tools/:
    python -m unittest tests.test_stub_scan
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

# Make the sibling `xtools` package importable (tools/ is tests/'s parent).
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from xtools.checks import _count_by_tag, _norm_tag, _scan_stub_text  # noqa: E402


class ScanStubText(unittest.TestCase):
    def test_tag_and_enclosing_symbol(self):
        text = "\n".join([
            "void foo()",
            "{",
            "    // XASH3DPP-STUB(chunk6-S9): the thing",
            "}",
        ])
        stubs = _scan_stub_text(text, "x.cpp")
        self.assertEqual(len(stubs), 1)
        self.assertEqual(stubs[0]["tag"], "chunk6-S9")
        self.assertEqual(stubs[0]["symbol"], "foo")
        self.assertEqual(stubs[0]["line"], 3)
        self.assertIn("XASH3DPP-STUB(chunk6-S9)", stubs[0]["marker"])

    def test_bare_todo_has_empty_tag(self):
        stubs = _scan_stub_text("// TODO: wire this later", "x.cpp")
        self.assertEqual(len(stubs), 1)
        self.assertEqual(stubs[0]["tag"], "")

    def test_todo_with_tag(self):
        self.assertEqual(
            _scan_stub_text("    // TODO(chunk7): later", "x.cpp")[0]["tag"],
            "chunk7")

    def test_mid_line_marker(self):
        stubs = _scan_stub_text(
            "    do_thing();  // STUB(pmove) placeholder", "x.cpp")
        self.assertEqual(stubs[0]["tag"], "pmove")

    def test_url_is_not_a_marker(self):
        # a bare URL containing // and TODO must not be picked up.
        self.assertEqual(_scan_stub_text("see https://x.dev/TODO", "x.cpp"), [])


class ByTag(unittest.TestCase):
    def test_counts_and_ordering(self):
        by = _count_by_tag([
            {"tag": "chunk6"}, {"tag": "chunk6"}, {"tag": "chunk6-S9"},
            {"tag": ""},
        ])
        self.assertEqual(by["chunk6"], 2)
        self.assertEqual(by["chunk6-s9"], 1)     # normalized (lowercased)
        self.assertEqual(by["(untagged)"], 1)
        self.assertEqual(next(iter(by)), "chunk6")  # sorted by count desc

    def test_norm_folds_case_and_space(self):
        self.assertEqual(_norm_tag("Chunk 7"), "chunk7")
        self.assertEqual(_norm_tag("chunk7"), "chunk7")
        self.assertEqual(_norm_tag("  S8-seam "), "s8-seam")
        self.assertEqual(_norm_tag(None), "(untagged)")
        self.assertEqual(_norm_tag("   "), "(untagged)")


if __name__ == "__main__":
    unittest.main()
