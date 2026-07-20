# xash3dpp_sound — audio engine core (Chunk 9). Slice S9.1: seams + types.
# Slice S9.2: IAudioCodec + WavCodec (soundlib decode layer, WAV only). Slice
# S9.3: the 12 mix kernels + paint pipeline core.  Slice S9.4: VOX sentence
# word-sequencer (sentences.txt parse/lookup/word-grammar, amplitude trim,
# the sentence pitch bend, the IVoxWordAdvance implementation).  Slice S9.5:
# room-effects DSP (RoomDsp : IRoomDsp — both preset tables + the SND-OQ-6
# padded sentinel row, AMod/Reverb/Delay/StereoDelay passes, delay-line state
# pool-owned per P-7).  The MPSC/SPSC thread topology lands in later slices
# (sound-boundary.md).
# ogg/opus/mp3 are separate satellite targets per the boundary Q-11 table —
# not linked here yet (codec.hpp reserves their rows).
# Legacy reference: engine/client/sound/, engine/platform/ (the SNDDMA seam),
# engine/{client,common}/soundlib/ (the decode layer).
# Slice S9.6 ("entry surface"): channel allocation/alter/spatialize
# (channel_alloc.cpp) + the sfx registry/lazy decode seam (registry.cpp),
# driving the S9.2-S9.5 components + the cmd_cvar census + a Filesystem-
# backed IAudioLoader.

add_library(xash3dpp_sound STATIC
    sound.cpp
    device.cpp
    codec_wav.cpp
    mixer.cpp
    vox.cpp
    dsp.cpp
    registry.cpp
    channel_alloc.cpp
)

target_include_directories(xash3dpp_sound
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_sound PUBLIC cxx_std_23)

target_link_libraries(xash3dpp_sound
    # utilities: Vec3 (ListenerSnapshot / provider seams); read_le/write_le +
    # crc32 (the WAV codec's chunk parse + broken-WAV allowlist, codec_wav.cpp);
    # atoi/atof (cvar/command argument parsing, sound.cpp).
    PUBLIC  xash3dpp_utilities
    # memory: the "sound" pool (create_pool / destroy_pool).
    PUBLIC  xash3dpp_memory
    # core: thread-role assertions + structured logging (PRIVATE — no public
    # header of xash3dpp_sound exposes a core type).
    PRIVATE xash3dpp_core
    # cmd_cvar: the S9.6 cvar/command census (PRIVATE — only sound.hpp's
    # forward-declared CmdCvarContext* is public; the full type is a .cpp
    # implementation detail).
    PRIVATE xash3dpp_cmd_cvar
    # filesystem: FilesystemAudioLoader's Filesystem::load_file (PRIVATE — same
    # forward-declared-pointer-only public surface as cmd_cvar above).
    PRIVATE xash3dpp_filesystem
    # platform: get_time (the dsp_profile command's Platform_DoubleTime,
    # s_dsp.c:902/906); S9.7b's spawn_thread rides the same edge.
    PRIVATE xash3dpp_platform
)
