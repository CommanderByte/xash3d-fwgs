// xash3dpp — fake game DLL test double (Chunk 6 S6).  Built as a MODULE
// library the loader tests dlopen at runtime; exercises the real export
// resolution, calling conventions, and handshake order across an actual
// DLL boundary (no hl.dll in CI).
//
// Variants (compile definitions set per CMake target):
//   (default)            — GiveFnptrsToDll + GetEntityAPI2 + GetEntityAPI
//                          + GetNewDLLFunctions, all succeeding (HLSDK
//                          exports both EntityAPI generations)
//   FAKE_NO_API2         — legacy-only DLL: no GetEntityAPI2 export
//   FAKE_NO_NEWAPI       — no GetNewDLLFunctions export
//   FAKE_NO_GIVEFNPTRS   — broken DLL: missing the handshake export
//   FAKE_API2_VERSION=N  — mimics a DLL built against interface N:
//                          GetEntityAPI2 writes N back and returns 0
//                          (the HLSDK version-mismatch behaviour)
//   FAKE_NEWAPI_FAIL     — GetNewDLLFunctions echoes version 2, returns 0

#include "fake_dll_state.hpp"

#include <xash3dpp/abi/entity_state.hpp>
#include <xash3dpp/abi/pm_defs.hpp> // playermove_t (P4 pfnPM_Move probe)
#include <xash3dpp/abi/usercmd.hpp>
#include <xash3dpp/abi/weaponinfo.hpp>

#include <cstdio>
#include <cstring>

#if defined( _WIN32 )
#define FAKE_EXPORT  extern "C" __declspec( dllexport )
#define FAKE_STDCALL __stdcall
#define FAKE_CDECL   __cdecl
#else
#define FAKE_EXPORT  extern "C" __attribute__(( visibility( "default" )))
#define FAKE_STDCALL
#define FAKE_CDECL
#endif

namespace abi = xash::abi;

static fake_dll::State g_state;

static void push_seq( int tag )
{
    if ( g_state.seq_len < 8 )
        g_state.seq[g_state.seq_len++] = tag;
}

// ---------------------------------------------------------------------------
// The DLL_FUNCTIONS / NEW_DLL_FUNCTIONS implementations the double hands
// back — only the slots the S6 tests drive.
// ---------------------------------------------------------------------------

static void fake_game_init( void )
{
    ++g_state.game_init_calls;
}

static void fake_copy( char *dst, std::size_t n, const char *src )
{
    if ( src == nullptr || n == 0 )
    {
        if ( n != 0 )
            dst[0] = '\0';
        return;
    }
    std::size_t i = 0;
    for ( ; src[i] != '\0' && i + 1 < n; ++i )
        dst[i] = src[i];
    dst[i] = '\0';
}

// Records every keyvalue and mimics a real game's KeyValue handler: the
// engine demands the game claim "classname" (fHandled), and stores angles/
// origin into the entvars the parse quirks read back.
static void fake_key_value( abi::edict_t *e, abi::KeyValueData *kvd )
{
    if ( g_state.kvd_len < 32 )
    {
        fake_dll::State::KvdRecord &r = g_state.kvds[g_state.kvd_len++];
        fake_copy( r.cls, sizeof( r.cls ), kvd->szClassName );
        fake_copy( r.key, sizeof( r.key ), kvd->szKeyName );
        fake_copy( r.val, sizeof( r.val ), kvd->szValue );
    }

    if ( std::strcmp( kvd->szKeyName, "classname" ) == 0 )
    {
        if ( g_state.engfuncs != nullptr &&
             g_state.engfuncs->pfnAllocString != nullptr )
            e->v.classname = g_state.engfuncs->pfnAllocString( kvd->szValue );
        kvd->fHandled = 1;
        return;
    }

    if ( std::strcmp( kvd->szKeyName, "angles" ) == 0 )
    {
        float a = 0.0f, b = 0.0f, c = 0.0f;
        std::sscanf( kvd->szValue, "%f %f %f", &a, &b, &c );
        e->v.angles[0] = a;
        e->v.angles[1] = b;
        e->v.angles[2] = c;
        kvd->fHandled = 1;
        return;
    }

    kvd->fHandled = 1;
}

static int fake_spawn( abi::edict_t *e )
{
    ++g_state.spawn_calls;

    // "trigger_reject" asks the engine to inhibit it (pfnSpawn == -1).
    if ( e != nullptr && g_state.engfuncs != nullptr &&
         g_state.engfuncs->pfnSzFromIndex != nullptr )
    {
        const char *cn = g_state.engfuncs->pfnSzFromIndex( e->v.classname );
        if ( cn != nullptr && std::strcmp( cn, "trigger_reject" ) == 0 )
            return -1;
    }
    return 0;
}

// pfnSetAbsBox (SetObjectCollisionBox): the standard origin ± bbox expansion.
static void fake_set_abs_box( abi::edict_t *e )
{
    ++g_state.set_abs_box_calls;
    for ( int i = 0; i < 3; ++i )
    {
        e->v.absmin[i] = e->v.origin[i] + e->v.mins[i];
        e->v.absmax[i] = e->v.origin[i] + e->v.maxs[i];
    }
}

static void fake_touch( abi::edict_t *, abi::edict_t * )
{
    ++g_state.touch_calls;
}

// pfnStartFrame: fires once per SV_Physics before the entity loop.
static void fake_start_frame( void )
{
    ++g_state.start_frame_calls;
}

// pfnThink: SV_RunThink dispatch when nextthink is due.
static void fake_think( abi::edict_t * )
{
    ++g_state.think_calls;
}

// pfnBlocked: a pusher hit an obstruction it could not move.
static void fake_blocked( abi::edict_t *, abi::edict_t * )
{
    ++g_state.blocked_calls;
}

// P4 pmove-run chain (SV_RunCmd) callbacks.
static void fake_cmd_start( const abi::edict_t *, const abi::usercmd_s *,
                            unsigned int random_seed )
{
    ++g_state.cmd_start_calls;
    g_state.cmd_start_seed = random_seed;
}

static void fake_cmd_end( const abi::edict_t * )
{
    ++g_state.cmd_end_calls;
}

static void fake_player_pre_think( abi::edict_t * )
{
    ++g_state.player_pre_think_calls;
}

static void fake_player_post_think( abi::edict_t * )
{
    ++g_state.player_post_think_calls;
}

// pfnPM_Move: the "motor".  Records the call, nudges origin[0] so the engine's
// SV_FinishPMove copyback is observable, and optionally stages a touch the
// SV_RunCmd dispatch loop must resolve to an edict + run through SV_Impact.
static void fake_pm_move( abi::playermove_s *ppmove, abi::qboolean server )
{
    ++g_state.pm_move_calls;
    g_state.pm_move_server = server;

    auto *pm = reinterpret_cast<abi::playermove_t *>( ppmove );
    if ( pm == nullptr )
        return;

    pm->origin[0] += g_state.pm_move_dx;

    if ( g_state.pm_move_inject_touch )
    {
        pm->numtouch          = 1;
        pm->touchindex[0]     = {};
        pm->touchindex[0].ent = g_state.pm_move_touch_ent;
        pm->touchindex[0].deltavelocity[0] = 111.0f;
        pm->touchindex[0].fraction         = 0.5f;
    }
}

// pfnServerActivate: the game DLL's per-map activation hook — records the
// edict/client counts the engine hands it (SV_ActivateServer).
static void fake_server_activate( abi::edict_t *, int edictCount, int clientMax )
{
    ++g_state.server_activate_calls;
    g_state.activate_edict_count = edictCount;
    g_state.activate_client_max  = clientMax;
}

static void fake_server_deactivate( void )
{
    ++g_state.server_deactivate_calls;
}

static const char *fake_game_description( void )
{
    return "Fake HL";
}

static int fake_get_hull_bounds( int hullnumber, float *mins, float *maxs )
{
    switch ( hullnumber )
    {
    case 0: // standing
        mins[0] = -16.0f; mins[1] = -16.0f; mins[2] = -36.0f;
        maxs[0] =  16.0f; maxs[1] =  16.0f; maxs[2] =  36.0f;
        return 1;
    case 1: // deliberately absent — the slot must stay zeroed
        return 0;
    case 2: // point hull
        mins[0] = mins[1] = mins[2] = 0.0f;
        maxs[0] = maxs[1] = maxs[2] = 0.0f;
        return 1;
    case 3: // large
        mins[0] = mins[1] = mins[2] = -32.0f;
        maxs[0] = maxs[1] = maxs[2] =  32.0f;
        return 1;
    default:
        return 0;
    }
}

static void fake_register_encoders( void )
{
    ++g_state.register_encoders_calls;
}

// S9 client-lifecycle callbacks (the connection state machine drives these).
static abi::qboolean fake_client_connect( abi::edict_t *, const char *,
                                          const char *, char szRejectReason[128] )
{
    ++g_state.client_connect_calls;
    if ( g_state.client_connect_should_reject )
    {
        fake_copy( szRejectReason, 128, "fake rejected you" );
        return 0;
    }
    return 1;
}

static void fake_client_put_in_server( abi::edict_t * )
{
    ++g_state.client_put_in_server_calls;
}

static void fake_client_command( abi::edict_t * )
{
    ++g_state.client_command_calls;
}

static void fake_client_userinfo_changed( abi::edict_t *, char * )
{
    ++g_state.client_userinfo_calls;
}

static void fake_client_disconnect( abi::edict_t * )
{
    ++g_state.client_disconnect_calls;
}

// S9 snapshot: the DLL fills each entity_state_t baseline (there is no engine-
// side SV_FillEntityState).  Minimal parity: number + model + origin/angles.
static void fake_create_baseline( int player, int eindex, abi::entity_state_t *state,
                                  abi::edict_t *ent, int playermodel,
                                  abi::vec3_t, abi::vec3_t )
{
    ++g_state.create_baseline_calls;
    if ( state == nullptr || ent == nullptr )
        return;
    state->number     = eindex;
    state->modelindex = player ? playermodel : ent->v.modelindex;
    for ( int i = 0; i < 3; ++i )
    {
        state->origin[i] = ent->v.origin[i];
        state->angles[i] = ent->v.angles[i];
    }
}

// pfnCreateInstancedBaselines: register one template so the roundtrip
// (DLL -> pfnCreateInstancedBaseline -> sv.instanced) is observable.
static void fake_create_instanced_baselines( void )
{
    ++g_state.create_instanced_calls;
    if ( g_state.engfuncs == nullptr )
        return;
    abi::entity_state_t base = {};
    base.modelindex = 42;
    g_state.engfuncs->pfnCreateInstancedBaseline( 7, &base );
}

// S9 snapshot gather: pfnSetupVisibility hands the engine the client's PVS/PHS
// (here NULL ⇒ fullvis, the simplest deterministic set); pfnAddToFullPack does
// BOTH the vis test and the entity_state_t fill and returns 1 to include.
static void fake_setup_visibility( abi::edict_t *, abi::edict_t *,
                                   unsigned char **pvs, unsigned char **pas )
{
    ++g_state.setup_visibility_calls;
    if ( pvs != nullptr )
        *pvs = nullptr; // fullvis
    if ( pas != nullptr )
        *pas = nullptr;
}

static int fake_add_to_full_pack( abi::entity_state_t *state, int e,
                                  abi::edict_t *ent, abi::edict_t *, int, int player,
                                  unsigned char * )
{
    ++g_state.add_to_full_pack_calls;
    if ( state == nullptr || ent == nullptr )
        return 0;
    std::memset( state, 0, sizeof( *state ));
    state->number     = e;
    state->entityType = abi::k_entity_normal;
    state->modelindex = ent->v.modelindex;
    ( void )player;
    for ( int i = 0; i < 3; ++i )
    {
        state->origin[i] = ent->v.origin[i];
        state->angles[i] = ent->v.angles[i];
    }
    return 1; // include every entity offered
}

// S9 send: pfnUpdateClientData fills the frame's clientdata; pfnGetWeaponData
// fills the 64-slot weapondata array (returns 1 when weapon prediction is on).
static void fake_update_client_data( const abi::edict_t *ent, int sendweapons,
                                     abi::clientdata_t *cd )
{
    ++g_state.update_client_data_calls;
    g_state.update_client_data_sendweapons = sendweapons;
    if ( cd == nullptr || ent == nullptr )
        return;
    cd->health = ent->v.health;
    for ( int i = 0; i < 3; ++i )
        cd->origin[i] = ent->v.origin[i];
}

static int fake_get_weapon_data( abi::edict_t *, abi::weapon_data_t *info )
{
    ++g_state.get_weapon_data_calls;
    if ( info == nullptr )
        return 0;
    info[0].m_iId  = 1;
    info[0].m_iClip = 30;
    return 1;
}

// ---------------------------------------------------------------------------
// Chunk 8 save/restore: a minimal CSave/CRestore over the SAVERESTOREDATA — one
// FIELD_FLOAT record ("hp") per entity, so the S8.7 server<->save bridges are
// exercised across the real DLL boundary (SAVERESTOREDATA projection, token
// interning into pTokens, pCurrentData/size advance, edict recreation).
// ---------------------------------------------------------------------------

static char k_hp_token[] = "hp"; // stable pointer stashed into pTokens

// HL-SDK CSaveRestoreBuffer::TokenHash de-facto ABI: rotate-right-4 XOR, linear
// probe into pSaveData->pTokens (must match xash::save::TokenTable).
static int fake_token_hash( abi::SAVERESTOREDATA *d, char *name )
{
    unsigned int hash = 0;
    for ( const char *p = name; *p != '\0'; ++p )
        hash = ( ( hash >> 4 ) | ( hash << 28 ) ) ^
               static_cast<unsigned int>( static_cast<int>( *p ) );
    if ( d->tokenCount <= 0 || d->pTokens == nullptr )
        return 0;
    const unsigned int start = hash % static_cast<unsigned int>( d->tokenCount );
    for ( int i = 0; i < d->tokenCount; ++i )
    {
        const int idx =
            static_cast<int>( ( start + static_cast<unsigned int>( i ) ) %
                              static_cast<unsigned int>( d->tokenCount ) );
        if ( d->pTokens[idx] == nullptr )
        {
            d->pTokens[idx] = name; // intern (the engine flattens this back)
            return idx;
        }
        if ( std::strcmp( d->pTokens[idx], name ) == 0 )
            return idx;
    }
    return 0;
}

// DispatchSave: write one [int16 size][int16 token][float value] field record.
static void fake_save( abi::edict_t *e, abi::SAVERESTOREDATA *d )
{
    ++g_state.save_calls;
    if ( e == nullptr || d == nullptr || d->pCurrentData == nullptr )
        return;
    const short idx    = static_cast<short>( fake_token_hash( d, k_hp_token ) );
    const short sz     = static_cast<short>( sizeof( float ) );
    const float health = e->v.health;
    g_state.saved_health = health;

    char *p = d->pCurrentData;
    std::memcpy( p, &sz, 2 );     p += 2;
    std::memcpy( p, &idx, 2 );    p += 2;
    std::memcpy( p, &health, 4 ); p += 4;
    d->pCurrentData = p;
    d->size += 8;
}

// DispatchRestore: read the record back and apply it (globalEntity mirrors the
// full/merge shouldPrecache flag).
static int fake_restore( abi::edict_t *e, abi::SAVERESTOREDATA *d, int globalEntity )
{
    ++g_state.restore_calls;
    g_state.restore_global = globalEntity;
    if ( d != nullptr )
        std::memcpy( g_state.restore_map_name, d->szCurrentMapName, sizeof( g_state.restore_map_name ) );
    if ( g_state.globals != nullptr )
        g_state.restore_changelevel = g_state.globals->changelevel;
    if ( e == nullptr || d == nullptr || d->pCurrentData == nullptr )
        return 0;
    char *p = d->pCurrentData;
    short sz = 0, idx = 0;
    float health = 0.0f;
    std::memcpy( &sz, p, 2 );     p += 2;
    std::memcpy( &idx, p, 2 );    p += 2;
    std::memcpy( &health, p, 4 ); p += 4;
    d->pCurrentData = p;
    e->v.health          = health;
    g_state.restored_health = health;
    return 1; // >= 0: a normal restore (a negative would FL_KILLME the edict)
}

static void fake_reset_global_state( void )
{
    ++g_state.reset_global_calls;
}

// pfnParmsChangeLevel: advertise `parms_conn_count` landmark connections back to
// a neighbouring level (fills the SAVERESTOREDATA the engine hung on globals).
static void fake_parms_change_level( void )
{
    ++g_state.parms_change_calls;
    if ( g_state.globals == nullptr || g_state.globals->pSaveData == nullptr )
        return;
    auto *d          = reinterpret_cast<abi::SAVERESTOREDATA *>( g_state.globals->pSaveData );
    d->connectionCount = g_state.parms_conn_count;
    for ( int i = 0; i < d->connectionCount && i < abi::k_max_level_connections; ++i )
    {
        fake_copy( d->levelList[i].mapName, sizeof( d->levelList[i].mapName ), "neighbor" );
        fake_copy( d->levelList[i].landmarkName, sizeof( d->levelList[i].landmarkName ), "lm" );
    }
}

static void fill_dll_functions( abi::DLL_FUNCTIONS *table )
{
    std::memset( table, 0, sizeof( *table ));
    table->pfnGameInit           = fake_game_init;
    table->pfnSpawn              = fake_spawn;
    table->pfnThink              = fake_think;
    table->pfnTouch              = fake_touch;
    table->pfnBlocked            = fake_blocked;
    table->pfnKeyValue           = fake_key_value;
    table->pfnSetAbsBox          = fake_set_abs_box;
    table->pfnServerActivate     = fake_server_activate;
    table->pfnServerDeactivate   = fake_server_deactivate;
    table->pfnStartFrame         = fake_start_frame;
    table->pfnCmdStart           = fake_cmd_start;
    table->pfnCmdEnd             = fake_cmd_end;
    table->pfnPlayerPreThink     = fake_player_pre_think;
    table->pfnPlayerPostThink    = fake_player_post_think;
    table->pfnPM_Move            = fake_pm_move;
    table->pfnGetGameDescription = fake_game_description;
    table->pfnGetHullBounds      = fake_get_hull_bounds;
    table->pfnRegisterEncoders   = fake_register_encoders;
    table->pfnClientConnect          = fake_client_connect;
    table->pfnClientPutInServer      = fake_client_put_in_server;
    table->pfnClientCommand          = fake_client_command;
    table->pfnClientUserInfoChanged  = fake_client_userinfo_changed;
    table->pfnClientDisconnect       = fake_client_disconnect;
    table->pfnCreateBaseline           = fake_create_baseline;
    table->pfnCreateInstancedBaselines = fake_create_instanced_baselines;
    table->pfnSetupVisibility          = fake_setup_visibility;
    table->pfnAddToFullPack            = fake_add_to_full_pack;
    table->pfnUpdateClientData         = fake_update_client_data;
    table->pfnGetWeaponData            = fake_get_weapon_data;
    table->pfnSave                     = fake_save;
    table->pfnRestore                  = fake_restore;
    table->pfnResetGlobalState         = fake_reset_global_state;
    table->pfnParmsChangeLevel         = fake_parms_change_level;
}

static void fake_game_shutdown( void )
{
    ++g_state.game_shutdown_calls;

    // S7 unload-order probe: pfnGameShutdown must still see the live cvar
    // chain (legacy PrepareToUnlink runs before it, the actual unlink
    // after — sv_game.c:5184-5202).  Writes .value on a test-owned struct
    // the lifecycle test reads back after the DLL is gone.  Null-guarded:
    // S6 handshake tests hand the DLL a zeroed table.
    if ( g_state.engfuncs != nullptr &&
         g_state.engfuncs->pfnCVarSetFloat != nullptr )
        g_state.engfuncs->pfnCVarSetFloat( "fake_shutdown_probe", 42.0f );
}

static void fake_on_free_private_data( abi::edict_t * )
{
    ++g_state.on_free_calls;
    if ( g_state.on_free_out != nullptr )
        ++*g_state.on_free_out;
}

// ---------------------------------------------------------------------------
// State access + handshake exports
// ---------------------------------------------------------------------------

FAKE_EXPORT fake_dll::State *fake_state( void )
{
    return &g_state;
}

FAKE_EXPORT void fake_reset( void )
{
    g_state = {};
}

// LINK_ENTITY-style per-classname spawn export.
FAKE_EXPORT void FAKE_CDECL fake_item( abi::entvars_t *pev )
{
    ++g_state.link_calls;
    pev->health = 123.0f;
}

// Entity-parse LINK exports (resolved by raw classname, like real DLLs).
FAKE_EXPORT void FAKE_CDECL worldspawn( abi::entvars_t * ) {}
FAKE_EXPORT void FAKE_CDECL info_player_start( abi::entvars_t * ) {}
FAKE_EXPORT void FAKE_CDECL trigger_reject( abi::entvars_t * ) {}

// The "custom" fallback export SV_AllocPrivateData resolves when a
// classname has no export of its own (custom-entity path).
FAKE_EXPORT void FAKE_CDECL custom( abi::entvars_t * )
{
    ++g_state.custom_link_calls;
}

// Drive the engine through the table received in GiveFnptrsToDll — the
// calls cross the DLL boundary exactly like a real game DLL's.
FAKE_EXPORT void fake_run_engine_probe( void )
{
    const abi::enginefuncs_t *ef = g_state.engfuncs;
    if ( ef == nullptr )
        return;

    g_state.probe_ran = 1;

    const int s = ef->pfnAllocString( "probe_string" );
    g_state.probe_string_ok =
        std::strcmp( ef->pfnSzFromIndex( s ), "probe_string" ) == 0 ? 1 : 0;

    abi::edict_t *ent = ef->pfnCreateEntity();
    if ( ent != nullptr )
    {
        g_state.probe_entity_index = ef->pfnIndexOfEdict( ent );
        g_state.probe_private = ef->pfnPvAllocEntPrivateData( ent, 17 );
        ef->pfnRemoveEntity( ent );
    }

    abi::CRC32_t crc = 0;
    ef->pfnCRC32_Init( &crc );
    ef->pfnCRC32_ProcessBuffer( &crc, "12345678", 8 );
    ef->pfnCRC32_ProcessByte( &crc, '9' );
    g_state.probe_crc = ef->pfnCRC32_Final( crc );

    g_state.probe_dedicated = ef->pfnIsDedicatedServer();
}

#if !defined( FAKE_NO_GIVEFNPTRS )
#if defined( _WIN32 ) && !defined( _WIN64 )
// On 32-bit MSVC, __stdcall + dllexport decorates the export to
// `_GiveFnptrsToDll@8` (2 pointer args = 8 bytes).  The real GoldSrc hl.dll
// exports it UNDECORATED via a .def; mirror that so the loader's
// GetProcAddress("GiveFnptrsToDll") resolves on x86 too.  (x64 has no
// decoration, so this alias is 32-bit-only.)
#pragma comment( linker, "/EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8" )
#endif
FAKE_EXPORT void FAKE_STDCALL GiveFnptrsToDll( abi::enginefuncs_t *engfuncs,
                                               abi::globalvars_t  *pGlobals )
{
    push_seq( fake_dll::k_seq_give_fnptrs );
    g_state.engfuncs = engfuncs;
    g_state.globals  = pGlobals;
}
#endif

#if !defined( FAKE_NO_API2 )
FAKE_EXPORT int GetEntityAPI2( abi::DLL_FUNCTIONS *pFunctionTable,
                               int *interfaceVersion )
{
    push_seq( fake_dll::k_seq_api2 );
    g_state.api2_version_in = *interfaceVersion;

#if defined( FAKE_API2_VERSION )
    // HLSDK behaviour when built against a different interface: write the
    // DLL's own version back and refuse.
    *interfaceVersion = FAKE_API2_VERSION;
    return 0;
#else
    if ( *interfaceVersion != abi::k_interface_version )
    {
        *interfaceVersion = abi::k_interface_version;
        return 0;
    }
    fill_dll_functions( pFunctionTable );
    return 1;
#endif
}
#endif

FAKE_EXPORT int GetEntityAPI( abi::DLL_FUNCTIONS *pFunctionTable,
                              int interfaceVersion )
{
    push_seq( fake_dll::k_seq_api );
    g_state.api_version_in = interfaceVersion;
    fill_dll_functions( pFunctionTable );
    return 1;
}

#if !defined( FAKE_NO_NEWAPI )
FAKE_EXPORT int GetNewDLLFunctions( abi::NEW_DLL_FUNCTIONS *pFunctionTable,
                                    int *interfaceVersion )
{
    push_seq( fake_dll::k_seq_new_api );
    g_state.newapi_version_in = *interfaceVersion;

#if defined( FAKE_NEWAPI_FAIL )
    *interfaceVersion = 2;
    return 0;
#else
    std::memset( pFunctionTable, 0, sizeof( *pFunctionTable ));
    pFunctionTable->pfnGameShutdown        = fake_game_shutdown;
    pFunctionTable->pfnOnFreeEntPrivateData = fake_on_free_private_data;
    return 1;
#endif
}
#endif
