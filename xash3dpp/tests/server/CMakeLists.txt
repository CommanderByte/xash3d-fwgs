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

xash3dpp_add_test(test_world_links SOURCE world/test_world_links.cpp LIBS xash3dpp_server xash3dpp_core xash3dpp_map_loader)
xash3dpp_add_test(test_world_trace SOURCE world/test_world_trace.cpp LIBS xash3dpp_server xash3dpp_core xash3dpp_map_loader xash3dpp_utilities)
xash3dpp_add_test(test_world_contents SOURCE world/test_world_contents.cpp LIBS xash3dpp_server xash3dpp_core xash3dpp_map_loader xash3dpp_utilities)
