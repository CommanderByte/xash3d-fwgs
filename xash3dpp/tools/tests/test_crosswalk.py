"""Regression tests for the legacy<->xash3dpp crosswalk parsers/matchers.

Stdlib `unittest` only, filesystem-free — pure functions over synthetic strings
and (lineno, code, raw) rows.  `_rows` mimics scan.code_lines' line-comment
blanking so _next_def's banner/comment skipping is exercised realistically.

Run:
    .venv\\Scripts\\python.exe -m unittest discover -s xash3dpp/tools/tests
"""

from __future__ import annotations

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from xtools.crosswalk import (  # noqa: E402
    _DEEP_DIVE, _STYLE_A, _STYLE_D, _STYLE_E, _entry, _index_rows,
    _looks_legacy, _match_fileline, _match_symbol, _missing, _next_def,
    _parse_header)


def _rows(*lines: str):
    """(lineno, code_only, raw) with // comments blanked out of code_only, as
    scan.code_lines yields."""
    return [(i + 1, re.sub(r"//.*$", "", ln), ln) for i, ln in enumerate(lines)]


class Grammars(unittest.TestCase):
    def test_style_a(self):
        m = _STYLE_A.search(
            "// SV_PlaybackEventFull (sv_game.c:4026): producer")
        self.assertEqual((m.group(1), m.group(2), m.group(3)),
                         ("SV_PlaybackEventFull", "sv_game.c", "4026"))

    def test_style_a_range(self):
        m = _STYLE_A.search("// SV_AngleMod (sv_game.c:122-156).")
        self.assertEqual((m.group(3), m.group(4)), ("122", "156"))

    def test_style_d_symbol(self):
        self.assertEqual(
            _STYLE_D.search("// Legacy Delta_ParseField").group(1),
            "Delta_ParseField")

    def test_style_d_rejects_reference_header(self):
        self.assertIsNone(_STYLE_D.search("// Legacy reference: engine/foo.c"))

    def test_deep_dive(self):
        hit = _DEEP_DIVE.findall("maps SV_FreeEdict (sv_game.c:1004) then")[0]
        self.assertEqual(hit[:3], ("SV_FreeEdict", "sv_game.c", "1004"))


class LooksLegacy(unittest.TestCase):
    def test_prefixed_kept(self):
        for s in ("SV_Foo", "Mod_LoadWorld", "Delta_ParseField", "pfnFoo"):
            self.assertTrue(_looks_legacy(s), s)

    def test_constants_and_prose_dropped(self):
        for s in ("MAX_CLIENTS", "FATPHS_RADIUS", "behavior"):
            self.assertFalse(_looks_legacy(s), s)


class NextDef(unittest.TestCase):
    def test_clean_def_high(self):
        rows = _rows("// SV_Foo (x.c:1)", "void bar_baz() {")
        self.assertEqual(_next_def(rows, 0), ("bar_baz", 2, "high"))

    def test_multiline_decl_resolves_symbol(self):
        rows = _rows("// SV_Foo (x.c:1)", "int sv_multicast( Bridge &b, int d,")
        sym, _line, conf = _next_def(rows, 0)
        self.assertEqual(sym, "sv_multicast")
        self.assertIn(conf, ("high", "symbol"))

    def test_bare_call_is_citation(self):
        rows = _rows("// SV_Foo (x.c:1)", "    do_thing();")
        self.assertEqual(_next_def(rows, 0), (None, None, "ref"))

    def test_return_statement_is_citation(self):
        rows = _rows("// SV_Foo (x.c:1)", "    return compute();")
        self.assertEqual(_next_def(rows, 0)[2], "ref")

    def test_skips_banner_and_blank(self):
        rows = _rows("// SV_Foo (x.c:1)", "// -----------", "",
                     "bool test_it() {")
        self.assertEqual(_next_def(rows, 0)[0], "test_it")


class StyleE(unittest.TestCase):
    """The bare-symbol doc/section-divider form: `// SV_Foo` above a def."""

    def test_bare_symbol_above_def_is_port(self):
        rows = _rows("// SV_LinkEdict", "void WorldLinks::link_edict( E &e,")
        entries = _index_rows(rows, "links.cpp")
        self.assertEqual(len(entries), 1)
        e = entries[0]
        self.assertEqual(e["legacy_symbol"], "SV_LinkEdict")
        self.assertEqual(e["cpp_symbol"], "WorldLinks::link_edict")
        self.assertEqual(e["source"], "code-symbol")
        self.assertEqual(e["confidence"], "symbol")
        self.assertTrue(e["ported"])

    def test_section_divider_block(self):
        # the real form: rule / symbol / rule / blank / definition.
        rows = _rows("// -----------", "// SV_LinkEdict", "// -----------", "",
                     "void link_edict() {")
        syms = [e["legacy_symbol"] for e in _index_rows(rows, "x.cpp")]
        self.assertEqual(syms, ["SV_LinkEdict"])  # dividers don't add entries

    def test_leading_symbol_above_statement_is_not_indexed(self):
        # a citation (symbol opens the comment, but a statement follows) stays
        # out of the index — the signature-gate is the noise filter.
        rows = _rows("// SV_Foo returns false here", "    return x;")
        self.assertEqual(_index_rows(rows, "x.cpp"), [])

    def test_style_a_not_shadowed_by_e(self):
        # a symbol WITH a file:line still resolves as Style A (high), not E.
        rows = _rows("// SV_Multicast (sv_game.c:1): fan out", "int sv_mc() {")
        entries = _index_rows(rows, "x.cpp")
        self.assertEqual(len(entries), 1)
        self.assertEqual(entries[0]["source"], "code-styleA")
        self.assertEqual(entries[0]["confidence"], "high")

    def test_prose_first_word_dropped(self):
        # a non-legacy leading word never fires E even above a def.
        rows = _rows("// helper that clips the move", "void clip_move() {")
        self.assertEqual(_index_rows(rows, "x.cpp"), [])
        # and the regex itself keeps only the first comment token.
        self.assertEqual(_STYLE_E.search("  // SV_Foo bar").group(1), "SV_Foo")


class Header(unittest.TestCase):
    def test_files_and_symbols(self):
        entries = _parse_header(
            "public/crtlib.c (COM_FileBase, COM_FileExtension)", "p.cpp", 2)
        files = [e["legacy_file"] for e in entries if e["legacy_symbol"] is None]
        syms = [e["legacy_symbol"] for e in entries if e["legacy_symbol"]]
        self.assertIn("public/crtlib.c", files)
        self.assertIn("COM_FileBase", syms)
        self.assertNotIn("public", syms)  # prose words filtered


class Matching(unittest.TestCase):
    def _e(self, sym, lf, ln, conf, source="code-styleA", cpp="foo"):
        return _entry(sym, lf, ln, "a.cpp", cpp, 1, source, conf)

    def test_symbol_exact_beats_substring(self):
        entries = [self._e("SV_Move", "x.c", 1, "high"),
                   self._e("SV_MoveBounds", "x.c", 2, "high")]
        got = _match_symbol(entries, "SV_Move")
        self.assertEqual([e["legacy_symbol"] for e in got], ["SV_Move"])

    def test_fileline_precise_beats_filelevel(self):
        precise = self._e("SV_Foo", "sv_game.c", 4026, "high")
        filelvl = _entry(None, "sv_game.c", None, "b.cpp", "<file scope>", 1,
                         "code-header", "file")
        self.assertEqual(
            _match_fileline([filelvl, precise], "sv_game.c", 4026), [precise])

    def test_missing_excludes_referenced(self):
        entries = [
            self._e("SV_Ported", "x.c", 1, "high"),
            _entry("SV_Ported", "x.c", 1, "d.md", None, 1, "deep-dive", "doc"),
            _entry("SV_Orphan", "x.c", 9, "d.md", None, 9, "deep-dive", "doc"),
        ]
        names = [e["legacy_symbol"] for e in _missing(entries)["missing"]]
        self.assertIn("SV_Orphan", names)
        self.assertNotIn("SV_Ported", names)


if __name__ == "__main__":
    unittest.main()
