#pragma once
// xash3dpp — the T_Main -> T_AudioDecoder command stream (Chunk 9, slice
// S9.7b).  Legacy reference: NONE — legacy is single-threaded (every
// S_StartSound/S_StopSound/S_AlterChannel/S_UpdateFrame call mutates
// `snd.channels[]` in place on T_Main, behind the SNDDMA_BeginPainting/Submit
// lock).  This header is the rewrite's replacement for that in-place mutation.
//
// Boundary spec: docs/boundaries/sound-boundary.md §Threading (ratified
// topology + the hazard table row "S_StartSound/S_RestoreSound/S_StopSound/
// S_AlterChannel mutation entry points -> crosses MPSC (P-1)"), Open questions
// SND-OQ-1 (providers read ONLY on T_Main) and SND-OQ-3 (queue-full policy).
// Design: threading-model.md §3.4/§5.1, thread-spawn-and-inbox-brief.md §3.1.
//
// ===========================================================================
// The split (SND-OQ-1, binding)
// ---------------------------------------------------------------------------
// T_Main owns: the sfx registry (name -> handle -> decoded AudioData), the
//   IEntitySpatialProvider / IMouthSink seams, the cvar poll, and the listener
//   pose.  It resolves EVERY provider- and registry-dependent value, packs the
//   results into a POD AudioCommand, and enqueues.
// T_AudioDecoder owns: the mix-side channel array (Mixer::channels()), the VOX
//   per-channel bindings, the DSP delay lines, the mix clock, and the paint.
//   It applies commands with the EXACT S9.6 legacy sequence (alter prologue ->
//   pick -> init -> bind source -> spatialize -> audibility drop) — the code in
//   audio_command.cpp is the single implementation BOTH modes run, so the
//   single-threaded (topology-off) path and the threaded path cannot diverge.
//
// The only step hoisted out of that sequence is
// `IEntitySpatialProvider::resolve_origin()` (SND-OQ-1 forbids it off T_Main).
// Its RESULT ships in the command and `spatialize_with_origin()`
// (channel_alloc.hpp) runs the remaining pure pan/attenuation math at the
// legacy call site on the decoder — bit-identical, because spatialize()'s
// inputs (entnum / static flag / origin / dist_mult / master_vol / listener)
// are all known before the channel is picked.
// ===========================================================================
//
// @thread-safety: AudioCommand is a trivially-copyable POD (static_assert
// below) — memcpy-safe across the MpscQueue with no allocation.
// AudioCommandQueue is multi-producer (T_Main today) / single-consumer
// (T_AudioDecoder).  ChannelApplyContext and apply_command() are confined to
// whichever thread owns the channel array: T_AudioDecoder when the topology
// runs, T_Main when it does not (assert sites live at the Sound entry points
// and at the decoder loop entry, not here — see topology.hpp).

#include <xash3dpp/abi/sound_api.hpp>
#include <xash3dpp/core/mpsc_queue.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/private/sound/dsp.hpp>
#include <xash3dpp/private/sound/mixer.hpp>
#include <xash3dpp/private/sound/registry.hpp>
#include <xash3dpp/private/sound/vox.hpp>
#include <xash3dpp/sound/constants.hpp> // k_snd_* (command_class)
#include <xash3dpp/sound/providers.hpp>
#include <xash3dpp/sound/sound.hpp> // SoundStats

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace xash::sound {

// ---------------------------------------------------------------------------
// AudioCommandType — the T_Main -> T_AudioDecoder mutation surface.
//
// `AlterChannel` covers BOTH the stop and the change legs, exactly like
// legacy's single `S_AlterChannel(entnum, channel, sfx, vol, pitch, flags)`
// (s_main.c:514) — which leg runs is decided by the SND_STOP / SND_CHANGE_VOL /
// SND_CHANGE_PITCH bits the command carries.  `Sound::stop_sound()` emits it
// with k_snd_stop.  S_StartSound's OWN alter prologue (s_main.c:643-648) is not
// a separate command: it is part of `StartSound`'s handler, so the
// alter-then-maybe-start fallthrough stays one indivisible decoder-side
// operation, as it is in legacy.
// ---------------------------------------------------------------------------
enum class AudioCommandType : std::uint8_t
{
    None = 0,
    StartSound,    // S_StartSound (s_main.c:626) — incl. its alter prologue
    AlterChannel,  // S_AlterChannel (s_main.c:514) — stop AND change legs
    StopAllSounds, // S_StopAllSounds (s_main.c:1465)
    FrameUpdate,   // S_UpdateFrame (s_main.c:1590) + the §4.3 per-frame cvar poll
    FlushEpoch,    // SND-OQ-2 quiesce fence (no legacy analogue)
};

// SND-OQ-3 admission class.  Reserved-class commands ride
// MpscQueue::push_reserved_class and can therefore never be refused by a
// START-saturated normal region.
enum class AudioCommandClass : std::uint8_t
{
    Normal,   // may be refused when the normal region is full (producer blocks, then drops)
    Reserved, // STOP/CHANGE/flush fast lane — never dropped
};

// ---------------------------------------------------------------------------
// MixConfigSnapshot (P-2) — the per-frame T_Main-side configuration the
// decoder-owned Mixer / RoomDsp need.  Legacy read these live off cvars and
// cl.* inside the mix loop; SND-OQ-1 makes them a one-way POD.  Field defaults
// mirror the registered cvar defaults (sound.cpp's census).
// ---------------------------------------------------------------------------
struct MixConfigSnapshot
{
    // Mixer
    bool  lerping = false; // s_lerping

    // RoomDsp setters (boundary §4.3 per-frame poll)
    bool  room_off        = false;
    float dsp_coeff_table = 0.0f;
    float room_type       = 0.0f;
    float waterroom_type  = 14.0f;
    int   hisound         = 2;
    float room_mod        = 0.0f;
    float room_lp         = 0.0f;
    float room_rvblp      = 1.0f;
    float room_refl       = 0.0f;
    float room_dlylp      = 1.0f;
    float room_feedback   = 0.2f;
    float room_size       = 0.0f;
    float room_delay      = 0.8f;
    float room_left       = 0.0f;

    // Paint-time gates + gain (S_PaintChannels' arguments).
    //
    // master_volume IS wired (parity F-5 / conformance F4): Sound::Impl::
    // poll_mix_config() derives it with master_volume_from() below — the
    // `volume` cvar and the snd_mute_losefocus focus gate.  Its 1.0f default is
    // what a headless/no-cvar load keeps (cvars_polled == false).
    //
    // STILL UNWIRED, deliberately (see master_volume_from()'s XASH3DPP-STUB
    // notes and sound.cpp's):
    //   • `gate` has NO producer at all — nothing in the tree supplies
    //     host.status / cls.key_dest / cl.paused / cl.background /
    //     CL_IsInGame() / Host_IsSinglePlayerGame(), so every gate stays false
    //     (which is the "in game, not paused, not in menu" shape the paint
    //     expects) and the focus mute is live but dormant;
    //   • `gate.soundfade_gain` / the S_UpdateSoundFade curve;
    //   • `pitch_mult`, the FWGS sys_timescale chipmunk multiplier.
    // The SEAM is here so a later slice fills them in without touching the
    // topology.
    MixGateSnapshot gate           {};
    float           master_volume  = 1.0f;
    double          pitch_mult     = 1.0;

    // false == the owning Sound has no CmdCvarContext (headless/test load):
    // the decoder leaves Mixer/RoomDsp configuration untouched, exactly like
    // S9.6's `if (impl_->cmd_cvar_ != nullptr)` guard.
    bool cvars_polled = false;
};

// ---------------------------------------------------------------------------
// master_volume_from — S_GetMasterVolume (s_main.c:115-135), ported term by
// term in legacy's exact float order of operations:
//
//   float scale = 1.0f;
//   if( host.status == HOST_NOFOCUS && snd_mute_losefocus.value != 0.0f )
//       return 0.0f;                                             // :119-123
//   if( cls.key_dest != key_menu && soundfade.percent != 0 )
//   {
//       scale = soundfade.percent / 100.f;
//       scale = bound( 0.0f, scale, 1.0f );
//       scale = 1.0f - scale;                                    // :127-132
//   }
//   return s_volume.value * scale;                               // :134
//
// The focus mute SHORT-CIRCUITS (it returns before the soundfade branch is even
// considered), which is why it is a `return` here too and not a multiply.
//
// `soundfade_scale` is legacy's `scale` — 1.0f means "no fade".
// XASH3DPP-STUB(chunk12): nothing in the tree can pass anything but 1.0f yet.
// The producer would be S_UpdateSoundFade (s_main.c:220-256), the per-frame
// fade CURVE, which is not implemented anywhere: cmd_s_fade_f/cmd_soundfade_f
// only latch FadeState::start_percent/active, and MixGateSnapshot::
// soundfade_gain has no writer. The parameter exists so wiring the curve later
// needs no change to this function or to its call site's shape.
// ---------------------------------------------------------------------------
[[nodiscard]] constexpr float master_volume_from( float volume_cvar, bool lost_focus, float mute_losefocus_cvar,
                                                  float soundfade_scale = 1.0f ) noexcept
{
    if( lost_focus && mute_losefocus_cvar != 0.0f )
        return 0.0f; // "we return zero volume to keep sounds running" (s_main.c:119-123)
    return volume_cvar * soundfade_scale; // s_main.c:134
}

// ---------------------------------------------------------------------------
// AudioCommand — the POD message.  Deliberately FLAT (not a union): every
// field is named and independently greppable, and the whole struct is well
// under a cache-line-multiple footprint that matters at these capacities
// (sound_command_queue_capacity + reserve slots, allocated once).
// ---------------------------------------------------------------------------
struct AudioCommand
{
    AudioCommandType type = AudioCommandType::None;

    // ---- StartSound / AlterChannel -------------------------------------
    ::xash::abi::sound_t sfx_handle = k_invalid_sound_handle;
    int           entnum     = 0;   // soundsource entity
    int           entchannel = 0;   // CHAN_*
    int           vol        = 0;   // bound(0, fvol*255, 255) — s_main.c:638 (T_Main)
    int           pitch      = 0;   // "Invasion issues" <=1 -> PITCH_NORM fixup — s_main.c:639 (T_Main)
    std::uint32_t flags      = 0;   // SND_*
    float         dist_mult  = 0.0f;// attn / SND_CLIP_DISTANCE — s_main.c:683 (T_Main)
    Vec3          origin     {};    // use_pos: caller pos, else the listener view-origin (s_main.c:651)

    // SND-OQ-1: IEntitySpatialProvider::resolve_origin()'s result, taken on
    // T_Main.  `entity_origin_valid == false` reproduces
    // `!CL_GetEntitySpatialization(ch)` (s_main.c:580-585 -> volumes zeroed).
    // Only meaningful when spatialize() would actually consult the provider
    // (non-static channel whose entnum != the listener's); T_Main skips the
    // call otherwise, exactly like spatialize()'s own early-outs.
    //
    // IN/OUT ON THE T_MAIN SIDE (F-8): `entity_origin` is PRE-FILLED with
    // `origin` above (the channel's origin-to-be — the server-supplied `pos`
    // when there was one) before resolve_origin() is called, because the
    // provider is contractually allowed to return true WITHOUT writing it
    // (absent/unparsed entity — cl_frame.c:1387-1392; see providers.hpp).
    // What crosses the queue is therefore "the origin to spatialize against",
    // not "the origin the provider produced".
    bool          entity_origin_valid = false;
    Vec3          entity_origin       {};

    // S_TestSoundChar(sfx->name, '!') — resolved on T_Main (needs the registry).
    bool          sfx_is_sentence = false;
    // sfx_t::name, NUL-terminated.  Used for chan->name (sentences,
    // s_main.c:700), the alter path's own '!' test (s_main.c:520) and the
    // "dropped sound" diagnostic (s_main.c:664).
    std::array<char, ::xash::limits::sound_command_name_max> sfx_name {};

    // Decoded audio for a PLAIN (non-sentence) channel — S_LoadSound's result,
    // resolved on T_Main (s_main.c:707).  Sentences leave this null: VOX
    // resolves per word at word-advance time on the decoder.
    // @lifetime: BORROWED.  Owned by SfxRegistry on T_Main; the decoder may
    //   only hold it until the next completed SND-OQ-2 flush (see topology.hpp).
    const AudioData *source = nullptr;

    // ---- FrameUpdate / StartSound --------------------------------------
    // The listener pose the command was built against.  Carried on StartSound
    // too (not just FrameUpdate) so the decoder spatializes against EXACTLY the
    // pose T_Main resolved the entity origin with — legacy is synchronous, so
    // any skew here would be a rewrite-only artefact.
    ListenerSnapshot listener {};

    // ---- FrameUpdate ----------------------------------------------------
    MixConfigSnapshot mix_config {};

    // ---- StopAllSounds --------------------------------------------------
    bool ambient = false; // S_StopAllSounds(bool ambient) — accepted, no-op (S9.6 parity)

    // ---- FlushEpoch -----------------------------------------------------
    std::uint64_t epoch = 0; // SND-OQ-2 generation being fenced
};

static_assert( std::is_trivially_copyable_v<AudioCommand>,
    "AudioCommand must be a trivially-copyable POD — MpscQueue memcpy-transfers it "
    "with no allocation on the enqueue path (thread-spawn-and-inbox-brief §3.1)." );
static_assert( std::is_trivially_copyable_v<ListenerSnapshot> );
static_assert( std::is_trivially_copyable_v<MixGateSnapshot> );
static_assert( std::is_trivially_copyable_v<MixConfigSnapshot> );

// Copy `name` into a command's fixed name buffer (always NUL-terminated,
// truncating like Q_strncpy).
void set_command_name( AudioCommand &cmd, std::string_view name ) noexcept;

// The SND-OQ-3 class of a command.  Everything that is not a plain "play this
// sound" rides the reserved lane: STOP and CHANGE (a dropped SND_STOP is
// AUDIBLE — the sound keeps playing when the caller expected silence), the
// flush fence (its ack is what T_Main waits on, so losing it turns the bounded
// wait into a timeout), and FrameUpdate (a CONTROL message with no legacy
// analogue for being lost — see its case below).  Only a plain StartSound is
// droppable, which is SND-OQ-3's own stated preference.
[[nodiscard]] constexpr AudioCommandClass command_class( const AudioCommand &cmd ) noexcept
{
    switch( cmd.type )
    {
    case AudioCommandType::AlterChannel:
    case AudioCommandType::StopAllSounds:
    case AudioCommandType::FlushEpoch:
        return AudioCommandClass::Reserved;
    case AudioCommandType::FrameUpdate:
        // CONTROL, not a sound (parity F-4).  FrameUpdate carries the per-frame
        // listener pose, the waterlevel -> RoomDsp handoff and the §4.3 cvar
        // poll; legacy's S_UpdateFrame is a direct call that CANNOT be lost.
        // Admitting it in the normal class made it silently refusable under a
        // START-saturated queue, which would freeze the decoder's whole view of
        // the world (stale pose -> wrong pan, stale waterlevel -> wrong DSP
        // room) for as long as the flood lasted.
        return AudioCommandClass::Reserved;
    case AudioCommandType::StartSound:
        // A START that CARRIES stop/change bits runs the alter prologue first
        // (s_main.c:643-648) — it is a stop/change request in START's clothing
        // and must not be droppable either.
        return ( cmd.flags & ( k_snd_stop | k_snd_change_vol | k_snd_change_pitch ) ) != 0
                   ? AudioCommandClass::Reserved
                   : AudioCommandClass::Normal;
    case AudioCommandType::None:
        break;
    }
    return AudioCommandClass::Normal;
}

// ---------------------------------------------------------------------------
// AudioCommandQueue — MpscQueue + the SND-OQ-3 admission POLICY.  The primitive
// itself never blocks and never drops silently (thread-spawn-and-inbox-brief
// §3.1: "full-queue behaviour is POLICY-PARAMETERIZED by the consumer
// subsystem"); this type IS sound's policy, kept in one named place so it is
// directly testable without a live decoder.
//
// Policy:
//   • Reserved class  -> push_reserved_class(). Cannot be refused while the
//     reserve has room; the reserve is untouchable by the normal class, so a
//     queue saturated with STARTs still admits every STOP.
//   • Normal class    -> try_push(); on refusal the producer BLOCKS (yield
//     spin, bounded by limits::sound_command_push_spin_max) rather than
//     dropping.  Only when that bound is exhausted is the command genuinely
//     dropped — and only THAT case is counted in SoundStats::dropped_sounds.
//     The bound exists so a wedged decoder degrades to dropped audio instead of
//     a hung frame loop (a hard assert here would turn a stalled audio thread
//     into a process abort).
//   • set_accepting(false) is shutdown step (a): the queue refuses everything,
//     so no command can be admitted after the decoder has been told to stop.
//     Refusals in that state are NOT counted as drops (the pipeline is going
//     away).
//
// @thread-safety: submit() from any producer thread; try_pop() from the single
// consumer (T_AudioDecoder).
// ---------------------------------------------------------------------------
class AudioCommandQueue
{
public:
    using Queue = ::xash::core::MpscQueue<AudioCommand, ::xash::limits::sound_command_queue_capacity,
                                          ::xash::limits::sound_command_queue_reserve>;

    AudioCommandQueue() noexcept                            = default;
    AudioCommandQueue( const AudioCommandQueue & )            = delete;
    AudioCommandQueue &operator=( const AudioCommandQueue & ) = delete;

    // Enqueue under the SND-OQ-3 policy.  Returns false only when the command
    // was genuinely refused (queue closed, or a normal-class command that
    // outlasted the block bound); `out_dropped` distinguishes the two so the
    // caller only bumps dropped_sounds for a real drop.
    [[nodiscard]] bool submit( const AudioCommand &cmd, bool *out_dropped = nullptr ) noexcept;

    // Single-consumer dequeue (T_AudioDecoder).  FIFO — a reserved-class
    // command does NOT jump ahead of earlier normal-class ones; the reserve is
    // an ADMISSION relaxation, not a priority lane.  (That FIFO property is
    // what makes the SND-OQ-2 flush fence total: by the time FlushEpoch is
    // popped, every command submitted before it has already been applied.)
    [[nodiscard]] bool try_pop( AudioCommand &out ) noexcept { return queue_.try_pop( out ); }

    // Shutdown step (a).  Idempotent; safe from any thread.
    void set_accepting( bool on ) noexcept { accepting_.store( on, std::memory_order_release ); }
    [[nodiscard]] bool accepting() const noexcept { return accepting_.load( std::memory_order_acquire ); }

    [[nodiscard]] std::size_t occupancy() const noexcept { return queue_.occupancy(); }
    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Queue::capacity(); }
    [[nodiscard]] static constexpr std::size_t reserve_capacity() noexcept { return Queue::reserve_capacity(); }

private:
    Queue             queue_ {};
    std::atomic<bool> accepting_ { true };
};

// ---------------------------------------------------------------------------
// ChannelApplyContext — everything apply_command() mutates or reads.  Exactly
// one instance exists per Sound; whichever thread owns the channel array uses
// it (T_AudioDecoder while the topology runs, T_Main otherwise) — never both.
//
// @lifetime: every pointer is borrowed from Sound::Impl and outlives the
//   context.
// ---------------------------------------------------------------------------
struct ChannelApplyContext
{
    Mixer      *mixer    = nullptr; // owns channels() — the snd.channels[] equivalent
    VoxSystem  *vox      = nullptr; // per-channel sentence bindings
    RoomDsp    *room_dsp = nullptr; // SX_ClearState on StopAllSounds + the §4.3 setters
    SoundStats *stats    = nullptr;

    // The IVoxAudioResolver VOX word-advance resolves through.  When the
    // topology runs this is a lock-guarded adapter over SfxRegistry (the
    // registry itself stays T_Main-owned) — see sound.cpp.
    IVoxAudioResolver *vox_resolver = nullptr;

    int              total_channels = 0;  // snd.total_channels high-water mark (in/out)
    ListenerSnapshot listener       {};   // last FrameUpdate (StartSound carries its own)

    // Last FrameUpdate's configuration.  Kept here (not in AudioTopology) so
    // the paint arguments are read from the SAME place in both modes.
    MixConfigSnapshot mix_config {};
};

// Apply one command, running the verbatim S9.6 legacy sequence.  This is the
// ONLY implementation of the channel-mutation surface: Sound calls it inline
// when the topology is off and the decoder calls it when the topology is on, so
// the two modes are identical by construction.
void apply_command( ChannelApplyContext &ctx, const AudioCommand &cmd ) noexcept;

// S_StopAllSounds' channel half + S_Shutdown's teardown half (was
// Sound::Impl::free_all_channels in S9.6).  Resets total_channels to
// MAX_DYNAMIC_CHANNELS.
void free_all_channels( ChannelApplyContext &ctx ) noexcept;

} // namespace xash::sound
