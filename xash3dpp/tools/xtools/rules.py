"""Data-only ruleset for compliance_scan.

Each rule mirrors a mechanical ([M]) check from the reviewer charter
(.github/agents/xash3dpp-reviewer.agent.md), pre-pr Phase 2, sweep-module
Step 2, or detail-audit CHECK-* — the `source_ref` names the origin so the
rule text can be traced. `candidate=True` marks heuristics whose hits need
agent judgment (possible false positives); they are reported with
`severity` prefixed "candidate-".

Scopes: "src", "include", "tests". `exclude_subsystems` skips whole
subsystems (documented exceptions); `exclude_path_re` skips files.

Matching surfaces: `pattern` runs against the comment-stripped code line
(or the RAW line when `match_raw=True` — for rules about comment content,
e.g. retired annotations).  `exclude_line_re` always runs against the RAW
line, because suppression markers (`@pre-reserved:`, `@lifetime:`,
`// SAFETY:`) live in comments.
"""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class Rule:
    check: str
    severity: str  # blocker | warning | note
    pattern: str  # regex applied to comment-stripped code (raw if match_raw)
    scopes: tuple[str, ...]
    hint: str
    source_ref: str
    exclude_subsystems: tuple[str, ...] = ()
    exclude_path_re: str = ""
    exclude_line_re: str = ""  # hit suppressed if this matches the RAW line
    candidate: bool = False
    sets: tuple[str, ...] = ("all",)  # membership: all / prepr / detail
    match_raw: bool = False  # pattern runs on the raw line (comment rules)


RULES: list[Rule] = [
    Rule(
        check="abi-alloc",
        severity="blocker",
        pattern=r"\b(malloc|calloc|realloc|free)\s*\(",
        scopes=("src", "include"),
        exclude_subsystems=("memory",),
        hint="use memory::mem_alloc / mem_free / pool_new (reviewer §6)",
        source_ref="reviewer §6; pre-pr Phase 2; sweep Forbidden patterns",
        sets=("all", "prepr", "detail"),
    ),
    Rule(
        check="raw-new-delete",
        severity="blocker",
        pattern=r"(^|[^_\w.:])new\s+[A-Za-z_:][\w:<>]*\s*[({]|(^|[^_\w])delete\s+[\w(]|(^|[^_\w])delete\[\]",
        scopes=("src", "include"),
        exclude_line_re=r"make_unique",
        exclude_subsystems=("memory",),
        hint="only std::make_unique<Impl>() is allowed; use pool_new/mem_free (reviewer §6)",
        source_ref="reviewer §6; sweep OWNERSHIP; detail CHECK-MEMORY",
        candidate=True,
        sets=("all", "prepr", "detail"),
    ),
    Rule(
        check="abi-file-io",
        severity="blocker",
        pattern=r"\b(fopen|fclose|fread|fwrite|CreateFileA|CreateFileW|CreateFile)\s*\(|\bFILE\s*\*",
        scopes=("src", "include"),
        exclude_subsystems=("platform", "filesystem"),
        hint="use the IFilesystem interface (reviewer §6)",
        source_ref="reviewer §6; sweep Forbidden patterns",
        sets=("all", "detail"),
    ),
    Rule(
        check="externc-stringview",
        severity="blocker",
        pattern=r"std::string_view",
        scopes=("src", "include"),
        hint="string_view must not cross an extern-C/DLL boundary — verify this file's extern-C exports (reviewer §11)",
        source_ref="reviewer §11; pre-pr Phase 2",
        candidate=True,  # only reported for files that also contain extern "C"
        sets=("all", "prepr"),
    ),
    Rule(
        check="exceptions-rtti",
        severity="blocker",
        pattern=r"\bthrow\s|\btry\s*\{|\bcatch\s*\(|\bdynamic_cast\s*<|\btypeid\s*\(",
        scopes=("src", "include"),
        hint="no exceptions/RTTI in xash3dpp (reviewer §3)",
        source_ref="reviewer §3",
        sets=("all", "prepr", "detail"),
    ),
    Rule(
        check="printf-diagnostics",
        severity="warning",
        pattern=r"\b(printf|fprintf|fputs)\s*\(|std::cout|OutputDebugString",
        scopes=("src",),
        exclude_subsystems=("platform",),
        hint="use platform::log / core::log (reviewer §6/§13; sweep LOGGING)",
        source_ref="reviewer §6/§13; pre-pr Phase 2; sweep LOGGING",
        sets=("all", "prepr", "detail"),
    ),
    Rule(
        check="raw-cstring",
        severity="warning",
        # Only UNqualified/global C calls: a preceding ':' '.' or '>' means the
        # call is namespace-qualified (std::/ut::/utilities::) or a member
        # (obj.strcmp / p->strcpy) and is a deliberate, allowed choice.
        pattern=r"(?<![:.>])\b(strlen|strcpy|strcmp|strcat|sprintf)\s*\(",
        scopes=("src",),
        # utilities is the definitional home of these wrappers (xash::utilities::
        # strcmp et al.) — like memory owns the allocators.
        exclude_subsystems=("utilities",),
        hint="use utilities:: equivalents for bare C string funcs (qualified std::/ut:: calls are fine) (reviewer §6)",
        source_ref="reviewer §6; sweep Forbidden patterns",
        sets=("all", "detail"),
    ),
    Rule(
        check="include-cstring-cstdio",
        severity="note",
        # The C headers are the smell; the C++ wrappers <cstring>/<cstdio> are
        # legitimately needed for std::memcpy/std::snprintf and are not flagged.
        pattern=r"#\s*include\s*<(string\.h|stdio\.h)>",
        scopes=("src", "include"),
        hint="prefer the C++ <cstring>/<cstdio> headers with std:: qualification (sweep)",
        source_ref="sweep Forbidden patterns",
        sets=("all", "detail"),
    ),
    Rule(
        check="os-socket",
        severity="blocker",
        pattern=r"::\s*(socket|bind|sendto|recvfrom)\s*\(|\b(WSAStartup|getaddrinfo)\s*\(",
        scopes=("src",),
        exclude_path_re=r"src[\\/]platform[\\/].*os_socket[^\\/]*\.cpp$",
        hint="all socket I/O confined to IPlatformSockets (reviewer §10; sockets.md)",
        source_ref="reviewer §10; pre-pr Phase 2; finish-subsystem §9",
        sets=("all", "prepr", "detail"),
    ),
    Rule(
        check="compat-global",
        severity="warning",
        pattern=r"IEngineCompatPolicy|GlobalCompatPolicy|unified_compat|\bCompatFlags\b",
        scopes=("src", "include"),
        hint="compat is per-subsystem per Q-12 (reviewer §10)",
        source_ref="reviewer §10; pre-pr Phase 2; detail CHECK-COMPAT",
        sets=("all", "prepr", "detail"),
    ),
    Rule(
        check="compat-ifdef",
        severity="warning",
        pattern=r"#\s*if(def)?\b.*(GOLDSRC_COMPAT|_COMPAT\b)",
        scopes=("src",),
        exclude_path_re=r"compat_[^\\/]*\.cpp$",
        hint="compat variants are link-time-selected compat_*.cpp files (detail CHECK-COMPAT)",
        source_ref="detail CHECK-COMPAT; instructions §Compat isolation",
        sets=("all", "detail"),
    ),
    Rule(
        check="assert-cassert",
        severity="warning",
        pattern=r"#\s*include\s*<cassert>|(^|[^_\w.])assert\s*\(",
        scopes=("src", "include"),
        hint="use XASH_ASSERT / XASH_FATAL from platform/assert.hpp (reviewer §13)",
        source_ref="reviewer §13; sweep ASSERTIONS",
        sets=("all", "detail"),
    ),
    Rule(
        check="future-promise",
        severity="warning",
        pattern=r"std::(future|promise)\b",
        scopes=("src", "include"),
        hint="use JobToken<T> for job completion (reviewer §7)",
        source_ref="reviewer §7",
        sets=("all",),
    ),
    Rule(
        check="int-width",
        severity="warning",
        pattern=r"\bunsigned\b(?!\s+(char|short|long))",
        scopes=("src", "include"),
        hint="use a width-qualified type (uint32_t, std::size_t, ...) (reviewer §13)",
        source_ref="reviewer §13; decisions-style INT_TYPES (QG)",
        candidate=True,
        sets=("all",),
    ),
    Rule(
        check="unique-ptr-nonpimpl",
        severity="warning",
        pattern=r"std::unique_ptr\s*<\s*(?!Impl\b)",
        scopes=("include",),
        # pool_ptr<T> (unique_ptr<T, PoolDeleter>) IS the sanctioned ownership
        # vocabulary — not a finding.
        exclude_line_re=r"PoolDeleter",
        hint="unique_ptr<T> is for pimpl Impl OR a pool-owned class (T carries "
             "the operator-delete pair and is built via a pool_new factory) — "
             "verify T qualifies (Q-22)",
        source_ref="reviewer §11; sweep OWNERSHIP; Q-22 LIFECYCLE_MODEL",
        candidate=True,
        sets=("all", "detail"),
    ),
    Rule(
        check="class-operator-new",
        severity="blocker",
        pattern=r"\boperator\s+new\b",
        scopes=("src", "include"),
        exclude_subsystems=("memory",),
        hint="class-scoped operator new is forbidden — pool-owned classes use "
             "the create_<thing> factory + operator delete idiom (Q-22)",
        source_ref="Q-22 LIFECYCLE_MODEL; reviewer §14; detail CHECK-LIFECYCLE",
        sets=("all", "prepr", "detail"),
    ),
    Rule(
        check="make-unique-outside-pimpl",
        severity="warning",
        pattern=r"std::make_unique\s*<\s*(?!Impl\b)",
        scopes=("src",),
        exclude_subsystems=("memory",),
        hint="make_unique is for pimpl Impl only; pool-owned objects are built "
             "by a create_<thing> factory via pool_new (Q-22)",
        source_ref="Q-22 LIFECYCLE_MODEL; detail CHECK-LIFECYCLE",
        candidate=True,
        sets=("all", "detail"),
    ),
    Rule(
        check="post-annotation-retired",
        severity="note",
        pattern=r"//\s*Post:",
        scopes=("src", "include"),
        hint="// Post: is retired (QN) — postconditions live in return types, "
             "[[nodiscard]], and asserts",
        source_ref="QN ANNOTATION_DISCIPLINE; detail CHECK-ANNOTATIONS",
        match_raw=True,
        sets=("all", "detail"),
    ),
    Rule(
        check="unsafe-cast-safety-comment",
        severity="warning",
        pattern=r"\breinterpret_cast\s*<",
        scopes=("src",),
        exclude_line_re=r"SAFETY:",
        hint="reinterpret_cast needs a // SAFETY: comment naming the invariant "
             "(QN; Q-16 pattern) — ABI-bridge puns included",
        source_ref="QN ANNOTATION_DISCIPLINE; detail CHECK-ANNOTATIONS",
        candidate=True,
        sets=("detail",),
    ),
    Rule(
        check="lifetime-annotation",
        severity="warning",
        # Raw-pointer / reference / stored-view member declarations in headers.
        pattern=r"^\s*[A-Za-z_][\w:<>,\s]*(?:[*&]\s*|std::(?:span|string_view)\s*<[^;]*>\s+)\w+_?\s*(=\s*[\w:]+)?;\s*$",
        scopes=("include",),
        exclude_line_re=r"@lifetime:|@annotation-exempt:",
        hint="raw pointer/reference/view members need '// @lifetime: <owner>' "
             "or an @annotation-exempt marker (QN, Q-9)",
        source_ref="QN ANNOTATION_DISCIPLINE; sweep OWNERSHIP; detail CHECK-ANNOTATIONS",
        candidate=True,
        sets=("detail",),
    ),
    Rule(
        check="pimpl-default-header",
        severity="warning",
        pattern=r"(~\w+\s*\(\s*\)|\w+\s*&&\s*\w*\s*\)\s*noexcept)\s*=\s*default",
        scopes=("include",),
        # `virtual ~IFoo() = default;` is the correct idiom for an abstract
        # interface, not a pimpl type — a pimpl dtor is non-virtual and
        # declared-only in the header.  The move-ctor half still catches real
        # pimpl violations (interfaces don't default a move ctor).
        exclude_line_re=r"\bvirtual\b",
        hint="pimpl dtor/move must be '= default' in the .cpp, declared-only in the header (sweep PIMPL_MOVE, Q-3)",
        source_ref="sweep PIMPL_MOVE (Q-3)",
        candidate=True,
        sets=("all", "detail"),
    ),
    Rule(
        check="mutable-global",
        severity="blocker",
        pattern=r"^(static\s+)?(?!(static\s+)?(const|constexpr|inline\s+constexpr))\w[\w:<>*&\s]*\s+g_\w+\s*(=|;|\{)",
        scopes=("src",),
        exclude_subsystems=("memory",),
        hint="no new mutable file-scope globals; state lives in context objects (reviewer §7; sweep DI_PARAMS)",
        source_ref="reviewer §7; sweep DI_PARAMS (no-globals)",
        candidate=True,
        sets=("all", "detail"),
    ),
    Rule(
        check="di-global-ref",
        severity="warning",
        pattern=r"(?<![\w.])g_[a-z]\w+",
        scopes=("src",),
        exclude_subsystems=("memory",),
        exclude_line_re=r"^(static\s+)?[\w:<>*&\s]+\sg_\w+\s*(=|;|\{)",
        hint="dependencies flow through InitParams, not process globals (detail CHECK-DI, Q-4)",
        source_ref="detail CHECK-DI (Q-4)",
        candidate=True,
        sets=("detail",),
    ),
    Rule(
        check="prereserve-annotation",
        severity="warning",
        pattern=r"std::(vector|deque)\s*<[^;]*;\s*$",
        scopes=("include",),
        exclude_line_re=r"@pre-reserved:",
        hint="hot-path container members need '// @pre-reserved: <LIMIT>' + .reserve() in init (Q-13); cold/warm-path exempt — judge",
        source_ref="sweep ALLOC_POLICY (Q-13); detail CHECK-MEMORY",
        candidate=True,
        sets=("all", "detail"),
    ),
]

# Checks that exist as structured scanners in checks.py rather than plain
# line regexes (they still belong to the [M] set):
STRUCTURED_CHECKS = [
    "hpp-under-src",  # file placement (reviewer §3; detail CHECK-HEADERS)
    "naming-file-case",  # snake_case filenames (reviewer §12)
    "naming-enum-kprefix",  # enum-block scan (reviewer §12; sweep NAMING_ENUM)
    "nodiscard-missing",  # header decl heuristic (reviewer §13; sweep NODISCARD)
    "thread-assert",  # mutator lookahead (reviewer §7; pre-pr Phase 2; sweep TH-Role)
    "ns-qualify",  # sibling-namespace qualification (sweep NS_QUALIFY)
    "test-macros",  # test_helpers.hpp usage (sweep TEST_MACROS)
    "operator-delete-pairing",  # both operator delete overloads (Q-22; detail CHECK-LIFECYCLE)
]

# [J] areas compliance_scan deliberately does NOT cover — reported back so
# the consuming prompt/reviewer knows what still needs human/LLM judgment.
JUDGMENT_CHECKS = [
    "stats tiering & gating (reviewer §5; detail CHECK-STATS)",
    "DI completeness of InitParams fields (reviewer §8; detail CHECK-DI)",
    "ownership annotations & span/lifetime judgment (reviewer §11)",
    "error-return logging at public boundary (reviewer §9; sweep ERROR_RETURN)",
    "Q-14 driver-inheritance / direction-asymmetry (reviewer §10)",
    "TH-Const const-correctness of query paths (sweep TH-Const)",
    "header public/private placement judgment (detail CHECK-HEADERS)",
    "ARRAY_SIZE_STACK size computation (sweep QL)",
    "Q-11 satellite scoring (reviewer §10; finish-subsystem §9)",
]

MUTATOR_NAMES = (
    "init|shutdown|reset|flush|clear|add|remove|register|unregister|set_\\w+|update_\\w+"
    # QN wave (2026-07-06): the mutating verbs the 6B retrofit measures —
    # candidate-judged, so const-path or leaf-helper matches stay judgeable.
    "|send|transmit|write_\\w+|spawn\\w*|activate\\w*|deactivate\\w*|run_\\w+"
    "|process|connect|disconnect|drop_\\w+|load\\w*|unload\\w*|start|stop"
)
