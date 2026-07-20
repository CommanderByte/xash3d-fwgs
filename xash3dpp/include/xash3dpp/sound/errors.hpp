#pragma once
// xash3dpp — sound subsystem error codes and Result<T> alias (Chunk 9, slice S9.1)
// @thread-safety: pure types (SoundError enum + Result<T> alias) — no shared state
// Legacy reference: engine/client/sound/s_main.c (S_Init/SNDDMA_Init failure
// paths return silently — the null backend's SNDDMA_Init always returns false,
// s_stub.c).  The rewrite surfaces typed failures per the house Q-5 pattern.
//
// Mirrors the save/networking/content Result<T> precedent
// (include/xash3dpp/save/errors.hpp): a subsystem-local error enum plus a
// std::expected alias.  Do NOT call .value() — only .has_value()/operator* (Q-5).

#include <cstdint>
#include <expected>

namespace xash::sound {

// ---------------------------------------------------------------------------
// SoundError — typed failure modes for the sound engine + its device seam.
// ---------------------------------------------------------------------------

enum class SoundError : std::uint32_t
{
    DeviceUnavailable,   // IAudioDevice::open failed — no audio hardware / backend init
                         //   returned false (legacy SNDDMA_Init false, s_stub.c)
    BadFormat,           // the requested DeviceSpec (speed/width/channels) is unsupported
    DecodeFailed,        // soundlib could not decode a WAV/MP3/OGG/Opus payload (S9.3)
    QueueFull,           // the audio-command MPSC is full and the command could not be
                         //   enqueued (SND-OQ-3: STOP/CHANGE take the reserved fast lane)
    NotInitialized,      // a call was made before init() / after shutdown()
    AlreadyInitialized,  // init() called twice without an intervening shutdown()
};

// ---------------------------------------------------------------------------
// Result<T> — success-or-SoundError alias (std::expected<T, SoundError>).
// ---------------------------------------------------------------------------

template<typename T>
using Result = std::expected<T, SoundError>;

} // namespace xash::sound
