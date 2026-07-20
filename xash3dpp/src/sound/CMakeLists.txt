# xash3dpp_sound — audio engine core (Chunk 9). Slice S9.1: seams + types.
# Slice S9.2: IAudioCodec + WavCodec (soundlib decode layer, WAV only). The
# channel mixer, VOX/mouth/DSP, and the MPSC/SPSC thread topology land in
# S9.3+ (sound-boundary.md).  ogg/opus/mp3 are separate satellite targets per
# the boundary Q-11 table — not linked here yet (codec.hpp reserves their rows).
# Legacy reference: engine/client/sound/, engine/platform/ (the SNDDMA seam),
# engine/{client,common}/soundlib/ (the decode layer).

add_library(xash3dpp_sound STATIC
    sound.cpp
    device.cpp
    codec_wav.cpp
)

target_include_directories(xash3dpp_sound
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_sound PUBLIC cxx_std_23)

target_link_libraries(xash3dpp_sound
    # utilities: Vec3 (ListenerSnapshot / provider seams); read_le/write_le +
    # crc32 (the WAV codec's chunk parse + broken-WAV allowlist, codec_wav.cpp).
    PUBLIC  xash3dpp_utilities
    # memory: the "sound" pool (create_pool / destroy_pool).
    PUBLIC  xash3dpp_memory
    # core: thread-role assertions + structured logging (PRIVATE — no public
    # header of xash3dpp_sound exposes a core type).
    PRIVATE xash3dpp_core
)
