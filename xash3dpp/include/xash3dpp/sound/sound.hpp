#pragma once
// xash3dpp — Sound: audio engine core (Chunk 9, slice S9.1 — seams + types).
// Legacy reference: engine/client/sound/ (s_main.c S_Init/S_Shutdown lifecycle,
// the `snd` global s_main.c:39) + engine/platform/ (the SNDDMA seam).
// Boundary spec: docs/boundaries/sound-boundary.md (authoritative surface).
// Decision refs:
//   • Q-22 LIFECYCLE_MODEL — Sound is a pimpl class with an init/shutdown RAII
//     lifecycle; create_sound() is the convenience factory.
//   • Q-23 (device pull-shape) — output goes through the IAudioDevice seam.
//   • SND-OQ-1 — cross-thread listener/entity/gate data via P-2 snapshots +
//     the IEntitySpatialProvider / IMouthSink seams (providers.hpp).
//   • SND-OQ-5 — interleaved-stereo int16 ring payload.
//
// This slice wires ONLY the lifecycle + device open/close.  No mixer, no codec,
// no channel plumbing (S9.2/S9.3).  The class is EngineContext-embeddable
// (direct member, like Server/Host) AND constructible via create_sound().
//
// Slice S9.6 ("entry surface") layers the channel-based mixer's public API on
// top: register_sound/start_sound/start_local_sound/stop_sound/
// stop_all_sounds (s_main.c/s_load.c), update_frame (the ListenerSnapshot
// publish point — also polls DSP-relevant cvars, boundary §4.3), and
// channels_snapshot() (the P-4 typed introspection surface). See
// docs/boundaries/sound-boundary.md §Interface for the full legacy mapping.

#include <xash3dpp/abi/sound_api.hpp>
#include <xash3dpp/sound/device.hpp>
#include <xash3dpp/sound/errors.hpp>
#include <xash3dpp/sound/providers.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xash::filesystem { class Filesystem; }
namespace xash::cmd_cvar { class CmdCvarContext; }

namespace xash::sound {

// ---------------------------------------------------------------------------
// SoundStats (P-2) — mix-block counters published as whole-struct relaxed
// atomics; any-thread readers (future debug thread, G-3) snapshot-read without
// touching live channel/mix state (three-tier model, debug-stats-design.md).
// The underrun counter is Tier-1 always-on per threading-model §5.2.
// ---------------------------------------------------------------------------
struct SoundStats
{
    // ---- Tier 1 — always-on ---------------------------------------------
    std::atomic<std::uint64_t> mix_blocks      { 0 }; // paint blocks completed
    std::atomic<std::uint64_t> underruns       { 0 }; // ring underruns (§5.2 always-on)
    std::atomic<std::uint32_t> active_channels { 0 }; // channels mixed last block
    std::atomic<std::uint32_t> dropped_sounds  { 0 }; // start requests dropped (queue full / no channel)
    std::atomic<std::int32_t>  dsp_room        { 0 }; // current DSP room index (idsp_room)

#if XASH_STATS
    // ---- Tier 2 — bookkeeping (profiling builds) ------------------------
    std::atomic<std::uint32_t> peak_active_channels { 0 };
#endif

#if XASH_DEBUG_SOUND
    // ---- Tier 3 — dev-only heavy tracing (not yet defined) --------------
#endif
};

// ---------------------------------------------------------------------------
// SoundInitParams (Q-4) — injected dependencies + init-time config.  All
// pointers are non-owning and must outlive the Sound.
// ---------------------------------------------------------------------------
struct SoundInitParams
{
    // Output device (Q-23).  nullptr -> Sound owns an internal NullDevice (the
    // ratified "null audio device only" fallback); tests inject a SinkDevice.
    IAudioDevice *device = nullptr;            // @lifetime: caller (outlives Sound)

    // SND-OQ-1 seams.  nullptr on a headless / test load: no spatialization /
    // no mouth write-back, but the lifecycle still runs.
    IEntitySpatialProvider *spatial = nullptr; // @lifetime: caller (outlives Sound)
    IMouthSink             *mouth   = nullptr; // @lifetime: caller (outlives Sound)

    // Requested output format (defaults to 44.1 kHz / stereo / 16-bit).
    DeviceSpec spec {};

    // soundlib decode (WAV; MP3/OGG/Opus are separate satellite targets, not
    // linked here).  nullptr -> registered sounds always resolve to the
    // synthesized default (silent) sound (SfxRegistry's S_CreateDefaultSound
    // fallback) — a headless/test load still runs the full entry surface.
    ::xash::filesystem::Filesystem *filesystem = nullptr; // @lifetime: caller (outlives Sound)

    // cmd_cvar context (S9.6): the 11 cvars / 13 commands S_Init registers
    // (sound-boundary.md §Cvar/command census).  nullptr -> no cvar/command
    // registration (a headless/test load can still call start_sound() etc.
    // directly).
    ::xash::cmd_cvar::CmdCvarContext *cmd_cvar = nullptr; // @lifetime: caller (outlives Sound)
};

// ---------------------------------------------------------------------------
// ChannelInfo / channels_snapshot() (P-4) — the typed introspection surface
// replacing direct snd.channels[]/S_GetCurrentStaticSounds/
// S_GetCurrentDynamicSounds array pokes (sound-boundary.md §Interface). Main-
// thread read; a copyable snapshot, safe to hold past the next start/stop.
// ---------------------------------------------------------------------------
enum class ChannelClass : std::uint8_t
{
    Dynamic, // [NUM_AMBIENTS, MAX_DYNAMIC_CHANNELS) — includes the (unallocated
             // this slice) ambient sub-range [0, NUM_AMBIENTS)
    Static,  // [MAX_DYNAMIC_CHANNELS, total_channels)
    Raw,     // raw/voice streaming channels (allocation not wired this slice —
             // reserved for the P-4 surface's future completeness)
};

struct ChannelInfo
{
    std::string  sfx_name;              // registered sfx name (chan->name for a sentence, else the sfx's own name)
    int          entnum          = 0;   // entity soundsource
    Vec3         origin          {};    // world position (entity-relative channels: last resolved origin)
    int          left_vol        = 0;   // 0-255
    int          right_vol       = 0;   // 0-255
    ChannelClass channel_class   = ChannelClass::Dynamic;
    double       position_samples = 0.0; // playback position, in source frames (chan->sample)
};

// ---------------------------------------------------------------------------
// Sound — the audio engine (pimpl).
//
// @thread-safety: init/shutdown and every public entry run on ThreadRole::Main
// and assert it (T_Main; QN).  The device drives the mix off its own real-time
// thread once running (S9.2+).  The sole any-thread surface is stats(): the
// SoundStats Tier-1 counters are std::atomic and readable from any thread.
// ---------------------------------------------------------------------------
class Sound
{
public:
    Sound();
    ~Sound();

    Sound( const Sound & )            = delete;
    Sound &operator=( const Sound & ) = delete;

    Sound( Sound && ) noexcept;
    Sound &operator=( Sound && ) noexcept;

    // Lifecycle — main-thread only.  Opens the (injected or fallback null)
    // device.  Rejects a second init() without an intervening shutdown()
    // (SoundError::AlreadyInitialized).  Propagates the device open failure.
    [[nodiscard]] Result<void> init( const SoundInitParams &params );
    void                       shutdown();

    [[nodiscard]] bool initialized() const noexcept;

    [[nodiscard]] const SoundStats &stats() const noexcept;

    // The active output device (the injected one, or the owned NullDevice
    // fallback).  Null before init() / after shutdown().
    [[nodiscard]] IAudioDevice     *device() const noexcept;
    // The format negotiated at open time.  Zero-valued before init().
    [[nodiscard]] const DeviceCaps &caps() const noexcept;

    // -----------------------------------------------------------------------
    // Entry surface (Chunk 9, slice S9.6). Main-thread only; every entry
    // asserts ThreadRole::Main (QN). See sound-boundary.md §Interface for the
    // legacy S_* mapping cited on each method below.
    // -----------------------------------------------------------------------

    // S_RegisterSound (s_load.c:312-336). '!'-prefixed names route to VOX and
    // resolve LAZILY at play time (the immediate-sentence slot is written,
    // nothing is decoded here).  Returns -1 (k_invalid_sound_handle) on an
    // empty/oversized name or table overflow.
    [[nodiscard]] ::xash::abi::sound_t register_sound( std::string_view name ) noexcept;

    // S_StartSound (s_main.c:626-740). Handles BOTH the dynamic and static
    // (chan == k_chan_static) variants internally, exactly like legacy.
    // `pos` mirrors the legacy `const vec3_t pos` NULL-ability: nullopt ->
    // the last update_frame()'d listener view-origin (`refState.vieworg`).
    void start_sound( std::optional<Vec3> pos, int ent, int chan, ::xash::abi::sound_t handle, float fvol,
                      float attn, int pitch, std::uint32_t flags ) noexcept;

    // S_StartLocalSound (s_main.c:955-966): registers + plays on the
    // listener entnum; `reliable` -> CHAN_STATIC else CHAN_AUTO, always
    // ATTN_NONE/PITCH_NORM/SND_LOCALSOUND|SND_STOP_LOOPING.
    void start_local_sound( std::string_view name, float volume, bool reliable ) noexcept;

    // S_StopSound (s_main.c:1451-1458, GAME_EXPORT): resolves `soundname`
    // via the registry (creating a fresh, cache-less slot if unknown — same
    // as legacy) then S_AlterChannel(...,SND_STOP).
    void stop_sound( int entnum, int channel, std::string_view soundname ) noexcept;

    // S_StopAllSounds (s_main.c:1465-1492): resets to MAX_DYNAMIC_CHANNELS,
    // frees every channel (incl. VOX unbind), clears DSP state, and zeroes
    // the soundfade. `ambient` restarts the ambient channel init (this
    // slice: a no-op — ambient allocation is not wired, see the S9.6
    // uncertainty list).
    void stop_all_sounds( bool ambient ) noexcept;

    // S_UpdateFrame (s_main.c:1590-1598): publishes the listener pose this
    // frame's start_sound()/spatialize calls read (SND-OQ-1: the ONLY
    // listener-state source — never a live global). ALSO polls the DSP
    // room-selection input (ListenerSnapshot::waterlevel) into the owned
    // RoomDsp instance (boundary §4.3 "wire per-frame cvar polling").
    void update_frame( const ListenerSnapshot &snapshot ) noexcept;

    // channels_snapshot() (P-4) — S_GetCurrentStaticSounds/
    // S_GetCurrentDynamicSounds/the s_show precedent, typed. Returns every
    // currently-occupied channel (dynamic + static; raw channels are always
    // empty this slice — see ChannelClass::Raw).
    [[nodiscard]] std::vector<ChannelInfo> channels_snapshot() const;

private:
    // Console command handlers (cmd_add's CommandCtxFn — campaign B5). `user`
    // is `this` (a Sound*, cast back inside each handler). Declared as members
    // (not free functions) so they can reach the private Impl — see sound.cpp.
    // Argument access goes through the SAME CmdCvarContext instance that is
    // currently dispatching the handler (cmd_argc/cmd_argv's documented
    // "valid only during cbuf_execute dispatch" contract).
    static void cmd_play_f( void *user ) noexcept;
    static void cmd_play2_f( void *user ) noexcept;
    static void cmd_playvol_f( void *user ) noexcept;
    static void cmd_stopsound_f( void *user ) noexcept;
    static void cmd_music_f( void *user ) noexcept;
    static void cmd_soundlist_f( void *user ) noexcept;
    static void cmd_s_info_f( void *user ) noexcept;
    static void cmd_s_fade_f( void *user ) noexcept;
    static void cmd_soundfade_f( void *user ) noexcept;
    static void cmd_voicerecord_start_f( void *user ) noexcept;
    static void cmd_voicerecord_stop_f( void *user ) noexcept;
    static void cmd_speak_f( void *user ) noexcept;
    static void cmd_spk_f( void *user ) noexcept;
    static void cmd_dsp_profile_f( void *user ) noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ---------------------------------------------------------------------------
// create_sound (Q-22 factory) — construct a Sound and init() it.  Returns the
// owned instance on success, or nullptr if init() failed (the reason is logged
// via core::log by init()).
// ---------------------------------------------------------------------------
[[nodiscard]] std::unique_ptr<Sound> create_sound( const SoundInitParams &params );

} // namespace xash::sound
