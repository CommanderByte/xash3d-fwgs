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

static int fake_spawn( abi::edict_t * )
{
    return 0;
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

static void fill_dll_functions( abi::DLL_FUNCTIONS *table )
{
    std::memset( table, 0, sizeof( *table ));
    table->pfnGameInit           = fake_game_init;
    table->pfnSpawn              = fake_spawn;
    table->pfnGetGameDescription = fake_game_description;
    table->pfnGetHullBounds      = fake_get_hull_bounds;
}

static void fake_game_shutdown( void )
{
    ++g_state.game_shutdown_calls;
}

static void fake_on_free_private_data( abi::edict_t * )
{
    ++g_state.on_free_calls;
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
