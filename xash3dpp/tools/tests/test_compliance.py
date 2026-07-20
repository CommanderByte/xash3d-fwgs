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

from xtools.checks import (_DECL_RX, _G_DEF_RX,  # noqa: E402
                           _UNIQUE_PTR_T_RX, _allow_context,
                           _decl_args_are_bare_ids, _filter_allows,
                           _initparams_flags, _stmt_start_idx, _violation)
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


class StmtStartIdx(unittest.TestCase):
    """`_stmt_start_idx` (T9a): a `// SAFETY:` comment above a MULTI-LINE
    statement must be reachable from a cast token on a continuation line —
    the statement-start's `_allow_context` carries the comment block."""

    @staticmethod
    def _lines(*raws: str):
        out = []
        for i, raw in enumerate(raws, start=1):
            code = "" if raw.lstrip().startswith("//") else raw
            out.append((i, code, raw))
        return out

    def test_at_statement_start_returns_idx(self):
        lines = self._lines(
            "    int y = 0;",
            "    foo( reinterpret_cast<char *>( p ) );")
        self.assertEqual(_stmt_start_idx(lines, 1), 1)

    def test_multiline_walks_to_statement_start(self):
        # codec_png shape: comment block, then a return statement whose cast
        # sits on the second physical line.
        lines = self._lines(
            "    // SAFETY: byte* -> uchar* object-representation read.",
            "    return static_cast<std::uint32_t>( mz_crc32(",
            "        MZ_CRC32_INIT, reinterpret_cast<const unsigned char *>( s.data() ), len ) );")
        start = _stmt_start_idx(lines, 2)
        self.assertEqual(start, 1)
        self.assertIn("SAFETY:", _allow_context(lines, start))

    def test_bounded_by_max_back(self):
        raws = ["    call("] + ["        arg%d," % i for i in range(8)] + \
               ["        reinterpret_cast<char *>( p ) );"]
        lines = self._lines(*raws)
        # 9 continuation lines back exceeds max_back=5: the walk stops early
        # and never reaches index 0.
        self.assertEqual(_stmt_start_idx(lines, len(lines) - 1), len(lines) - 1 - 5)

    def test_brace_and_semicolon_stop_the_walk(self):
        lines = self._lines(
            "    {",
            "    foo( reinterpret_cast<char *>( p )",
            "         );")
        self.assertEqual(_stmt_start_idx(lines, 2), 1)


class InitParamsFlags(unittest.TestCase):
    """`_initparams_flags` (T9b): raw-pointer members of transient DI param
    structs leave the @lifetime REQUIRED denominator."""

    @staticmethod
    def _lines(*raws: str):
        out = []
        for i, raw in enumerate(raws, start=1):
            code = "" if raw.lstrip().startswith("//") else raw
            out.append((i, code, raw))
        return out

    def test_named_and_bare_initparams_spans(self):
        lines = self._lines(
            "struct HostInitParams",
            "{",
            "    Filesystem *filesystem = nullptr;",
            "};",
            "class Host {",
            "    Filesystem *fs_;",
            "};")
        flags = _initparams_flags(lines)
        self.assertEqual(flags, [True, True, True, True, False, False, False])
        # bare `struct InitParams` (content's shape) matches too
        self.assertTrue(_initparams_flags(self._lines("struct InitParams {"))[0])

    def test_suffix_name_not_matched(self):
        # `InitParamsHelper` has no word boundary after InitParams.
        flags = _initparams_flags(self._lines(
            "struct InitParamsHelper {",
            "    Foo *f;",
            "};"))
        self.assertEqual(flags, [False, False, False])

    def test_span_closes_at_brace_semicolon(self):
        lines = self._lines(
            "struct AInitParams { Foo *f = nullptr; };",
            "    Bar *bar_member;")
        flags = _initparams_flags(lines)
        self.assertEqual(flags, [True, False])


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

    def test_prereserve_skips_function_declarations(self):
        # A declaration RETURNING a container is not a member needing
        # @pre-reserved: (the paren keeps it out).
        self.assertFalse(_hits(
            "prereserve-annotation",
            "[[nodiscard]] std::vector<std::string> list_directory( std::string_view path ) noexcept;"))
        self.assertTrue(_hits("prereserve-annotation",
                              "    std::vector<Slot> slots_;"))

    def test_g_def_rx_captures_global_name(self):
        # di-global-ref definition-allow propagation keys on this capture.
        m = _G_DEF_RX.match("JniState g_jni;")
        self.assertIsNotNone(m)
        self.assertEqual(m.group(1), "g_jni")
        m = _G_DEF_RX.match("std::atomic<LogCallback> g_log_callback{ nullptr };")
        self.assertIsNotNone(m)
        self.assertEqual(m.group(1), "g_log_callback")
        # Array globals are definitions too (6B S2 def-shape fix).
        m = _G_DEF_RX.match("AssetManagerHandle g_handles[2];")
        self.assertIsNotNone(m)
        self.assertEqual(m.group(1), "g_handles")
        self.assertTrue(_hits("mutable-global", "AssetManagerHandle g_handles[2];"))
        self.assertFalse(_hits("di-global-ref", "AssetManagerHandle g_handles[2];"))
        # Raw-pointer globals glue the * to the name (6B S9a def-shape fix):
        # `Foo *g_x = nullptr;` must still register as a definition.
        m = _G_DEF_RX.match("EngineBridge *g_bridge = nullptr;")
        self.assertIsNotNone(m)
        self.assertEqual(m.group(1), "g_bridge")
        self.assertTrue(_hits("mutable-global", "EngineBridge *g_bridge = nullptr;"))
        self.assertFalse(_hits("di-global-ref", "EngineBridge *g_bridge = nullptr;"))
        # The star-spaced form still works too.
        self.assertIsNotNone(_G_DEF_RX.match("EngineBridge * g_bridge = nullptr;"))
        # A use is not a definition.
        self.assertIsNone(_G_DEF_RX.match("    g_jni.env = env;"))

    def test_unique_ptr_inner_type_capture(self):
        # The pool-owned suppression keys on this capture.
        for line, want in (
            ("    std::unique_ptr<File> f;", "File"),
            ("std::unique_ptr<ISearchBackend> create_wad(...);", "ISearchBackend"),
            ("    std::unique_ptr< xash::filesystem::File > f;",
             "xash::filesystem::File"),
            ("std::unique_ptr<class Foo> p;", "Foo"),
        ):
            m = _UNIQUE_PTR_T_RX.search(line)
            self.assertIsNotNone(m, line)
            self.assertEqual(m.group(1), want, line)

    def test_collect_pool_owned_types(self):
        # A class declaring operator delete is pool-owned; a plain one is not;
        # a forward declaration opens no body.
        import tempfile
        from pathlib import Path
        from xtools.checks import _collect_pool_owned_types
        with tempfile.TemporaryDirectory() as d:
            hpp = Path(d) / "sample.hpp"
            hpp.write_text(
                "class Owned {\n"
                "public:\n"
                "    static void operator delete(void* p) noexcept;\n"
                "};\n"
                "class Plain {\n"
                "    int x;\n"
                "};\n"
                "class Fwd;\n", encoding="utf-8")
            owned = _collect_pool_owned_types([hpp])
        self.assertIn("Owned", owned)
        self.assertNotIn("Plain", owned)
        self.assertNotIn("Fwd", owned)

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


class EntvarsConfinement(unittest.TestCase):
    """Q-20 promised "a compliance-scan rule (no `->v.` outside the allowed
    set) added when the server scaffold lands"; the scaffold landed and the
    rule did not, while server-boundary.md asserted it "is the guard".  The
    confinement did in fact hold by convention, so writing the rule made the
    doc claim true rather than weakening it."""

    def test_raw_entvars_access_is_flagged(self):
        for line in ("    ed->v.flags |= k_fl_killme;",
                     "    if ( pent->v.solid != 0 )",
                     "    ent -> v . nextthink = 0.0f;"):
            with self.subTest(line=line):
                self.assertTrue(_hits("entvars-confinement", line))

    def test_entityview_access_is_not_flagged(self):
        for line in ("    EntityView v( ed );",
                     "    if ( v.freed() ) return;",
                     "    rt.move_env.world = &world;"):
            with self.subTest(line=line):
                self.assertFalse(_hits("entvars-confinement", line))

    def test_owner_paths_are_excluded_on_both_separators(self):
        """str(path) is backslashed on Windows and slashed elsewhere, so the
        exclusion must accept both -- with `[\\/]` (only a slash) it silently
        excluded nothing and the rule reported 254 false violations."""
        rx = re.compile(_rule("entvars-confinement").exclude_path_re)
        for p in (r"C:\git\x\xash3dpp\src\server\game\edict_arena.cpp",
                  "/home/x/xash3dpp/src/server/game/edict_arena.cpp",
                  r"C:\x\xash3dpp\src\save\level_state_writer.cpp",
                  "/x/xash3dpp/src/server/physics/pmove.cpp",
                  r"C:\x\xash3dpp\src\server\lifecycle\save_bridge.cpp"):
            with self.subTest(path=p):
                self.assertTrue(rx.search(p))

    def test_non_owner_server_paths_are_not_excluded(self):
        rx = re.compile(_rule("entvars-confinement").exclude_path_re)
        for p in ("/x/xash3dpp/src/server/clients/client_state.cpp",
                  "/x/xash3dpp/src/server/world/links.cpp"):
            with self.subTest(path=p):
                self.assertIsNone(rx.search(p))


class RoleMarkerToken(unittest.TestCase):
    """Q-25: a `// ROLE: <value>` banner must use a defined role value; an
    unknown token trips the misuse guard, a defined one does not."""

    def test_defined_tokens_pass(self):
        for tok in ("shared-deterministic", "shared-format",
                    "server-authoritative", "client-only", "role-neutral",
                    "offline-tool"):
            with self.subTest(tok=tok):
                self.assertFalse(
                    _hits("role-marker-token", "",
                          raw="// ROLE: %s — rationale" % tok))

    def test_unknown_token_is_flagged(self):
        for bad in ("// ROLE: bogus", "// ROLE: shared", "// ROLE: server"):
            with self.subTest(line=bad):
                self.assertTrue(_hits("role-marker-token", "", raw=bad))


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

    def test_suffixed_bare_verbs_match(self):
        """2026-07-20 audit: the 16 bare verbs anchored straight to `(`, so a
        suffixed mutator could never match.  55 definition sites across 51
        names were invisible — including register_thread_role itself."""
        for line in (
                "void MapLoader::clear_world() noexcept {",
                "bool NetworkContext::send_packet( const Packet &p ) {",
                "void transmit_client( ServerRuntime &rt, int i ) noexcept {",
                "bool connect_client( ServerRuntime &rt, const Addr &a ) {",
                "void register_thread_role( ThreadRole role ) noexcept {",
                "void Filesystem::add_game_directory( const char *d ) {",
                "void unregister_save_commands( ServerRuntime &rt ) noexcept {",
        ):
            with self.subTest(line=line):
                self.assertTrue(self._m(line))

    def _d(self, line: str) -> bool:
        from xtools.checks import _is_mutator_def
        return _is_mutator_def(line)

    def test_declarations_and_statements_are_not_definitions(self):
        """Broadening the verb set surfaced two artifact classes that are not
        function definitions at all.  Silencing them with compliance-allow in
        source would be annotating around a scanner bug."""
        for line in (
                "    virtual void set_current_map( std::string_view n ) noexcept = 0;",
                "        : clear_trace();",
                "    LevelStateLoader loader( *buf_, *table_ );",
                "    sv_save::LevelStateLoader loader( *lbuf, table );",
                "void register_late( ServerRuntime &rt ) noexcept;",
        ):
            with self.subTest(line=line):
                self.assertFalse(self._d(line))

    def test_const_member_function_is_not_a_mutator(self):
        """A const-qualified member cannot mutate however its name reads."""
        self.assertFalse(self._d(
            "double Netchan::connect_time() const noexcept { return t_; }"))

    def test_const_parameter_does_not_exempt_a_definition(self):
        """The `\\)` anchor keeps const PARAMETERS in scope."""
        self.assertTrue(self._d(
            "void Filesystem::add_game_directory( const char *dir ) {"))

    def test_reset_wildcard_matches_legacy_quirk_name(self):
        """`reset` is the one verb promoted to `reset\\w*` rather than
        `reset(?:_\\w+)?`: both of its wildcard-form matches in the tree are
        genuine mutators (legacy Quirk 3 resetkeys)."""
        self.assertTrue(self._m("void KeyTable::resetkeys() noexcept {"))

    def test_verb_prefixed_non_mutators_not_matched(self):
        """Why `(?:_\\w+)?` and not `\\w*`: measured over src/**/*.cpp, `\\w*`
        surfaces these 6 extra names and every one is a false positive."""
        for line in (
                "int sendto( int s, const void *b, int n ) noexcept {",
                "bool Server::initialized() const noexcept {",
                "bool Sound::initialized() const noexcept {",
                "double Clock::starttime() const noexcept {",
                "const char *addr_string( const netadr_t &a ) noexcept {",
                "bool sends_qport() const noexcept {",
        ):
            with self.subTest(line=line):
                self.assertFalse(self._m(line))


if __name__ == "__main__":
    unittest.main()
