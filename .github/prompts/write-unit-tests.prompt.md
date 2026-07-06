---
name: "Write xash3dpp unit tests"
description: "Write unit tests for a xash3dpp utility module. Uses legacy C behaviour as the behavioural baseline. Skips quirks that are pure C-architecture artifacts with no engine-observable effect."
argument-hint: "Module to test, e.g. 'utf', 'matrix', 'path'"
agent: agent
tools: [read, search, edit, execute, todo, xash-tools/*]
model: claude-sonnet-4-6
---

Write unit tests for the xash3dpp utility module: **$ARGUMENTS**

## Workflow

1. **Read the new header and implementation** under `xash3dpp/include/xash3dpp/utilities/` and `xash3dpp/src/utilities/`.
2. **Read the legacy reference** listed in the implementation file's top comment (usually `public/crtlib.c`, `public/utflib.c`, `public/xash3d_mathlib.c`, etc.).
3. **Read the existing boundary doc** in `xash3dpp/docs/boundaries/` if one exists for this module.
4. **Read existing test files** in `xash3dpp/tests/utilities/` to match the established style.

## Baseline rule

Use the legacy C implementation as the behavioural truth for any observable engine contract:
- Numeric parsing results (`Q_atoi`, `Q_atof`, hex prefixes, character literals, whitespace handling)
- String comparison semantics (`Q_stricmp` locale-independence)
- Vector/matrix math output values
- Tokeniser output sequence for known inputs
- UTF decode output codepoints

**Skip** writing tests for behaviours that were purely C-architecture artifacts with no engine-observable effect. Do NOT test:
- That a function returns 0 for a null pointer when the C++ overload no longer accepts null (the null forwarder is already tested)
- Internal allocation patterns or struct layout
- Exact pointer arithmetic steps (only the output matters)
- Whitespace/comment-stripping internals that are invisible to callers

When in doubt about whether a behaviour is an engine contract or an artifact, write the test and add a brief comment explaining which legacy function it mirrors.

## Test style (match existing files exactly)

Construct pool-owned objects the way production does — through their
`create_<thing>` factories (Q-22), never by reimplementing allocation in the
test. Register `ThreadRole::Main` in `main()` so thread asserts stay
meaningful.

```cpp
// xash3dpp — <module> tests
// Covers: <comma-separated list of functions>

#include <xash3dpp/utilities/<module>.hpp>
#include <cstring>  // add others as needed: <cstdint>, <limits>, etc.

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

static void test_<function>()
{
    // legacy: <Q_FunctionName> in <legacy_file>
    CHECK( xash::utilities::<function>( ... ) == expected );
    // edge cases, boundary values, quirk cases with comments
}

int main()
{
    test_<function>();
    // ...
    std::printf( "<module>: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
```

Rules:
- Use `#include "../test_helpers.hpp"` for `CHECK`, `CHECK_EQ`, `CHECK_NE`, `CHECK_STREQ`, `REQUIRE`. No local `#define CHECK`.
- No third-party test framework — only `<cstring>`, `<cstdint>`, `<limits>`, standard library (no `<cassert>`, no `<cstdio>` — test_helpers.hpp covers output)
- One `static void test_<function>()` per public function
- Each `CHECK` line tests exactly one thing; add a `// comment` for non-obvious cases
- Cover: typical inputs, boundary values, empty/zero inputs, negative numbers, any documented quirks that are engine contracts
- Use `xash::utilities::` (or the correct sub-namespace) on every call — no `using namespace`
- `printf` prefix must include the module name: `"<module>: %d passed, %d failed\n"`

## Pitfalls

**Floating-point comparisons** — define a local `near()` helper for any module involving math:
```cpp
static bool near( float a, float b, float tol = 1e-4f ) noexcept
{
    const float d = a - b;
    return ( d < 0.0f ? -d : d ) <= tol;
}
```

**Signed `char` vs `uint8_t`** — functions that return or write `char` buffers can hold values ≥ 0x80.  Comparing a `char` directly against `0xC2u` will silently fail on platforms where `char` is signed.  Cast via `static_cast<uint8_t>()` or a local helper before comparison:
```cpp
static constexpr uint8_t u8( char c ) noexcept { return static_cast<uint8_t>( c ); }
// ...
CHECK( u8( buf[0] ) == 0xC2u );
```

**Unimplemented declarations** — before writing a test for a declared function, verify it has a definition (grep for its name in `src/`).  If no definition exists, note it with a comment and skip the test; do not call an undefined function.

**Updating existing test files** — preserve all existing `test_*` functions unchanged.  Add new ones above `int main()`, then add their calls inside `main()`.  Update the `Covers:` comment at the top.

## Output

After writing the test file:
1. Add the new file to `xash3dpp/tests/utilities/CMakeLists.txt` (append to the `add_executable` sources list).
2. Run `get_errors` on the new file and fix any compile errors before finishing.
3. Commit once all tests pass:

```
git add xash3dpp/tests/utilities/
$coauthor = & .venv\Scripts\python.exe xash3dpp\tools\agent_workflow.py coauthor <framework> "<model>"
git commit -m "utilities: add unit tests for $ARGUMENTS" -m "$coauthor"
```

Commit message bullets: one bullet per test scenario added.
End with the `Co-Authored-By` trailer naming the active framework/model.
