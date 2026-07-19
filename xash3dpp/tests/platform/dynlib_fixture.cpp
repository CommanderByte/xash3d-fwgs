// xash3dpp — dynlib reverse-lookup test fixture (SAV-OQ-3).
// Built as its OWN small MODULE library (a real Win32 .dll / POSIX .so) so
// test_dynlib.cpp can exercise name_for_symbol()/enumerate_exports() against
// a REAL loaded module in-process, the same way the FIELD_FUNCTION save
// codec will exercise it against a real game DLL.
//
// Kept self-contained under tests/platform/ (no dependency on
// tests/server/abi's fake_game_dll — that target is registered by a LATER
// add_subdirectory() call in the top-level tests/CMakeLists.txt than
// tests/platform, so it is not yet a known CMake target when this
// directory's CMakeLists.txt is processed).
//
// Exports (named, non-ordinal-only, non-forwarder — every export here is a
// case the reverse-lookup walk must match):
//   dynlib_fixture_add(int,int)   -> int    simple function, easy round-trip
//   dynlib_fixture_mul(int,int)   -> int    a second, distinct address
//   dynlib_fixture_name(void)     -> const char*   returns a fixed string

#if defined( _WIN32 )
#define FIXTURE_EXPORT extern "C" __declspec( dllexport )
#else
#define FIXTURE_EXPORT extern "C" __attribute__(( visibility( "default" ) ))
#endif

FIXTURE_EXPORT int dynlib_fixture_add( int a, int b )
{
    return a + b;
}

FIXTURE_EXPORT int dynlib_fixture_mul( int a, int b )
{
    return a * b;
}

FIXTURE_EXPORT const char *dynlib_fixture_name( void )
{
    return "dynlib_fixture";
}
