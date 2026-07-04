// xash3dpp — precache registry quirk pins (Chunk 6 S7)
// Legacy reference: engine/server/sv_init.c :103-274, sv_game.c :1315.
// Pins: 1-based sequential indices, case-insensitive dedup, leading-slash
// strip (model/sound ONLY), backslash normalisation, sentence-name reject,
// overflow → Host_Error hook, the late-precache sink/warn split, and the
// lookup-only pfnModelIndex path.

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/private/server/precache.hpp>

#include "../../test_helpers.hpp"

#include <cstring>

namespace sv  = xash::server;
namespace mem = xash::memory;

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Capture hooks
// ---------------------------------------------------------------------------

namespace
{

struct SinkCapture
{
    int              calls = 0;
    sv::PrecacheKind kind{};
    char             name[64] = {};
    int              index    = 0;
    std::uint32_t    flags    = 0;
};
SinkCapture g_sink;

void sink_fn( void *ctx, sv::PrecacheKind kind, const char *name, int index,
              std::uint32_t flags )
{
    auto *cap  = static_cast<SinkCapture *>( ctx );
    cap->calls += 1;
    cap->kind   = kind;
    cap->index  = index;
    cap->flags  = flags;
    std::snprintf( cap->name, sizeof( cap->name ), "%s", name );
}

int  g_err_calls = 0;
char g_err_msg[128];

void err_fn( void *, const char *msg )
{
    ++g_err_calls;
    std::snprintf( g_err_msg, sizeof( g_err_msg ), "%s", msg );
}

// Small-cap fixture: 3 usable slots per table (index 0 is never used).
struct Fixture
{
    mem::PoolHandle    pool;
    sv::PrecacheTables pre;

    Fixture()
    {
        pool = mem::create_pool( "test_precache" );
        sv::PrecacheCaps caps;
        caps.models = caps.sounds = caps.events = caps.generics = 4;
        REQUIRE( pre.init( pool, caps ));
        pre.set_error_hook( err_fn, nullptr );
        g_sink      = {};
        g_err_calls = 0;
    }

    ~Fixture()
    {
        pre.shutdown();
        mem::destroy_pool( pool );
    }
};

} // namespace

// ---------------------------------------------------------------------------
// Registration quirks
// ---------------------------------------------------------------------------

static void test_model_registration()
{
    Fixture f;
    f.pre.set_loading( true );

    CHECK_EQ( f.pre.model_index( nullptr ), 0 );
    CHECK_EQ( f.pre.model_index( "" ), 0 );

    CHECK_EQ( f.pre.model_index( "models/barney.mdl" ), 1 );
    CHECK_EQ( f.pre.model_index( "models/gman.mdl" ), 2 );

    // Case-insensitive dedup (Q_stricmp).
    CHECK_EQ( f.pre.model_index( "MODELS/Barney.MDL" ), 1 );

    // ONE leading slash stripped, backslashes normalised — all aliases of
    // slot 1.
    CHECK_EQ( f.pre.model_index( "/models/barney.mdl" ), 1 );
    CHECK_EQ( f.pre.model_index( "\\models\\barney.mdl" ), 1 );

    CHECK_STREQ( f.pre.model_name( 1 ), "models/barney.mdl" );
    CHECK_STREQ( f.pre.model_name( 3 ), "" );

    // Flags OR like the legacy SetBits call sites.
    f.pre.set_model_flags( 1, xash::abi::k_res_fatalifmissing );
    CHECK_EQ( f.pre.model_flags( 1 ), xash::abi::k_res_fatalifmissing );

    CHECK_EQ( g_err_calls, 0 );
    CHECK_EQ( g_sink.calls, 0 ); // loading: never a late broadcast
}

static void test_model_overflow()
{
    Fixture f;
    f.pre.set_loading( true );

    CHECK_EQ( f.pre.model_index( "a.mdl" ), 1 );
    CHECK_EQ( f.pre.model_index( "b.mdl" ), 2 );
    CHECK_EQ( f.pre.model_index( "c.mdl" ), 3 );

    // Table full (cap 4, slots 1..3): the fourth name hard-errors.
    CHECK_EQ( f.pre.model_index( "d.mdl" ), 0 );
    CHECK_EQ( g_err_calls, 1 );
    CHECK( std::strstr( g_err_msg, "MAX_MODELS limit exceeded (4)" ) != nullptr );

    // A dedup hit still resolves after the table filled up.
    CHECK_EQ( f.pre.model_index( "b.mdl" ), 2 );
}

static void test_sound_quirks()
{
    Fixture f;
    f.pre.set_loading( true );

    // Sentence names are rejected, not registered.
    CHECK_EQ( f.pre.sound_index( "!HG_GREET0" ), 0 );
    CHECK_EQ( f.pre.sound_index( "weapons/fire.wav" ), 1 );
    CHECK_EQ( f.pre.sound_index( "/weapons/fire.wav" ), 1 ); // slash strip
    CHECK_STREQ( f.pre.sound_name( 1 ), "weapons/fire.wav" );
}

static void test_event_generic_no_strip()
{
    Fixture f;
    f.pre.set_loading( true );

    // Events/generic do NOT strip the leading slash (legacy asymmetry):
    // "/x" and "x" are DIFFERENT entries.
    CHECK_EQ( f.pre.event_index( "events/glock1.sc" ), 1 );
    CHECK_EQ( f.pre.event_index( "/events/glock1.sc" ), 2 );

    CHECK_EQ( f.pre.generic_index( "maps/de_dust.txt" ), 1 );
    CHECK_EQ( f.pre.generic_index( "/maps/de_dust.txt" ), 2 );

    // Backslashes still normalise.
    CHECK_EQ( f.pre.event_index( "events\\glock1.sc" ), 1 );
}

// ---------------------------------------------------------------------------
// Late precache (outside ss_loading)
// ---------------------------------------------------------------------------

static void test_late_precache()
{
    Fixture f;
    f.pre.set_late_sink( sink_fn, &g_sink );
    f.pre.set_loading( false );

    // Fresh model registration: sink fires with the CURRENT (zero) flags —
    // pfnPrecacheModel only sets RES_FATALIFMISSING afterwards.
    CHECK_EQ( f.pre.model_index( "models/late.mdl" ), 1 );
    CHECK_EQ( g_sink.calls, 1 );
    CHECK( g_sink.kind == sv::PrecacheKind::Model );
    CHECK_EQ( g_sink.index, 1 );
    CHECK_EQ( g_sink.flags, 0u );
    CHECK_STREQ( g_sink.name, "models/late.mdl" );

    // Dedup hit: legacy returns before the broadcast — no sink call.
    CHECK_EQ( f.pre.model_index( "models/late.mdl" ), 1 );
    CHECK_EQ( g_sink.calls, 1 );

    // Events/generic broadcast RES_FATALIFMISSING (silently — the warn
    // distinction is console-only and not visible to the sink).
    CHECK_EQ( f.pre.event_index( "events/late.sc" ), 1 );
    CHECK_EQ( g_sink.calls, 2 );
    CHECK( g_sink.kind == sv::PrecacheKind::Event );
    CHECK_EQ( g_sink.flags, xash::abi::k_res_fatalifmissing );

    CHECK_EQ( f.pre.generic_index( "gfx/late.txt" ), 1 );
    CHECK_EQ( g_sink.calls, 3 );
    CHECK( g_sink.kind == sv::PrecacheKind::Generic );

    CHECK_EQ( f.pre.sound_index( "late.wav" ), 1 );
    CHECK_EQ( g_sink.calls, 4 );
    CHECK( g_sink.kind == sv::PrecacheKind::Sound );
    CHECK_EQ( g_sink.flags, 0u );

    // Back in loading state: no more sink traffic.
    f.pre.set_loading( true );
    CHECK_EQ( f.pre.model_index( "models/load-time.mdl" ), 2 );
    CHECK_EQ( g_sink.calls, 4 );
}

// ---------------------------------------------------------------------------
// clear() + lookup-only find_model
// ---------------------------------------------------------------------------

static void test_clear_and_find()
{
    Fixture f;
    f.pre.set_loading( true );

    CHECK_EQ( f.pre.model_index( "models/one.mdl" ), 1 );
    f.pre.set_model_flags( 1, xash::abi::k_res_fatalifmissing );

    CHECK_EQ( f.pre.find_model( "models/one.mdl" ), 1 );
    CHECK_EQ( f.pre.find_model( "MODELS/ONE.MDL" ), 1 );
    CHECK_EQ( f.pre.find_model( "models/none.mdl" ), 0 ); // miss: no register
    CHECK_STREQ( f.pre.model_name( 2 ), "" );

    // Per-spawn wipe: names AND flags reset, indices restart at 1.
    f.pre.clear();
    CHECK_STREQ( f.pre.model_name( 1 ), "" );
    CHECK_EQ( f.pre.model_flags( 1 ), 0u );
    CHECK_EQ( f.pre.model_index( "models/two.mdl" ), 1 );
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_model_registration );
    RUN_TEST( test_model_overflow );
    RUN_TEST( test_sound_quirks );
    RUN_TEST( test_event_generic_no_strip );
    RUN_TEST( test_late_precache );
    RUN_TEST( test_clear_and_find );

    std::printf( "server_precache: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
