# xash3dpp_save — save/restore codec core: token table + field-record framing
# (Chunk 8, slice S8.1).  Separate target per save-boundary.md §Satellite
# components (Q-11 "codec testability" verdict): the .sav format machinery is
# pure state->bytes and must be unit-testable WITHOUT a live server, edict
# arena, or game DLL.  Deliberately does NOT depend on xash3dpp_server — save is
# driven through the ILevelChangeExecutor seam (server-core) in a later slice.
# Legacy reference: engine/server/sv_save.c

add_library(xash3dpp_save STATIC
    token_table.cpp
    save_buffer.cpp
    field_sink.cpp
)

target_include_directories(xash3dpp_save
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_save PUBLIC cxx_std_23)

target_link_libraries(xash3dpp_save
    PUBLIC  xash3dpp_memory
    PUBLIC  xash3dpp_utilities
    PRIVATE xash3dpp_core
)
