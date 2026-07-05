#pragma once
// xash3dpp — shared state record for the fake game DLL test double
// (Chunk 6 S6).  Included by fake_game_dll.cpp (the MODULE) and the
// loader tests; the test reads it back through the `fake_state` export.

#include <xash3dpp/abi/eiface.hpp>

namespace fake_dll {

// Handshake call-order tags pushed into State::seq.
inline constexpr int k_seq_give_fnptrs = 1;
inline constexpr int k_seq_new_api     = 2;
inline constexpr int k_seq_api2        = 3;
inline constexpr int k_seq_api         = 4;

struct State
{
    int seq[8];
    int seq_len;

    ::xash::abi::enginefuncs_t *engfuncs; // pointers received in GiveFnptrsToDll
    ::xash::abi::globalvars_t  *globals;

    int api2_version_in;   // *interfaceVersion on GetEntityAPI2 entry
    int api_version_in;    // GetEntityAPI by-value version parameter
    int newapi_version_in; // *interfaceVersion on GetNewDLLFunctions entry

    int game_init_calls;
    int game_shutdown_calls;
    int register_encoders_calls; // pfnRegisterEncoders (fires at load, S7)
    int on_free_calls;
    int link_calls; // fake_item LINK_ENTITY export invocations

    // S7b entity-parse probes.
    int spawn_calls;        // pfnSpawn invocations
    int set_abs_box_calls;  // pfnSetAbsBox invocations
    int touch_calls;        // pfnTouch invocations
    int custom_link_calls;  // "custom" LINK export invocations (custom-entity)

    // S7c level-orchestration probes.
    int server_activate_calls;   // pfnServerActivate invocations
    int server_deactivate_calls; // pfnServerDeactivate invocations
    int activate_edict_count;    // edictCount handed to the last ServerActivate
    int activate_client_max;     // clientMax handed to the last ServerActivate

    // S8 physics-frame probes.
    int start_frame_calls; // pfnStartFrame invocations (once per SV_Physics)
    int think_calls;       // pfnThink invocations (SV_RunThink dispatch)
    int blocked_calls;     // pfnBlocked invocations (pusher obstruction)

    // S9 client-lifecycle probes.
    int client_connect_calls;        // pfnClientConnect invocations
    int client_put_in_server_calls;  // pfnClientPutInServer invocations
    int client_command_calls;        // pfnClientCommand invocations
    int client_userinfo_calls;       // pfnClientUserInfoChanged invocations
    int client_disconnect_calls;     // pfnClientDisconnect invocations
    int client_connect_should_reject; // when set, pfnClientConnect returns 0

    // S9 snapshot-pipeline probes.
    int create_baseline_calls;   // pfnCreateBaseline invocations
    int create_instanced_calls;  // pfnCreateInstancedBaselines invocations
    int setup_visibility_calls;  // pfnSetupVisibility invocations
    int add_to_full_pack_calls;  // pfnAddToFullPack invocations
    int update_client_data_calls;       // pfnUpdateClientData invocations
    int update_client_data_sendweapons; // last sendweapons flag received
    int get_weapon_data_calls;          // pfnGetWeaponData invocations

    // Every pfnKeyValue the double receives (class/key/value snapshot) — the
    // parse test reads these back to pin the quirk transforms.
    struct KvdRecord
    {
        char cls[64];
        char key[64];
        char val[160];
    };
    KvdRecord kvds[32];
    int       kvd_len;

    // When set by the test, pfnOnFreeEntPrivateData also increments the
    // pointee — TEST-owned memory, so the S7 unload test can observe the
    // release sweep after the DLL itself is gone.
    int *on_free_out;

    // fake_run_engine_probe results — the double calling back INTO the
    // engine through the received table (cross-DLL slot exercise).
    int          probe_ran;
    int          probe_string_ok;   // AllocString → SzFromIndex roundtrip
    int          probe_entity_index; // CreateEntity → IndexOfEdict
    void        *probe_private;      // PvAllocEntPrivateData(17)
    unsigned int probe_crc;          // CRC32 quartet over "123456789"
    int          probe_dedicated;    // pfnIsDedicatedServer
};

using StateFn = State *( * )();

} // namespace fake_dll
