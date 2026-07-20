// xash3dpp — sound entry surface tests (Chunk 9, slice S9.6). PARITY-CRITICAL.
// Covers channel_alloc.{hpp,cpp} (SND_PickDynamicChannel/PickStaticChannel/
// S_AlterChannel/SND_Spatialize) via direct unit tests, registry.{hpp,cpp}
// (S_RegisterSound/S_LoadSound lazy-resolution), VoxSystem's S9.6 additions
// (IVoxTimeLeftQuery), and the full Sound entry surface end-to-end (start/
// stop/register/channels_snapshot + a command dispatched through a real
// CmdCvarContext).

#include <xash3dpp/private/sound/channel_alloc.hpp>
#include <xash3dpp/private/sound/registry.hpp>
#include <xash3dpp/private/sound/vox.hpp>

#include <xash3dpp/sound/constants.hpp>
#include <xash3dpp/sound/sound.hpp>

#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/private/cmd_cvar/compat_policy.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

using namespace xash::sound;
using ::xash::abi::portable_samplepair_t;

// ===========================================================================
// AudioData builder (mirrors test_sound_mixer.cpp/test_sound_vox.cpp).
// Uses LOUD sample values (>k_vox_trim_below_8=2) so trim_start/trim_end
// never adjust anything unexpectedly in tests that don't care about trimming.
// ===========================================================================
namespace {

AudioData make_audio( std::uint32_t samples, bool looped = false ) noexcept
{
    AudioData a;
    a.rate     = 44100;
    a.width    = 1;
    a.channels = 1;
    a.samples  = samples;
    a.type     = AudioFormatType::Pcm;
    a.flags    = looped ? AudioFlags::Looped : AudioFlags::None;
    a.buffer.assign( samples, std::byte{ 50 } ); // loud, well above the VOX trim threshold
    return a;
}

} // namespace

// ===========================================================================
// 1. Channel allocation — SND_PickDynamicChannel (s_main.c:342-416).
//    Every span is sized to exactly [ambient(4) + 3 dynamic slots] so each
//    scenario is hand-traceable index-by-index.
// ===========================================================================

static void test_pick_dynamic_same_entity_override()
{
    // channels[5] occupied by entnum=7/CHAN_WEAPON(1) with FULL remaining time
    // (100 samples untouched) — would NEVER win by least-time-left ranking
    // against channels[4], a genuinely free slot (timeleft 0). The "always
    // override same entity" rule must still pick index 5 via immediate
    // `break`, discarding whatever free-channel candidate was already found.
    AudioData audio = make_audio( 100 );
    std::vector<MixChannel> channels( 7 ); // 4 ambient + 3 dynamic (indices 4,5,6)
    channels[5].source     = &audio;
    channels[5].entnum     = 7;
    channels[5].entchannel = 1; // CHAN_WEAPON
    channels[5].sample     = 0.0;

    const DynamicPickResult r = pick_dynamic_channel( channels, /*listener*/ 0, /*entnum*/ 7, /*channel*/ 1,
                                                      /*sfx*/ 99, nullptr, nullptr );
    CHECK_EQ( r.index, 5 );
    CHECK( !r.ignore );
    CHECK( channels[5].source == nullptr ); // the winner got freed by the picker
}

static void test_pick_dynamic_client_protection()
{
    // channels[4] = a CLIENT-owned channel (entnum == listener) with the
    // LEAST remaining time (1 sample) — the naive "least time left" ranking
    // would pick it. channels[5] = a non-client channel with MORE remaining
    // time (1000) but must win anyway because index4 is PROTECTED (monster
    // sounds must not evict a client's sound, s_main.c:378-379). channels[6]
    // has even MORE remaining time (2000) so it never competes.
    AudioData a4 = make_audio( 100 ); // sample=99 -> remaining=1
    AudioData a5 = make_audio( 1000 );
    AudioData a6 = make_audio( 2000 );

    std::vector<MixChannel> channels( 7 );
    channels[4].source = &a4;
    channels[4].entnum = 1; // == listener_entnum below
    channels[4].sample = 99.0;
    channels[5].source = &a5;
    channels[5].entnum = 55;
    channels[5].sample = 0.0;
    channels[6].source = &a6;
    channels[6].entnum = 77;
    channels[6].sample = 0.0;

    const DynamicPickResult r =
        pick_dynamic_channel( channels, /*listener*/ 1, /*entnum*/ 99, /*channel*/ 0 /*CHAN_AUTO*/, /*sfx*/ 1,
                              nullptr, nullptr );
    CHECK_EQ( r.index, 5 ); // NOT 4 — protection skips the client channel entirely
    CHECK( !r.ignore );
    CHECK( channels[4].source == &a4 ); // untouched — never even considered
}

static void test_pick_dynamic_anti_restart_ignore()
{
    // Same-entity+same-channel+same-sfx match on an ALREADY-LOOPING sound ->
    // "don't restart looping sounds for the same entity" (s_main.c:396-409):
    // the victim is left untouched and `ignore` is set so the caller does not
    // log "dropped sound".
    AudioData looped = make_audio( 100, /*looped*/ true );
    std::vector<MixChannel> channels( 7 );
    channels[5].source     = &looped;
    channels[5].entnum     = 7;
    channels[5].entchannel = 1;
    channels[5].sfx_handle = 42;

    const DynamicPickResult r =
        pick_dynamic_channel( channels, /*listener*/ 0, /*entnum*/ 7, /*channel*/ 1, /*sfx*/ 42, nullptr, nullptr );
    CHECK_EQ( r.index, -1 );
    CHECK( r.ignore );
    CHECK( channels[5].source == &looped ); // untouched
}

static void test_pick_dynamic_stream_already_playing()
{
    std::vector<MixChannel> channels( 7 );
    channels[4].sfx_handle = 42;
    channels[4].entchannel = 5; // CHAN_STREAM

    const DynamicPickResult r =
        pick_dynamic_channel( channels, /*listener*/ 0, /*entnum*/ 3, /*channel*/ 5 /*CHAN_STREAM*/, /*sfx*/ 42,
                              nullptr, nullptr );
    CHECK_EQ( r.index, -1 );
    CHECK( r.ignore ); // s_main.c:354-359 — short-circuits before the main scan
}

static void test_channel_time_left_plain_and_finished()
{
    AudioData audio = make_audio( 10 );
    MixChannel ch{};
    ch.source = &audio;
    ch.sample = 3.0;
    CHECK_EQ( channel_time_left( ch, nullptr ), 7 ); // bound(0, 10-3, 10) (s_main.c:324-328)

    ch.flags |= ::xash::abi::k_fl_chan_finished;
    CHECK_EQ( channel_time_left( ch, nullptr ), 0 ); // s_main.c:288

    MixChannel free_ch{};
    CHECK_EQ( channel_time_left( free_ch, nullptr ), 0 ); // !ch->sfx
}

// ===========================================================================
// 2. pick_static_channel (s_main.c:428-462).
// ===========================================================================

static void test_pick_static_reuse_exact_match()
{
    AudioData audio = make_audio( 10 );
    std::vector<MixChannel> channels( 70 ); // 4 ambient + 60 dynamic + 6 static slots
    // total_channels=65 -> channel[64] is an ALREADY-ALLOCATED static slot
    // (indices [MAX_DYNAMIC_CHANNELS(64), total_channels(65)) are the
    // currently-active static range the scan covers, s_main.c:434).
    int total_channels = 65;

    channels[64].source     = &audio;
    channels[64].sfx_handle = 7;
    channels[64].origin     = Vec3{ 1.0f, 2.0f, 3.0f };

    const int idx = pick_static_channel( channels, total_channels, Vec3{ 1.0f, 2.0f, 3.0f }, 7 );
    CHECK_EQ( idx, 64 );          // exact origin + sfx match -> reuse (s_main.c:439-440)
    CHECK_EQ( total_channels, 65 ); // unchanged — no new slot allocated
}

static void test_pick_static_allocate_new()
{
    std::vector<MixChannel> channels( 70 );
    int total_channels = 64;

    const int idx = pick_static_channel( channels, total_channels, Vec3{ 5.0f, 0.0f, 0.0f }, 3 );
    CHECK_EQ( idx, 64 );          // first empty slot at the high-water mark
    CHECK_EQ( total_channels, 65 ); // bumped (s_main.c:458-459)
}

static void test_pick_static_overflow()
{
    AudioData audio = make_audio( 10 );
    // channels.size() == total_channels == 65: the ONE static slot in range
    // (index 64) is OCCUPIED by a non-matching sound, so the scan finds no
    // reusable slot, and there is no headroom to grow into -> overflow.
    std::vector<MixChannel> channels( 65 );
    channels[64].source     = &audio;
    channels[64].sfx_handle = 999;
    channels[64].origin     = Vec3{ 42.0f, 0.0f, 0.0f };
    int total_channels = 65; // == channels.size() -> "no free channels"

    const int idx = pick_static_channel( channels, total_channels, Vec3{}, 1 );
    CHECK_EQ( idx, -1 ); // s_main.c:451-455
    CHECK_EQ( total_channels, 65 );
}

// ===========================================================================
// 3. S_AlterChannel / S_MaybeAlterChannel (s_main.c:464-532).
// ===========================================================================

static void test_alter_channel_stop()
{
    AudioData audio = make_audio( 10 );
    std::vector<MixChannel> channels( 7 );
    channels[5].source     = &audio;
    channels[5].entnum     = 3;
    channels[5].entchannel = 2; // CHAN_VOICE
    channels[5].sfx_handle = 10;

    const bool altered =
        alter_channel( channels, 7, /*entnum*/ 3, /*channel*/ 2, /*sfx*/ 10, /*sfx_name*/ "foo.wav", 0, 0,
                       k_snd_stop, nullptr );
    CHECK( altered );
    CHECK( channels[5].source == nullptr ); // S_FreeChannel'd
}

static void test_alter_channel_change_vol_pitch()
{
    AudioData audio = make_audio( 10 );
    std::vector<MixChannel> channels( 7 );
    channels[5].source     = &audio;
    channels[5].entnum     = 3;
    channels[5].entchannel = 2;
    channels[5].sfx_handle = 10;
    channels[5].master_vol = 50;
    channels[5].base_pitch = 100.0;

    const bool altered = alter_channel( channels, 7, 3, 2, 10, "foo.wav", /*vol*/ 200, /*pitch*/ 150,
                                        k_snd_change_vol | k_snd_change_pitch, nullptr );
    CHECK( altered );
    CHECK( channels[5].source != nullptr ); // still playing
    CHECK_EQ( channels[5].master_vol, 200 );
    CHECK( channels[5].base_pitch == 150.0 );
}

static void test_alter_channel_sentence_matches_any_sfx()
{
    // "assume the entity is only playing one sentence at a time" (s_main.c:
    // 516-520): a sentence STOP matches ANY bound-sentence channel on the
    // same entnum+entchannel, ignoring the specific sfx identity passed in.
    AudioData audio = make_audio( 10 );
    std::vector<MixChannel> channels( 7 );
    channels[5].source     = &audio;
    channels[5].entnum     = 9;
    channels[5].entchannel = 6; // CHAN_STATIC
    channels[5].is_sentence = true;
    channels[5].sfx_handle  = 111; // deliberately DIFFERENT from the sfx passed below

    const bool altered =
        alter_channel( channels, 7, 9, 6, /*sfx*/ 222, /*sfx_name*/ "!some_sentence", 0, 0, k_snd_stop, nullptr );
    CHECK( altered );
    CHECK( channels[5].source == nullptr );
}

static void test_alter_channel_no_match_returns_false()
{
    std::vector<MixChannel> channels( 7 ); // all free
    CHECK( !alter_channel( channels, 7, 3, 2, 10, "foo.wav", 0, 0, k_snd_stop, nullptr ) );
}

// ===========================================================================
// 4. SND_Spatialize / S_SpatializeChannel (s_main.c:539-608). Hand-derived
//    goldens — every value computed in the comment beside it.
// ===========================================================================

// A trivial IEntitySpatialProvider stub returning a fixed origin (or failing).
namespace {
class FixedProvider final : public IEntitySpatialProvider
{
public:
    Vec3 origin_ {};
    bool succeed_ = true;
    bool resolve_origin( int, Vec3 &out ) noexcept override
    {
        if( !succeed_ )
            return false;
        out = origin_;
        return true;
    }
};
} // namespace

static void test_spatialize_pan_golden()
{
    // spatialize_pan(200, dot=0.5, dist=0.2):
    //   scale_r = (1-0.2)*(1+0.5) = 0.8*1.5 = 1.2 -> rvol = round(200*1.2) = 240
    //   scale_l = (1-0.2)*(1-0.5) = 0.8*0.5 = 0.4 -> lvol = round(200*0.4) = 80
    const SpatializePan p1 = spatialize_pan( 200, 0.5f, 0.2f );
    CHECK_EQ( p1.right_vol, 240 );
    CHECK_EQ( p1.left_vol, 80 );

    // spatialize_pan(255, dot=-1.0, dist=0.0): right clamps to 0, left clamps
    // to 255 (raw 510 saturates).
    const SpatializePan p2 = spatialize_pan( 255, -1.0f, 0.0f );
    CHECK_EQ( p2.right_vol, 0 );
    CHECK_EQ( p2.left_vol, 255 );
}

static void test_spatialize_full_pan_golden()
{
    // listener at origin, facing so `right` == +X. Provider places the
    // channel at {5,0,0}: source_vec = {5,0,0}, dist(raw) = 5, dir = {1,0,0},
    // dot = dot({1,0,0},{1,0,0}) = 1.0. dist_mult = 0.1 -> dist arg = 5*0.1 = 0.5.
    // spatialize_pan(100, 1.0, 0.5): scale_r=(0.5)*(2)=1.0 -> rvol=100;
    //                                scale_l=(0.5)*(0)=0.0 -> lvol=0.
    FixedProvider provider;
    provider.origin_ = Vec3{ 5.0f, 0.0f, 0.0f };

    MixChannel ch{};
    ch.entnum     = 5;    // != listener_entnum(1)
    ch.master_vol = 100;
    ch.dist_mult  = 0.1f;

    spatialize( ch, /*listener_entnum*/ 1, /*listener_origin*/ Vec3{}, /*listener_right*/ Vec3{ 1.0f, 0.0f, 0.0f },
               /*bugcomp_attn_none*/ false, &provider );
    CHECK_EQ( ch.rightvol, 100 );
    CHECK_EQ( ch.leftvol, 0 );
    CHECK( ch.origin.x == 5.0f && ch.origin.y == 0.0f && ch.origin.z == 0.0f ); // provider's origin was written back
}

// ---------------------------------------------------------------------------
// F-8: IEntitySpatialProvider::resolve_origin is IN/OUT. A provider mirroring
// CL_GetEntitySpatialization's absent/unparsed-entity branch verbatim
// (cl_frame.c:1387-1392) NEVER writes `origin` and returns "is the caller's
// existing origin non-zero?".
// ---------------------------------------------------------------------------
namespace {
class AbsentEntityProvider final : public IEntitySpatialProvider
{
public:
    int calls = 0;
    bool resolve_origin( int, Vec3 &origin ) noexcept override
    {
        ++calls;
        // valid_origin = VectorIsNull( ch->origin ) ? false : true;  (:1387)
        // ...then `return valid_origin;` WITHOUT touching ch->origin (:1392).
        return !( origin.x == 0.0f && origin.y == 0.0f && origin.z == 0.0f );
    }
};
} // namespace

static void test_spatialize_unresolvable_entity_keeps_server_origin()
{
    AbsentEntityProvider provider;

    // A sound started with a valid server-supplied `pos` for an entity the
    // client has not parsed yet. Legacy plays it AT `pos`; before the F-8 fix
    // the seam handed the provider a zero-initialised origin, so it reported
    // "invalid" and the sound was silently inaudible.
    MixChannel ch{};
    ch.entnum     = 5; // != listener(1) and not FL_CHAN_STATIC_SOUND -> provider IS consulted
    ch.master_vol = 100;
    ch.dist_mult  = 0.1f;
    ch.origin     = Vec3{ 5.0f, 0.0f, 0.0f }; // the server-supplied pos

    spatialize( ch, /*listener_entnum*/ 1, Vec3{}, Vec3{ 1.0f, 0.0f, 0.0f }, false, &provider );
    CHECK_EQ( provider.calls, 1 );
    // Audible, and at the ORIGINAL position — same golden as
    // test_spatialize_full_pan_golden (dot=1.0, dist=5*0.1=0.5).
    CHECK_EQ( ch.rightvol, 100 );
    CHECK_EQ( ch.leftvol, 0 );
    CHECK( ch.origin.x == 5.0f && ch.origin.y == 0.0f && ch.origin.z == 0.0f );

    // The other half of the same legacy line: a ZERO origin with no entity is
    // genuinely invalid, and legacy zeroes the volumes (s_main.c:582-585).
    MixChannel silent{};
    silent.entnum     = 5;
    silent.master_vol = 100;
    silent.dist_mult  = 0.1f;
    spatialize( silent, 1, Vec3{}, Vec3{ 1.0f, 0.0f, 0.0f }, false, &provider );
    CHECK_EQ( silent.leftvol, 0 );
    CHECK_EQ( silent.rightvol, 0 );
}

static void test_spatialize_view_entity_full_volume()
{
    MixChannel ch{};
    ch.entnum     = 1;
    ch.master_vol = 77;
    spatialize( ch, /*listener_entnum*/ 1, Vec3{}, Vec3{}, false, nullptr ); // no provider needed
    CHECK_EQ( ch.leftvol, 77 );
    CHECK_EQ( ch.rightvol, 77 );
}

static void test_spatialize_provider_failure_zeroes_volume()
{
    FixedProvider provider;
    provider.succeed_ = false;

    MixChannel ch{};
    ch.entnum     = 5;
    ch.master_vol = 200;
    spatialize( ch, 1, Vec3{}, Vec3{}, false, &provider );
    CHECK_EQ( ch.leftvol, 0 );
    CHECK_EQ( ch.rightvol, 0 );
}

static void test_spatialize_static_channel_ignores_provider()
{
    // FL_CHAN_STATIC_SOUND channels use ch.origin AS-IS — the provider must
    // NOT be consulted (nullptr proves it).
    MixChannel ch{};
    ch.entnum     = 5;
    ch.master_vol = 100;
    ch.dist_mult  = 0.1f;
    ch.flags      = ::xash::abi::k_fl_chan_static_sound;
    ch.origin     = Vec3{ 5.0f, 0.0f, 0.0f };

    spatialize( ch, 1, Vec3{}, Vec3{ 1.0f, 0.0f, 0.0f }, false, /*provider*/ nullptr );
    // Same golden as test_spatialize_full_pan_golden (dot=1.0, dist=0.5).
    CHECK_EQ( ch.rightvol, 100 );
    CHECK_EQ( ch.leftvol, 0 );
}

static void test_spatialize_bugcomp_attn_none_toggle()
{
    // ATTN_NONE (dist_mult == 0): the "dist" argument is always 0 regardless
    // of bugcomp (dist_mult multiplies the raw distance to 0 either way).
    // bugcomp_attn_none FALSE (modern, default) zeroes `dot` too:
    //   spatialize_pan(100, dot=0, dist=0): scale=(1)*(1+0)=1 -> rvol=100;
    //                                       scale=(1)*(1-0)=1 -> lvol=100.
    // bugcomp_attn_none TRUE (legacy compat kept) leaves dot=1.0:
    //   spatialize_pan(100, dot=1, dist=0): scale=(1)*(2)=2 -> rvol=200 (clamped from 200, still 200);
    //                                       scale=(1)*(0)=0 -> lvol=0.
    FixedProvider provider;
    provider.origin_ = Vec3{ 5.0f, 0.0f, 0.0f };

    MixChannel modern{};
    modern.entnum     = 5;
    modern.master_vol = 100;
    modern.dist_mult  = 0.0f; // ATTN_NONE
    spatialize( modern, 1, Vec3{}, Vec3{ 1.0f, 0.0f, 0.0f }, /*bugcomp*/ false, &provider );
    CHECK_EQ( modern.leftvol, 100 );
    CHECK_EQ( modern.rightvol, 100 );

    MixChannel legacy_compat{};
    legacy_compat.entnum     = 5;
    legacy_compat.master_vol = 100;
    legacy_compat.dist_mult  = 0.0f;
    spatialize( legacy_compat, 1, Vec3{}, Vec3{ 1.0f, 0.0f, 0.0f }, /*bugcomp*/ true, &provider );
    CHECK_EQ( legacy_compat.leftvol, 0 );
    CHECK_EQ( legacy_compat.rightvol, 200 );
}

// ===========================================================================
// 5. SfxRegistry — S_RegisterSound/S_LoadSound lazy resolution (S9.6).
// ===========================================================================

namespace {
class CountingLoader final : public IAudioLoader
{
public:
    int load_calls = 0;
    std::optional<AudioData> load( std::string_view name ) noexcept override
    {
        ++load_calls;
        if( name == "known.wav" )
            return make_audio( 3 );
        return std::nullopt;
    }
};
} // namespace

static void test_registry_register_does_not_resolve_play_does()
{
    CountingLoader loader;
    SfxRegistry     reg( &loader );

    const SfxHandle h = reg.register_sound( "known.wav" );
    CHECK( h != k_invalid_sound_handle );
    CHECK_EQ( loader.load_calls, 0 ); // "register does not resolve"

    const AudioData *audio = reg.load_sfx( h );
    REQUIRE( audio != nullptr );
    CHECK_EQ( loader.load_calls, 1 ); // "play does"
    CHECK_EQ( audio->samples, 3u );

    // Second load hits the cache — no further loader call.
    const AudioData *audio2 = reg.load_sfx( h );
    CHECK_EQ( audio2, audio );
    CHECK_EQ( loader.load_calls, 1 );
}

// ---------------------------------------------------------------------------
// SfxRegistry retains decoded audio for its whole lifetime (gate findings
// parity F-1/F-2, concurrency CONC-1). Two channels share ONE sfx; one retires
// its VOX word (VOX_FreeWord -> IVoxAudioResolver::release, which legacy
// implements as FS_FreeSound(word->sfx->cache)); the survivor's borrowed
// pointer must remain valid, with unchanged samples.
//
// This is the invariant that makes it safe to put a borrowed const AudioData*
// in a queued AudioCommand and in MixChannel::source while T_AudioDecoder owns
// the channel array — see SfxSlot::cache in registry.hpp.
// ---------------------------------------------------------------------------
static void test_registry_retains_shared_audio_when_one_borrower_releases()
{
    CountingLoader loader;
    SfxRegistry    reg( &loader ); // the production IVoxAudioResolver

    VoxSystem vox;

    // Two channels, each with a one-word sentence naming the SAME sfx. The word
    // is left unresolved (audio == nullptr, path set) so bind_channel()'s
    // load_word() goes through the real resolve() path — exactly like a live
    // VOX word advance.
    const auto one_word_sentence = []() {
        VoxSentence s;
        VoxWord     w{};
        w.path = "known.wav";
        s.words.push_back( w );
        return s;
    };

    MixChannel chan_a{};
    MixChannel chan_b{};
    vox.bind_channel( chan_a, one_word_sentence(), &reg );
    vox.bind_channel( chan_b, one_word_sentence(), &reg );

    REQUIRE( chan_a.source != nullptr );
    CHECK_EQ( chan_b.source, chan_a.source ); // ONE decode, ONE shared address
    CHECK_EQ( loader.load_calls, 1 );

    const AudioData    *shared        = chan_a.source;
    const std::uint32_t shared_samples = shared->samples;
    const std::size_t   shared_bytes   = shared->buffer.size();
    const std::byte     first_byte     = shared->buffer.front();

    // chan_b's word resolved with in_cache == true (already decoded), so it
    // never calls release(). chan_a's word is the OWNER by the legacy
    // FL_VOXWORD_IN_CACHE rule, so retiring it is what would run
    // FS_FreeSound in legacy.
    vox.unbind_channel( chan_a );
    CHECK( chan_a.source == nullptr ); // vox_free_word_fields' unconditional zeroing — PINNED, unchanged

    // The survivor still points at LIVE, UNCHANGED audio. Before the fix this
    // read was a use-after-free (and, with the decoder owning chan_b, a
    // cross-thread one).
    REQUIRE( chan_b.source != nullptr );
    CHECK_EQ( chan_b.source, shared );
    CHECK_EQ( chan_b.source->samples, shared_samples );
    CHECK_EQ( chan_b.source->buffer.size(), shared_bytes );
    CHECK_EQ( chan_b.source->buffer.front(), first_byte );

    // ...and the registry still hands out that SAME address, with no silent
    // re-decode (which would also have moved it).
    const SfxHandle h = reg.find_name( "known.wav" );
    CHECK_EQ( reg.load_sfx( h ), shared );
    CHECK_EQ( reg.cached( h ), shared );
    CHECK_EQ( loader.load_calls, 1 );

    vox.unbind_channel( chan_b );
    CHECK_EQ( reg.cached( h ), shared ); // still retained after the LAST borrower left
}

// The CONC-9 lock/unlock/lock split: install() is double-checked, so a caller
// that decoded with the mutex released and lost the race discards its own
// decode and gets the incumbent's (address-stable) pointer.
static void test_registry_double_checked_install_keeps_the_incumbent()
{
    SfxRegistry     reg( nullptr );
    const SfxHandle h = reg.find_name( "shared.wav" );
    REQUIRE( h != k_invalid_sound_handle );

    CHECK( reg.cached( h ) == nullptr ); // phase 1: nothing decoded yet

    // Winner installs first.
    const AudioData *winner = reg.install( h, make_audio( 11 ) );
    REQUIRE( winner != nullptr );
    CHECK_EQ( winner->samples, 11u );
    CHECK_EQ( reg.cached( h ), winner );

    // Loser's decode is discarded; it gets the incumbent back, unchanged.
    const AudioData *loser = reg.install( h, make_audio( 22 ) );
    CHECK_EQ( loser, winner );      // same address (never relocated)
    CHECK_EQ( loser->samples, 11u ); // ...and the winner's content survives

    // The nullopt path is the S_CreateDefaultSound fallback, and it too is
    // subject to the double check.
    const SfxHandle h2 = reg.find_name( "other.wav" );
    const AudioData *def = reg.install( h2, std::nullopt );
    REQUIRE( def != nullptr );
    CHECK_EQ( def->samples, static_cast<std::uint32_t>( ::xash::limits::sound_dma_speed ) );
    CHECK_EQ( reg.install( h2, make_audio( 5 ) ), def );

    // Sentence/invalid handles have no slot at all.
    CHECK( reg.install( k_sentence_handle, make_audio( 1 ) ) == nullptr );
    CHECK( reg.cached( k_invalid_sound_handle ) == nullptr );
}

static void test_registry_default_sound_fallback()
{
    SfxRegistry reg( nullptr ); // no loader at all -- headless
    const SfxHandle h = reg.register_sound( "missing.wav" );
    const AudioData *audio = reg.load_sfx( h );
    REQUIRE( audio != nullptr );
    CHECK_EQ( audio->samples, static_cast<std::uint32_t>( ::xash::limits::sound_dma_speed ) ); // S_CreateDefaultSound
    CHECK_EQ( static_cast<int>( audio->channels ), 1 );
}

static void test_registry_find_name_dedup()
{
    SfxRegistry reg( nullptr );
    const SfxHandle h1 = reg.find_name( "weapons/ak47.wav" );
    const SfxHandle h2 = reg.find_name( "weapons/ak47.wav" );
    CHECK_EQ( h1, h2 ); // same name -> same slot (s_load.c:166-179)
}

static void test_registry_sentence_routing_and_overwrite_quirk()
{
    SfxRegistry reg( nullptr );
    const SfxHandle h1 = reg.register_sound( "!hello" );
    CHECK_EQ( h1, k_sentence_handle );
    const SfxHandle h2 = reg.register_sound( "!world" );
    CHECK_EQ( h2, k_sentence_handle ); // same sentinel every time (s_load.c:32)

    // The documented single-slot overwrite quirk: get() re-derives from
    // WHATEVER the immediate slot currently holds — the SECOND registration.
    const SfxSlot *slot = reg.get( k_sentence_handle );
    REQUIRE( slot != nullptr );
    CHECK( slot->name == "!world" );
}

// ===========================================================================
// 6. VoxSystem's S9.6 additions — IVoxTimeLeftQuery.
// ===========================================================================

static void test_vox_time_left()
{
    // word0: 4 loud samples, start=0%/end=100% -> chan.sample=0, forced_end=4
    // (no trim adjustment — see make_audio's loud-sample rationale).
    // word1: 2 samples, end=50% -> contributes round(2*0.01*50)=1 sample of
    // lookahead.
    AudioData word0 = make_audio( 4 );
    AudioData word1 = make_audio( 2 );

    VoxSentence sentence;
    sentence.words.push_back( VoxWord{ &word0, 100, 100, 0, 0, 100, false } );
    sentence.words.push_back( VoxWord{ &word1, 100, 100, 0, 0, 50, false } );

    VoxSystem  vox;
    MixChannel chan{};
    vox.bind_channel( chan, std::move( sentence ), /*resolver*/ nullptr ); // loads word 0
    REQUIRE( ( chan.flags & ::xash::abi::k_fl_chan_sentence_finished ) == 0 );

    // current word remaining (forced_end - sample) + word1's 1-sample lookahead.
    CHECK_EQ( vox.time_left( chan ), static_cast<int>( chan.forced_end - chan.sample ) + 1 );

    chan.flags |= ::xash::abi::k_fl_chan_sentence_finished;
    CHECK_EQ( vox.time_left( chan ), 0 );

    vox.unbind_channel( chan );

    MixChannel unbound{};
    CHECK_EQ( vox.time_left( unbound ), 0 );
}

// S9.6 parity-audit fix (F-1): time_left FORCE-resolves lookahead words like
// legacy's S_LoadSound calls (s_main.c:311-313) — an unresolved future word
// must contribute to the estimate, and the resolve must be cached with
// load_word()'s bookkeeping (no re-resolve on the next query or advance).
namespace {
class TestLookaheadResolver final : public IVoxAudioResolver
{
public:
    const AudioData *w0       = nullptr;
    const AudioData *w1       = nullptr;
    int              resolves = 0;

    [[nodiscard]] const AudioData *resolve( std::string_view path, bool &in_cache ) noexcept override
    {
        ++resolves;
        in_cache = true; // shared/keep-cached: VoxSystem must never release these
        if( path == "w0" ) return w0;
        if( path == "w1" ) return w1;
        return nullptr;
    }
    void release( const AudioData * ) noexcept override {}
};
} // namespace

static void test_vox_time_left_forces_lookahead_resolution()
{
    AudioData word0 = make_audio( 4 );
    AudioData word1 = make_audio( 2 );

    TestLookaheadResolver resolver;
    resolver.w0 = &word0;
    resolver.w1 = &word1;

    // Production shape: paths recorded, `audio` unset (what build_sentence
    // leaves behind under the S9.6 lazy-resolution fix).
    VoxSentence sentence;
    sentence.words.push_back( VoxWord{ nullptr, 100, 100, 0, 0, 100, false, "w0" } );
    sentence.words.push_back( VoxWord{ nullptr, 100, 100, 0, 0, 50, false, "w1" } );

    VoxSystem  vox;
    MixChannel chan{};
    vox.bind_channel( chan, std::move( sentence ), &resolver );
    REQUIRE( resolver.resolves == 1 ); // load_word resolved word 0 only

    // word1 (2 samples, end=50%) must be FORCE-resolved and contribute
    // (int)((float)remaining + 2*0.01f*50) = remaining + 1 (s_main.c:311-318).
    const int expected = static_cast<int>( chan.forced_end - chan.sample ) + 1;
    CHECK_EQ( vox.time_left( chan ), expected );
    CHECK_EQ( resolver.resolves, 2 ); // lookahead resolve happened...
    CHECK_EQ( vox.time_left( chan ), expected );
    CHECK_EQ( resolver.resolves, 2 ); // ...and was cached (load_word bookkeeping)

    vox.unbind_channel( chan );
}

// S9.6 parity-audit fix (F-3): legacy accumulates `remaining += samples *
// 0.01f * end` via float compound-assign on an int — remaining is promoted
// to float, added, truncated back EACH iteration (s_main.c:317-318). With a
// negative current-word remaining and a small fractional term the shapes
// differ: (int)(-1.0f + 0.7f) == 0 (legacy), -1 + (int)0.7f == -1 (a
// per-term-truncating port).
static void test_vox_time_left_negative_float_accumulation()
{
    AudioData word0 = make_audio( 4 );
    AudioData word1 = make_audio( 1 );

    VoxSentence sentence;
    sentence.words.push_back( VoxWord{ &word0, 100, 100, 0, 0, 100, false } );
    sentence.words.push_back( VoxWord{ &word1, 100, 100, 0, 0, 70, false } ); // 1*0.01f*70 = 0.7f

    VoxSystem  vox;
    MixChannel chan{};
    vox.bind_channel( chan, std::move( sentence ), /*resolver*/ nullptr );

    chan.sample = chan.forced_end + 1; // current word momentarily over-run: remaining = -1
    CHECK_EQ( vox.time_left( chan ), 0 ); // (int)(-1.0f + 0.7f) — NOT -1

    vox.unbind_channel( chan );
}

// ===========================================================================
// 7. Sound end-to-end: start/stop/register/channels_snapshot + a command
//    dispatched through a real CmdCvarContext (proving the user-data
//    plumbing — campaign B5's Context::cmd_add(name, CommandCtxFn, user,...)
//    overload).
// ===========================================================================

namespace {

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

} // namespace

static void test_sound_start_local_and_snapshot()
{
    AlwaysTrustedOracle oracle;
    NullPolicy          policy;
    ::xash::cmd_cvar::CmdCvarContext ctx;
    REQUIRE( ctx.init( { &oracle, &policy } ) );

    SinkDevice        sink;
    SoundInitParams   params;
    params.device   = &sink;
    params.cmd_cvar = &ctx;

    Sound snd;
    REQUIRE( snd.init( params ).has_value() );

    // No update_frame() call: listener_.entnum defaults to 0, and
    // start_local_sound() always plays ON the listener entity — the "view
    // entity full volume" spatialize path (no provider needed).
    snd.start_local_sound( "cmdtest_direct.wav", 1.0f, /*reliable*/ false );

    std::vector<ChannelInfo> snap = snd.channels_snapshot();
    REQUIRE( snap.size() == std::size_t{ 1 } );
    CHECK( snap[0].sfx_name == "cmdtest_direct.wav" );
    CHECK_EQ( snap[0].entnum, 0 );
    CHECK_EQ( snap[0].left_vol, 255 );  // VOL_NORM(1.0)*255, full-volume view-entity path
    CHECK_EQ( snap[0].right_vol, 255 );
    CHECK( snap[0].channel_class == ChannelClass::Dynamic );

    // stop_all_sounds resets to an empty snapshot (S_StopAllSounds, s_main.c:1465-1492).
    snd.stop_all_sounds( true );
    CHECK( snd.channels_snapshot().empty() );

    snd.shutdown();
}

static void test_sound_command_dispatch_user_data_plumbing()
{
    AlwaysTrustedOracle oracle;
    NullPolicy          policy;
    ::xash::cmd_cvar::CmdCvarContext ctx;
    REQUIRE( ctx.init( { &oracle, &policy } ) );

    SinkDevice      sink;
    SoundInitParams params;
    params.device   = &sink;
    params.cmd_cvar = &ctx;

    Sound snd;
    REQUIRE( snd.init( params ).has_value() );

    // "play <name>" reaches Sound::cmd_play_f (user == &snd), which calls
    // start_local_sound() — proving the CommandCtxFn user-data plumbing
    // (campaign B5) round-trips correctly through a REAL CmdCvarContext.
    ctx.cmd_execute_string( "play cmd_plumbing_test.wav" );

    std::vector<ChannelInfo> snap = snd.channels_snapshot();
    REQUIRE( snap.size() == std::size_t{ 1 } );
    CHECK( snap[0].sfx_name == "cmd_plumbing_test.wav" );

    // "stopsound" (-> Sound::cmd_stopsound_f -> stop_all_sounds(true)) also
    // round-trips through the same plumbing.
    ctx.cmd_execute_string( "stopsound" );
    CHECK( snd.channels_snapshot().empty() );

    snd.shutdown();
}

static void test_sound_register_is_lazy_end_to_end()
{
    AlwaysTrustedOracle oracle;
    NullPolicy          policy;
    ::xash::cmd_cvar::CmdCvarContext ctx;
    REQUIRE( ctx.init( { &oracle, &policy } ) );

    SinkDevice      sink;
    SoundInitParams params; // no filesystem -> any load falls back to the
                             // synthesized default sound (never touches disk)
    params.device   = &sink;
    params.cmd_cvar = &ctx;

    Sound snd;
    REQUIRE( snd.init( params ).has_value() );

    // register_sound() alone must not create any channel/audible state.
    const ::xash::abi::sound_t h = snd.register_sound( "lazy.wav" );
    CHECK( h >= 0 );
    CHECK( snd.channels_snapshot().empty() );

    // start_sound() with the SAME handle is what actually resolves/plays it.
    snd.start_sound( Vec3{ 0.0f, 0.0f, 0.0f }, /*ent*/ 0, /*chan*/ k_chan_static, h, 1.0f, k_attn_none,
                     k_pitch_norm_flag, 0 );
    CHECK( snd.channels_snapshot().size() == std::size_t{ 1 } );

    snd.shutdown();
}

// The same F-8 scenario end-to-end through the real entry surface, which is
// where the T_Main-side pre-fill lives (Sound::start_sound packs
// AudioCommand::entity_origin). Before the fix the channel was allocated,
// spatialized to silence and then dropped by the first-audibility check
// (s_main.c:719-734) — so the snapshot came back EMPTY.
static void test_sound_start_with_pos_survives_unresolvable_entity()
{
    AbsentEntityProvider provider;

    SinkDevice      sink;
    SoundInitParams params;
    params.device  = &sink;
    params.spatial = &provider;

    Sound snd;
    REQUIRE( snd.init( params ).has_value() );

    ListenerSnapshot listener {};
    listener.entnum = 1; // the sound's entnum (5) is NOT the listener
    snd.update_frame( listener );

    const ::xash::abi::sound_t h = snd.register_sound( "unparsed_entity.wav" );
    REQUIRE( h != k_invalid_sound_handle );

    snd.start_sound( Vec3{ 5.0f, 0.0f, 0.0f }, /*ent*/ 5, k_chan_auto, h, 1.0f, k_attn_none, k_pitch_norm_flag,
                     0 );
    CHECK_EQ( provider.calls, 1 );

    const std::vector<ChannelInfo> snap = snd.channels_snapshot();
    REQUIRE( snap.size() == std::size_t{ 1 } ); // audible, not silenced
    CHECK( snap[0].left_vol != 0 || snap[0].right_vol != 0 );
    CHECK( snap[0].origin.x == 5.0f );

    // A sound with NO usable position for the same unparsed entity is still
    // dropped, exactly as legacy drops it.
    snd.stop_all_sounds( true );
    snd.start_sound( Vec3{ 0.0f, 0.0f, 0.0f }, /*ent*/ 5, k_chan_auto, h, 1.0f, k_attn_none, k_pitch_norm_flag,
                     0 );
    CHECK( snd.channels_snapshot().empty() );

    snd.shutdown();
}

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    // 1. Channel allocation (SND_PickDynamicChannel).
    RUN_TEST( test_pick_dynamic_same_entity_override );
    RUN_TEST( test_pick_dynamic_client_protection );
    RUN_TEST( test_pick_dynamic_anti_restart_ignore );
    RUN_TEST( test_pick_dynamic_stream_already_playing );
    RUN_TEST( test_channel_time_left_plain_and_finished );

    // 2. SND_PickStaticChannel.
    RUN_TEST( test_pick_static_reuse_exact_match );
    RUN_TEST( test_pick_static_allocate_new );
    RUN_TEST( test_pick_static_overflow );

    // 3. S_AlterChannel / S_MaybeAlterChannel.
    RUN_TEST( test_alter_channel_stop );
    RUN_TEST( test_alter_channel_change_vol_pitch );
    RUN_TEST( test_alter_channel_sentence_matches_any_sfx );
    RUN_TEST( test_alter_channel_no_match_returns_false );

    // 4. SND_Spatialize / S_SpatializeChannel (hand-derived goldens).
    RUN_TEST( test_spatialize_pan_golden );
    RUN_TEST( test_spatialize_full_pan_golden );
    RUN_TEST( test_spatialize_view_entity_full_volume );
    RUN_TEST( test_spatialize_provider_failure_zeroes_volume );
    RUN_TEST( test_spatialize_unresolvable_entity_keeps_server_origin );
    RUN_TEST( test_spatialize_static_channel_ignores_provider );
    RUN_TEST( test_spatialize_bugcomp_attn_none_toggle );

    // 5. SfxRegistry lazy resolution.
    RUN_TEST( test_registry_register_does_not_resolve_play_does );
    RUN_TEST( test_registry_retains_shared_audio_when_one_borrower_releases );
    RUN_TEST( test_registry_double_checked_install_keeps_the_incumbent );
    RUN_TEST( test_registry_default_sound_fallback );
    RUN_TEST( test_registry_find_name_dedup );
    RUN_TEST( test_registry_sentence_routing_and_overwrite_quirk );

    // 6. VoxSystem::time_left (IVoxTimeLeftQuery).
    RUN_TEST( test_vox_time_left );
    RUN_TEST( test_vox_time_left_forces_lookahead_resolution );
    RUN_TEST( test_vox_time_left_negative_float_accumulation );

    // 7. Sound end-to-end + command dispatch through a real CmdCvarContext.
    RUN_TEST( test_sound_start_local_and_snapshot );
    RUN_TEST( test_sound_command_dispatch_user_data_plumbing );
    RUN_TEST( test_sound_register_is_lazy_end_to_end );
    RUN_TEST( test_sound_start_with_pos_survives_unresolvable_entity );

    std::printf( "sound_entry: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
