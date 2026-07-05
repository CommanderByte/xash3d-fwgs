// xash3dpp — enginefuncs_t population: all 159 slots (Chunk 6 S6)
// Legacy reference: engine/server/sv_game.c — gEngfuncs (:4705-4866) and
// the pfn* implementations it names (line refs at each function).
// Deep dive: docs/legacy-survey/deep-dive-server-game-dll-bridge.md §2.
//
// Slots bind to the subsystems that exist today (S3 PHS, S4 arena/string
// pool, S5 world interaction, utilities, platform); everything owned by a
// later slice (precache/lifecycle S7, sv_move/sv_phys/save S8, messaging/
// clients/delta/events S9, studio Chunk 7) is an XASH3DPP-STUB(chunk6)
// no-op returning the legacy-safe default.  Raw `->v.` access is
// permitted here (Q-20: src/server/abi/ is inside the seam).
//
// This file is the ONE translation unit that projects engine types onto
// the frozen ABI (SvTrace → TraceResult, Vec3 → float[3]).
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/engine_bridge.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/map_loader/pvs.hpp>
#include <xash3dpp/platform/platform.hpp>
#include <xash3dpp/private/server/clients.hpp>
#include <xash3dpp/private/server/entity_view.hpp>
#include <xash3dpp/private/server/info_string.hpp>
#include <xash3dpp/private/server/snapshot.hpp>
#include <xash3dpp/utilities/hash.hpp>
#include <xash3dpp/utilities/math.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace xash::server {

namespace abi = ::xash::abi;
namespace ml  = ::xash::map_loader;
namespace ut  = ::xash::utilities;
using ut::Vec3;

namespace {

EngineBridge *g_bridge = nullptr;

// legacy MAX_MAP_LEAFS (common/bspfile.h, bsp30) — sizes the file-static
// fat-visibility buffers exactly like sv_game.c:31-32.
inline constexpr std::size_t k_fat_vis_bytes = ( 32767 + 7 ) / 8;

std::byte s_fatpvs[k_fat_vis_bytes];
std::byte s_fatphs[k_fat_vis_bytes];

// pfnSetFatPAS against a world with no PHS built yet (legacy: requesting
// PHS without PHS data yields full visibility).
const ml::PhsTable s_empty_phs{};

// The ABI hands vectors around as bare float* — the array-reference
// helpers in entity_view.hpp can't bind those.
[[nodiscard]] Vec3 vec_from_ptr( const float *p ) noexcept
{
    return { p[0], p[1], p[2] };
}

void store_to_ptr( float *p, const Vec3 &v ) noexcept
{
    p[0] = v.x;
    p[1] = v.y;
    p[2] = v.z;
}

[[nodiscard]] bool valid_edict( const abi::edict_t *ed ) noexcept
{
    return ed != nullptr && !ed->free;
}

void host_error( const char *msg ) noexcept
{
    if ( g_bridge != nullptr && g_bridge->host_error != nullptr )
        g_bridge->host_error( g_bridge->host_error_ctx, msg );
    else
        ::xash::core::log_error( "server", msg );
}

// SV_FreeEdict entry (sv_game.c:1004): unlink from the world FIRST, then
// arena scrub (the arena does not know about area links by design).
void bridge_free_edict( abi::edict_t *ed )
{
    WorldLinks::unlink_edict( ed );
    g_bridge->arena->free_edict( ed, g_bridge->sv_time );
}

// SV_ConvertTrace (sv_game.c:276) — including the trace_flags reset side
// effect on every conversion.
void convert_trace( abi::TraceResult *dst, const SvTrace &src ) noexcept
{
    if ( dst == nullptr )
        return;

    dst->fAllSolid   = src.t.allsolid ? 1 : 0;
    dst->fStartSolid = src.t.startsolid ? 1 : 0;
    dst->fInOpen     = src.t.inopen ? 1 : 0;
    dst->fInWater    = src.t.inwater ? 1 : 0;
    dst->flFraction  = src.t.fraction;
    store_vec3( dst->vecEndPos, src.t.endpos );
    dst->flPlaneDist = src.t.plane.dist;
    store_vec3( dst->vecPlaneNormal, src.t.plane.normal );
    dst->pHit      = src.ent;
    dst->iHitgroup = src.hitgroup;

    // g-cont: always reset config flags when trace is finished
    if ( g_bridge->globals != nullptr )
        g_bridge->globals->trace_flags = 0;
}

// Pre-world default: a clean no-hit trace ending at `end` (not a legacy
// path — game DLLs only trace once a map is up; kept total for safety).
SvTrace no_hit_trace( const Vec3 &end ) noexcept
{
    SvTrace tr;
    tr.t.fraction = 1.0f;
    tr.t.endpos   = end;
    tr.ent        = g_bridge->arena != nullptr
                        ? g_bridge->arena->edict_num( 0 )
                        : nullptr;
    return tr;
}

// anglemod (public/mathlib.h) — the classic 16-bit wrap.
[[nodiscard]] float anglemod_f( float a ) noexcept
{
    return ( 360.0f / 65536.0f ) *
           static_cast<float>( static_cast<int>( a * ( 65536.0f / 360.0f )) &
                               65535 );
}

// SV_AngleMod (sv_game.c:122-156).
[[nodiscard]] float sv_angle_mod( float ideal, float current,
                                  float speed ) noexcept
{
    float move;

    current = anglemod_f( current );

    if ( current == ideal ) // already there?
        return current;

    move = ideal - current;

    if ( ideal > current )
    {
        if ( move >= 180.0f )
            move = move - 360.0f;
    }
    else
    {
        if ( move <= -180.0f )
            move = move + 360.0f;
    }

    if ( move > 0.0f )
    {
        if ( move > speed )
            move = speed;
    }
    else
    {
        if ( move < -speed )
            move = -speed;
    }

    return anglemod_f( current + move );
}

// SV_PEntityOfEntIndex (sv_game.c:42-61); `allentities` selects the fixed
// (<=) vs broken (<) player-range rule.
[[nodiscard]] abi::edict_t *pentity_of_ent_index( int index,
                                                  bool allentities ) noexcept
{
    EdictArena *arena = g_bridge->arena;

    if ( arena == nullptr || index < 0 ||
         static_cast<std::size_t>( index ) >= arena->max_edicts() )
        return nullptr;

    abi::edict_t *ed = arena->edict_num( static_cast<std::size_t>( index ));
    const bool    player =
        allentities ? index <= g_bridge->max_clients
                    : index < g_bridge->max_clients;

    // TODO(chunk6-S7): ENGINE_QUAKE_COMPATIBLE bypass joins with the host
    // feature flags wiring.
    if ( index == 0 )
        return ed;

    if ( valid_edict( ed ) && ed->pvPrivateData != nullptr )
        return ed;

    // g-cont: world and clients can be accessed even without private data
    if ( valid_edict( ed ) && player )
        return ed;

    return nullptr;
}

// ---------------------------------------------------------------------------
// External cvar chain (pfnCVarRegister family)
// ---------------------------------------------------------------------------

[[nodiscard]] abi::cvar_t *find_external_cvar( const char *name ) noexcept
{
    if ( name == nullptr )
        return nullptr;

    for ( abi::cvar_t *v = g_bridge->external_cvars; v != nullptr;
          v = v->next )
    {
        if ( ::xash::utilities::strcmp( v->name, name ) == 0 )
            return v;
    }
    return nullptr;
}

void register_external_cvar( abi::cvar_t *variable, bool ext_dll )
{
    if ( variable == nullptr || variable->name == nullptr )
        return;

    if ( find_external_cvar( variable->name ) != nullptr )
        return; // legacy Cvar_RegisterVariable ignores re-registration

    if ( ext_dll )
        variable->flags |= abi::k_fcvar_extdll; // pfnCvar_RegisterServerVariable :2857

    variable->value = static_cast<float>(
        variable->string != nullptr ? std::atof( variable->string ) : 0.0 );

    variable->next            = g_bridge->external_cvars;
    g_bridge->external_cvars  = variable;
}

// Engine-owned cvar string allocations, chained through a header so
// reset_external_cvars() can bulk-free them (legacy leaks these into the
// svgame mempool and frees the pool at unload).
struct CvarStringNode
{
    CvarStringNode *next;
    // the character data follows the node in the same allocation
};

// Replace a game cvar's string with an engine-owned copy (legacy Cvar_Set
// zone copy; the DLL's original static stays untouched).
void set_external_cvar_string( abi::cvar_t *var, const char *value )
{
    if ( value == nullptr )
        value = "";

    const std::size_t len = ::xash::utilities::strlen( value ) + 1;
    auto *node = static_cast<CvarStringNode *>( ::xash::memory::mem_alloc(
        g_bridge->misc_pool, sizeof( CvarStringNode ) + len ));

    if ( node == nullptr )
        return; // OOM: keep the previous string (legacy Host_Error path)

    node->next = static_cast<CvarStringNode *>( g_bridge->cvar_string_allocs );
    g_bridge->cvar_string_allocs = node;

    char *copy = reinterpret_cast<char *>( node + 1 );
    std::memcpy( copy, value, len );
    var->string = copy;
    var->value  = static_cast<float>( std::atof( copy ));
}

// ===========================================================================
// Slot implementations, in table order
// ===========================================================================

// --- precache / model ------------------------------------------------------

// pfnPrecacheModel (sv_game.c:1280): '!' prefix marks the model optional
// (no RES_FATALIFMISSING); empty input warns and returns world (the FWGS
// deviation from GoldSrc's Host_Error, kept).
int pfn_precache_model( const char *s )
{
    if ( g_bridge->precache == nullptr )
        return 0;

    if ( s == nullptr || s[0] == '\0' )
    {
        ::xash::core::log_warning(
            "server", "pfnPrecacheModel: NULL pointer or empty string as "
                      "model name, returning world..." );
        return 0;
    }

    bool optional = false;
    if ( *s == '!' )
    {
        optional = true;
        ++s;
    }

    const int i = g_bridge->precache->model_index( s );
    if ( i == 0 )
        return 0;

    // Legacy fills sv.models[i] = Mod_ForName here; xash3dpp resolves brush
    // models lazily through IModelResolver (model_resolver.hpp) from this
    // same precache name, so there is no cache array to populate.
    // TODO(chunk7): studio/sprite loading (Mod_ForName for non-brush models).

    if ( !optional )
        g_bridge->precache->set_model_flags(
            static_cast<std::size_t>( i ), abi::k_res_fatalifmissing );

    return i;
}

// Legacy binds SV_SoundIndex directly as the slot (sv_game.c gEngfuncs).
int pfn_precache_sound( const char *s )
{
    if ( g_bridge->precache == nullptr )
        return 0;
    return g_bridge->precache->sound_index( s );
}

// ServerState::Active mirror value (lifecycle.hpp) — kept as a local
// numeral so the ABI shim does not include the lifecycle header.
constexpr int k_ss_active = 2;

// Defined below; SV_SetModel needs it before the definition point.
void set_min_max_size( abi::edict_t *e, const float *mins, const float *maxs,
                       bool relink );

// SV_SetModel (sv_game.c:218-267): register the name, stamp model/modelindex,
// then set bounds from the model.  Legacy gives every NON-studio model its
// real mins/maxs — brush AND sprite — and only mod_studio zero bounds
// (sv_game.c:264-266).  xash3dpp can resolve brush submodels today; sprites
// and studio models are unloadable until the Chunk 7 content pipeline, so
// both take zero bounds for now.  TODO(chunk7): sprites need real bounds.
void pfn_set_model( abi::edict_t *ent, const char *modelname )
{
    if ( !valid_edict( ent ))
    {
        ::xash::core::log_warning( "server", "SetModel: invalid entity" );
        return;
    }

    if ( modelname == nullptr ||
         static_cast<unsigned char>( modelname[0] ) <= ' ' )
    {
        ::xash::core::log_warning( "server", "SetModel: null name" );
        return;
    }

    if ( *modelname == '\\' || *modelname == '/' )
        ++modelname; // strip ONE leading slash (SV_ModelIndex strips again)

    if ( g_bridge->precache == nullptr )
        return;

    const int i = g_bridge->precache->model_index( modelname );
    if ( i == 0 )
    {
        if ( g_bridge->server_state == k_ss_active )
            ::xash::core::log_error(
                "server", "SetModel: world model cannot be changed" );
        return;
    }

    ent->v.model =
        g_bridge->strings->make_string( g_bridge->precache->model_name( i ));
    ent->v.modelindex = i;

    // Bounds: brush submodel extents; zero for studio/sprite/unknown until
    // the Chunk 7 content pipeline can load them (see the header comment).
    float mn[3] = { 0.0f, 0.0f, 0.0f };
    float mx[3] = { 0.0f, 0.0f, 0.0f };
    const MoveEnv *env = g_bridge->move_env;
    if ( env != nullptr && env->models != nullptr && env->world != nullptr )
    {
        const std::optional<BrushModel> bm = env->models->brush_model( i );
        if ( bm.has_value() )
        {
            const auto subs = env->world->submodels();
            if ( bm->submodel < subs.size() )
            {
                const ml::SubModel &sm = subs[bm->submodel];
                mn[0] = sm.mins.x; mn[1] = sm.mins.y; mn[2] = sm.mins.z;
                mx[0] = sm.maxs.x; mx[1] = sm.maxs.y; mx[2] = sm.maxs.z;
            }
        }
    }

    set_min_max_size( ent, mn, mx, true );
}

// pfnModelIndex (sv_game.c:1315): lookup only, never registers.
int pfn_model_index( const char *m )
{
    if ( g_bridge->precache == nullptr )
        return 0;
    return g_bridge->precache->find_model( m );
}

int pfn_model_frames( int )
{
    // XASH3DPP-STUB(chunk6): needs the model cache (S7); legacy default
    // for unknown models is 1.
    return 1;
}

// SV_SetMinMaxSize (sv_game.c:165-186); pfnSetSize passes relink = true.
void set_min_max_size( abi::edict_t *e, const float *mins, const float *maxs,
                       bool relink )
{
    if ( !valid_edict( e ))
        return;

    for ( int i = 0; i < 3; ++i )
    {
        if ( mins[i] > maxs[i] )
        {
            ::xash::core::log_error( "server",
                                     "backwards mins/maxs in SetSize" );
            if ( relink && g_bridge->links != nullptr &&
                 g_bridge->link_env != nullptr )
                g_bridge->links->link_edict( e, false, *g_bridge->link_env );
            return;
        }
    }

    for ( int i = 0; i < 3; ++i )
    {
        e->v.mins[i] = mins[i];
        e->v.maxs[i] = maxs[i];
        e->v.size[i] = maxs[i] - mins[i];
    }

    if ( relink && g_bridge->links != nullptr && g_bridge->link_env != nullptr )
        g_bridge->links->link_edict( e, false, *g_bridge->link_env );
}

void pfn_set_size( abi::edict_t *e, const float *rgflMin, const float *rgflMax )
{
    set_min_max_size( e, rgflMin, rgflMax, true ); // sv_game.c:1363
}

void pfn_change_level( const char *, const char * )
{
    // XASH3DPP-STUB(chunk6-S9): the game calls pfnChangeLevel to request a
    // landmark transition; legacy SV_ChangeLevel stages the save then queues
    // a "changelevel" console command that the host dispatches into
    // MapLoader::change_level → ILevelChangeExecutor::exec_change_level (the
    // executor seam is in place; the save body is Chunk 8).  The ABI→command
    // dispatch needs the server command surface (Cbuf), which rides with the
    // operator-command machinery in S9 — the bridge has no Cbuf seam yet.
}

void pfn_get_spawn_parms( abi::edict_t * )
{
    // legacy: OBSOLETE, UNUSED (sv_game.c:1411) — empty on purpose.
}

void pfn_save_spawn_parms( abi::edict_t * )
{
    // legacy: OBSOLETE, UNUSED (sv_game.c:1422) — empty on purpose.
}

// --- math helpers (utilities) ----------------------------------------------

float pfn_vec_to_yaw( const float *rgflVector )
{
    if ( rgflVector == nullptr )
        return 0.0f;
    return ut::vec_to_yaw( vec_from_ptr( rgflVector ));
}

void pfn_vec_to_angles( const float *rgflVectorIn, float *rgflVectorOut )
{
    if ( rgflVectorIn == nullptr || rgflVectorOut == nullptr )
        return;
    store_to_ptr( rgflVectorOut,
                  ut::vector_angles( vec_from_ptr( rgflVectorIn )));
}

void pfn_move_to_origin( abi::edict_t *, const float *, float, int )
{
    // XASH3DPP-STUB(chunk6): SV_MoveToOrigin (sv_move.c) lands in S8.
}

void pfn_change_yaw( abi::edict_t *ent )
{
    if ( !valid_edict( ent ))
        return;
    // sv_game.c:1462 — angles[YAW] stepped toward ideal_yaw.
    ent->v.angles[1] =
        sv_angle_mod( ent->v.ideal_yaw, ent->v.angles[1], ent->v.yaw_speed );
}

void pfn_change_pitch( abi::edict_t *ent )
{
    if ( !valid_edict( ent ))
        return;
    ent->v.angles[0] = sv_angle_mod( ent->v.idealpitch, ent->v.angles[0],
                                     ent->v.pitch_speed );
}

// --- entity search ----------------------------------------------------------

// gEntvarsDescription (sv_game.c:71-86): the 13 searchable string fields.
struct EntvarsStringField
{
    const char *name;
    std::size_t offset;
};

inline constexpr EntvarsStringField k_entvars_string_fields[] = {
    { "classname", offsetof( abi::entvars_t, classname ) },
    { "globalname", offsetof( abi::entvars_t, globalname ) },
    { "model", offsetof( abi::entvars_t, model ) },
    { "viewmodel", offsetof( abi::entvars_t, viewmodel ) },
    { "weaponmodel", offsetof( abi::entvars_t, weaponmodel ) },
    { "target", offsetof( abi::entvars_t, target ) },
    { "targetname", offsetof( abi::entvars_t, targetname ) },
    { "netname", offsetof( abi::entvars_t, netname ) },
    { "message", offsetof( abi::entvars_t, message ) },
    { "noise", offsetof( abi::entvars_t, noise ) },
    { "noise1", offsetof( abi::entvars_t, noise1 ) },
    { "noise2", offsetof( abi::entvars_t, noise2 ) },
    { "noise3", offsetof( abi::entvars_t, noise3 ) },
};

// SV_FindEntityByString (sv_game.c:1485): only the fields above are
// searchable; failure returns WORLD, not NULL; string_t 0 (== pStringBase)
// never matches.
abi::edict_t *pfn_find_entity_by_string( abi::edict_t *pStartEdict,
                                         const char *pszField,
                                         const char *pszValue )
{
    EdictArena *arena = g_bridge->arena;
    StringPool *pool  = g_bridge->strings;
    abi::edict_t *world = arena != nullptr ? arena->edict_num( 0 ) : nullptr;

    if ( arena == nullptr || pool == nullptr )
        return world;
    if ( pszValue == nullptr || pszValue[0] == '\0' || pszField == nullptr )
        return world;

    const EntvarsStringField *desc = nullptr;
    for ( const auto &field : k_entvars_string_fields )
    {
        if ( ::xash::utilities::strcmp( pszField, field.name ) == 0 )
        {
            desc = &field;
            break;
        }
    }

    if ( desc == nullptr )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, "server",
                            "FindEntityByString: field %s not a string",
                            pszField );
        return world;
    }

    int e = 0;
    if ( pStartEdict != nullptr )
        e = arena->index_of( pStartEdict );

    for ( ++e; e < static_cast<int>( arena->num_entities() ); ++e )
    {
        abi::edict_t *ed = arena->edict_num( static_cast<std::size_t>( e ));
        if ( !valid_edict( ed ))
            continue;

        // TODO(chunk6-S9): legacy also skips client edicts not in game
        // (cs_spawned); no client array exists before S9, so the gate
        // never fires yet.

        abi::string_t s;
        std::memcpy( &s,
                     reinterpret_cast<const char *>( &ed->v ) + desc->offset,
                     sizeof( s ));

        if ( s == 0 ) // string points at pStringBase — the empty string
            continue;

        if ( ::xash::utilities::strcmp( pool->get_string( s ), pszValue ) == 0 )
            return ed;
    }

    return world;
}

int pfn_get_entity_illum( abi::edict_t *pEnt )
{
    return light_for_entity( pEnt );
}

// pfnFindEntityInSphere (sv_game.c:1569): box distance against
// absmin/absmax; per-axis early-out is <=, the final accept is strict <.
abi::edict_t *pfn_find_entity_in_sphere( abi::edict_t *pStartEdict,
                                         const float *org, float flRadius )
{
    EdictArena *arena = g_bridge->arena;
    abi::edict_t *world = arena != nullptr ? arena->edict_num( 0 ) : nullptr;

    if ( arena == nullptr || org == nullptr )
        return world;

    flRadius *= flRadius;

    int e = 0;
    if ( valid_edict( pStartEdict ))
        e = arena->index_of( pStartEdict );

    for ( ++e; e < static_cast<int>( arena->num_entities() ); ++e )
    {
        abi::edict_t *ent = arena->edict_num( static_cast<std::size_t>( e ));
        if ( !valid_edict( ent ))
            continue;

        // TODO(chunk6-S9): clients-not-in-game skip (no clients yet).

        float distSquared = 0.0f;
        for ( int j = 0; j < 3 && distSquared <= flRadius; ++j )
        {
            float eorg;
            if ( org[j] < ent->v.absmin[j] )
                eorg = org[j] - ent->v.absmin[j];
            else if ( org[j] > ent->v.absmax[j] )
                eorg = org[j] - ent->v.absmax[j];
            else
                eorg = 0.0f;

            distSquared += eorg * eorg;
        }

        if ( distSquared < flRadius )
            return ent;
    }

    return world;
}

abi::edict_t *pfn_find_client_in_pvs( abi::edict_t * )
{
    // XASH3DPP-STUB(chunk6): needs the client array + the 0.1 s
    // round-robin cache (S9); legacy failure value is WORLD.
    return g_bridge->arena != nullptr ? g_bridge->arena->edict_num( 0 )
                                      : nullptr;
}

abi::edict_t *pfn_entities_in_pvs( abi::edict_t * )
{
    // XASH3DPP-STUB(chunk6): SV_BoxInPVS chain walk lands with the
    // snapshot pipeline (S9); legacy failure value is WORLD.
    return g_bridge->arena != nullptr ? g_bridge->arena->edict_num( 0 )
                                      : nullptr;
}

void pfn_make_vectors( const float *rgflVector )
{
    if ( rgflVector == nullptr || g_bridge->globals == nullptr )
        return;

    const ut::AngleVectors av = ut::angle_vectors( vec_from_ptr( rgflVector ));
    store_vec3( g_bridge->globals->v_forward, av.fwd );
    store_vec3( g_bridge->globals->v_right, av.right );
    store_vec3( g_bridge->globals->v_up, av.up );
}

void pfn_angle_vectors( const float *rgflVector, float *forward, float *right,
                        float *up )
{
    if ( rgflVector == nullptr )
        return;

    const ut::AngleVectors av = ut::angle_vectors( vec_from_ptr( rgflVector ));
    if ( forward != nullptr )
        store_to_ptr( forward, av.fwd );
    if ( right != nullptr )
        store_to_ptr( right, av.right );
    if ( up != nullptr )
        store_to_ptr( up, av.up );
}

// --- edict lifecycle (S4 arena) ---------------------------------------------

abi::edict_t *pfn_create_entity( void )
{
    if ( g_bridge->arena == nullptr )
        return nullptr;

    abi::edict_t *ed = g_bridge->arena->alloc_edict( g_bridge->sv_time );
    if ( ed == nullptr )
        host_error( "ED_AllocEdict: no free edicts\n" ); // sv_game.c:1076
    return ed;
}

void pfn_remove_entity( abi::edict_t *e )
{
    if ( g_bridge->arena == nullptr || !valid_edict( e ))
        return;

    // never free client or world entity (sv_game.c:1801)
    if ( g_bridge->arena->index_of( e ) < g_bridge->max_clients + 1 )
    {
        ::xash::core::log_error( "server", "can't delete world or client" );
        return;
    }

    bridge_free_edict( e );
}

// SV_CreateNamedEntity (sv_game.c:1153) = SV_AllocPrivateData( NULL,
// className, NULL ): a fresh edict, no "custom" fallback (null out-param).
abi::edict_t *pfn_create_named_entity( int className )
{
    return alloc_private_data( nullptr,
                               static_cast<abi::string_t>( className ),
                               nullptr );
}

void pfn_make_static( abi::edict_t * )
{
    // XASH3DPP-STUB(chunk6): static-entity baselines + signon buffer land
    // in S9 (svc_spawnstatic, FL_KILLME at frame end).
}

int pfn_ent_is_on_floor( abi::edict_t * )
{
    // XASH3DPP-STUB(chunk6): SV_CheckBottom (sv_move.c) lands in S8.
    return 0;
}

// pfnDropToFloor (sv_game.c:1868).
int pfn_drop_to_floor( abi::edict_t *e )
{
    if ( !valid_edict( e ) || g_bridge->move_env == nullptr ||
         g_bridge->links == nullptr || g_bridge->link_env == nullptr )
        return 0;

    const bool monster_clip = ( e->v.flags & abi::k_fl_monsterclip ) != 0;
    const Vec3 origin       = to_vec3( e->v.origin );
    Vec3       end          = origin;
    end.z -= 256.0f;

    const SvTrace trace =
        move( *g_bridge->move_env, origin, to_vec3( e->v.mins ),
              to_vec3( e->v.maxs ), end, abi::k_move_normal, e, monster_clip );

    if ( trace.t.allsolid )
        return -1;

    if ( trace.t.fraction == 1.0f )
        return 0;

    store_vec3( e->v.origin, trace.t.endpos );
    g_bridge->links->link_edict( e, false, *g_bridge->link_env );
    e->v.flags |= abi::k_fl_onground;
    e->v.groundentity = trace.ent;

    return 1;
}

int pfn_walk_move( abi::edict_t *, float, float, int )
{
    // XASH3DPP-STUB(chunk6): SV_MoveStep/SV_MoveTest (sv_move.c) land in
    // S8 (legacy Host_Error's on an unknown mode).
    return 0;
}

void pfn_set_origin( abi::edict_t *e, const float *rgflOrigin )
{
    if ( !valid_edict( e ) || rgflOrigin == nullptr )
        return;

    for ( int i = 0; i < 3; ++i )
        e->v.origin[i] = rgflOrigin[i];

    if ( g_bridge->links != nullptr && g_bridge->link_env != nullptr )
        g_bridge->links->link_edict( e, false, *g_bridge->link_env );
}

// --- sound (S9 messaging) ---------------------------------------------------

void pfn_emit_sound( abi::edict_t *, int, const char *, float, float, int,
                     int )
{
    // XASH3DPP-STUB(chunk6): SV_StartSound needs the multicast pipeline
    // (S9; sentence encodings, SND_* flag folding).
}

void pfn_emit_ambient_sound( abi::edict_t *, float *, const char *, float,
                             float, int, int )
{
    // XASH3DPP-STUB(chunk6): multicast pipeline lands in S9.
}

// --- tracing (S5 world) -----------------------------------------------------

void pfn_trace_line( const float *v1, const float *v2, int fNoMonsters,
                     abi::edict_t *pentToSkip, abi::TraceResult *ptr )
{
    if ( v1 == nullptr || v2 == nullptr )
        return;

    const Vec3 start = vec_from_ptr( v1 ), end = vec_from_ptr( v2 );

    SvTrace trace =
        g_bridge->move_env != nullptr
            ? move( *g_bridge->move_env, start, {}, {}, end, fNoMonsters,
                    pentToSkip, false )
            : no_hit_trace( end );

    if ( !valid_edict( trace.ent ) && g_bridge->arena != nullptr )
        trace.ent = g_bridge->arena->edict_num( 0 ); // sv_game.c:2126

    convert_trace( ptr, trace );
}

void pfn_trace_toss( abi::edict_t *pent, abi::edict_t *, abi::TraceResult * )
{
    if ( !valid_edict( pent ))
        return;
    // XASH3DPP-STUB(chunk6): SV_MoveToss (sv_phys.c) lands in S8.
}

int pfn_trace_monster_hull( abi::edict_t *pEdict, const float *v1,
                            const float *v2, int fNoMonsters,
                            abi::edict_t *pentToSkip, abi::TraceResult *ptr )
{
    if ( !valid_edict( pEdict ) || v1 == nullptr || v2 == nullptr )
        return 0;

    const Vec3 end = vec_from_ptr( v2 );
    const bool monster_clip = ( pEdict->v.flags & abi::k_fl_monsterclip ) != 0;

    const SvTrace trace =
        g_bridge->move_env != nullptr
            ? move( *g_bridge->move_env, vec_from_ptr( v1 ),
                    to_vec3( pEdict->v.mins ), to_vec3( pEdict->v.maxs ), end,
                    fNoMonsters, pentToSkip, monster_clip )
            : no_hit_trace( end );

    convert_trace( ptr, trace );

    return ( trace.t.allsolid || trace.t.fraction != 1.0f ) ? 1 : 0;
}

void pfn_trace_hull( const float *v1, const float *v2, int fNoMonsters,
                     int hullNumber, abi::edict_t *pentToSkip,
                     abi::TraceResult *ptr )
{
    if ( v1 == nullptr || v2 == nullptr )
        return;

    if ( hullNumber < 0 || hullNumber > 3 ) // sv_game.c:2158
        hullNumber = 0;

    const Vec3 end = vec_from_ptr( v2 );
    SvTrace    trace;

    if ( g_bridge->move_env != nullptr && g_bridge->move_env->world != nullptr )
    {
        const ml::HullDescriptor &hull =
            g_bridge->move_env->world->submodels()[0]
                .hulls[static_cast<std::size_t>( hullNumber )];
        trace = move( *g_bridge->move_env, vec_from_ptr( v1 ), hull.clip_mins,
                      hull.clip_maxs, end, fNoMonsters, pentToSkip, false );
    }
    else
    {
        trace = no_hit_trace( end );
    }

    convert_trace( ptr, trace );
}

// pfnTraceModel (sv_game.c:2194): SOLID_CUSTOM always goes through the
// custom clip; brush models get movetype/solid forced around the clip.
void pfn_trace_model( const float *v1, const float *v2, int hullNumber,
                      abi::edict_t *pent, abi::TraceResult *ptr )
{
    if ( !valid_edict( pent ) || v1 == nullptr || v2 == nullptr )
        return;

    if ( hullNumber < 0 || hullNumber > 3 )
        hullNumber = 0;

    const Vec3 end = vec_from_ptr( v2 );

    if ( g_bridge->move_env == nullptr || g_bridge->move_env->world == nullptr )
    {
        const SvTrace trace = no_hit_trace( end );
        convert_trace( ptr, trace );
        return;
    }

    MoveEnv &env = *g_bridge->move_env;
    const ml::HullDescriptor &hull =
        env.world->submodels()[0].hulls[static_cast<std::size_t>( hullNumber )];

    SvTrace trace;

    if ( pent->v.solid == abi::k_solid_custom )
    {
        // NOTE: always goes through custom clipping move even if our
        // callbacks is not initialized (sv_game.c:2212)
        if ( env.hooks != nullptr )
        {
            env.hooks->custom_clip( pent, vec_from_ptr( v1 ), hull.clip_mins,
                                    hull.clip_maxs, end, trace );
        }
        else
        {
            trace            = no_hit_trace( end );
            trace.t.allsolid = false;
        }
    }
    else if ( env.models != nullptr &&
              env.models->brush_model( pent->v.modelindex ).has_value() )
    {
        const int oldmovetype = pent->v.movetype;
        const int oldsolid    = pent->v.solid;
        pent->v.movetype      = abi::k_movetype_push;
        pent->v.solid         = abi::k_solid_bsp;

        trace = clip_move_to_entity( env, pent, vec_from_ptr( v1 ), hull.clip_mins,
                                     hull.clip_maxs, end );

        pent->v.movetype = oldmovetype;
        pent->v.solid    = oldsolid;
    }
    else
    {
        trace = clip_move_to_entity( env, pent, vec_from_ptr( v1 ), hull.clip_mins,
                                     hull.clip_maxs, end );
    }

    convert_trace( ptr, trace );
}

const char *pfn_trace_texture( abi::edict_t *pTextureEntity, const float *,
                               const float * )
{
    if ( !valid_edict( pTextureEntity ))
        return nullptr;
    // XASH3DPP-STUB(chunk6): SV_TraceTexture needs surface/texinfo trace
    // support in the map_loader kernel (tracked follow-up); legacy
    // no-texture answer is NULL.
    return nullptr;
}

void pfn_trace_sphere( const float *, const float *, int, float,
                       abi::edict_t *, abi::TraceResult * )
{
    // legacy: OBSOLETE, UNUSED (sv_game.c:2258) — empty on purpose.
}

// pfnGetAimVector (sv_game.c:2269): autoaim scan; `speed` is unused in
// legacy too.  bestdist seeds from the autoaim threshold (0 = disabled).
void pfn_get_aim_vector( abi::edict_t *ent, float /*speed*/,
                         float *rgflReturn )
{
    if ( rgflReturn == nullptr )
        return;

    Vec3 forward{ 1.0f, 0.0f, 0.0f };
    if ( g_bridge->globals != nullptr )
        forward = to_vec3( g_bridge->globals->v_forward );

    store_to_ptr( rgflReturn, forward ); // assume failure if it returns early

    if ( !valid_edict( ent ) || ( ent->v.flags & abi::k_fl_fakeclient ) != 0 )
        return;

    EdictArena *arena = g_bridge->arena;
    if ( arena == nullptr || g_bridge->move_env == nullptr )
        return;

    const Vec3 start = to_vec3( ent->v.origin ) + to_vec3( ent->v.view_ofs );

    // try sending a trace straight
    const SvTrace straight =
        move( *g_bridge->move_env, start, {}, {},
              start + forward * 2048.0f, abi::k_move_normal, ent, false );

    // don't aim at teammate
    if ( straight.ent != nullptr &&
         ( straight.ent->v.takedamage == abi::k_damage_aim ||
           ent->v.team <= 0 || ent->v.team != straight.ent->v.team ))
        return;

    Vec3  bestdir  = forward;
    float bestdist = g_bridge->autoaim_threshold;

    for ( int i = 1; i < static_cast<int>( arena->num_entities() ); ++i )
    {
        abi::edict_t *check =
            arena->edict_num( static_cast<std::size_t>( i ));

        if ( check->v.takedamage != abi::k_damage_aim )
            continue;
        if (( check->v.flags & abi::k_fl_fakeclient ) != 0 )
            continue;
        if ( ent->v.team > 0 && ent->v.team == check->v.team )
            continue;
        if ( check == ent )
            continue;

        const Vec3 end{
            check->v.origin[0] + 0.5f * ( check->v.mins[0] + check->v.maxs[0] ),
            check->v.origin[1] + 0.5f * ( check->v.mins[1] + check->v.maxs[1] ),
            check->v.origin[2] + 0.5f * ( check->v.mins[2] + check->v.maxs[2] ),
        };

        Vec3 dir = end - start;
        const float len = std::sqrt( dir.dot( dir ));
        if ( len > 0.0f )
            dir = dir * ( 1.0f / len );

        const float dist = dir.dot( forward );
        if ( dist < bestdist )
            continue; // to far to turn

        const SvTrace tr = move( *g_bridge->move_env, start, {}, {}, end,
                                 abi::k_move_normal, ent, false );

        if ( tr.ent == check )
        {
            bestdir  = dir;
            bestdist = dist;
        }
    }

    store_to_ptr( rgflReturn, bestdir );
}

// --- server command buffer (S7 host wiring) --------------------------------

void pfn_server_command( const char * )
{
    // XASH3DPP-STUB(chunk6): Cbuf_AddText + SV_IsValidCmd validation land
    // with the host command buffer wiring (S7).
}

void pfn_server_execute( void )
{
    // XASH3DPP-STUB(chunk6): Cbuf_Execute lands with S7.
}

void pfn_client_command( abi::edict_t *, char *, ... )
{
    // XASH3DPP-STUB(chunk6): stufftext to a client netchan lands in S9.
}

void pfn_particle_effect( const float *, const float *, float, float )
{
    // XASH3DPP-STUB(chunk6): sv.datagram particle message lands in S9.
}

// pfnLightStyle (sv_game.c:2437): hard error past the style table; the
// svc_lightstyle broadcast and the sv.loadgame guard join in S7/S9.
void pfn_light_style( int style, const char *val )
{
    if ( g_bridge->lightstyles == nullptr )
        return;

    if ( style < 0 ||
         static_cast<std::size_t>( style ) >=
             ::xash::limits::server_lightstyles )
    {
        host_error( "SV_SetLightStyle: style: bad style index\n" );
        return;
    }

    // TODO(chunk6-S7): ignore during loadgame to protect restored styles.
    ( void )g_bridge->lightstyles->set( style, val != nullptr ? val : "",
                                        static_cast<float>( g_bridge->sv_time ));
    // TODO(chunk6-S9): broadcast svc_lightstyle when the server is active.
}

int pfn_decal_index( const char * )
{
    // XASH3DPP-STUB(chunk6): host.draw_decals table lands in S7; legacy
    // failure value is -1 (NOT 0).
    return -1;
}

int pfn_point_contents( const float *rgflVector )
{
    if ( rgflVector == nullptr || g_bridge->move_env == nullptr )
        return ml::k_contents_empty;

    return point_contents( *g_bridge->move_env, vec_from_ptr( rgflVector ));
}

// --- user messages (S9 messaging pipeline) ----------------------------------

void pfn_message_begin( int dest, int num, const float *origin,
                        abi::edict_t *ed )
{
    message_begin( *g_bridge, dest, num, origin, ed ); // S9 pipeline
}

void pfn_message_end( void )
{
    message_end( *g_bridge );
}

void pfn_write_byte( int v )
{
    if ( g_bridge->clients != nullptr )
        message_write_byte( *g_bridge->clients, v );
}

void pfn_write_char( int v )
{
    if ( g_bridge->clients != nullptr )
        message_write_char( *g_bridge->clients, v );
}

void pfn_write_short( int v )
{
    if ( g_bridge->clients != nullptr )
        message_write_short( *g_bridge->clients, v );
}

void pfn_write_long( int v )
{
    if ( g_bridge->clients != nullptr )
        message_write_long( *g_bridge->clients, v );
}

void pfn_write_angle( float v )
{
    if ( g_bridge->clients != nullptr )
        message_write_angle( *g_bridge->clients, v );
}

void pfn_write_coord( float v )
{
    if ( g_bridge->clients != nullptr )
        message_write_coord( *g_bridge->clients, v );
}

void pfn_write_string( const char *s )
{
    if ( g_bridge->clients != nullptr )
        message_write_string( *g_bridge->clients, s );
}

void pfn_write_entity( int v )
{
    if ( g_bridge->clients != nullptr )
        message_write_entity( *g_bridge->clients, v );
}

// --- cvars (bridge-local external chain; cmd_cvar unification in S7) -------

void pfn_cvar_register( abi::cvar_t *pCvar )
{
    // pfnCvar_RegisterServerVariable (sv_game.c:2853): FCVAR_EXTDLL set.
    register_external_cvar( pCvar, true );
}

float pfn_cvar_get_float( const char *szVarName )
{
    const abi::cvar_t *var = find_external_cvar( szVarName );
    // TODO(chunk6-S7): fall through to the engine cvar registry.
    return var != nullptr ? var->value : 0.0f;
}

const char *pfn_cvar_get_string( const char *szVarName )
{
    const abi::cvar_t *var = find_external_cvar( szVarName );
    return var != nullptr && var->string != nullptr ? var->string : "";
}

void pfn_cvar_set_float( const char *szVarName, float flValue )
{
    abi::cvar_t *var = find_external_cvar( szVarName );
    if ( var == nullptr )
        return; // TODO(chunk6-S7): engine cvar registry

    char buf[32];
    std::snprintf( buf, sizeof( buf ), "%g", flValue );
    set_external_cvar_string( var, buf );
}

void pfn_cvar_set_string( const char *szVarName, const char *szValue )
{
    abi::cvar_t *var = find_external_cvar( szVarName );
    if ( var == nullptr )
        return; // TODO(chunk6-S7): engine cvar registry

    set_external_cvar_string( var, szValue );
}

// pfnAlertMessage (sv_game.c:2869): at_logged goes to the server log in
// MP; everything else is developer-gated console output.
void pfn_alert_message( abi::ALERT_TYPE atype, char *szFmt, ... )
{
    if ( szFmt == nullptr )
        return;

    char    buffer[2048];
    va_list args;

    va_start( args, szFmt );
    std::vsnprintf( buffer, sizeof( buffer ), szFmt, args );
    va_end( args );

    if ( atype == abi::at_logged && g_bridge->max_clients > 1 )
    {
        ::xash::core::log_info( "server", buffer ); // Log_Printf seam (S9)
        return;
    }

    if ( g_bridge->developer <= 0 )
        return;

    // g-cont: some mods have wrong aiconsole messages that crash the
    // engine — DEV_EXTENDED gate (sv_game.c:2887).
    if ( atype == abi::at_aiconsole && g_bridge->developer < 2 )
        return;

    switch ( atype )
    {
    case abi::at_warning:
        ::xash::core::log_warning( "server", buffer );
        break;
    case abi::at_error:
        ::xash::core::log_error( "server", buffer );
        break;
    default:
        ::xash::core::log_info( "server", buffer );
        break;
    }
}

void pfn_engine_fprintf( std::FILE *, char *, ... ) // compliance-allow(abi-file-io): frozen slot signature (eiface.h:168)
{
    // legacy: OBSOLETE, UNUSED (sv_game.c:2922) — empty on purpose.
}

// --- private data / string pool (S4) ----------------------------------------

void *pfn_pv_alloc_ent_private_data( abi::edict_t *pEdict, long cb )
{
    if ( pEdict == nullptr || g_bridge->arena == nullptr )
        return nullptr;

    g_bridge->arena->free_private( pEdict ); // sv_game.c:2950

    if ( cb > 0 )
        return g_bridge->arena->alloc_private(
            pEdict, static_cast<std::size_t>( cb ));

    return pEdict->pvPrivateData;
}

void *pfn_pv_ent_private_data( abi::edict_t *pEdict )
{
    return pEdict != nullptr ? pEdict->pvPrivateData : nullptr;
}

void pfn_free_ent_private_data( abi::edict_t *pEdict )
{
    if ( pEdict != nullptr && g_bridge->arena != nullptr )
        g_bridge->arena->free_private( pEdict );
}

const char *pfn_sz_from_index( int iString )
{
    return g_bridge->strings != nullptr
               ? g_bridge->strings->get_string( iString )
               : "";
}

int pfn_alloc_string( const char *szValue )
{
    return g_bridge->strings != nullptr
               ? g_bridge->strings->alloc_string( szValue )
               : 0;
}

// --- edict identity ----------------------------------------------------------

abi::entvars_t *pfn_get_vars_of_ent( abi::edict_t *pEdict )
{
    return pEdict != nullptr ? &pEdict->v : nullptr;
}

abi::edict_t *pfn_pentity_of_ent_offset( int iEntOffset )
{
    return g_bridge->arena != nullptr
               ? g_bridge->arena->ent_of_offset( iEntOffset )
               : nullptr;
}

int pfn_ent_offset_of_pentity( const abi::edict_t *pEdict )
{
    if ( pEdict == nullptr || g_bridge->arena == nullptr )
        return 0;
    return static_cast<int>( g_bridge->arena->offset_of( pEdict ));
}

int pfn_index_of_edict( const abi::edict_t *pEdict )
{
    if ( pEdict == nullptr || g_bridge->arena == nullptr )
        return 0;
    const int idx = g_bridge->arena->index_of( pEdict );
    return idx >= 0 ? idx : 0;
}

abi::edict_t *pfn_pentity_of_ent_index( int iEntIndex )
{
    return pentity_of_ent_index( iEntIndex, true );
}

abi::edict_t *pfn_pentity_of_ent_index_broken( int iEntIndex )
{
    // BUGCOMP_PENTITYOFENTINDEX_FLAG (sv_game.c:3406): GoldSrc treated the
    // last player slot as out of player range.
    return pentity_of_ent_index( iEntIndex, false );
}

abi::edict_t *pfn_find_entity_by_vars( abi::entvars_t *pvars )
{
    EdictArena *arena = g_bridge->arena;
    if ( arena == nullptr || pvars == nullptr )
        return nullptr;

    for ( std::size_t i = 0; i < arena->num_entities(); ++i )
    {
        abi::edict_t *ed = arena->edict_num( i );
        if ( &ed->v == pvars )
            return ed;
    }
    return nullptr;
}

void *pfn_get_model_ptr( abi::edict_t * )
{
    // XASH3DPP-STUB(chunk6): studio extradata lands with the model cache
    // (S7 / Chunk 7); legacy failure value is NULL.
    return nullptr;
}

int pfn_reg_user_msg( const char *pszName, int iSize )
{
    return reg_user_msg( *g_bridge, pszName, iSize ); // S9 registry
}

void pfn_animation_automove( const abi::edict_t *, float )
{
    // legacy: empty (sv_game.c:3546) — on purpose.
}

void pfn_get_bone_position( const abi::edict_t *, int, float *rgflOrigin,
                            float *rgflAngles )
{
    // XASH3DPP-STUB(chunk6): Mod_GetBonePosition needs studio support
    // (Chunk 7).  Zero the outs so callers see deterministic values.
    if ( rgflOrigin != nullptr )
        rgflOrigin[0] = rgflOrigin[1] = rgflOrigin[2] = 0.0f;
    if ( rgflAngles != nullptr )
        rgflAngles[0] = rgflAngles[1] = rgflAngles[2] = 0.0f;
}

unsigned long pfn_function_from_name( const char * )
{
    // XASH3DPP-STUB(chunk6): the save/restore symbol↔ordinal table
    // (COM_FunctionFromName_SR) lands with Chunk 8; 0 = not found.
    return 0;
}

const char *pfn_name_for_function( unsigned long )
{
    // XASH3DPP-STUB(chunk6): Chunk 8 (see pfn_function_from_name).
    return nullptr;
}

void pfn_client_printf( abi::edict_t *, abi::PRINT_TYPE, const char *szMsg )
{
    // XASH3DPP-STUB(chunk6): per-client printing lands in S9; mirror to
    // the console so dedicated logs keep the text.
    if ( szMsg != nullptr )
        ::xash::core::log_info( "server", szMsg );
}

void pfn_server_print( const char *szMsg )
{
    if ( szMsg != nullptr )
        ::xash::core::log_info( "server", szMsg );
}

const char *pfn_cmd_args( void )
{
    // XASH3DPP-STUB(chunk6): the active command context lands in S7;
    // legacy returns NULL when no command is being processed.
    return nullptr;
}

const char *pfn_cmd_argv( int )
{
    // XASH3DPP-STUB(chunk6): S7; legacy returns "" out of range.
    return "";
}

int pfn_cmd_argc( void )
{
    // XASH3DPP-STUB(chunk6): S7.
    return 0;
}

void pfn_get_attachment( const abi::edict_t *, int, float *rgflOrigin,
                         float *rgflAngles )
{
    // XASH3DPP-STUB(chunk6): studio attachments land in Chunk 7.
    if ( rgflOrigin != nullptr )
        rgflOrigin[0] = rgflOrigin[1] = rgflOrigin[2] = 0.0f;
    if ( rgflAngles != nullptr )
        rgflAngles[0] = rgflAngles[1] = rgflAngles[2] = 0.0f;
}

// --- CRC32 (utilities/hash) --------------------------------------------------

void pfn_crc32_init( abi::CRC32_t *pulCRC )
{
    if ( pulCRC != nullptr )
        ut::crc32_init( *pulCRC );
}

void pfn_crc32_process_buffer( abi::CRC32_t *pulCRC, const void *p, int len )
{
    if ( pulCRC != nullptr && p != nullptr && len > 0 )
        ut::crc32_update( *pulCRC, p, static_cast<std::size_t>( len ));
}

void pfn_crc32_process_byte( abi::CRC32_t *pulCRC, unsigned char ch )
{
    if ( pulCRC != nullptr )
        ut::crc32_update( *pulCRC, static_cast<std::uint8_t>( ch ));
}

abi::CRC32_t pfn_crc32_final( abi::CRC32_t pulCRC )
{
    return ut::crc32_final( pulCRC );
}

// --- random (engine-local; COM_RandomLong parity is a tracked follow-up) ----

// XASH3DPP-STUB(chunk6): legacy COM_RandomLong/Float (idtech RNG) parity
// port pending; this xorshift keeps the slots deterministic and non-null.
std::uint32_t s_rng_state = 0x29Au;

[[nodiscard]] std::uint32_t rng_next() noexcept
{
    std::uint32_t x = s_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng_state = x;
    return x;
}

int pfn_random_long( int lLow, int lHigh )
{
    if ( lHigh <= lLow )
        return lLow;
    const std::uint32_t range =
        static_cast<std::uint32_t>( lHigh - lLow ) + 1u;
    return lLow + static_cast<int>( rng_next() % range );
}

float pfn_random_float( float flLow, float flHigh )
{
    const float t =
        static_cast<float>( rng_next() ) / 4294967295.0f;
    return flLow + t * ( flHigh - flLow );
}

// --- client view / time ------------------------------------------------------

void pfn_set_view( const abi::edict_t *, const abi::edict_t * )
{
    // XASH3DPP-STUB(chunk6): svc_setview per-client message lands in S9.
}

float pfn_time( void )
{
    return static_cast<float>( ::xash::platform::get_time() ); // Sys_FloatTime
}

void pfn_crosshair_angle( const abi::edict_t *, float, float )
{
    // XASH3DPP-STUB(chunk6): svc_crosshairangle lands in S9.
}

// --- files (S7 filesystem wiring) --------------------------------------------

abi::byte *pfn_load_file_for_me( const char *, int *pLength )
{
    // XASH3DPP-STUB(chunk6): COM_LoadFileForMe joins the filesystem
    // wiring in S7; legacy failure sets *pLength = 0 and returns NULL.
    if ( pLength != nullptr )
        *pLength = 0;
    return nullptr;
}

void pfn_free_file( void * )
{
    // XASH3DPP-STUB(chunk6): S7 (paired with pfn_load_file_for_me).
}

void pfn_end_section( const char * )
{
    // XASH3DPP-STUB(chunk6): "oem_end_credits" → Host_Credits, everything
    // else stuffs `disconnect` — host wiring lands in S7.
}

int pfn_compare_file_time( const char *, const char *, int *iCompare )
{
    // XASH3DPP-STUB(chunk6): S7 filesystem wiring; legacy failure is 0.
    if ( iCompare != nullptr )
        *iCompare = 0;
    return 0;
}

void pfn_get_game_dir( char *szGetGameDir )
{
    if ( szGetGameDir == nullptr )
        return;
    // TODO(chunk6-S7): BUGCOMP_GET_GAME_DIR_FULL_PATH pre-1.1.1.1 path
    // emulation joins with the host bugcomp flags.
    ut::strncpy( szGetGameDir, g_bridge->game_dir, 256 );
}

void pfn_cvar_register_variable( abi::cvar_t *variable )
{
    // pfnCvar_RegisterEngineVariable (sv_game.c:3765): registered WITHOUT
    // FCVAR_EXTDLL so it survives DLL unload.
    register_external_cvar( variable, false );
}

void pfn_fade_client_volume( const abi::edict_t *, int, int, int, int )
{
    // XASH3DPP-STUB(chunk6): svc_soundfade lands in S9.
}

void pfn_set_client_maxspeed( const abi::edict_t *, float )
{
    // XASH3DPP-STUB(chunk6): client physinfo lands in S9 (incl. the
    // clamp-to-movevars deviation from GoldSrc).
}

abi::edict_t *pfn_create_fake_client( const char * )
{
    // XASH3DPP-STUB(chunk6): SV_FakeConnect lands in S9.
    return nullptr;
}

void pfn_run_player_move( abi::edict_t *, const float *, float, float, float,
                          unsigned short, abi::byte, abi::byte )
{
    // XASH3DPP-STUB(chunk6): the pmove bridge lands in S8.
}

int pfn_number_of_entities( void )
{
    return g_bridge->arena != nullptr
               ? static_cast<int>( g_bridge->arena->num_entities() )
               : 0;
}

// --- info strings (S9 client state) ------------------------------------------

char *pfn_get_info_key_buffer( abi::edict_t *e )
{
    // pfnGetInfoKeyBuffer (sv_game.c): world/null → serverinfo; a client edict
    // → that client's userinfo; anything else → "".
    static char s_empty[1] = { '\0' };

    if ( g_bridge->clients == nullptr )
        return s_empty;
    ClientMachinery &cm = *g_bridge->clients;

    if ( e == nullptr ||
         ( g_bridge->arena != nullptr && g_bridge->arena->index_of( e ) == 0 ) )
        return cm.serverinfo;

    ServerClient *cl = client_for_edict( cm, e );
    return cl != nullptr ? cl->userinfo : s_empty;
}

const char *pfn_info_key_value( const char *infobuffer, const char *key )
{
    // rotating caller buffer would be ideal; the ABI contract only needs a
    // stable pointer for the duration of the call — a file-static suffices.
    static char s_value[k_max_info_string];
    return info_value_for_key( infobuffer, key, s_value, sizeof( s_value ) );
}

void pfn_set_key_value( char *infobuffer, char *key, char *value )
{
    // legacy: localinfo/serverinfo only (star keys allowed for serverinfo).
    if ( infobuffer != nullptr )
        info_set_value_for_key( infobuffer, key, value, k_max_serverinfo, true );
}

void pfn_set_client_key_value( int clientIndex, char *infobuffer, char *key,
                               char *value )
{
    if ( g_bridge->clients == nullptr || infobuffer == nullptr )
        return;
    // XASH3DPP-STUB(S8-seam): FCL_RESEND_USERINFO flagging (re-broadcast on
    // the next frame) hangs off the client flags the send path owns.
    info_set_value_for_key( infobuffer, key, value, k_max_info_string );
    ( void )clientIndex;
}

int pfn_is_map_valid( char * )
{
    // XASH3DPP-STUB(chunk6): SV_MapIsValid (spawn-entity scan) lands in
    // S7; legacy failure value is 0.
    return 0;
}

void pfn_static_decal( const float *, int, int, int )
{
    // XASH3DPP-STUB(chunk6): svc_bspdecal into the signon lands in S9.
}

// Legacy binds SV_GenericIndex directly as the slot.
int pfn_precache_generic( const char *s )
{
    if ( g_bridge->precache == nullptr )
        return 0;
    return g_bridge->precache->generic_index( s );
}

int pfn_get_player_user_id( abi::edict_t *e )
{
    if ( g_bridge->clients == nullptr )
        return -1;
    const ServerClient *cl = client_for_edict( *g_bridge->clients, e );
    return cl != nullptr ? cl->userid : -1;
}

void pfn_build_sound_msg( abi::edict_t *, int, const char *, float, float,
                          int, int, int, int, const float *, abi::edict_t * )
{
    // XASH3DPP-STUB(chunk6): SV_BuildSoundMsg + message pipeline land in
    // S9.
}

int pfn_is_dedicated_server( void )
{
    return g_bridge->dedicated ? 1 : 0;
}

abi::cvar_t *pfn_cvar_get_pointer( const char *szVarName )
{
    // TODO(chunk6-S7): fall through to the engine cvar registry.
    return find_external_cvar( szVarName );
}

unsigned int pfn_get_player_won_id( abi::edict_t * ) // compliance-allow(int-width): frozen slot signature (eiface.h:223)
{
    // legacy: always (uint)-1 (sv_game.c:3736).
    return static_cast<unsigned int>( -1 ); // compliance-allow(int-width): ABI return type
}

void pfn_info_remove_key( char *, const char * )
{
    // XASH3DPP-STUB(chunk6): Info_RemoveKey wiring lands in S9.
}

const char *pfn_get_physics_key_value( const abi::edict_t *, const char * )
{
    // XASH3DPP-STUB(chunk6): physinfo lands in S9.
    return "";
}

void pfn_set_physics_key_value( const abi::edict_t *, const char *,
                                const char * )
{
    // XASH3DPP-STUB(chunk6): physinfo lands in S9.
}

const char *pfn_get_physics_info_string( const abi::edict_t * )
{
    // XASH3DPP-STUB(chunk6): physinfo lands in S9.
    return "";
}

// pfnPrecacheEvent (sv_game.c:4015): the type argument is ignored.
unsigned short pfn_precache_event( int, const char *psz )
{
    if ( g_bridge->precache == nullptr )
        return 0;
    return static_cast<unsigned short>( g_bridge->precache->event_index( psz ));
}

void pfn_playback_event( int, const abi::edict_t *, unsigned short, float,
                         float *, float *, float, float, int, int, int, int )
{
    // XASH3DPP-STUB(chunk6): SV_PlaybackEventFull (FEV_* routing) lands
    // in S9.
}

// --- fat visibility (S3 PHS / map_loader PVS) --------------------------------

// FATPVS_RADIUS / FATPHS_RADIUS (mod_local.h:31-32).
inline constexpr float k_fatpvs_radius = 8.0f;
inline constexpr float k_fatphs_radius = 8.0f;

unsigned char *pfn_set_fat_pvs( const float *org )
{
    const MoveEnv *env = g_bridge->move_env;

    if ( env == nullptr || env->world == nullptr )
    {
        std::memset( s_fatpvs, 0xFF, sizeof( s_fatpvs )); // pre-world fullvis
        return reinterpret_cast<unsigned char *>( s_fatpvs );
    }

    // TODO(chunk12): fold CL_DisableVisibility() into fullvis when the
    // listen-server client lands (sv_game.c:4260; compiles to false on
    // client-less builds — today's dedicated-only reality).
    const bool fullvis = env->world->visbytes() == 0 || g_bridge->novis ||
                         org == nullptr;

    ( void )ml::fat_pvs( *env->world,
                         org != nullptr ? vec_from_ptr( org ) : Vec3{},
                         k_fatpvs_radius, std::span<std::byte>( s_fatpvs ),
                         g_bridge->merge_visibility, fullvis );

    return reinterpret_cast<unsigned char *>( s_fatpvs );
}

unsigned char *pfn_set_fat_pas( const float *org )
{
    const MoveEnv *env = g_bridge->move_env;

    if ( env == nullptr || env->world == nullptr )
    {
        std::memset( s_fatphs, 0xFF, sizeof( s_fatphs ));
        return reinterpret_cast<unsigned char *>( s_fatphs );
    }

    // TODO(chunk12): CL_DisableVisibility() — see pfn_set_fat_pvs.
    const bool fullvis = env->world->visbytes() == 0 || g_bridge->novis ||
                         org == nullptr;
    const ml::PhsTable &phs =
        g_bridge->phs != nullptr ? *g_bridge->phs : s_empty_phs;

    ( void )ml::fat_phs( *env->world, phs,
                         org != nullptr ? vec_from_ptr( org ) : Vec3{},
                         k_fatphs_radius, std::span<std::byte>( s_fatphs ),
                         g_bridge->merge_visibility, fullvis );

    return reinterpret_cast<unsigned char *>( s_fatphs );
}

// pfnCheckVisibility (sv_game.c:4329): leaf cache first, headnode walk as
// fallback — which MUTATES the const edict, caching the found cluster
// with wrap-around (returns 2 on that path).
int pfn_check_visibility( const abi::edict_t *entity, unsigned char *pset )
{
    const MoveEnv *env = g_bridge->move_env;

    if ( !valid_edict( entity ))
        return 0;

    if ( pset == nullptr )
        return 1; // vis not set - fullvis enabled

    if ( env == nullptr || env->world == nullptr )
        return 1;

    const ml::WorldData &world = *env->world;
    const bool large_leafs = world.version() == ml::BspVersion::Bsp2;
    const int  max_ent_leafs =
        large_leafs ? abi::k_max_ent_leafs_32 : abi::k_max_ent_leafs_16;

    // upcast beams to my owner
    if (( entity->v.flags & abi::k_fl_customentity ) != 0 &&
        entity->v.owner != nullptr &&
        ( entity->v.owner->v.flags & abi::k_fl_client ) != 0 )
        entity = entity->v.owner;

    const auto bit_set = [pset]( int cluster ) noexcept {
        return cluster >= 0 &&
               ( pset[cluster >> 3] & ( 1u << ( cluster & 7 ))) != 0;
    };

    if ( entity->headnode < 0 )
    {
        // check individual leafs
        for ( int i = 0; i < entity->num_leafs; ++i )
        {
            const int leafnum = large_leafs ? entity->leafnums32[i]
                                            : entity->leafnums16[i];
            if ( bit_set( leafnum ))
                return 1; // visible passed by leaf
        }
        return 0;
    }

    for ( int i = 0; i < max_ent_leafs; ++i )
    {
        const int leafnum =
            large_leafs ? entity->leafnums32[i] : entity->leafnums16[i];
        if ( leafnum == -1 )
            break;
        if ( bit_set( leafnum ))
            return 1; // visible passed by leaf
    }

    // too many leafs for individual check, go by headnode
    int lastleaf = -1;
    const std::span<const std::byte> bits{
        reinterpret_cast<const std::byte *>( pset ), world.visbytes() };

    if ( !ml::headnode_visible( world, entity->headnode, bits, &lastleaf ))
        return 0;

    // legacy caches the found cluster on the CONST edict (deliberate).
    auto *mutable_ent = const_cast<abi::edict_t *>( entity );
    if ( large_leafs )
        mutable_ent->leafnums32[entity->num_leafs] = lastleaf;
    else
        mutable_ent->leafnums16[entity->num_leafs] =
            static_cast<short>( lastleaf );
    mutable_ent->num_leafs = ( entity->num_leafs + 1 ) % max_ent_leafs;

    return 2; // visible passed by headnode
}

// --- delta encoding (S9; networking owns the codec) --------------------------

void pfn_delta_set_field( abi::delta_t *, const char * )
{
    // XASH3DPP-STUB(chunk6): net_encode delta tables land in S9.
}

void pfn_delta_unset_field( abi::delta_t *, const char * )
{
    // XASH3DPP-STUB(chunk6): S9.
}

void pfn_delta_add_encoder( char *,
                            void ( * )( abi::delta_t *, const unsigned char *,
                                        const unsigned char * ))
{
    // XASH3DPP-STUB(chunk6): S9 (pfnRegisterEncoders pairing).
}

int pfn_get_current_player( void )
{
    // XASH3DPP-STUB(chunk6): sv.current_client lands in S8/S9; legacy
    // out-of-range value is -1.
    return -1;
}

int pfn_can_skip_player( const abi::edict_t * )
{
    // XASH3DPP-STUB(chunk6): FCL_LOCAL_WEAPONS lands in S9.
    return 0;
}

int pfn_delta_find_field( abi::delta_t *, const char * )
{
    // XASH3DPP-STUB(chunk6): S9; legacy not-found value is -1.
    return -1;
}

void pfn_delta_set_field_by_index( abi::delta_t *, int )
{
    // XASH3DPP-STUB(chunk6): S9.
}

void pfn_delta_unset_field_by_index( abi::delta_t *, int )
{
    // XASH3DPP-STUB(chunk6): S9.
}

void pfn_set_group_mask( int mask, int op )
{
    // sv_game.c:4413 — svs.groupmask/groupop.
    g_bridge->group_mask = mask;
    g_bridge->group_op   = op;

    if ( g_bridge->move_env != nullptr )
    {
        g_bridge->move_env->group_mask = mask;
        g_bridge->move_env->group_op =
            op == 1 ? GroupOp::Nand : GroupOp::And;
    }
    if ( g_bridge->links != nullptr )
        g_bridge->links->set_group_op( op == 1 ? GroupOp::Nand
                                               : GroupOp::And );
}

int pfn_create_instanced_baseline( int classname, abi::entity_state_t *baseline )
{
    // SV_CreateInstancedBaseline (sv_game.c:4425): append the classname-keyed
    // template to sv.instanced.  Null bridge/snapshot ⇒ legacy no-server 0.
    if ( g_bridge == nullptr || g_bridge->snapshot == nullptr )
        return 0;
    return create_instanced_baseline( *g_bridge->snapshot,
                                      static_cast<abi::string_t>( classname ),
                                      baseline );
}

void pfn_cvar_direct_set( abi::cvar_t *var, const char *value )
{
    if ( var == nullptr )
        return;
    set_external_cvar_string( var, value );
}

void pfn_force_unmodified( abi::FORCE_TYPE, float *, float *, const char * )
{
    // XASH3DPP-STUB(chunk6): consistency list lands in S9.
}

void pfn_get_player_stats( const abi::edict_t *, int *ping, int *packet_loss )
{
    // XASH3DPP-STUB(chunk6): client stats land in S9.
    if ( ping != nullptr )
        *ping = 0;
    if ( packet_loss != nullptr )
        *packet_loss = 0;
}

void pfn_add_server_command( const char *, void ( * )( void ))
{
    // XASH3DPP-STUB(chunk6): CMD_SERVERDLL registration lands in S7.
}

abi::qboolean pfn_voice_get_client_listening( int, int )
{
    // XASH3DPP-STUB(chunk6): voice matrices land in S9.
    return 0;
}

abi::qboolean pfn_voice_set_client_listening( int, int, abi::qboolean )
{
    // XASH3DPP-STUB(chunk6): S9.
    return 0;
}

const char *pfn_get_player_auth_id( abi::edict_t * )
{
    // XASH3DPP-STUB(chunk6): client auth ids land in S9.
    return "";
}

void *pfn_sequence_get( const char *, const char * )
{
    // XASH3DPP-STUB(chunk6): sentence sequence files land with sound
    // support; legacy not-found value is NULL.
    return nullptr;
}

void *pfn_sequence_pick_sentence( const char *, int, int *picked )
{
    // XASH3DPP-STUB(chunk6): see pfn_sequence_get.
    if ( picked != nullptr )
        *picked = 0;
    return nullptr;
}

int pfn_get_file_size( const char * )
{
    // XASH3DPP-STUB(chunk6): S7 filesystem wiring.
    return 0;
}

unsigned int pfn_get_approx_wave_play_len( const char * ) // compliance-allow(int-width): frozen slot signature (eiface.h:269)
{
    // XASH3DPP-STUB(chunk6): sound duration probing lands with sound.
    return 0;
}

int pfn_is_career_match( void )
{
    // legacy: always 0 (CZ career mode does not exist here).
    return 0;
}

int pfn_get_localized_string_length( const char * )
{
    // legacy: always 0 (sv_game.c:4651).
    return 0;
}

void pfn_register_tutor_message_shown( int )
{
    // legacy: empty (sv_game.c:4664) — on purpose.
}

int pfn_get_times_tutor_message_shown( int )
{
    // legacy: always 0 ("only exists in PlayStation version").
    return 0;
}

void pfn_process_tutor_message_decay_buffer( int *, int )
{
    // legacy: external stub — empty on purpose.
}

void pfn_construct_tutor_message_decay_buffer( int *, int )
{
    // legacy: external stub — empty on purpose.
}

void pfn_reset_tutor_message_decay_data( void )
{
    // legacy: external stub — empty on purpose.
}

// pfnQueryClientCvarValue (sv_game.c:4596): the non-client path calls the
// game's callback with the literal "Bad Player"; the real client query
// lands in S9.
void pfn_query_client_cvar_value( const abi::edict_t *player,
                                  const char * )
{
    // TODO(chunk6-S9): valid-client path sends svc_querycvarvalue.
    if ( g_bridge->game != nullptr && g_bridge->game->has_new_api() &&
         g_bridge->game->new_funcs().pfnCvarValue != nullptr )
        g_bridge->game->new_funcs().pfnCvarValue( player, "Bad Player" );
}

void pfn_query_client_cvar_value2( const abi::edict_t *player,
                                   const char *cvarName, int requestID )
{
    // TODO(chunk6-S9): valid-client path sends svc_querycvarvalue2.
    if ( g_bridge->game != nullptr && g_bridge->game->has_new_api() &&
         g_bridge->game->new_funcs().pfnCvarValue2 != nullptr )
        g_bridge->game->new_funcs().pfnCvarValue2( player, requestID,
                                                   cvarName, "Bad Player" );
}

int pfn_check_parm( char *, char ** )
{
    // XASH3DPP-STUB(chunk6): host command line lands in S7; legacy
    // not-found value is 0.
    return 0;
}

} // namespace

// ===========================================================================
// Bridge install + table build
// ===========================================================================

void install_engine_bridge( EngineBridge *bridge ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    g_bridge = bridge;
}

void reset_external_cvars( EngineBridge &bridge ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    auto *node = static_cast<CvarStringNode *>( bridge.cvar_string_allocs );
    while ( node != nullptr )
    {
        CvarStringNode *next = node->next;
        ::xash::memory::mem_free( node );
        node = next;
    }
    bridge.cvar_string_allocs = nullptr;
    bridge.external_cvars     = nullptr;
}

EngineBridge *engine_bridge() noexcept
{
    return g_bridge;
}

// SV_AllocPrivateData (sv_game.c:1092): the one LINK_ENTITY dispatch, shared
// by pfnCreateNamedEntity and the lifecycle entity-parse path.
::xash::abi::edict_t *alloc_private_data( ::xash::abi::edict_t *ent,
                                          ::xash::abi::string_t className,
                                          bool *customentity ) noexcept
{
    if ( g_bridge == nullptr || g_bridge->arena == nullptr ||
         g_bridge->strings == nullptr )
        return nullptr;

    const char *classname = g_bridge->strings->get_string( className );

    if ( customentity != nullptr )
        *customentity = false;

    if ( ent == nullptr )
    {
        ent = g_bridge->arena->alloc_edict( g_bridge->sv_time );
        if ( ent == nullptr )
        {
            host_error( "ED_AllocEdict: no free edicts\n" ); // sv_game.c:1076
            return nullptr;
        }
    }
    else if ( ent->free )
    {
        g_bridge->arena->init_edict( ent ); // SV_InitEdict — re-init
    }

    ent->v.classname         = className;
    ent->v.pContainingEntity = ent; // re-link

    abi::LINK_ENTITY_FUNC spawn =
        g_bridge->game != nullptr ? g_bridge->game->entity_link( classname )
                                  : nullptr;

    if ( spawn == nullptr )
    {
        // TODO(chunk6-S8): physFuncs.SV_CreateEntity custom-entity hook
        // (Xash extension) joins with the physics interface.

        // Fall back to the "custom" export when the caller opted in.
        if ( customentity != nullptr )
        {
            spawn = g_bridge->game != nullptr
                        ? g_bridge->game->entity_link( "custom" )
                        : nullptr;
            *customentity = ( spawn != nullptr );
        }

        if ( spawn == nullptr )
        {
            ::xash::core::logf( ::xash::core::LogLevel::Error, "server",
                                "No spawn function for \"%s\"", classname );
            bridge_free_edict( ent );
            return nullptr;
        }
    }

    spawn( &ent->v );
    return ent;
}

::xash::abi::enginefuncs_t build_engine_table( bool peoei_broken ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    abi::enginefuncs_t t{};

    t.pfnPrecacheModel                  = pfn_precache_model;
    t.pfnPrecacheSound                  = pfn_precache_sound;
    t.pfnSetModel                       = pfn_set_model;
    t.pfnModelIndex                     = pfn_model_index;
    t.pfnModelFrames                    = pfn_model_frames;
    t.pfnSetSize                        = pfn_set_size;
    t.pfnChangeLevel                    = pfn_change_level;
    t.pfnGetSpawnParms                  = pfn_get_spawn_parms;
    t.pfnSaveSpawnParms                 = pfn_save_spawn_parms;
    t.pfnVecToYaw                       = pfn_vec_to_yaw;
    t.pfnVecToAngles                    = pfn_vec_to_angles;
    t.pfnMoveToOrigin                   = pfn_move_to_origin;
    t.pfnChangeYaw                      = pfn_change_yaw;
    t.pfnChangePitch                    = pfn_change_pitch;
    t.pfnFindEntityByString             = pfn_find_entity_by_string;
    t.pfnGetEntityIllum                 = pfn_get_entity_illum;
    t.pfnFindEntityInSphere             = pfn_find_entity_in_sphere;
    t.pfnFindClientInPVS                = pfn_find_client_in_pvs;
    t.pfnEntitiesInPVS                  = pfn_entities_in_pvs;
    t.pfnMakeVectors                    = pfn_make_vectors;
    t.pfnAngleVectors                   = pfn_angle_vectors;
    t.pfnCreateEntity                   = pfn_create_entity;
    t.pfnRemoveEntity                   = pfn_remove_entity;
    t.pfnCreateNamedEntity              = pfn_create_named_entity;
    t.pfnMakeStatic                     = pfn_make_static;
    t.pfnEntIsOnFloor                   = pfn_ent_is_on_floor;
    t.pfnDropToFloor                    = pfn_drop_to_floor;
    t.pfnWalkMove                       = pfn_walk_move;
    t.pfnSetOrigin                      = pfn_set_origin;
    t.pfnEmitSound                      = pfn_emit_sound;
    t.pfnEmitAmbientSound               = pfn_emit_ambient_sound;
    t.pfnTraceLine                      = pfn_trace_line;
    t.pfnTraceToss                      = pfn_trace_toss;
    t.pfnTraceMonsterHull               = pfn_trace_monster_hull;
    t.pfnTraceHull                      = pfn_trace_hull;
    t.pfnTraceModel                     = pfn_trace_model;
    t.pfnTraceTexture                   = pfn_trace_texture;
    t.pfnTraceSphere                    = pfn_trace_sphere;
    t.pfnGetAimVector                   = pfn_get_aim_vector;
    t.pfnServerCommand                  = pfn_server_command;
    t.pfnServerExecute                  = pfn_server_execute;
    t.pfnClientCommand                  = pfn_client_command;
    t.pfnParticleEffect                 = pfn_particle_effect;
    t.pfnLightStyle                     = pfn_light_style;
    t.pfnDecalIndex                     = pfn_decal_index;
    t.pfnPointContents                  = pfn_point_contents;
    t.pfnMessageBegin                   = pfn_message_begin;
    t.pfnMessageEnd                     = pfn_message_end;
    t.pfnWriteByte                      = pfn_write_byte;
    t.pfnWriteChar                      = pfn_write_char;
    t.pfnWriteShort                     = pfn_write_short;
    t.pfnWriteLong                      = pfn_write_long;
    t.pfnWriteAngle                     = pfn_write_angle;
    t.pfnWriteCoord                     = pfn_write_coord;
    t.pfnWriteString                    = pfn_write_string;
    t.pfnWriteEntity                    = pfn_write_entity;
    t.pfnCVarRegister                   = pfn_cvar_register;
    t.pfnCVarGetFloat                   = pfn_cvar_get_float;
    t.pfnCVarGetString                  = pfn_cvar_get_string;
    t.pfnCVarSetFloat                   = pfn_cvar_set_float;
    t.pfnCVarSetString                  = pfn_cvar_set_string;
    t.pfnAlertMessage                   = pfn_alert_message;
    t.pfnEngineFprintf                  = pfn_engine_fprintf;
    t.pfnPvAllocEntPrivateData          = pfn_pv_alloc_ent_private_data;
    t.pfnPvEntPrivateData               = pfn_pv_ent_private_data;
    t.pfnFreeEntPrivateData             = pfn_free_ent_private_data;
    t.pfnSzFromIndex                    = pfn_sz_from_index;
    t.pfnAllocString                    = pfn_alloc_string;
    t.pfnGetVarsOfEnt                   = pfn_get_vars_of_ent;
    t.pfnPEntityOfEntOffset             = pfn_pentity_of_ent_offset;
    t.pfnEntOffsetOfPEntity             = pfn_ent_offset_of_pentity;
    t.pfnIndexOfEdict                   = pfn_index_of_edict;
    t.pfnPEntityOfEntIndex              = pfn_pentity_of_ent_index;
    t.pfnFindEntityByVars               = pfn_find_entity_by_vars;
    t.pfnGetModelPtr                    = pfn_get_model_ptr;
    t.pfnRegUserMsg                     = pfn_reg_user_msg;
    t.pfnAnimationAutomove              = pfn_animation_automove;
    t.pfnGetBonePosition                = pfn_get_bone_position;
    t.pfnFunctionFromName               = pfn_function_from_name;
    t.pfnNameForFunction                = pfn_name_for_function;
    t.pfnClientPrintf                   = pfn_client_printf;
    t.pfnServerPrint                    = pfn_server_print;
    t.pfnCmd_Args                       = pfn_cmd_args;
    t.pfnCmd_Argv                       = pfn_cmd_argv;
    t.pfnCmd_Argc                       = pfn_cmd_argc;
    t.pfnGetAttachment                  = pfn_get_attachment;
    t.pfnCRC32_Init                     = pfn_crc32_init;
    t.pfnCRC32_ProcessBuffer            = pfn_crc32_process_buffer;
    t.pfnCRC32_ProcessByte              = pfn_crc32_process_byte;
    t.pfnCRC32_Final                    = pfn_crc32_final;
    t.pfnRandomLong                     = pfn_random_long;
    t.pfnRandomFloat                    = pfn_random_float;
    t.pfnSetView                        = pfn_set_view;
    t.pfnTime                           = pfn_time;
    t.pfnCrosshairAngle                 = pfn_crosshair_angle;
    t.pfnLoadFileForMe                  = pfn_load_file_for_me;
    t.pfnFreeFile                       = pfn_free_file;
    t.pfnEndSection                     = pfn_end_section;
    t.pfnCompareFileTime                = pfn_compare_file_time;
    t.pfnGetGameDir                     = pfn_get_game_dir;
    t.pfnCvar_RegisterVariable          = pfn_cvar_register_variable;
    t.pfnFadeClientVolume               = pfn_fade_client_volume;
    t.pfnSetClientMaxspeed              = pfn_set_client_maxspeed;
    t.pfnCreateFakeClient               = pfn_create_fake_client;
    t.pfnRunPlayerMove                  = pfn_run_player_move;
    t.pfnNumberOfEntities               = pfn_number_of_entities;
    t.pfnGetInfoKeyBuffer               = pfn_get_info_key_buffer;
    t.pfnInfoKeyValue                   = pfn_info_key_value;
    t.pfnSetKeyValue                    = pfn_set_key_value;
    t.pfnSetClientKeyValue              = pfn_set_client_key_value;
    t.pfnIsMapValid                     = pfn_is_map_valid;
    t.pfnStaticDecal                    = pfn_static_decal;
    t.pfnPrecacheGeneric                = pfn_precache_generic;
    t.pfnGetPlayerUserId                = pfn_get_player_user_id;
    t.pfnBuildSoundMsg                  = pfn_build_sound_msg;
    t.pfnIsDedicatedServer              = pfn_is_dedicated_server;
    t.pfnCVarGetPointer                 = pfn_cvar_get_pointer;
    t.pfnGetPlayerWONId                 = pfn_get_player_won_id;
    t.pfnInfo_RemoveKey                 = pfn_info_remove_key;
    t.pfnGetPhysicsKeyValue             = pfn_get_physics_key_value;
    t.pfnSetPhysicsKeyValue             = pfn_set_physics_key_value;
    t.pfnGetPhysicsInfoString           = pfn_get_physics_info_string;
    t.pfnPrecacheEvent                  = pfn_precache_event;
    t.pfnPlaybackEvent                  = pfn_playback_event;
    t.pfnSetFatPVS                      = pfn_set_fat_pvs;
    t.pfnSetFatPAS                      = pfn_set_fat_pas;
    t.pfnCheckVisibility                = pfn_check_visibility;
    t.pfnDeltaSetField                  = pfn_delta_set_field;
    t.pfnDeltaUnsetField                = pfn_delta_unset_field;
    t.pfnDeltaAddEncoder                = pfn_delta_add_encoder;
    t.pfnGetCurrentPlayer               = pfn_get_current_player;
    t.pfnCanSkipPlayer                  = pfn_can_skip_player;
    t.pfnDeltaFindField                 = pfn_delta_find_field;
    t.pfnDeltaSetFieldByIndex           = pfn_delta_set_field_by_index;
    t.pfnDeltaUnsetFieldByIndex         = pfn_delta_unset_field_by_index;
    t.pfnSetGroupMask                   = pfn_set_group_mask;
    t.pfnCreateInstancedBaseline        = pfn_create_instanced_baseline;
    t.pfnCvar_DirectSet                 = pfn_cvar_direct_set;
    t.pfnForceUnmodified                = pfn_force_unmodified;
    t.pfnGetPlayerStats                 = pfn_get_player_stats;
    t.pfnAddServerCommand               = pfn_add_server_command;
    t.pfnVoice_GetClientListening       = pfn_voice_get_client_listening;
    t.pfnVoice_SetClientListening       = pfn_voice_set_client_listening;
    t.pfnGetPlayerAuthId                = pfn_get_player_auth_id;
    t.pfnSequenceGet                    = pfn_sequence_get;
    t.pfnSequencePickSentence           = pfn_sequence_pick_sentence;
    t.pfnGetFileSize                    = pfn_get_file_size;
    t.pfnGetApproxWavePlayLen           = pfn_get_approx_wave_play_len;
    t.pfnIsCareerMatch                  = pfn_is_career_match;
    t.pfnGetLocalizedStringLength       = pfn_get_localized_string_length;
    t.pfnRegisterTutorMessageShown      = pfn_register_tutor_message_shown;
    t.pfnGetTimesTutorMessageShown      = pfn_get_times_tutor_message_shown;
    t.pfnProcessTutorMessageDecayBuffer = pfn_process_tutor_message_decay_buffer;
    t.pfnConstructTutorMessageDecayBuffer =
        pfn_construct_tutor_message_decay_buffer;
    t.pfnResetTutorMessageDecayData     = pfn_reset_tutor_message_decay_data;
    t.pfnQueryClientCvarValue           = pfn_query_client_cvar_value;
    t.pfnQueryClientCvarValue2          = pfn_query_client_cvar_value2;
    t.pfnCheckParm                      = pfn_check_parm;
    t.pfnPEntityOfEntIndexAllEntities   = pfn_pentity_of_ent_index;

    // BUGCOMP_PENTITYOFENTINDEX_FLAG patch (sv_game.c:5250-5251).
    if ( peoei_broken )
        t.pfnPEntityOfEntIndex = pfn_pentity_of_ent_index_broken;

    return t;
}

} // namespace xash::server
