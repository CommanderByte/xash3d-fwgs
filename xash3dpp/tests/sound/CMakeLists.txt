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
