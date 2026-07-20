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
xash3dpp_add_test(test_sound_mixer LIBS xash3dpp_sound xash3dpp_core)

# VOX sentence word-sequencer (slice S9.4): every embedded XASH_ENGINE_TESTS
# case from s_vox.c ported, plus grammar edge cases, the FreeWord zeroing pin,
# TrimStart/End vectors, the ModifyPitch float-chain pin, the single-slot
# overwrite quirk, sentence-terminates-on-bad-word, and a full 3-word sentence
# mix through vox_mix_channel_to_buffer (byte-pinned).
xash3dpp_add_test(test_sound_vox LIBS xash3dpp_sound)

# Room-effects DSP (slice S9.5): both 29-row preset tables pinned (+ the
# SND-OQ-6 padded sentinel row), coeff-table switching, per-pass hand-derived
# integer-math vectors (AMod/Reverb/Delay/StereoDelay), a full RoomFX block
# through the S9.3 paint pipeline with a known preset (byte-pinned), ClearState
# semantics, and the room-selection matrix (waterlevel/room_type/off cases).
xash3dpp_add_test(test_sound_dsp LIBS xash3dpp_sound xash3dpp_memory xash3dpp_core)

# Entry surface (slice S9.6): channel allocation (SND_PickDynamicChannel/
# PickStaticChannel channel-steal order + every early-out), S_AlterChannel
# flag matches, hand-derived SND_Spatialize goldens (pan + attenuation),
# SfxRegistry lazy resolution (register does not resolve; play does),
# VoxSystem::time_left, and the full Sound entry surface end-to-end
# (start/stop/register/channels_snapshot + a command dispatched through a
# real CmdCvarContext proving the user-data plumbing).
xash3dpp_add_test(test_sound_entry LIBS xash3dpp_sound xash3dpp_cmd_cvar xash3dpp_core)

# Thread topology (slice S9.7b): the POD AudioCommand shape + the SND-OQ-3
# admission policy (STOP survives a START-saturated queue, asserted directly),
# the command round-trip to audible channel state, the T_AudioCallback underrun
# path (silence + the always-on counter), a deterministic decoder drive across
# the real MPSC/SPSC pair, the SND-OQ-2 stop-and-free race in a hard loop,
# 32 clean start/stop cycles, the shutdown order, and a threaded soak through
# the real Sound entry surface with debug asserts live.
#
# std::thread in the test itself needs -pthread on GCC/Clang (invisible on
# MSVC); xash3dpp_sound links Threads::Threads PUBLIC, but the test names it
# explicitly per the tests/core/CMakeLists.txt precedent.
find_package(Threads REQUIRED)
xash3dpp_add_test(test_sound_topology LIBS xash3dpp_sound xash3dpp_core Threads::Threads)

# Sink-device integration WITNESS (slice S9.8): a scripted console-command
# sequence (G-5 stage-(a) shape) driven through a real CmdCvarContext into the
# as-built S9.7b topology and out through SinkDevice, producing PCM that is
# byte-identical across repeated runs and pinned against a committed hash so an
# x64-vs-x86 divergence FAILS. Externally driven on both ends (no decoder
# thread, no pump thread, no wall clock); synthetic integer-generated source
# audio via the injected IAudioLoader — no filesystem, no retail assets.
xash3dpp_add_test(test_sound_witness LIBS xash3dpp_sound xash3dpp_cmd_cvar xash3dpp_core Threads::Threads)
