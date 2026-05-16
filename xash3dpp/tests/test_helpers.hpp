#pragma once
// xash3dpp — standard test helper macros (TEST_MACROS / QK, decisions-style.md)
//
// Usage:
//   #include "../../test_helpers.hpp"   // (adjust depth as needed)
//
// Macros:
//   CHECK(expr)          — non-fatal; increments g_fail and prints on failure
//   CHECK_EQ(a, b)       — non-fatal equality
//   CHECK_NE(a, b)       — non-fatal inequality
//   CHECK_LT(a, b)       — non-fatal less-than
//   CHECK_LE(a, b)       — non-fatal less-or-equal
//   CHECK_STREQ(a, b)    — non-fatal C-string equality (strcmp == 0)
//   REQUIRE(expr)        — fatal; prints and calls std::exit(1)
//
// Convention:
//   Each test executable must define exactly two int variables at file scope:
//       static int g_pass = 0, g_fail = 0;
//   and call the following at the end of main():
//       std::printf("<test-name>: %d passed, %d failed\n", g_pass, g_fail);
//       return g_fail == 0 ? 0 : 1;

#include <cstdio>    // std::printf
#include <cstring>   // std::strcmp
#include <cstdlib>   // std::exit

// On MSVC, suppress CRT error/assert dialog boxes so that assertion failures
// are printed to stderr (captured by CTest) instead of blocking on a popup.
#if defined(_MSC_VER) && defined(_WIN32)
#  include <crtdbg.h>
namespace {
struct SuppressCrtDialogs {
    SuppressCrtDialogs() noexcept
    {
        _set_error_mode( _OUT_TO_STDERR );
        _CrtSetReportMode( _CRT_ERROR,  _CRTDBG_MODE_FILE );
        _CrtSetReportFile( _CRT_ERROR,  _CRTDBG_FILE_STDERR );
        _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
        _CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
        _CrtSetReportMode( _CRT_WARN,   _CRTDBG_MODE_FILE );
        _CrtSetReportFile( _CRT_WARN,   _CRTDBG_FILE_STDERR );
    }
} const s_suppress_crt_dialogs;
} // anonymous namespace
#endif // _MSC_VER

// Runs a named test function, printing its name before execution and a
// pass/fail result after it returns.
// Usage: RUN_TEST( test_my_feature );
#define RUN_TEST( fn ) \
    do { \
        std::printf( "  [ run ] " #fn "\n" ); \
        int const _xtest_fail_before = g_fail; \
        (fn)(); \
        if( g_fail == _xtest_fail_before ) \
            std::printf( "  [ ok  ] " #fn "\n" ); \
        else \
            std::printf( "  [FAIL] " #fn "\n" ); \
    } while( 0 )

// Non-fatal check — records pass/fail, always continues.
#define CHECK( expr ) \
    do { \
        if( ( expr ) ) { ++g_pass; } \
        else { ++g_fail; \
               std::printf( "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #expr ); } \
    } while( 0 )

// Equality helpers (avoids macro-expansion pitfalls with operator<< etc.)
#define CHECK_EQ( a, b ) CHECK( ( a ) == ( b ) )
#define CHECK_NE( a, b ) CHECK( ( a ) != ( b ) )
#define CHECK_LT( a, b ) CHECK( ( a ) <  ( b ) )
#define CHECK_LE( a, b ) CHECK( ( a ) <= ( b ) )

// C-string equality (null-safe: two nulls are equal, null vs non-null is not).
#define CHECK_STREQ( a, b ) \
    do { \
        const char *_a = ( a ); \
        const char *_b = ( b ); \
        bool _eq = ( _a == _b ) || ( _a && _b && std::strcmp( _a, _b ) == 0 ); \
        if( _eq ) { ++g_pass; } \
        else { ++g_fail; \
               std::printf( "FAIL [%s:%d]: strcmp(%s, %s) != 0\n", \
                            __FILE__, __LINE__, #a, #b ); } \
    } while( 0 )

// Fatal check — prints failure and exits immediately.
#define REQUIRE( expr ) \
    do { \
        if( ( expr ) ) { ++g_pass; } \
        else { std::printf( "FATAL [%s:%d]: %s\n", __FILE__, __LINE__, #expr ); \
               std::exit( 1 ); } \
    } while( 0 )
