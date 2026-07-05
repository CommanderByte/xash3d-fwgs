"""Regression tests for the compliance-scan ruleset + the allow-hatch.

Stdlib `unittest` only — the xash3dpp/tools CLI path is stdlib-only by
contract (see tools/README.md), so these tests must not import pytest or any
third-party package.  They are filesystem-free: regex rules are tested by
compiling the rule's own pattern against snippet lines, and the structured
allow-hatch is tested against `_filter_allows` directly.

Run:
    .venv\\Scripts\\python.exe -m unittest discover -s xash3dpp/tools/tests
or from xash3dpp/tools/:
    python -m unittest tests.test_compliance
"""

from __future__ import annotations

import re
import sys
import unittest
from pathlib import Path

# Make the sibling `xtools` package importable (tools/ is tests/'s parent).
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from xtools.checks import _allow_context, _filter_allows, _violation  # noqa: E402
from xtools.rules import RULES  # noqa: E402


def _rule(check: str):
    for r in RULES:
        if r.check == check:
            return r
    raise KeyError(check)


def _hits(check: str, line: str) -> bool:
    """True when `line` trips the named regex rule, honoring exclude_line_re
    (mirrors the regex-rule scan in checks.compliance_scan)."""
    r = _rule(check)
    rx = re.compile(r.pattern)
    ex = re.compile(r.exclude_line_re) if r.exclude_line_re else None
    return bool(rx.search(line)) and not (ex and ex.search(line))


class RawCstring(unittest.TestCase):
    def test_bare_calls_flagged(self):
        self.assertTrue(_hits("raw-cstring", "    n = strlen( s );"))
        self.assertTrue(_hits("raw-cstring", "    strcmp( a, b );"))
        self.assertTrue(_hits("raw-cstring", "    sprintf( buf, fmt );"))

    def test_qualified_and_member_calls_exempt(self):
        for line in (
            "    n = std::strlen( s );",
            "    n = ut::strlen( s );",
            "    n = utilities::strlen( s );",
            "    if ( p->strcmp( a, b ) ) {}",
            "    obj.strcpy( d, s );",
        ):
            self.assertFalse(_hits("raw-cstring", line), line)

    def test_snprintf_never_matches(self):
        # snprintf is not in the rule and must not be caught by `sprintf`.
        self.assertFalse(_hits("raw-cstring", "    snprintf( b, n, f );"))
        self.assertFalse(_hits("raw-cstring", "    std::snprintf( b, n, f );"))


class IncludeCHeaders(unittest.TestCase):
    def test_c_headers_flagged(self):
        self.assertTrue(_hits("include-cstring-cstdio", "#include <string.h>"))
        self.assertTrue(_hits("include-cstring-cstdio", "# include <stdio.h>"))

    def test_cpp_wrappers_exempt(self):
        self.assertFalse(_hits("include-cstring-cstdio", "#include <cstring>"))
        self.assertFalse(_hits("include-cstring-cstdio", "#include <cstdio>"))


class PimplDefaultHeader(unittest.TestCase):
    def test_pimpl_dtor_flagged(self):
        self.assertTrue(_hits("pimpl-default-header", "    ~Widget() = default;"))

    def test_pimpl_move_flagged(self):
        self.assertTrue(_hits(
            "pimpl-default-header",
            "    Widget( Widget && ) noexcept = default;"))

    def test_interface_virtual_dtor_exempt(self):
        self.assertFalse(_hits(
            "pimpl-default-header", "    virtual ~IObserver() = default;"))


class AllowHatch(unittest.TestCase):
    @staticmethod
    def _viol(check: str, raw: str) -> dict:
        return {"check": check, "severity": "candidate-warning",
                "file": "x.cpp", "line": 3, "excerpt": raw.strip()[:200],
                "hint": "", "rule_ref": "", "_raw": raw}

    def test_marker_routes_to_allows(self):
        v = self._viol(
            "thread-assert",
            "void X::reset() noexcept "
            "// compliance-allow(thread-assert): thread-agnostic value type")
        kept, allowed = _filter_allows([v])
        self.assertEqual(kept, [])
        self.assertEqual(len(allowed), 1)
        self.assertEqual(allowed[0]["check"], "thread-assert")
        self.assertEqual(allowed[0]["line"], 3)

    def test_no_marker_kept_and_raw_stripped(self):
        v = self._viol("thread-assert", "void X::reset() noexcept")
        kept, allowed = _filter_allows([v])
        self.assertEqual(len(kept), 1)
        self.assertEqual(allowed, [])
        self.assertNotIn("_raw", kept[0])  # never leaks into the envelope

    def test_marker_for_other_check_is_kept(self):
        v = self._viol(
            "thread-assert",
            "void X::reset() // compliance-allow(ns-qualify)")
        kept, allowed = _filter_allows([v])
        self.assertEqual(len(kept), 1)
        self.assertEqual(allowed, [])

    def test_multi_check_marker(self):
        v = self._viol(
            "ns-qualify",
            "utilities::foo() // compliance-allow(thread-assert, ns-qualify)")
        kept, allowed = _filter_allows([v])
        self.assertEqual(kept, [])
        self.assertEqual(len(allowed), 1)


class AllowContext(unittest.TestCase):
    """`_allow_context`: the flagged line plus its contiguous preceding `//`
    comment block (so a marker on the doc-comment above a finding is honored)."""

    @staticmethod
    def _lines(*raws: str):
        # code_lines yields (lineno, code, raw); a // comment line has code=""
        # (blanked), everything else keeps its text as `code` for adjacency.
        out = []
        for i, raw in enumerate(raws, start=1):
            code = "" if raw.lstrip().startswith("//") else raw
            out.append((i, code, raw))
        return out

    def test_flagged_line_only(self):
        lines = self._lines("    foo( bar );")
        self.assertEqual(_allow_context(lines, 0), "    foo( bar );")

    def test_preceding_comment_block_included(self):
        lines = self._lines(
            "    // reason line one",
            "    // compliance-allow(int-width): ABI unsigned int",
            "    foo( (unsigned int)x );")
        ctx = _allow_context(lines, 2)
        self.assertIn("compliance-allow(int-width)", ctx)
        self.assertIn("(unsigned int)x", ctx)

    def test_preceding_code_line_stops_the_walk(self):
        # a non-comment line above the flag ends the block: a marker further up
        # (separated by code) must NOT leak in.
        lines = self._lines(
            "    // compliance-allow(int-width): far above",
            "    int y = 0;",
            "    foo( (unsigned int)x );")
        ctx = _allow_context(lines, 2)
        self.assertNotIn("compliance-allow", ctx)

    def test_routes_via_violation_allow_ctx(self):
        # end-to-end: a marker in the preceding comment block, threaded through
        # _violation(allow_ctx=...) → _filter_allows, exempts the finding and
        # the allow entry reports the CLEAN flagged-line excerpt (not the ctx).
        lines = self._lines(
            "    // compliance-allow(int-width): ABI unsigned int",
            "    return (unsigned int)seed;")
        v = _violation("int-width", "warning", Path("x.cpp"), 2,
                       lines[1][2], "hint", "ref", candidate=True,
                       allow_ctx=_allow_context(lines, 1))
        kept, allowed = _filter_allows([v])
        self.assertEqual(kept, [])
        self.assertEqual(len(allowed), 1)
        self.assertEqual(allowed[0]["excerpt"], "return (unsigned int)seed;")
        self.assertNotIn("\n", allowed[0]["excerpt"])  # single clean line


if __name__ == "__main__":
    unittest.main()
