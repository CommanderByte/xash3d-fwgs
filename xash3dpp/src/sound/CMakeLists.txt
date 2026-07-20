# xash3dpp_sound — audio engine core (Chunk 9, slice S9.1: seams + types).
# The channel mixer, soundlib codec layer, VOX/mouth/DSP, and the MPSC/SPSC
# thread topology land in S9.2/S9.3 (sound-boundary.md).  ogg/opus/mp3 are
# separate satellite targets per the boundary Q-11 table — not linked here yet.
# Legacy reference: engine/client/sound/, engine/platform/ (the SNDDMA seam).

add_library(xash3dpp_sound STATIC
    sound.cpp
    device.cpp
)

target_include_directories(xash3dpp_sound
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_sound PUBLIC cxx_std_23)

target_link_libraries(xash3dpp_sound
    # utilities: Vec3 (ListenerSnapshot / provider seams).
    PUBLIC  xash3dpp_utilities
    # memory: the "sound" pool (create_pool / destroy_pool).
    PUBLIC  xash3dpp_memory
    # core: thread-role assertions + structured logging (PRIVATE — no public
    # header of xash3dpp_sound exposes a core type).
    PRIVATE xash3dpp_core
)
