# xash3dpp_server — dedicated-server core (Chunk 6).
# Legacy reference: engine/server/ (sv_main.c, sv_game.c, sv_world.c,
# sv_phys.c, sv_pmove.c, sv_frame.c, sv_client.c + query/filter/log
# same-target satellites).
#
# ABI note: enginefuncs_t (159 slots) and entvars_t layout are FROZEN.
# Q-20: the ABI-exact edict array is the single store behind a zero-cost
# accessor seam; raw entvars_t access is confined to abi/ + the pmove
# bridge (+ the Chunk 8 save serializer).
#
# Subfolder plan (decided at scaffold time — see server-boundary.md):
#   abi/      — game-DLL ABI shim: DLL loader, GiveFnptrsToDll handshake,
#               enginefuncs_t population, DLL_FUNCTIONS resolution (S6);
#               EdictArena + string pool + typed accessor seam (S4, Q-20)
#   world/    — areanodes, SV_LinkEdict, SV_Move/SV_ClipMoveToEntity over
#               the map_loader trace kernel, lightstyles (S5)
#   physics/  — sv_phys movetypes/pushers + the sv_pmove bridge (S8)
#   clients/  — connection state machine, snapshots, multicast, user
#               messages, query.cpp/filter.cpp/log.cpp satellites (S9)

add_library(xash3dpp_server STATIC
    server.cpp
    abi/edict_arena.cpp
    abi/string_pool.cpp
    abi/game_dll.cpp
    abi/engine_table.cpp
    world/links.cpp
    world/hulls.cpp
    world/clip.cpp
    world/contents.cpp
    world/light.cpp
    lifecycle/precache.cpp
    lifecycle/game_host.cpp
    lifecycle/model_resolver.cpp
    lifecycle/world_hooks.cpp
    lifecycle/entity_parse.cpp
    lifecycle/spawn.cpp
    # physics/  sources land in S8
    # clients/  sources land in S9
)

target_include_directories(xash3dpp_server
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_server PUBLIC cxx_std_23)

target_link_libraries(xash3dpp_server
    PUBLIC  xash3dpp_utilities
    PUBLIC  xash3dpp_memory
    PUBLIC  xash3dpp_map_loader
    PUBLIC  xash3dpp_networking    # S7 — DeltaTables (Delta_Init per spawn)
    PUBLIC  xash3dpp_cmd_cvar      # S7 — state mirrors, FCVAR_EXTDLL dance
    PUBLIC  xash3dpp_filesystem    # S7 — delta.lst, resource lists (via delta)
    PRIVATE xash3dpp_core
    PRIVATE xash3dpp_platform      # S6 — dynlib load, time
    # Added as the slices land (see server-boundary.md Dependencies):
    # (networking netchan/master_list usage widens in S9)
)
