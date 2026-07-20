# xash3dpp — server tests
# Slice subfolders (abi/, world/, physics/, clients/) will be registered
# from THIS file by relative source path — no nested CMakeLists
# (tests/map_loader precedent).
cmake_minimum_required(VERSION 3.16)

xash3dpp_add_test(test_server LIBS xash3dpp_server xash3dpp_core)

xash3dpp_add_test(test_edict_layout SOURCE abi/test_edict_layout.cpp LIBS xash3dpp_server)
# The layout test includes the ACTUAL legacy headers (engine/progdefs.h,
# engine/edict.h) for byte-exact offset parity; common/ resolves their
# guarded-off "const.h" include.
target_include_directories(test_edict_layout PRIVATE
    ${PROJECT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/../common
)
xash3dpp_add_test(test_edict_arena SOURCE abi/test_edict_arena.cpp LIBS xash3dpp_server xash3dpp_core)
xash3dpp_add_test(test_string_pool SOURCE abi/test_string_pool.cpp LIBS xash3dpp_server xash3dpp_core xash3dpp_utilities)

xash3dpp_add_test(test_eiface_layout SOURCE abi/test_eiface_layout.cpp LIBS xash3dpp_server)
# Same include dance as test_edict_layout: the REAL legacy headers
# (engine/eiface.h + common/cvardef.h) with their prerequisites shimmed.
target_include_directories(test_eiface_layout PRIVATE
    ${PROJECT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/../common
)

xash3dpp_add_test(test_pmove_layout SOURCE abi/test_pmove_layout.cpp LIBS xash3dpp_server)
# Same include dance: the REAL legacy header pm_shared/pm_defs.h (which pulls
# its sibling pm_info.h) for byte-exact pmove-struct offset parity.
target_include_directories(test_pmove_layout PRIVATE
    ${PROJECT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/../common
)

# S6 — fake game DLL doubles: MODULE libraries the loader test dlopens.
# One source, per-target defines select the export surface (see
# abi/fake_game_dll.cpp header comment).
foreach(variant full legacy nohandshake badver)
    add_library(fake_game_dll_${variant} MODULE abi/fake_game_dll.cpp)
    target_include_directories(fake_game_dll_${variant}
        PRIVATE ${PROJECT_SOURCE_DIR}/include
    )
    target_compile_features(fake_game_dll_${variant} PRIVATE cxx_std_23)
endforeach()
target_compile_definitions(fake_game_dll_legacy PRIVATE
    FAKE_NO_API2=1 FAKE_NO_NEWAPI=1)
target_compile_definitions(fake_game_dll_nohandshake PRIVATE
    FAKE_NO_GIVEFNPTRS=1)
target_compile_definitions(fake_game_dll_badver PRIVATE
    FAKE_API2_VERSION=138 FAKE_NEWAPI_FAIL=1)

xash3dpp_add_test(test_game_dll SOURCE abi/test_game_dll.cpp LIBS xash3dpp_server xash3dpp_core)
target_compile_definitions(test_game_dll PRIVATE
    FAKE_DLL_FULL="$<TARGET_FILE:fake_game_dll_full>"
    FAKE_DLL_LEGACY="$<TARGET_FILE:fake_game_dll_legacy>"
    FAKE_DLL_NOHANDSHAKE="$<TARGET_FILE:fake_game_dll_nohandshake>"
    FAKE_DLL_BADVER="$<TARGET_FILE:fake_game_dll_badver>"
)
add_dependencies(test_game_dll
    fake_game_dll_full fake_game_dll_legacy
    fake_game_dll_nohandshake fake_game_dll_badver
)

xash3dpp_add_test(test_engine_table SOURCE abi/test_engine_table.cpp
    LIBS xash3dpp_server xash3dpp_core xash3dpp_map_loader xash3dpp_utilities)
target_compile_definitions(test_engine_table PRIVATE
    FAKE_DLL_FULL="$<TARGET_FILE:fake_game_dll_full>"
)
add_dependencies(test_engine_table fake_game_dll_full)


# S7 — lifecycle: precache registries + game-DLL host orchestration.
xash3dpp_add_test(test_precache SOURCE lifecycle/test_precache.cpp
    LIBS xash3dpp_server xash3dpp_core)
# The lifecycle test drives a real CmdCvarContext (cmd_cvar_test_stubs is
# built by tests/cmd_cvar, processed earlier) and a real Filesystem tree
# with delta.lst.
xash3dpp_add_test(test_game_lifecycle SOURCE lifecycle/test_game_lifecycle.cpp
    LIBS xash3dpp_server xash3dpp_core xash3dpp_filesystem cmd_cvar_test_stubs)
target_compile_definitions(test_game_lifecycle PRIVATE
    FAKE_DLL_FULL="$<TARGET_FILE:fake_game_dll_full>"
)
add_dependencies(test_game_lifecycle fake_game_dll_full)

# S7b — entity-string parse (SV_SpawnEntities/LoadFromFile/ParseEdict),
# model resolver + SV_SetModel, over the real fake DLL + a minimal world.
xash3dpp_add_test(test_entity_parse SOURCE lifecycle/test_entity_parse.cpp
    LIBS xash3dpp_server xash3dpp_core xash3dpp_filesystem
         xash3dpp_map_loader xash3dpp_utilities cmd_cvar_test_stubs)
target_compile_definitions(test_entity_parse PRIVATE
    FAKE_DLL_FULL="$<TARGET_FILE:fake_game_dll_full>"
)
add_dependencies(test_entity_parse fake_game_dll_full)

# S7c — level orchestration (SV_SetupClients/SpawnServer/ActivateServer +
# the deactivate live path) driven end-to-end through MapLoader (real BSP on
# disk) + the fake DLL to ss_active and back to ss_dead.
xash3dpp_add_test(test_spawn_server SOURCE lifecycle/test_spawn_server.cpp
    LIBS xash3dpp_server xash3dpp_core xash3dpp_filesystem
         xash3dpp_map_loader xash3dpp_utilities cmd_cvar_test_stubs)
target_compile_definitions(test_spawn_server PRIVATE
    FAKE_DLL_FULL="$<TARGET_FILE:fake_game_dll_full>"
)
add_dependencies(test_spawn_server fake_game_dll_full)

# Chunk 8 S8.7 — save->load round trip through the fake game DLL's pfnSave/
# pfnRestore + the server<->save bridges, plus command registration/flags.
xash3dpp_add_test(test_save_restore SOURCE lifecycle/test_save_restore.cpp
    LIBS xash3dpp_server xash3dpp_save xash3dpp_core xash3dpp_filesystem
         xash3dpp_map_loader xash3dpp_utilities cmd_cvar_test_stubs)
target_compile_definitions(test_save_restore PRIVATE
    FAKE_DLL_FULL="$<TARGET_FILE:fake_game_dll_full>"
)
add_dependencies(test_save_restore fake_game_dll_full)

# S15 milestone smoke — load the REAL retail valve/dlls/hl.dll and run one map
# frame.  Asset-gated (XASH_HL_ROOT) + arch-gated (x86 only, hl.dll is 32-bit);
# a SKIP is a pass, so this stays green on CI / x64 / machines without HL.  No
# fake DLL: the game binary comes from the pointed-at Half-Life install.
xash3dpp_add_test(test_hl_smoke SOURCE lifecycle/test_hl_smoke.cpp
    LIBS xash3dpp_server xash3dpp_core xash3dpp_filesystem
         xash3dpp_map_loader xash3dpp_utilities cmd_cvar_test_stubs)

# S8 — physics driver + fixed-step frame loop over the fake DLL + a spawned
# world: settle frames, gravity, thinks, pusher linear move, Host_ServerFrame.
xash3dpp_add_test(test_physics SOURCE physics/test_physics.cpp
    LIBS xash3dpp_server xash3dpp_core xash3dpp_filesystem
         xash3dpp_map_loader xash3dpp_utilities cmd_cvar_test_stubs)
target_compile_definitions(test_physics PRIVATE
    FAKE_DLL_FULL="$<TARGET_FILE:fake_game_dll_full>"
)
add_dependencies(test_physics fake_game_dll_full)

# S9 — clients + messaging + satellites.  Self-contained unit tests first.
xash3dpp_add_test(test_info_string SOURCE clients/test_info_string.cpp
    LIBS xash3dpp_server xash3dpp_core xash3dpp_utilities)
xash3dpp_add_test(test_client_filter SOURCE clients/test_filter.cpp
    LIBS xash3dpp_server xash3dpp_core xash3dpp_networking)
xash3dpp_add_test(test_client_log SOURCE clients/test_log.cpp
    LIBS xash3dpp_server xash3dpp_core xash3dpp_utilities)
xash3dpp_add_test(test_client_messages SOURCE clients/test_messages.cpp
    LIBS xash3dpp_server xash3dpp_core xash3dpp_networking)

# S9 — connection state machine end-to-end against the fake DLL + a real map.
xash3dpp_add_test(test_client_state SOURCE clients/test_client_state.cpp
    LIBS xash3dpp_server xash3dpp_core xash3dpp_filesystem
         xash3dpp_map_loader xash3dpp_utilities xash3dpp_networking
         xash3dpp_platform cmd_cvar_test_stubs)
target_compile_definitions(test_client_state PRIVATE
    FAKE_DLL_FULL="$<TARGET_FILE:fake_game_dll_full>"
)
add_dependencies(test_client_state fake_game_dll_full)

# S9 seam — the server ↔ networking I/O bridge (SV_ReadPackets connectionless
# ingress + Netchan_OutOfBandPrint egress) over a real NetworkContext driven by
# a fake IPlatformSockets.  No game DLL / map needed — getchallenge only touches
# the challenge salt + realtime.
xash3dpp_add_test(test_net_io SOURCE clients/test_net_io.cpp
    LIBS xash3dpp_server xash3dpp_core xash3dpp_networking xash3dpp_platform)
