// xash3dpp — the S9.8 sink-device integration WITNESS (Chunk 9, final slice).
//
// What this file is: an end-to-end proof that a scripted sequence of CONSOLE
// COMMANDS drives real channel allocation, resample/mix, VOX and the room DSP,
// through the as-built S9.7b topology (MPSC -> decoder -> SPSC ring ->
// callback), into `SinkDevice`, and produces interleaved-stereo int16 PCM that
// is BYTE-IDENTICAL across repeated runs and across x64/x86.
//
// What this file is NOT: new engine behaviour. Every component it exercises is
// already pinned by its own slice's tests (S9.2 codec, S9.3 mixer kernels, S9.4
// VOX, S9.5 DSP, S9.6 entry surface, S9.7b topology). The value here is that
// they are exercised TOGETHER, through the public/console surface, with a
// pinned output — so a regression anywhere in the chain, or an arch-dependent
// arithmetic difference, fails loudly instead of degrading quietly.
//
// Boundary spec: docs/boundaries/sound-boundary.md §As-built topology (S9.7b),
// §Assert/annotation duty ("the constraint ... binds the future shim (and
// S9.8's witness)"), SND-OQ-2/3/5.
//
// ===========================================================================
// THE DETERMINISM ARGUMENT (why the pinned hash below is legitimate)
// ---------------------------------------------------------------------------
// The output is a pure function of this file's own inputs. Source by source:
//
//  1. WALL CLOCK — none in the measured path. The topology is started in the
//     externally-driven shape: `SoundInitParams::external_decoder = true` (no
//     T_AudioDecoder thread, hence no spin-then-park pacer) and
//     `SinkDevice::drives_own_callback() == true` (no internal pump thread,
//     hence no `platform::sleep`-paced consumer). NOTHING sleeps, polls a
//     clock, or races a deadline. `Sound::update_frame` is handed a
//     ListenerSnapshot with `frametime = 0` from the script table, not from a
//     timer.
//
//  2. THREAD INTERLEAVING — none. Two worker threads exist ONLY because
//     `Mixer::paint_channels()` asserts ThreadRole::AudioDecoder and
//     `RingFillSource::fill()` asserts ThreadRole::AudioCallback (the
//     documented §Assert/annotation duty constraint). `RoleWorker` runs exactly
//     one job at a time and T_Main BLOCKS until it returns, so at every instant
//     exactly one thread is inside the sound subsystem, in a fixed order
//     (main -> decoder -> callback -> main -> ...). The mutex/condvar handoff
//     gives a total happens-before order — a strictly stronger fence than the
//     SND-OQ-2 `flush()` epoch ack, which is why no flush() appears here: the
//     licence flush() grants is already implied by "the decoder step RETURNED".
//     (Ordering by completion, like ordering by join, is the strongest form.)
//
//  3. UNINITIALISED MEMORY — none reachable. `Mixer`'s paintbuffer/roombuffer
//     are cleared by `S_ClearBuffers` at the head of every block; the DSP delay
//     lines come from `mem_calloc` (PoolIntBuffer::allocate, zero-filled by
//     contract); `SinkDevice::pump()` zero-fills before pulling; the ring's
//     backing array is value-initialised. A fresh `Sound` (and therefore a
//     fresh Mixer/RoomDsp/SfxRegistry/ring/queue) is built per run, so no state
//     survives from the previous iteration either — which is what makes the
//     repeat-run comparison meaningful rather than tautological.
//
//  4. RNG — SoundInitParams leaves the injected random-long callback null.
//     Every DSP processing call site is documented-dead for a live RoomDsp
//     (sxmod1_/sxmod2_ are fixed 350/450, reverb's `dly.mod` is always nonzero,
//     stereodly's is always forced to 0), and `profile()` is never invoked.
//     The witness therefore proves that decoder processing does not reach the
//     dormant sites; it makes no cross-subsystem stream-scheduling claim.
//
//  5. HASH / MAP ITERATION ORDER — `SfxRegistry::by_name_`
//     (unordered_map<string,handle>) and `VoxSystem::bound_`
//     (unordered_map<const MixChannel*,...>) are only ever probed by key
//     (find/emplace/erase); neither is iterated, so neither bucket order nor
//     the pointer VALUES used as keys can reach the output. Handles come from
//     `slots_`, a vector indexed in registration order, and registration order
//     is fixed by the script.
//
//  6. POINTER VALUES — the only pointers that influence behaviour are
//     identity-compared (`ch.source == nullptr`, the vox bound_ key), never
//     ordered or hashed into a decision. Address-space layout therefore cannot
//     move a byte of output.
//
//  7. FLOATING POINT ACROSS ARCHES — the build is `/fp:precise` globally
//     (CMakeLists.txt:30) and MSVC x86 targets SSE2, so float/double are IEEE
//     doubles/singles with identical rounding on both arches; no x87 excess
//     precision, no contraction. The synthetic source PCM below is generated
//     with INTEGER arithmetic only, so the inputs are bit-identical before any
//     float ever runs. This is the same posture the S9.3/S9.5 byte-pinned mix
//     and DSP vectors already validate on both arches.
//
//  8. FILESYSTEM / ASSETS — none. `SoundInitParams::audio_loader` injects the
//     WitnessLoader below, which synthesizes every sound from its NAME. No
//     `Filesystem`, no codec, no retail asset, no disk order.
//
// ===========================================================================

#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/private/cmd_cvar/compat_policy.hpp>
#include <xash3dpp/private/sound/registry.hpp> // IAudioLoader (the injected decode seam)
#include <xash3dpp/sound/constants.hpp>
#include <xash3dpp/sound/device.hpp>
#include <xash3dpp/sound/sound.hpp>

#include "../test_helpers.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

static int g_pass = 0, g_fail = 0;

using xash::core::ThreadRole;
using namespace xash::sound;

namespace {

// ===========================================================================
// Tunables — the shape of one "host frame" in the script.
// ===========================================================================

// One decoder step paints exactly sound_decoder_block_frames (512) frames, and
// the pump pulls exactly the same number, so the ring is drained to empty every
// frame and the underrun counter must stay at zero. Deriving both from the same
// limit keeps that invariant true if the limit ever changes.
constexpr std::size_t k_frames_per_step = ::xash::limits::sound_decoder_block_frames;

// ===========================================================================
// FNV-1a 64 over the PCM. Hashed SAMPLE BY SAMPLE with explicit low/high byte
// extraction rather than over the raw memory, so the digest depends on the
// int16 VALUES only — never on the host's byte order or on vector padding.
// ===========================================================================
[[nodiscard]] std::uint64_t pcm_hash( std::span<const std::int16_t> pcm ) noexcept
{
    std::uint64_t       h      = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    for( const std::int16_t s : pcm )
    {
        const std::uint16_t bits = static_cast<std::uint16_t>( s );
        h = ( h ^ static_cast<std::uint64_t>( bits & 0x00FFu ) ) * prime;
        h = ( h ^ static_cast<std::uint64_t>( ( bits >> 8 ) & 0x00FFu ) ) * prime;
    }
    return h;
}

// ===========================================================================
// Synthetic source audio — INTEGER ONLY (see determinism note 7). A triangle
// wave whose every sample is an exact integer expression of its index, so the
// decoded input is bit-identical on every target before any float math runs.
// ===========================================================================
[[nodiscard]] AudioData make_tone( std::uint32_t rate, std::uint8_t width, std::uint8_t channels,
                                   std::uint32_t frames, int period, int amplitude, bool looped )
{
    AudioData a;
    a.rate       = rate;
    a.width      = width;
    a.channels   = channels;
    a.samples    = frames;
    a.type       = AudioFormatType::Pcm;
    a.flags      = looped ? AudioFlags::Looped : AudioFlags::None;
    a.loop_start = looped ? frames / 4 : 0;
    a.buffer.assign( static_cast<std::size_t>( frames ) * width * channels, std::byte{ 0 } );

    const int half = period / 2;
    for( std::uint32_t f = 0; f < frames; ++f )
    {
        for( std::uint8_t c = 0; c < channels; ++c )
        {
            // Per-channel phase offset so a stereo source is genuinely stereo.
            const int t   = static_cast<int>( ( f + c * static_cast<std::uint32_t>( half ) ) %
                                            static_cast<std::uint32_t>( period ) );
            const int tri = ( t < half ) ? t : ( period - t );          // 0 .. half
            const int v   = ( tri * 2 * amplitude ) / half - amplitude; // -amp .. +amp

            const std::size_t idx = ( static_cast<std::size_t>( f ) * channels + c ) * width;
            if( width == 2 )
            {
                const std::int16_t s16 = static_cast<std::int16_t>( v );
                std::memcpy( a.buffer.data() + idx, &s16, sizeof( s16 ) );
            }
            else
            {
                // 8-bit PCM is UNSIGNED (128 == silence), matching WAV.
                const int          u8   = 128 + ( v * 127 ) / ( amplitude != 0 ? amplitude : 1 );
                const std::uint8_t byte = static_cast<std::uint8_t>( u8 < 0 ? 0 : ( u8 > 255 ? 255 : u8 ) );
                a.buffer[idx]           = static_cast<std::byte>( byte );
            }
        }
    }
    return a;
}

// ---------------------------------------------------------------------------
// WitnessLoader — the injected IAudioLoader (SoundInitParams::audio_loader).
// A PURE function of the requested name: same name in, byte-identical AudioData
// out, on every call, in every run, on every arch. Deliberately covers several
// (rate, width, channels, looped) combinations so more than one mix kernel and
// the resampler are on the measured path.
//
// @thread-safety: stateless apart from the diagnostic counter. It is genuinely
// called from BOTH roles — T_Main for plain channels (Sound::start_sound hoists
// the decode) and T_AudioDecoder for VOX words (LockedSfxResolver, at
// word-advance time) — so purity is a requirement, not a nicety.
// ---------------------------------------------------------------------------
class WitnessLoader final : public IAudioLoader
{
public:
    std::atomic<int> load_calls { 0 };

    [[nodiscard]] std::optional<AudioData> load( std::string_view name ) noexcept override
    {
        load_calls.fetch_add( 1, std::memory_order_relaxed );

        // 16-bit mono at the device rate — the unity-rate kernel.
        if( name == "witness/beep_a.wav" )
            return make_tone( 44100, 2, 1, 3000, 64, 9000, /*looped*/ false );
        // 16-bit STEREO at half the device rate — the stereo + upsample kernels.
        if( name == "witness/beep_b.wav" )
            return make_tone( 22050, 2, 2, 2200, 48, 7000, false );
        // 8-bit mono at 11 kHz, looped — the 8-bit kernel + the loop-wrap edge.
        if( name == "witness/amb_c.wav" )
            return make_tone( 11025, 1, 1, 1500, 32, 6000, /*looped*/ true );
        // 16-bit mono at a rate that is NOT a clean ratio of 44100 — exercises
        // the fractional resample accumulator rather than a whole-step advance.
        if( name == "witness/beep_d.wav" )
            return make_tone( 32000, 2, 1, 4000, 100, 8000, false );
        // The VOX word. `speak <w>` becomes the immediate sentence "!#<w>";
        // VOX_GetDirectory prefixes a bare word with "vox/" (s_vox.c:223-255).
        if( name == "vox/witness_word" )
            return make_tone( 44100, 2, 1, 1800, 40, 10000, false );

        // Anything else resolves to SfxRegistry's S_CreateDefaultSound fallback
        // (1 s of silence) — same as a missing file in production.
        return std::nullopt;
    }
};

// ===========================================================================
// RoleWorker — a thread pinned to ONE ThreadRole that executes one posted job
// at a time while T_Main blocks. See determinism note 2: this exists purely to
// satisfy the paint's / callback's role asserts, NOT to introduce concurrency.
// ===========================================================================
class RoleWorker
{
public:
    using JobFn = void ( * )( void * ) noexcept;

    explicit RoleWorker( ThreadRole role )
    {
        thread_ = std::thread( [this, role]() noexcept {
            xash::core::register_thread_role( role );
            for( ;; )
            {
                JobFn fn   = nullptr;
                void *user = nullptr;
                {
                    std::unique_lock<std::mutex> lock( mutex_ );
                    posted_cv_.wait( lock, [this] { return job_ != nullptr || quit_; } );
                    if( job_ == nullptr )
                        return; // quit_, with nothing left to run
                    fn   = job_;
                    user = user_;
                }
                fn( user );
                {
                    const std::lock_guard<std::mutex> lock( mutex_ );
                    job_ = nullptr;
                }
                done_cv_.notify_one();
            }
        } );
    }

    ~RoleWorker()
    {
        {
            const std::lock_guard<std::mutex> lock( mutex_ );
            quit_ = true;
        }
        posted_cv_.notify_one();
        thread_.join();
    }

    RoleWorker( const RoleWorker & )            = delete;
    RoleWorker &operator=( const RoleWorker & ) = delete;

    // Post `fn` and BLOCK until it has returned. On return, everything the job
    // did happens-before the caller's next statement (mutex release/acquire).
    void run( JobFn fn, void *user ) noexcept
    {
        {
            const std::lock_guard<std::mutex> lock( mutex_ );
            job_  = fn;
            user_ = user;
        }
        posted_cv_.notify_one();
        std::unique_lock<std::mutex> lock( mutex_ );
        done_cv_.wait( lock, [this] { return job_ == nullptr; } );
    }

private:
    std::mutex              mutex_;
    std::condition_variable posted_cv_;
    std::condition_variable done_cv_;
    JobFn                   job_  = nullptr;
    void                   *user_ = nullptr;
    bool                    quit_ = false;
    std::thread             thread_;
};

// --- the two jobs ----------------------------------------------------------

struct DecoderJob
{
    Sound *sound     = nullptr;
    int    steps     = 0;
    bool   did_work  = false;
};

void decoder_job( void *user ) noexcept
{
    auto *job = static_cast<DecoderJob *>( user );
    job->did_work = false;
    for( int i = 0; i < job->steps; ++i )
        job->did_work = job->sound->decoder_step() || job->did_work;
}

struct PumpJob
{
    SinkDevice   *sink    = nullptr;
    std::size_t   frames  = 0;
    std::int16_t *dst     = nullptr; // pre-sized destination window (never grown here)
    std::size_t   dst_len = 0;
    std::size_t   copied  = 0;
};

void pump_job( void *user ) noexcept
{
    auto                               *job = static_cast<PumpJob *>( user );
    const std::span<const std::int16_t> pcm = job->sink->pump( job->frames );
    // Copy into a PRE-SIZED window rather than growing a vector: MSVC's counted
    // range-insert carries a rollback try/catch, which is a C4530 under the
    // project's /EHs-c-. (It also keeps the callback-side job allocation-free,
    // which is the shape a real audio callback must have anyway.)
    job->copied = pcm.size() < job->dst_len ? pcm.size() : job->dst_len;
    if( job->copied != 0 )
        std::memcpy( job->dst, pcm.data(), job->copied * sizeof( std::int16_t ) );
}

// ===========================================================================
// The script.
//
// G-5 STAGE-(a) REHEARSAL, deliberately: extension-goals.md §G-5 stage (a) is
// "debug automation, scripted test scenarios, console scripting on the cold
// path". This table is exactly that scenario shape — a future scripting runtime
// would emit the same `console` strings through the same CmdCvarContext. Which
// is why the sequence goes through `cbuf_add_text` + `cbuf_execute` and NOT
// through direct Sound::start_sound() calls: the command layer, the tokenizer,
// the cvar store and the S9.6 command registrations are all part of what this
// witness pins.
//
// One host frame per row: publish the listener pose, execute this row's console
// text, run ONE decoder step (512 frames painted), pull those 512 frames.
// ===========================================================================
struct ScriptStep
{
    const char *console;       // "" == no console input this frame
    int         listener_ent;  // ListenerSnapshot::entnum (start_local_sound plays ON it)
    int         waterlevel;    // DSP room-selection input (>2 selects waterroom_type)
};

constexpr ScriptStep k_script[] = {
    // -- DSP engagement through the room cvars (S9.5 on the measured path) ---
    // room_type 5 == the "tunnel" preset: nonzero room_size/room_refl (reverb),
    // room_delay/room_feedback (mono delay) and room_left (stereo delay), so
    // all four SX_RoomFX passes actually run.
    { "room_off 0; dsp_coeff_table 0; room_hires 2; room_type 5; s_lerping 0", 0, 0 }, //  0
    { "play witness/beep_a.wav",                                              0, 0 }, //  1 plain dynamic, CHAN_AUTO
    { "",                                                                     0, 0 }, //  2
    { "playvol witness/beep_b.wav 0.35",                                      0, 0 }, //  3 playvol (scaled master_vol)
    { "",                                                                     0, 0 }, //  4
    { "play2 witness/amb_c.wav",                                              0, 0 }, //  5 CHAN_STATIC -> static range
    { "",                                                                     0, 0 }, //  6
    { "play witness/beep_d.wav",                                              7, 0 }, //  7 different entity
    { "",                                                                     7, 0 }, //  8
    { "speak witness_word",                                                   9, 0 }, //  9 VOX sentence (S9.4)
    { "",                                                                     9, 0 }, // 10
    { "",                                                                     9, 0 }, // 11
    { "s_lerping 1",                                                          9, 0 }, // 12 interpolating kernels
    { "play witness/beep_a.wav",                                              9, 0 }, // 13
    { "",                                                                     9, 0 }, // 14
    { "room_type 19",                                                         9, 0 }, // 15 live preset change
    { "",                                                                     9, 0 }, // 16
    { "",                                                                     9, 0 }, // 17
    { "",                                                                     9, 3 }, // 18 submerged -> waterroom_type
    { "",                                                                     9, 3 }, // 19
    { "stopsound",                                                            9, 3 }, // 20 THE STOP (+ SX_ClearState)
    { "",                                                                     0, 0 }, // 21
    { "play witness/beep_b.wav",                                              0, 0 }, // 22 pipeline still live after
    { "",                                                                     0, 0 }, // 23
    { "",                                                                     0, 0 }, // 24
};

constexpr std::size_t k_script_steps = sizeof( k_script ) / sizeof( k_script[0] );

// ===========================================================================
// One complete run of the witness.
// ===========================================================================

struct AlwaysTrustedOracle final : ::xash::cmd_cvar::ITrustOracle
{
    bool stuffcmd_is_trusted() const noexcept override { return true; }
};

struct NullPolicy final : ::xash::cmd_cvar::ICompatPolicy
{
    const char *redirect_cvar_name( std::string_view ) const noexcept override { return nullptr; }
    bool        is_filterable_exempt( std::string_view ) const noexcept override { return false; }
    bool        is_overridable_command( std::string_view ) const noexcept override { return false; }
};

struct WitnessResult
{
    std::vector<std::int16_t>  pcm;             // the whole captured output
    std::vector<std::uint32_t> active_trace;    // occupied channels after each decoder step
    std::uint64_t              mix_blocks = 0;
    std::uint64_t              underruns  = 0;
    std::uint32_t              dropped    = 0;
    int                        load_calls = 0;
    std::uint64_t              frames_pumped = 0;
};

void run_witness( WitnessResult &out )
{
    AlwaysTrustedOracle              oracle;
    NullPolicy                       policy;
    ::xash::cmd_cvar::CmdCvarContext ctx;
    REQUIRE( ctx.init( { &oracle, &policy } ) );

    SinkDevice     sink;
    WitnessLoader  loader;

    SoundInitParams params;
    params.device       = &sink;    // drives_own_callback() == true -> no internal pump
    params.cmd_cvar     = &ctx;     // the 11 cvars + 13 commands the script drives
    params.audio_loader = &loader;  // synthetic PCM; no filesystem, no assets
    params.threaded     = true;     // real MPSC + real SPSC ring on the path...
    params.external_decoder = true; // ...but WE step the decoder (no wall clock)

    Sound sound;
    REQUIRE( sound.init( params ).has_value() );
    REQUIRE( sound.topology_running() );

    // The two role-pinned workers (determinism note 2).
    RoleWorker decoder( ThreadRole::AudioDecoder );
    RoleWorker callback( ThreadRole::AudioCallback );

    // Sized once, up front: the callback job writes into a fixed window and
    // never allocates (see pump_job).
    out.pcm = std::vector<std::int16_t>( k_script_steps * k_frames_per_step * 2, std::int16_t{ 0 } );
    out.active_trace.clear();
    out.active_trace.reserve( k_script_steps );

    for( std::size_t i = 0; i < k_script_steps; ++i )
    {
        const ScriptStep &step = k_script[i];

        // (1) T_Main — the per-frame listener publish. There is no console
        //     command for this (S_UpdateFrame is a host-frame call, not a
        //     command), so it is the one non-scripted input, and every field it
        //     carries comes from the table above rather than from a clock.
        ListenerSnapshot listener {};
        listener.entnum     = step.listener_ent;
        listener.waterlevel = step.waterlevel;
        listener.frametime  = 0.0f; // no wall clock anywhere (determinism note 1)
        sound.update_frame( listener );

        // (2) T_Main — this frame's console text, through the REAL command
        //     buffer (G-5 stage-(a) shape; see the table's header comment).
        if( step.console[0] != '\0' )
        {
            ctx.cbuf_add_text( step.console );
            ctx.cbuf_execute();
        }

        // (3) T_AudioDecoder — drain the MPSC and paint exactly one block.
        DecoderJob dec { &sound, /*steps*/ 1, false };
        decoder.run( &decoder_job, &dec );
        CHECK( dec.did_work ); // a block is painted every frame, always

        out.active_trace.push_back( sound.stats().active_channels.load( std::memory_order_relaxed ) );

        // (4) T_AudioCallback — pull exactly what was painted, through
        //     RingFillSource::fill() into SinkDevice's buffer.
        const std::size_t window = k_frames_per_step * 2;
        PumpJob           pump { &sink, k_frames_per_step, out.pcm.data() + i * window, window, 0 };
        callback.run( &pump_job, &pump );
        CHECK_EQ( pump.copied, window );
    }

    out.mix_blocks    = sound.stats().mix_blocks.load( std::memory_order_relaxed );
    out.underruns     = sound.stats().underruns.load( std::memory_order_relaxed );
    out.dropped       = sound.stats().dropped_sounds.load( std::memory_order_relaxed );
    out.load_calls    = loader.load_calls.load( std::memory_order_relaxed );
    out.frames_pumped = sink.frames_pumped();

    sound.shutdown();
}

// ===========================================================================
// The pinned expectation.
//
// This value was DERIVED from the code's own output, which is only legitimate
// because the pipeline is deterministic BY CONSTRUCTION rather than by
// observation — see the eight-point argument in the file header. Its job is to
// make a cross-arch or cross-revision difference FAIL rather than pass
// silently: a golden that is only ever compared against itself would catch
// neither.
//
// If you change k_script, the WitnessLoader's synthetic audio, or any mix/DSP/
// VOX arithmetic, this value MUST change — and the correct response is to
// understand WHY before re-pinning it, not to paste the new number in.
// ===========================================================================
constexpr std::uint64_t k_expected_pcm_hash = 0xDF1088E7D0D531CFull;
constexpr std::size_t   k_expected_samples  = k_script_steps * k_frames_per_step * 2;

} // namespace

// ===========================================================================
// 1. The witness is byte-identical across repeated runs in the same process.
// ===========================================================================
void test_witness_is_repeatable()
{
    constexpr int k_runs = 12;

    WitnessResult first;
    run_witness( first );

    CHECK_EQ( first.pcm.size(), k_expected_samples );
    CHECK_EQ( first.frames_pumped, static_cast<std::uint64_t>( k_script_steps * k_frames_per_step ) );
    // Every frame pulled exactly what the step painted, so the ring never ran
    // dry: an underrun here would mean the pump/paint sizes had drifted apart.
    CHECK_EQ( first.underruns, std::uint64_t{ 0 } );
    CHECK_EQ( first.mix_blocks, static_cast<std::uint64_t>( k_script_steps ) );
    CHECK_EQ( first.dropped, std::uint32_t{ 0 } );

    // The output must actually contain AUDIO. A silent witness would pass every
    // equality check below while proving nothing at all.
    std::size_t nonzero = 0;
    std::int16_t peak   = 0;
    for( const std::int16_t s : first.pcm )
    {
        if( s != 0 )
            ++nonzero;
        if( s > peak )
            peak = s;
    }
    CHECK( nonzero > first.pcm.size() / 4 );
    CHECK( peak > 1000 );

    // The script really did allocate channels, and `stopsound` really did clear
    // them (step 20 is the stop; the step-21 trace entry is the frame after it).
    REQUIRE( first.active_trace.size() == k_script_steps );
    CHECK_EQ( first.active_trace[0], std::uint32_t{ 0 } ); // cvars only, nothing playing
    CHECK( first.active_trace[1] > 0 );                    // "play" allocated a channel
    CHECK( first.active_trace[6] >= 3 );                   // plain + playvol + static all live
    CHECK_EQ( first.active_trace[20], std::uint32_t{ 0 } ); // "stopsound" freed everything
    CHECK( first.active_trace[22] > 0 );                   // ...and the pipeline still plays after

    const std::uint64_t first_hash = pcm_hash( first.pcm );
    std::printf( "  witness pcm: %zu samples, hash 0x%016llX (%zu nonzero, peak %d)\n", first.pcm.size(),
                 static_cast<unsigned long long>( first_hash ), nonzero, static_cast<int>( peak ) );

    // Repeat, in the same process. Note this also re-runs everything that has
    // A match confirms that no injected/random-dependent DSP path is reached
    // by decoder processing (determinism note 4).
    for( int run = 1; run < k_runs; ++run )
    {
        WitnessResult again;
        run_witness( again );

        CHECK_EQ( again.pcm.size(), first.pcm.size() );
        CHECK_EQ( again.mix_blocks, first.mix_blocks );
        CHECK_EQ( again.underruns, first.underruns );
        CHECK_EQ( again.load_calls, first.load_calls );
        CHECK( again.active_trace == first.active_trace );

        // Byte comparison, not just the digest: a hash collision cannot hide a
        // difference that this misses.
        const bool identical = again.pcm.size() == first.pcm.size() &&
                               std::memcmp( again.pcm.data(), first.pcm.data(),
                                            first.pcm.size() * sizeof( std::int16_t ) ) == 0;
        CHECK( identical );
        if( !identical )
        {
            for( std::size_t i = 0; i < again.pcm.size() && i < first.pcm.size(); ++i )
            {
                if( again.pcm[i] != first.pcm[i] )
                {
                    std::printf( "    first divergence at sample %zu: run0=%d run%d=%d\n", i,
                                 static_cast<int>( first.pcm[i] ), run, static_cast<int>( again.pcm[i] ) );
                    break;
                }
            }
        }
        CHECK_EQ( pcm_hash( again.pcm ), first_hash );
    }
}

// ===========================================================================
// 2. The witness matches the COMMITTED expectation — this is the check that
//    makes an x64-vs-x86 divergence a failure instead of a silent pass.
// ===========================================================================
void test_witness_matches_pinned_hash()
{
    WitnessResult result;
    run_witness( result );

    CHECK_EQ( result.pcm.size(), k_expected_samples );

    const std::uint64_t hash = pcm_hash( result.pcm );
    CHECK_EQ( hash, k_expected_pcm_hash );
    if( hash != k_expected_pcm_hash )
        std::printf( "  PINNED-HASH MISMATCH: expected 0x%016llX, observed 0x%016llX\n",
                     static_cast<unsigned long long>( k_expected_pcm_hash ),
                     static_cast<unsigned long long>( hash ) );

    // A handful of spot samples, so a mismatch says WHERE as well as THAT. The
    // indices are spread across the script: mid-first-sound, post-VOX, and
    // after the live DSP preset change.
    constexpr std::size_t k_probe_step[] = { 2, 11, 17, 23 };
    for( const std::size_t step : k_probe_step )
    {
        const std::size_t base = step * k_frames_per_step * 2;
        std::printf( "  probe step %2zu: L=%6d R=%6d\n", step, static_cast<int>( result.pcm[base] ),
                     static_cast<int>( result.pcm[base + 1] ) );
    }
}

// ===========================================================================
// 3. CONC-6 regression pin: the device answers "do I drive my own callback?"
//    and Sound must believe it. A SinkDevice that got an internal pump thread
//    as well would put a SECOND consumer on the single-consumer PCM ring (and
//    a wall clock back into the witness).
// ===========================================================================
void test_device_declares_its_own_callback()
{
    SinkDevice sink;
    NullDevice null_dev;
    CHECK( sink.drives_own_callback() );      // pump() IS the callback
    CHECK( !null_dev.drives_own_callback() ); // never pulls; needs the internal pump

    // End to end: with the sink installed and the topology running, the ONLY
    // consumer of the ring is the caller of pump(). If an internal pump thread
    // had also been spawned, this pump would race it — and in a debug build
    // SpscRing's single-consumer assert would abort the process rather than
    // return a short read.
    AlwaysTrustedOracle              oracle;
    NullPolicy                       policy;
    ::xash::cmd_cvar::CmdCvarContext ctx;
    REQUIRE( ctx.init( { &oracle, &policy } ) );

    WitnessLoader   loader;
    SoundInitParams params;
    params.device           = &sink;
    params.cmd_cvar         = &ctx;
    params.audio_loader     = &loader;
    params.threaded         = true;
    params.external_decoder = true;

    Sound sound;
    REQUIRE( sound.init( params ).has_value() );

    RoleWorker decoder( ThreadRole::AudioDecoder );
    RoleWorker callback( ThreadRole::AudioCallback );

    ctx.cmd_execute_string( "play witness/beep_a.wav" );

    DecoderJob dec { &sound, 1, false };
    decoder.run( &decoder_job, &dec );
    CHECK( dec.did_work );

    std::vector<std::int16_t> pcm( k_frames_per_step * 2, std::int16_t{ 0 } );
    PumpJob                   pump { &sink, k_frames_per_step, pcm.data(), pcm.size(), 0 };
    callback.run( &pump_job, &pump );

    CHECK_EQ( pump.copied, pcm.size() );
    CHECK_EQ( sound.stats().underruns.load( std::memory_order_relaxed ), std::uint64_t{ 0 } );

    bool nonzero = false;
    for( const std::int16_t s : pcm )
        if( s != 0 )
            nonzero = true;
    CHECK( nonzero );

    sound.shutdown();
}

// ===========================================================================
// 4. The injected loader really is the decode seam (SoundInitParams::
//    audio_loader) — no filesystem is consulted, and an unknown name still
//    falls back to S_CreateDefaultSound exactly as a missing file would.
// ===========================================================================
void test_injected_loader_is_the_decode_seam()
{
    AlwaysTrustedOracle              oracle;
    NullPolicy                       policy;
    ::xash::cmd_cvar::CmdCvarContext ctx;
    REQUIRE( ctx.init( { &oracle, &policy } ) );

    SinkDevice      sink;
    WitnessLoader   loader;
    SoundInitParams params;
    params.device       = &sink;
    params.cmd_cvar     = &ctx;
    params.audio_loader = &loader;
    // params.filesystem deliberately left null AND unused — the injected loader
    // is a replacement for that path, not an addition to it.

    Sound sound;
    REQUIRE( sound.init( params ).has_value() );

    // Topology off: the S9.6 single-threaded path, so channels_snapshot() reads
    // the live array directly and needs no decoder handshake.
    CHECK( !sound.topology_running() );

    ctx.cmd_execute_string( "play witness/beep_a.wav" );
    CHECK_EQ( loader.load_calls.load( std::memory_order_relaxed ), 1 );
    CHECK_EQ( sound.channels_snapshot().size(), std::size_t{ 1 } );

    // Second play of the SAME name hits the registry cache — the loader is not
    // asked twice (S9.6 lazy-resolution semantics, unchanged by the injection).
    ctx.cmd_execute_string( "play witness/beep_a.wav" );
    CHECK_EQ( loader.load_calls.load( std::memory_order_relaxed ), 1 );

    // An unknown name is a loader MISS, not an error: SfxRegistry synthesizes
    // the silent default sound, so the channel is still allocated.
    ctx.cmd_execute_string( "play witness/definitely_not_registered.wav" );
    CHECK_EQ( loader.load_calls.load( std::memory_order_relaxed ), 2 );

    sound.shutdown();
}

int main()
{
    xash::core::register_thread_role( ThreadRole::Main );

    RUN_TEST( test_device_declares_its_own_callback );
    RUN_TEST( test_injected_loader_is_the_decode_seam );
    RUN_TEST( test_witness_is_repeatable );
    RUN_TEST( test_witness_matches_pinned_hash );

    std::printf( "sound_witness: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
