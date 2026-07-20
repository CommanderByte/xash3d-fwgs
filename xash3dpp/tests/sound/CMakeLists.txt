cmake_minimum_required(VERSION 3.16)

# xash3dpp — sound tests (Chunk 9, slice S9.1: seams + types)

# Vendored SoundAPI ABI layout parity — includes the REAL common/sound_api.h in
# a sealed namespace for byte-exact offset checks.  Same include dance as
# tests/server/abi/test_eiface_layout.cpp: repo root + common on the header path
# (their xash3d_types.h include is guarded off inside the test).
xash3dpp_add_test(test_sound_api_layout LIBS xash3dpp_sound)
target_include_directories(test_sound_api_layout PRIVATE
    ${PROJECT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/../common
)

# Sound lifecycle: create/init/shutdown, double-init reject, device wiring, stats.
xash3dpp_add_test(test_sound_lifecycle LIBS xash3dpp_sound xash3dpp_core)

# Device seam (NullDevice / SinkDevice) + SND-OQ-1 provider/sink shapes.
xash3dpp_add_test(test_sound_device LIBS xash3dpp_sound xash3dpp_core)

# IAudioCodec + WavCodec parity (slice S9.2): synthesized-in-test WAV images,
# no committed fixture files (tests/README no-committed-binaries rule).
xash3dpp_add_test(test_sound_codec LIBS xash3dpp_sound)

# Mixer kernels + paint pipeline (slice S9.3): per-kernel bit-exact vectors for
# all 12 variants, dispatch-table selection matrix, CLIP16 guard band, loop-wrap
# out_count edge math, mini paint-pipeline + gate matrix, the 0x40000000 reset.
xash3dpp_add_test(test_sound_mixer LIBS xash3dpp_sound)

# VOX sentence word-sequencer (slice S9.4): every embedded XASH_ENGINE_TESTS
# case from s_vox.c ported, plus grammar edge cases, the FreeWord zeroing pin,
# TrimStart/End vectors, the ModifyPitch float-chain pin, the single-slot
# overwrite quirk, sentence-terminates-on-bad-word, and a full 3-word sentence
# mix through vox_mix_channel_to_buffer (byte-pinned).
xash3dpp_add_test(test_sound_vox LIBS xash3dpp_sound)
