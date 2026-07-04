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

xash3dpp_add_test(test_world_links SOURCE world/test_world_links.cpp LIBS xash3dpp_server xash3dpp_core xash3dpp_map_loader)
xash3dpp_add_test(test_world_trace SOURCE world/test_world_trace.cpp LIBS xash3dpp_server xash3dpp_core xash3dpp_map_loader xash3dpp_utilities)
xash3dpp_add_test(test_world_contents SOURCE world/test_world_contents.cpp LIBS xash3dpp_server xash3dpp_core xash3dpp_map_loader xash3dpp_utilities)
