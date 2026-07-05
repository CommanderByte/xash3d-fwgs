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

from xtools.checks import (_DECL_RX, _allow_context,  # noqa: E402
                           _decl_args_are_bare_ids, _filter_allows, _violation)
from xtools.rules import RULES  # noqa: E402


def _rule(check: str):
    for r in RULES:
        if r.check == check:
            return r
    raise KeyError(check)


def _hits(check: str, line: str, raw: str | None = None) -> bool:
    """True when a line trips the named regex rule (mirrors the regex-rule
    scan in checks.compliance_scan): `line` is the comment-stripped code,
    `raw` the full source line (defaults to `line`).  Patterns match code
    (raw when the rule sets match_raw); exclude_line_re always matches RAW —
    suppression markers live in comments."""
    r = _rule(check)
    raw = line if raw is None else raw
    rx = re.compile(r.pattern)
    ex = re.compile(r.exclude_line_re) if r.exclude_line_re else None
    subject = raw if r.match_raw else line
    return bool(rx.search(subject)) and not (ex and ex.search(raw))


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


class LifecycleRules(unittest.TestCase):
    """Q-22 LIFECYCLE_MODEL rules (2026-07-06 wave)."""

    def test_class_operator_new_flagged(self):
        self.assertTrue(_hits(
            "class-operator-new",
            "    static void *operator new( std::size_t n );"))
        self.assertTrue(_hits(
            "class-operator-new",
            "void *Thing::operator new( std::size_t n ) {"))

    def test_operator_delete_not_flagged(self):
        self.assertFalse(_hits(
            "class-operator-new",
            "    static void operator delete( void *p ) noexcept;"))

    def test_make_unique_nonimpl_flagged_impl_exempt(self):
        self.assertTrue(_hits(
            "make-unique-outside-pimpl",
            "    auto chan = std::make_unique<Netchan>( cfg );"))
        self.assertFalse(_hits(
            "make-unique-outside-pimpl",
            "    impl_ = std::make_unique<Impl>();"))


class AnnotationRules(unittest.TestCase):
    """QN ANNOTATION_DISCIPLINE rules (2026-07-06 wave)."""

    def test_post_retired_matches_comment_content(self):
        # match_raw rule: the pattern must hit the RAW line (comment text).
        code = ""  # comments are blanked from the code view
        raw = "    // Post: the buffer is flushed"
        self.assertTrue(_hits("post-annotation-retired", code, raw))

    def test_unsafe_cast_needs_safety_comment(self):
        code = "    auto *e = reinterpret_cast<edict_t *>( p );"
        self.assertTrue(_hits("unsafe-cast-safety-comment", code, code))
        annotated = code + "  // SAFETY: ABI slot contract, layout pinned"
        self.assertFalse(_hits("unsafe-cast-safety-comment", code, annotated))

    def test_lifetime_annotation_member_flag_and_suppress(self):
        code = "    Filesystem *fs_ = nullptr;"
        self.assertTrue(_hits("lifetime-annotation", code, code))
        annotated = code + "  // @lifetime: engine"
        self.assertFalse(_hits("lifetime-annotation", code, annotated))
        exempt = code + "  // @annotation-exempt: cold-value-type"
        self.assertFalse(_hits("lifetime-annotation", code, exempt))

    def test_lifetime_annotation_ignores_statements(self):
        # Inline bodies live in headers: statement keywords are not members
        # (`return *this;` used to match the raw-pointer-member shape).
        for line in (
            "        return *this;",
            "        delete *it;",
            "        throw *err;",
        ):
            self.assertFalse(_hits("lifetime-annotation", line), line)

    def test_nodiscard_decl_rx_skips_explicit_ctor(self):
        # A constructor is not a discardable-return declaration.
        self.assertFalse(
            _DECL_RX.match("    explicit Atlas( int size ) noexcept;"))

    def test_nodiscard_initializer_heuristic(self):
        # Local variable init inside an inline header body — not a decl.
        self.assertTrue(
            _decl_args_are_bare_ids("    std::string result( s );"))
        # Real declarations: params carry types (or PascalCase type names).
        self.assertFalse(_decl_args_are_bare_ids(
            "int snprintf( char *buf, std::size_t size, const char *fmt, ... ) noexcept;"))
        self.assertFalse(_decl_args_are_bare_ids("int f();"))
        self.assertFalse(_decl_args_are_bare_ids("int f( Config );"))

    def test_prereserve_suppression_matches_raw(self):
        # Regression for the exclude-on-code bug: the @pre-reserved: marker
        # lives in a trailing comment, which the code view blanks — the
        # exclusion must therefore run on the RAW line or it never fires.
        code = "    std::vector<Slot> slots_;"
        raw = code + "  // @pre-reserved: XASH_LIMIT_NET_SLOTS"
        self.assertTrue(_hits("prereserve-annotation", code, code))
        self.assertFalse(_hits("prereserve-annotation", code, raw))


class OperatorDeletePairing(unittest.TestCase):
    @staticmethod
    def _lines(*raws: str):
        return [(i, raw, raw) for i, raw in enumerate(raws, start=1)]

    def test_single_overload_flagged(self):
        from xtools.checks import _operator_delete_pairing_issues
        issues = _operator_delete_pairing_issues(self._lines(
            "class File {",
            "    static void operator delete( void *p ) noexcept;",
            "};"))
        self.assertEqual(len(issues), 1)
        self.assertEqual(issues[0][1], 2)  # lineno of the first overload

    def test_both_overloads_pass(self):
        from xtools.checks import _operator_delete_pairing_issues
        issues = _operator_delete_pairing_issues(self._lines(
            "class File {",
            "    static void operator delete( void *p ) noexcept;",
            "    static void operator delete( void *p, std::size_t ) noexcept;",
            "};"))
        self.assertEqual(issues, [])

    def test_no_overloads_pass(self):
        from xtools.checks import _operator_delete_pairing_issues
        self.assertEqual(_operator_delete_pairing_issues(
            self._lines("class Plain {", "};")), [])


class MutatorDefRx(unittest.TestCase):
    """The broadened thread-assert definition matcher (QN wave)."""

    def _m(self, line: str) -> bool:
        from xtools.checks import _MUTATOR_DEF_RX
        return bool(_MUTATOR_DEF_RX.match(line))

    def test_void_method_still_matches(self):
        self.assertTrue(self._m("void CmdCvarContext::init( const P &p ) {"))

    def test_nonvoid_method_matches(self):
        self.assertTrue(self._m("bool Netchan::process( MessageBuf &b ) {"))

    def test_free_function_mutator_matches(self):
        self.assertTrue(self._m(
            "bool spawn_server( ServerRuntime &rt, const char *map ) noexcept {"))

    def test_indented_call_not_matched(self):
        self.assertFalse(self._m("        spawn_server( rt, map );"))

    def test_return_statement_not_matched(self):
        self.assertFalse(self._m("    return load( x );"))


if __name__ == "__main__":
    unittest.main()
