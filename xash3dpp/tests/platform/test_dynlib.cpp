// xash3dpp — platform dynlib reverse-lookup tests (SAV-OQ-3)
// Covers: name_for_symbol round-trip against a REAL loaded module
//         (get_symbol(name) -> name_for_symbol(addr) == name), negative
//         paths (null lib, null addr, address not in the module),
//         enumerate_exports (Win32: contains every known fixture export and
//         its address matches get_symbol; POSIX: unsupported, always false,
//         visitor never invoked — matches the platform-capability asymmetry
//         documented in platform.hpp).

#include <xash3dpp/platform/platform.hpp>

#include <cstring>   // std::strcmp

#include "../test_helpers.hpp"

#ifndef DYNLIB_FIXTURE_PATH
#error "DYNLIB_FIXTURE_PATH must be defined by CMake (see tests/platform/CMakeLists.txt)"
#endif

static int g_pass = 0, g_fail = 0;

using xash::platform::LibHandle;
using xash::platform::close_library;
using xash::platform::enumerate_exports;
using xash::platform::get_symbol;
using xash::platform::name_for_symbol;
using xash::platform::open_library;

namespace {

// RAII wrapper so every test function gets a fresh, independently-closed
// handle without hand-rolled cleanup at every early CHECK failure.
struct ScopedLib
{
    LibHandle h;
    ScopedLib() : h( open_library( DYNLIB_FIXTURE_PATH ) ) {}
    ~ScopedLib() { close_library( h ); }
    ScopedLib( const ScopedLib & ) = delete;
    ScopedLib &operator=( const ScopedLib & ) = delete;
};

} // namespace

// ---------------------------------------------------------------------------
// Round-trip: get_symbol(name) -> name_for_symbol(addr) == name
// ---------------------------------------------------------------------------

static void test_roundtrip_add()
{
    ScopedLib lib;
    REQUIRE( static_cast<bool>( lib.h ) );

    void *addr = get_symbol( lib.h, "dynlib_fixture_add" );
    REQUIRE( addr != nullptr );

    auto name = name_for_symbol( lib.h, addr );
    REQUIRE( name.has_value() );
    CHECK( *name == "dynlib_fixture_add" );
}

static void test_roundtrip_mul()
{
    ScopedLib lib;
    REQUIRE( static_cast<bool>( lib.h ) );

    void *addr = get_symbol( lib.h, "dynlib_fixture_mul" );
    REQUIRE( addr != nullptr );

    auto name = name_for_symbol( lib.h, addr );
    REQUIRE( name.has_value() );
    CHECK( *name == "dynlib_fixture_mul" );
}

static void test_roundtrip_name_fn()
{
    ScopedLib lib;
    REQUIRE( static_cast<bool>( lib.h ) );

    void *addr = get_symbol( lib.h, "dynlib_fixture_name" );
    REQUIRE( addr != nullptr );

    auto name = name_for_symbol( lib.h, addr );
    REQUIRE( name.has_value() );
    CHECK( *name == "dynlib_fixture_name" );

    // The two lookups must agree on which function this is: calling through
    // the get_symbol address must produce the same string the export table
    // says its name is.
    using NameFn = const char *( * )( void );
    auto fn = reinterpret_cast<NameFn>( addr );
    CHECK( std::strcmp( fn(), "dynlib_fixture" ) == 0 );
}

// ---------------------------------------------------------------------------
// Negative paths
// ---------------------------------------------------------------------------

static void test_name_for_symbol_null_lib()
{
    LibHandle h{};
    void *addr = reinterpret_cast<void *>( &test_roundtrip_add ); // any valid code address
    CHECK( !name_for_symbol( h, addr ).has_value() );
}

static void test_name_for_symbol_null_addr()
{
    ScopedLib lib;
    REQUIRE( static_cast<bool>( lib.h ) );
    CHECK( !name_for_symbol( lib.h, nullptr ).has_value() );
}

static void test_name_for_symbol_address_not_in_module()
{
    ScopedLib lib;
    REQUIRE( static_cast<bool>( lib.h ) );

    // This test binary's own code is not part of the fixture module, so an
    // address here must never resolve to a fixture export name.
    void *addr = reinterpret_cast<void *>( &test_roundtrip_add );
    CHECK( !name_for_symbol( lib.h, addr ).has_value() );
}

// ---------------------------------------------------------------------------
// enumerate_exports
// ---------------------------------------------------------------------------

namespace {

struct Collected
{
    static constexpr int k_max = 16;
    std::string_view names[k_max];
    const void       *addrs[k_max];
    int               count = 0;
};

void collect_visitor( std::string_view name, const void *addr, void *userdata ) noexcept
{
    auto *out = static_cast<Collected *>( userdata );
    if( out->count < Collected::k_max )
    {
        out->names[out->count] = name;
        out->addrs[out->count] = addr;
        ++out->count;
    }
}

bool collected_has( const Collected &c, std::string_view name, const void *addr )
{
    for( int i = 0; i < c.count; ++i )
        if( c.names[i] == name && c.addrs[i] == addr )
            return true;
    return false;
}

} // namespace

static void test_enumerate_exports_null_lib()
{
    LibHandle h{};
    Collected out;
    CHECK( !enumerate_exports( h, collect_visitor, &out ) );
    CHECK_EQ( out.count, 0 );
}

static void test_enumerate_exports_null_visitor()
{
    ScopedLib lib;
    REQUIRE( static_cast<bool>( lib.h ) );
    CHECK( !enumerate_exports( lib.h, nullptr, nullptr ) );
}

#if defined( _WIN32 )

// Win32 supports enumeration: every known fixture export must appear with
// the same address get_symbol() resolves.
static void test_enumerate_exports_contains_known_symbols()
{
    ScopedLib lib;
    REQUIRE( static_cast<bool>( lib.h ) );

    Collected out;
    CHECK( enumerate_exports( lib.h, collect_visitor, &out ) );
    CHECK( out.count >= 3 );

    void *add_addr  = get_symbol( lib.h, "dynlib_fixture_add" );
    void *mul_addr  = get_symbol( lib.h, "dynlib_fixture_mul" );
    void *name_addr = get_symbol( lib.h, "dynlib_fixture_name" );
    REQUIRE( add_addr != nullptr );
    REQUIRE( mul_addr != nullptr );
    REQUIRE( name_addr != nullptr );

    CHECK( collected_has( out, "dynlib_fixture_add",  add_addr ) );
    CHECK( collected_has( out, "dynlib_fixture_mul",  mul_addr ) );
    CHECK( collected_has( out, "dynlib_fixture_name", name_addr ) );
}

#else

// POSIX has no enumeration primitive (see platform.hpp doc) — the visitor
// must never be invoked and the call must report failure.
static void test_enumerate_exports_unsupported_on_posix()
{
    ScopedLib lib;
    REQUIRE( static_cast<bool>( lib.h ) );

    Collected out;
    CHECK( !enumerate_exports( lib.h, collect_visitor, &out ) );
    CHECK_EQ( out.count, 0 );
}

#endif

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    RUN_TEST( test_roundtrip_add );
    RUN_TEST( test_roundtrip_mul );
    RUN_TEST( test_roundtrip_name_fn );
    RUN_TEST( test_name_for_symbol_null_lib );
    RUN_TEST( test_name_for_symbol_null_addr );
    RUN_TEST( test_name_for_symbol_address_not_in_module );
    RUN_TEST( test_enumerate_exports_null_lib );
    RUN_TEST( test_enumerate_exports_null_visitor );
#if defined( _WIN32 )
    RUN_TEST( test_enumerate_exports_contains_known_symbols );
#else
    RUN_TEST( test_enumerate_exports_unsupported_on_posix );
#endif

    std::printf( "dynlib: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
