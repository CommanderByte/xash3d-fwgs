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
