// xash3dpp — SV_InitClientMove (Chunk 6 pmove-bridge P3b): install the ~30-entry
// PM_* callback table the game DLL's PM_Move invokes into rt.pmove, then call
// the DLL's pfnPM_Init.
// Legacy reference: engine/server/sv_pmove.c — SV_InitClientMove (:442-498) and
// the pfn* wrappers it installs (:324-434).  Deep dive:
// deep-dive-server-physics.md §4.
//
// The callbacks are context-free C function pointers (the DLL calls them with
// no server handle), so — exactly like the 159-slot enginefuncs_t table — they
// reach the installed EngineBridge (engine_bridge()) for rt.pmove + the trace
// environment.  The collision-relevant slots (the trace family, point contents,
// StuckTouch, the string/random/time utilities, the event producer) are wired
// to real implementations; slots owned by a later slice degrade to a
// documented safe default and carry an XASH3DPP-STUB marker naming the owner:
//   * PM_Particle / PM_PlaySound        — S9 messaging / Chunk 9 sound
//   * PM_GetModelType / PM_GetModelBounds — Chunk 7 model cache (needs handles)
//   * PM_HullForBsp / PM_HullPointContents — the opaque-hull round-trip; off the
//     pm_shared PM_PlayerMove path (rarely called), deferred
//   * PM_TraceTexture / PM_TraceSurface  — Chunk 7 surface/miptex data (group c)
//   * COM_FileSize/LoadFile/FreeFile / memfgets — Chunk 7 material files (real
//     pfnPM_Init degrades to default texture types on a null load)
//
// Q-20: pmove bridge.  Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/pmove.hpp>

#include <xash3dpp/abi/pm_defs.hpp>
#include <xash3dpp/abi/server_consts.hpp> // k_fev_nothost
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/platform/platform.hpp> // get_time (Sys_FloatTime)
#include <xash3dpp/private/server/clients.hpp>       // playback_event_full
#include <xash3dpp/private/server/edict_arena.hpp>
#include <xash3dpp/private/server/engine_bridge.hpp> // EngineBridge, engine_bridge()
#include <xash3dpp/private/server/info_string.hpp>   // info_value_for_key
#include <xash3dpp/private/server/lifecycle.hpp>     // ServerRuntime
#include <xash3dpp/physics/pm_trace.hpp>

#include <cstdarg>
#include <cstddef> // offsetof
#include <cstdint>
#include <cstdio>
#include <cstring> // memcpy (trace_t shared-prefix fill)

namespace xash::server {

namespace abi = ::xash::abi;
namespace ml  = ::xash::map_loader;
namespace ut  = ::xash::utilities;
namespace phy = ::xash::physics;
using ut::Vec3;

namespace {

// --- ABI float[3] <-> Vec3 ---------------------------------------------------

[[nodiscard]] Vec3 vec_of( const float *p ) noexcept
{
    return { p[0], p[1], p[2] };
}

void store_vec( float *p, const Vec3 &v ) noexcept
{
    p[0] = v.x;
    p[1] = v.y;
    p[2] = v.z;
}

// Resolve the pmove working set + trace environment from the installed bridge.
// Returns false (callbacks then answer a clear-trace/empty default) until
// SV_InitClientMove has run and a world is bound.
[[nodiscard]] bool pm_context( EngineBridge *&bridge, abi::playermove_t *&pm,
                               phy::PmTraceEnv &env ) noexcept
{
    bridge = engine_bridge();
    if ( bridge == nullptr || bridge->pmove == nullptr ||
         bridge->pmove_model_indices == nullptr ||
         bridge->move_env == nullptr )
        return false;
    pm            = bridge->pmove;
    env.world     = bridge->move_env->world;
    env.models    = bridge->move_env->models;
    env.model_indices = bridge->pmove_model_indices;
    env.player_bounds = bridge->player_bounds;
    env.pusher_ext    = bridge->move_env->pusher_ext;
    env.cvars         = bridge->move_env->cvars; // OQ-2: mod_studiocache gate
    return env.world != nullptr && env.models != nullptr &&
           env.player_bounds != nullptr;
}

// compliance-allow(thread-assert): pure value factory returning a fresh
// miss-trace by value, reads and writes nothing
[[nodiscard]] abi::pmtrace_t clear_trace() noexcept
{
    abi::pmtrace_t t{};
    t.fraction = 1.0f;
    t.ent      = -1;
    return t;
}

// ---------------------------------------------------------------------------
// Trace family (pfn* wrappers, sv_pmove.c:345-434) → the P3a pm_* functions.
// ---------------------------------------------------------------------------

abi::pmtrace_t pfn_player_trace( float *start, float *end, int flags,
                                 int ignore_pe )
{
    EngineBridge     *b = nullptr;
    abi::playermove_t *pm = nullptr;
    phy::PmTraceEnv   env;
    if ( !pm_context( b, pm, env ) )
        return clear_trace();
    return phy::pm_player_trace_ext(
        env, *pm, vec_of( start ), vec_of( end ), flags,
        phy::PmPhysentView {
            std::span<abi::physent_t>( pm->physents,
                                       static_cast<std::size_t>( pm->numphysent )),
            std::span<const int>( env.model_indices->physents.data(),
                                  static_cast<std::size_t>( pm->numphysent )) },
        ignore_pe, nullptr );
}

abi::pmtrace_t pfn_player_trace_ex( float *start, float *end, int flags,
                                    int ( *filter )( abi::physent_t * ) )
{
    EngineBridge     *b = nullptr;
    abi::playermove_t *pm = nullptr;
    phy::PmTraceEnv   env;
    if ( !pm_context( b, pm, env ) )
        return clear_trace();
    return phy::pm_player_trace_ext(
        env, *pm, vec_of( start ), vec_of( end ), flags,
        phy::PmPhysentView {
            std::span<abi::physent_t>( pm->physents,
                                       static_cast<std::size_t>( pm->numphysent )),
            std::span<const int>( env.model_indices->physents.data(),
                                  static_cast<std::size_t>( pm->numphysent )) },
        -1, filter );
}

int pfn_test_player_position( float *pos, abi::pmtrace_t *ptrace )
{
    EngineBridge     *b = nullptr;
    abi::playermove_t *pm = nullptr;
    phy::PmTraceEnv   env;
    if ( !pm_context( b, pm, env ) )
        return -1;
    return phy::pm_test_player_position( env, *pm, vec_of( pos ), ptrace, nullptr );
}

int pfn_test_player_position_ex( float *pos, abi::pmtrace_t *ptrace,
                                 int ( *filter )( abi::physent_t * ) )
{
    EngineBridge     *b = nullptr;
    abi::playermove_t *pm = nullptr;
    phy::PmTraceEnv   env;
    if ( !pm_context( b, pm, env ) )
        return -1;
    return phy::pm_test_player_position( env, *pm, vec_of( pos ), ptrace, filter );
}

abi::pmtrace_t *pfn_trace_line( float *start, float *end, int flags, int usehull,
                                int ignore_pe )
{
    // ABI returns a pointer to persistent storage (legacy `static pmtrace_t tr`).
    static abi::pmtrace_t tr;
    EngineBridge     *b = nullptr;
    abi::playermove_t *pm = nullptr;
    phy::PmTraceEnv   env;
    tr = pm_context( b, pm, env )
             ? phy::pm_trace_line( env, *pm, vec_of( start ), vec_of( end ), flags,
                              usehull, ignore_pe )
             : clear_trace();
    return &tr;
}

abi::pmtrace_t *pfn_trace_line_ex( float *start, float *end, int flags,
                                   int usehull,
                                   int ( *filter )( abi::physent_t * ) )
{
    static abi::pmtrace_t tr;
    EngineBridge     *b = nullptr;
    abi::playermove_t *pm = nullptr;
    phy::PmTraceEnv   env;
    tr = pm_context( b, pm, env )
             ? phy::pm_trace_line_ex( env, *pm, vec_of( start ), vec_of( end ), flags,
                                 usehull, filter )
             : clear_trace();
    return &tr;
}

float pfn_trace_model( abi::physent_t *pe, float *start, float *end,
                       abi::trace_t *trace )
{
    EngineBridge     *b = nullptr;
    abi::playermove_t *pm = nullptr;
    phy::PmTraceEnv   env;
    abi::pmtrace_t result = clear_trace();
    if ( pm_context( b, pm, env ) && pe != nullptr && b->arena != nullptr &&
         pe->info >= 0 )
    {
        const abi::edict_t *ed =
            b->arena->edict_num( static_cast<std::size_t>( pe->info ));
        if ( ed != nullptr )
            result = phy::pm_trace_model( env, *pm, pe, ed->v.modelindex,
                                     vec_of( start ), vec_of( end ));
    }
    // Fill ONLY the shared trace_t/pmtrace_t prefix (allsolid..plane) — the
    // exact fields legacy's PM_RecursiveHullCheck writes through the
    // `(pmtrace_t *)trace` pun (pm_trace.c:777).  A whole-struct copy would
    // OVERFLOW the caller's buffer: the engine trace_t (const.h:746) is
    // smaller — no `deltavelocity`, and `ent` is an `edict_t *`, not `int`.
    // XASH3DPP-STUB(chunk6): the trailing ent/hitgroup fields (legacy sets
    // ent = NULL) stay the caller's — full trace_t parity needs the engine
    // trace_t vendored (a tracked follow-up); PM_TraceModel is off the
    // pm_shared PM_PlayerMove path.
    if ( trace != nullptr )
        std::memcpy( trace, &result, offsetof( abi::pmtrace_t, ent ) );
    return result.fraction;
}

int pfn_point_contents( float *p, int *truecontents )
{
    EngineBridge     *b = nullptr;
    abi::playermove_t *pm = nullptr;
    phy::PmTraceEnv   env;
    if ( !pm_context( b, pm, env ) )
        return ml::k_contents_none;
    return phy::pm_point_contents_pmove( env, *pm, vec_of( p ), truecontents );
}

int pfn_true_point_contents( float *p )
{
    EngineBridge     *b = nullptr;
    abi::playermove_t *pm = nullptr;
    phy::PmTraceEnv   env;
    if ( !pm_context( b, pm, env ) )
        return ml::k_contents_empty;
    return phy::pm_true_point_contents( env, *pm, vec_of( p ) );
}

void pfn_stuck_touch( int hitent, abi::pmtrace_t *tr )
{
    EngineBridge     *b = nullptr;
    abi::playermove_t *pm = nullptr;
    phy::PmTraceEnv   env;
    if ( pm_context( b, pm, env ) )
        phy::pm_stuck_touch( *pm, hitent, tr );
}

// ---------------------------------------------------------------------------
// Utilities (string / logging / time / random) — sv_pmove.c:465-487.
// ---------------------------------------------------------------------------

const char *pfn_info_value_for_key( const char *s, const char *key )
{
    static char s_value[256]; // ABI return buffer (legacy Info_ValueForKey)
    return info_value_for_key( s, key, s_value, sizeof( s_value ) );
}

void pfn_con_printf( const char *fmt, ... )
{
    char    buf[1024];
    va_list ap;
    va_start( ap, fmt );
    std::vsnprintf( buf, sizeof( buf ), fmt, ap );
    va_end( ap );
    ::xash::core::log_info( "pmove", buf );
}

void pfn_con_dprintf( const char *fmt, ... )
{
    EngineBridge *b = engine_bridge();
    if ( b == nullptr || b->developer == 0 )
        return; // Con_DPrintf only prints in developer mode
    char    buf[1024];
    va_list ap;
    va_start( ap, fmt );
    std::vsnprintf( buf, sizeof( buf ), fmt, ap );
    va_end( ap );
    ::xash::core::log_info( "pmove", buf );
}

void pfn_con_nprintf( int /*idx*/, const char *fmt, ... )
{
    char    buf[1024];
    va_list ap;
    va_start( ap, fmt );
    std::vsnprintf( buf, sizeof( buf ), fmt, ap );
    va_end( ap );
    ::xash::core::log_info( "pmove", buf );
}

double pfn_sys_float_time()
{
    return ::xash::platform::get_time();
}

// ---------------------------------------------------------------------------
// Event producer (sv_pmove.c:400-414) → the S9 SV_PlaybackEventFull.
// ---------------------------------------------------------------------------

void pfn_playback_event_full( int flags, int clientindex,
                              unsigned short eventindex, float delay,
                              float *origin, float *angles, float fparam1,
                              float fparam2, int iparam1, int iparam2,
                              int bparam1, int bparam2 )
{
    EngineBridge *b = engine_bridge();
    if ( b == nullptr || b->arena == nullptr )
        return;
    abi::edict_t *ent =
        b->arena->edict_num( static_cast<std::size_t>( clientindex + 1 ) );
    if ( ent == nullptr || ent->free )
        return;
    // GoldSrc always forces FEV_NOTHOST in the PMove variant (sv_pmove.c:408).
    playback_event_full( *b, flags | abi::k_fev_nothost, ent, eventindex, delay,
                         origin, angles, fparam1, fparam2, iparam1, iparam2,
                         bparam1, bparam2 );
}

// ---------------------------------------------------------------------------
// Deferred slots — safe defaults, each marked with its owning slice.
// ---------------------------------------------------------------------------

// XASH3DPP-STUB(chunk6-S9): PM_Particle emits svc_particle into
// sv.reliable_datagram — the S9 messaging datagram surface.
void pfn_particle( const float * /*origin*/, int /*color*/, float /*life*/,
                   int /*zpos*/, int /*zvel*/ )
{
}

// XASH3DPP-STUB(chunk9): PM_PlaySound routes to SV_StartSound (the sound
// subsystem lands in Chunk 9).
void pfn_play_sound( int /*channel*/, const char * /*sample*/, float /*volume*/,
                     float /*attenuation*/, int /*flags*/, int /*pitch*/ )
{
}

// XASH3DPP-STUB(chunk7): PM_GetModelType/Bounds need the studio/brush model
// handles the Chunk 7 content cache resolves (physents carry no model_s here).
int pfn_get_model_type( abi::model_s * /*mod*/ )
{
    return 0; // mod_bad
}

void pfn_get_model_bounds( abi::model_s * /*mod*/, float *mins, float *maxs )
{
    if ( mins != nullptr )
        store_vec( mins, Vec3{} );
    if ( maxs != nullptr )
        store_vec( maxs, Vec3{} );
}

// XASH3DPP-STUB(chunk6): PM_HullForBsp/PM_HullPointContents round-trip an opaque
// engine hull pointer; off the pm_shared PM_PlayerMove path (the trace family
// covers collision).  Wiring needs persistent hull storage for the returned
// pointer — deferred.
void *pfn_hull_for_bsp( abi::physent_t * /*pe*/, float *offset )
{
    if ( offset != nullptr )
        store_vec( offset, Vec3{} );
    return nullptr;
}

int pfn_hull_point_contents( abi::hull_s * /*hull*/, int /*num*/, float * /*p*/ )
{
    return ml::k_contents_empty;
}

// XASH3DPP-STUB(chunk7): PM_TraceTexture/TraceSurface (group c) need facet-bevel
// + miptex original-buffer data WorldData carries only after the content
// pipeline; return "no surface" until then.
const char *pfn_trace_texture( int /*ground*/, float * /*vstart*/,
                               float * /*vend*/ )
{
    return nullptr;
}

abi::msurface_s *pfn_trace_surface( int /*ground*/, float * /*vstart*/,
                                    float * /*vend*/ )
{
    return nullptr;
}

// XASH3DPP-STUB(chunk7): COM_* / memfgets back the material-file load the real
// pfnPM_Init runs (PM_InitTextureTypes -> "sound/materials.txt").  A null load
// degrades the DLL to default texture types — correct milestone behaviour.
int pfn_com_file_size( const char * /*filename*/ )
{
    return -1;
}

std::uint8_t *pfn_com_load_file( const char * /*path*/, int /*usehunk*/,
                                 int *length )
{
    if ( length != nullptr )
        *length = 0;
    return nullptr;
}

void pfn_com_free_file( void * /*buffer*/ )
{
}

char *pfn_memfgets( std::uint8_t * /*mem*/, int /*size*/, int * /*pos*/,
                    char * /*buf*/, int /*bufsize*/ )
{
    return nullptr;
}

// ---------------------------------------------------------------------------
// Table install (SV_InitClientMove :464-494).
// ---------------------------------------------------------------------------

void install_pmove_table( abi::playermove_t &pm ) noexcept
{
    pm.PM_Info_ValueForKey    = pfn_info_value_for_key;
    pm.PM_Particle            = pfn_particle;
    pm.PM_TestPlayerPosition  = pfn_test_player_position;
    pm.Con_NPrintf            = pfn_con_nprintf;
    pm.Con_DPrintf            = pfn_con_dprintf;
    pm.Con_Printf             = pfn_con_printf;
    pm.Sys_FloatTime          = pfn_sys_float_time;
    pm.PM_StuckTouch          = pfn_stuck_touch;
    pm.PM_PointContents       = pfn_point_contents;
    pm.PM_TruePointContents   = pfn_true_point_contents;
    pm.PM_HullPointContents   = pfn_hull_point_contents;
    pm.PM_PlayerTrace         = pfn_player_trace;
    pm.PM_TraceLine           = pfn_trace_line;
    pm.RandomLong             = effective_random_long();
    pm.RandomFloat            = effective_random_float();
    pm.PM_GetModelType        = pfn_get_model_type;
    pm.PM_GetModelBounds      = pfn_get_model_bounds;
    pm.PM_HullForBsp          = pfn_hull_for_bsp;
    pm.PM_TraceModel          = pfn_trace_model;
    pm.COM_FileSize           = pfn_com_file_size;
    pm.COM_LoadFile           = pfn_com_load_file;
    pm.COM_FreeFile           = pfn_com_free_file;
    pm.memfgets               = pfn_memfgets;
    pm.PM_PlaySound           = pfn_play_sound;
    pm.PM_TraceTexture        = pfn_trace_texture;
    pm.PM_PlaybackEventFull   = pfn_playback_event_full;
    pm.PM_PlayerTraceEx       = pfn_player_trace_ex;
    pm.PM_TestPlayerPositionEx = pfn_test_player_position_ex;
    pm.PM_TraceLineEx         = pfn_trace_line_ex;
    pm.PM_TraceSurface        = pfn_trace_surface;
}

} // namespace

void sv_init_client_move( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    if ( rt.pmove == nullptr )
        return;

    abi::playermove_t &pm = *rt.pmove;

    pm.server   = 1;
    pm.movevars = &rt.movevars;
    pm.runfuncs = 0;

    // enumerate the client hulls into the pmove table (host.player_mins/maxs).
    for ( std::size_t i = 0; i < 4; ++i )
    {
        store_vec( pm.player_mins[i], rt.hull_bounds[i].mins );
        store_vec( pm.player_maxs[i], rt.hull_bounds[i].maxs );
    }

    install_pmove_table( pm );

    // register the working set + hull table on the bridge the callbacks reach.
    rt.bridge.pmove         = &pm;
    rt.bridge.pmove_model_indices = &rt.pmove_model_indices;
    rt.bridge.player_bounds = &rt.hull_bounds;

    // Pmove_Init is a no-op here: the box hull is a per-call value type
    // (map_loader BoxHull), not the legacy shared static.  Then hand the pmove
    // to the DLL's initializer (a DLL_FUNCTIONS export; absent in stubs).
    if ( rt.game.funcs().pfnPM_Init != nullptr )
        rt.game.funcs().pfnPM_Init(
            reinterpret_cast<abi::playermove_s *>( &pm ) );        // SAFETY: playermove_t->playermove_s ABI pun — pm is the complete xash3dpp mirror; the DLL_FUNCTIONS slot names the forward-declared struct tag (eiface.hpp:224); same frozen SDK layout (typedef struct playermove_s playermove_t)
}

} // namespace xash::server
