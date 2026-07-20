#pragma once
// xash3dpp — VOX sentence word-sequencer (Chunk 9, slice S9.4). PARITY-CRITICAL.
// Legacy reference: engine/client/sound/s_vox.c (ALL of it — sentences.txt
// parse, sentence lookup, word-sequence build, the word param grammar, the
// voxword_t lifecycle, amplitude trimming, the sentence pitch bend) +
// engine/client/sound/s_load.c:32-33,312-345 (the `s_sentenceImmediateName`
// single-slot handoff) + common/sound_api.h (voxword_t). s_mouth.c (lip-sync
// amplitude tracking) is REFERENCE-ONLY background for this slice's §3 recon
// pairing but is NOT ported here — no deliverable in the S9.4 brief names it,
// and the two mouth-write call sites the S9.3 Mixer flagged
// ("(S9.4) mouth ... omitted") stay deferred; only the VOX_ModifyPitch mix-
// side gap that same slice flagged is closed here (see mixer.hpp's
// MixChannel::vox_pitch + compute_channel_pitch's new word_pitch parameter).
//
// Boundary spec: docs/boundaries/sound-boundary.md §Quirks ("VOX single-
// immediate-slot + FreeWord"), §Owned state (`rgpszrawsentence`/
// `cszrawsentences` -> Sound-owned table, `s_sentenceImmediateName` -> single
// slot), §Extension axes P-3 ("Rewrite should own the sentence table on a
// Sound-subsystem object and replace the immediate-name hack with an
// explicit return/parameter").
//
// S9.6 wiring point (this slice is a SELF-CONTAINED component, no S_FindName/
// S_RegisterSound dependency): S9.6 owns real channel_t<->MixChannel bridging
// and the sfx_t registry. Until then:
//   - IVoxAudioResolver is the S_FindName-registration + S_LoadSound-decode
//     seam, COLLAPSED into one call (see its doc comment for why this is a
//     documented simplification, not a silent behaviour change).
//   - VoxSystem::bind_channel()/unbind_channel() are the S_StartSound /
//     S_FreeChannel hook points S9.6 calls once real channel allocation
//     lands; VoxSystem::next_word() (IVoxWordAdvance) is already wired into
//     Mixer::set_vox_advance() by whoever owns Sound::Impl construction.
//   - ImmediateSentenceSlot is the s_sentenceImmediateName replacement S9.6's
//     S_RegisterSound-equivalent will read/write around the SENTENCE_INDEX
//     sentinel handle.
//
// @thread-safety: VoxSystem is confined to whichever thread OWNS THE CHANNEL
// ARRAY, because next_word()/bind_channel()/unbind_channel() mutate the same
// per-channel state the mix loop reads every block. That owner is
// T_AudioDecoder while the S9.7b topology runs and T_Main when it does not
// (audio_command.hpp §The split) — a CONDITIONAL role, not a fixed one.
//
// compliance-allow(thread-assert): the confinement is real but its role is
// conditional, so no single per-call assert can express it — asserting
// AudioDecoder here would fire on every legitimate topology-off call, and
// asserting Main would fire on every threaded one. It is enforced at the two
// entry points that DO know the mode instead: Sound::* asserts
// ThreadRole::Main (sound.cpp) and AudioTopology::decoder_step() asserts
// ThreadRole::AudioDecoder (topology.cpp), and every VoxSystem call in the tree
// is reached through exactly one of them — build_sentence/bind_channel/
// apply_word_volume/time_left via apply_command() -> apply_start()
// (audio_command.cpp) or free_all_channels(), and next_word() via
// Mixer::paint_channels(), which itself asserts AudioDecoder (mixer.cpp). A
// redundant per-call assert underneath those gates would buy nothing and would
// have to encode the mode to be correct at all.
// (The S9.3/S9.4 rationale — "no decoder thread exists until S9.7b" — EXPIRED
// with this slice and has been replaced by the above.)
//
// load_sentence_file() and ImmediateSentenceSlot::set() are T_Main
// registration-time entry points in the legacy model (VOX_Init,
// S_RegisterSound); with the topology running they must be marshalled to the
// channel-owning thread the same way ListenerSnapshot/MixGateSnapshot cross
// today (SND-OQ-1) — not enforced here. Neither has a caller in the tree yet.

#include <xash3dpp/abi/sound_api.hpp>
#include <xash3dpp/private/sound/mixer.hpp> // MixChannel, IVoxWordAdvance
#include <xash3dpp/sound/audio_data.hpp>
#include <xash3dpp/limits.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace xash::sound {

// ---------------------------------------------------------------------------
// VoxSentenceEntry — one sentences.txt row (name + raw word-list text). The
// legacy storage ("name\0value" back-to-back in one Mem_Malloc'd buffer,
// s_vox.c:557-566) is replaced by two owned std::strings; `value` is kept
// RAW (not leading-whitespace-trimmed) exactly like the legacy buffer — the
// trim happens at lookup time (vox_lookup_string), matching
// VOX_LookupString's own `for( ; *c==' '||*c=='\t'; c++ )` scan verbatim.
// ---------------------------------------------------------------------------
struct VoxSentenceEntry
{
    std::string name;
    std::string value;
};

// parse_sentence_file (VOX_ReadSentenceFile_, s_vox.c:523-568) — appends
// parsed entries to `out` up to xash::limits::sound_vox_sentence_table_max
// (CVOXFILESENTENCEMAX, 4096); silently stops at the cap, matching legacy's
// bare `break` (no error). `data` need NOT be NUL-terminated (unlike
// legacy's FS_LoadFile buffer, which always is) — see vox.cpp for the one
// documented bounds-guard this requires (H-4 discipline: never read past the
// span). VoxSystem::load_sentence_file() clears the table first (VOX_Shutdown
// + VOX_ReadSentenceFile's own VOX_Shutdown() call, s_vox.c:575); this free
// function only APPENDS, so it is independently testable against a
// hand-built table.
void parse_sentence_file( std::span<const std::byte> data, std::vector<VoxSentenceEntry> &out ) noexcept;

// vox_lookup_string (VOX_LookupString, s_vox.c:257-298) — pure lookup over an
// explicit table span, exposed standalone so the embedded legacy test's
// "manually seed 5 entries, bypass file parsing" isolation
// (Test_VOX_LookupString, s_vox.c:624-656) ports directly without needing a
// full VoxSystem + sentences.txt. Order of resolution (verbatim): '#' prefix
// -> immediate (return the remainder, no table lookup at all); else an
// ALL-DIGIT string (Q_isdigit's whole-string semantics, not just the first
// character) -> numeric index, reset to "not found" if out of range; else a
// case-insensitive linear name scan. Returns nullopt if nothing matched.
[[nodiscard]] std::optional<std::string_view> vox_lookup_string( std::span<const VoxSentenceEntry> table,
                                                                  std::string_view pszin ) noexcept;

// vox_get_directory (VOX_GetDirectory, s_vox.c:223-255) — splits a leading
// "dir/" prefix off `psz` into `out_dir` (default "vox/" when there is no
// '/'), returning the remainder. The HACKHACK leading-'/' skip (s_vox.c:
// 228-232, "some modders send strings like /fvox/_period four") is preserved
// verbatim. `dir_max` mirrors VOX_LoadSound's real caller-supplied
// `szpath[32]` bound (s_vox.c:446) — a directory prefix (INCLUDING the
// trailing '/') longer than `dir_max` fails the call (nullopt), matching
// legacy's "invalid directory in: %s" error path (s_vox.c:245-249). Unlike
// legacy's fixed `char[32]`, `out_dir` is an owned std::string with no
// overflow-write hazard; the LEGACY buffer's own bound check is in fact
// off-by-one (`len > nsize` permits `len == nsize`, writing the NUL
// terminator one byte past `szpath[31]`) — this port keeps the same
// ACCEPT/REJECT decision boundary (`len > dir_max`) without the corresponding
// overflow, since std::string has no fixed capacity to overrun.
[[nodiscard]] std::optional<std::string_view> vox_get_directory(
    std::string_view psz, std::string &out_dir,
    std::size_t dir_max = ::xash::limits::sound_vox_dir_max ) noexcept;

// parse_string (VOX_ParseString, s_vox.c:300-352) — splits `buf` into up to
// xash::limits::sound_vox_word_max (CVOXWORDMAX, 64) words IN PLACE (legacy
// inserts a NUL at each delimiter; this port does the same via buf.data(),
// relying on std::string's guaranteed data()[size()]==0 sentinel exactly like
// legacy relies on its C-string's terminator). A mid-sentence '.'/',' that is
// NOT immediately followed by end-of-string synthesizes a "_period"/"_comma"
// pseudo-word (verbatim quirk); end-of-string punctuation does not. Returns
// the word count; `out[0..count)` are valid pointers into `buf` (or the two
// pseudo-word literals) usable directly with parse_word_params (which also
// mutates through them). One safety deviation from legacy (H-4 discipline,
// documented in vox.cpp): a "(...)" group with no closing ')' before the end
// of the buffer stops AT the terminator instead of reading one byte past it
// (legacy's raw psz++ there is UB on a non-legacy-shaped buffer).
std::size_t parse_string( std::string &buf, std::array<char *, ::xash::limits::sound_vox_word_max> &out ) noexcept;

// make_default_vox_word (VOX_MakeDefaultWordParams, s_vox.c:354-361) —
// volume=100, pitch=100, end=100 (start/timecompress default to 0). Same
// VoxWord type doubles as both the "params being parsed" and "resolved word"
// shape, matching legacy's single voxword_t use for both roles.
struct VoxWord
{
    const AudioData *audio        = nullptr; // resolved decoded audio (word->sfx, collapsed — see IVoxAudioResolver); nullptr = unresolved/failed (s_vox.c:150,182-187 both collapse to this)
    std::uint16_t    volume       = 100;     // voxword_t::volume (percent)
    std::uint16_t    pitch        = 100;     // voxword_t::pitch  (percent; PITCH_NORM == 100)
    std::uint8_t     timecompress = 0;       // voxword_t::timecompress (percent of skipped data)
    std::uint8_t     start        = 0;       // voxword_t::start  (percent playback start)
    std::uint8_t     end          = 100;     // voxword_t::end    (percent playback end)
    bool             in_cache     = false;   // FL_VOXWORD_IN_CACHE — "already registered/cached, don't release"

    // S9.6 lazy-resolution fix: the resolved "<dir>/<word>" path
    // (s_vox.c:500), stored so `audio` can be resolved LATER (at
    // word-advance time, load_word()) instead of eagerly at build_sentence()
    // time — see IVoxAudioResolver's doc comment and build_sentence()'s.
    // Appended at the END of the struct (not interleaved) so every existing
    // positional aggregate-init call site (`VoxWord{ &audio, 100, ... }`)
    // keeps compiling unchanged with `path` defaulting to "".
    std::string path;
};

[[nodiscard]] constexpr VoxWord make_default_vox_word() noexcept { return VoxWord{}; }

// parse_word_params (VOX_ParseWordParams, s_vox.c:363-442) — mutates `psz` in
// place (splits the bare word text from its "(vNN pNN sNN eNN tNN)" param
// block by inserting a NUL at '('), starting from `running_default` (the
// sentence-wide "current defaults" that carry across calls) and writing the
// resolved numeric fields into `word`. Grammar quirks preserved verbatim: an
// unrecognised leading run before a param letter is skipped, not an error;
// numeric literals are captured up to 7 digits (sizeof legacy sznum[8]-1) and
// STOP ADVANCING past the 8th digit character (excess digits fall through to
// the next command-search iteration, silently ignored as non-command chars);
// e/s/t clamp to [0,100], p/v clamp to [0,65535] (UINT16_MAX). Returns false
// for two DIFFERENT reasons the caller must NOT distinguish (matching
// VOX_LoadSound's own `continue` on either): a genuine syntax error (a ')'
// with no preceding '(') and a legitimate "defaults-only" block (empty word
// text after the split) — in the latter case `running_default` is updated
// to `word` before returning, so LATER calls see the new defaults
// (s_vox.c:434-439, the deliberate cross-call default-carryover the legacy
// embedded test pins).
[[nodiscard]] bool parse_word_params( char *psz, VoxWord &word, VoxWord &running_default ) noexcept;

// ---------------------------------------------------------------------------
// Amplitude trim (S_TrimStart/S_TrimEnd/S_TrimStartEndTimes, s_vox.c:31-139).
// PCM-only (non-PCM AudioData::type is a documented no-op passthrough,
// reserved for the mp3/ogg/opus satellite targets that never produce PCM).
// STRIDE QUIRK (verbatim, preserved even though it looks like a bug): each
// scan step advances the returned frame index by `channels` (1 or 2), not by
// 1 frame — for stereo audio this means the returned start/end drifts by 2x
// the number of frames actually scanned (s_vox.c:69-70,106-107). Confirmed
// against the legacy source; not something this port silently "fixes".
// ---------------------------------------------------------------------------

inline constexpr int k_vox_trim_scan_max   = 255; // TRIM_SCAN_MAX
inline constexpr int k_vox_trim_below_8    = 2;   // TRIM_SAMPLES_BELOW_8
inline constexpr int k_vox_trim_below_16   = 512; // TRIM_SAMPLES_BELOW_16 (65k*2/256)

[[nodiscard]] int trim_start( const AudioData &wav, int start ) noexcept;
[[nodiscard]] int trim_end( const AudioData &wav, int end ) noexcept;

// S_TrimStartEndTimes (s_vox.c:127-139) — trims `start`, derives `end` (the
// `end==0` sentinel means "to the last frame", `wav.samples - wav.channels`
// verbatim) and re-clamps it to the TRIMMED start before trimming it too,
// then writes chan.sample/chan.forced_end (as doubles, matching
// `ch->sample = start` / `ch->forced_end = ...`'s implicit int->double widen).
void trim_start_end_times( MixChannel &chan, const AudioData &wav, int start, int end ) noexcept;

// vox_percent_to_samples — the `round( percent * 0.01f * data->samples )`
// float-then-double-round chain VOX_LoadWord uses to turn a word's
// start/end PERCENT into a frame index (s_vox.c:167). FLOAT-EXACT: the
// multiply happens in `float` (0.01f is a float literal; the uint32 samples
// count promotes to float, not double, to match it) and is only WIDENED to
// double for the `round()` call (C's `round(double)` — the float argument
// undergoes the standard float->double promotion at the call site) — mirrors
// mixer.hpp's compute_channel_pitch float-chain-fidelity requirement.
[[nodiscard]] int vox_percent_to_samples( int percent, std::uint32_t samples ) noexcept;

// vox_free_word_fields — the UNCONDITIONAL half of VOX_FreeWord (s_vox.c:
// 170-188): zeroes chan.sample/chan.forced_end, clears FL_CHAN_FINISHED, and
// nulls chan.source (ch->data = NULL) on EVERY call, even when there is no
// valid word/word_index bound to it. Preserved verbatim per the legacy
// author's own inline TODO ("don't set random fields to zero lol, was memset
// before") — a documented suspect quirk, not fixed. Exposed standalone (no
// VoxSystem/resolver context needed) so this exact unconditional-zeroing
// behaviour is independently pinnable; VoxSystem::free_word() (private) calls
// this FIRST, then handles the word-array-side cache release.
void vox_free_word_fields( MixChannel &chan ) noexcept;

// vox_scale_volume (VOX_SetChanVol, s_vox.c:190-204) — scales an already-
// clamped channel volume [0,255] by the current word's volume percent
// (no-op at 100, the default). NOT wired into the Mixer this slice: legacy
// calls VOX_SetChanVol from S_SpatializeChannel (s_main.c), a pre-mix
// spatialization step outside the paint pipeline entirely (S9.5/S9.6
// territory, unlike VOX_ModifyPitch which sits INSIDE
// S_MixNormalChannelsToRoombuffer and therefore needed closing this slice —
// see mixer.hpp's MixChannel::vox_pitch). Exposed here as the word-level
// primitive for whichever slice wires spatialization.
[[nodiscard]] constexpr int vox_scale_volume( int vol, std::uint16_t word_volume ) noexcept
{
    if( word_volume == 100 )
        return vol;
    return static_cast<int>( static_cast<float>( vol ) * static_cast<float>( word_volume ) * 0.01f );
}

// ---------------------------------------------------------------------------
// VoxSentence — one channel's word list (channel_t::words, sized to exactly
// the resolved word count — std::vector::size() replaces legacy's null-sfx
// terminator entry; see vox.cpp for why that substitution is behaviourally
// identical).
// ---------------------------------------------------------------------------
struct VoxSentence
{
    std::vector<VoxWord> words;
};

// ---------------------------------------------------------------------------
// IVoxAudioResolver — the S_FindName (registration) + S_LoadSound (decode)
// seam VOX word-list construction needs, COLLAPSED into one call (documented
// simplification, not a silent behaviour change): legacy distinguishes
// "S_FindName found no sfx slot" (word->sfx == NULL, checked in VOX_LoadWord
// BEFORE decode, s_vox.c:150) from "S_LoadSound decode failed for an
// already-resolved sfx" (data == NULL, checked AFTER, s_vox.c:155) — both
// terminate the sentence IDENTICALLY (VOX_LoadWord's early return leaves
// FL_CHAN_SENTENCE_FINISHED set either way), so collapsing the two failure
// classes into one nullptr changes no observable mix-side behaviour.
//
// S9.6 LAZY-RESOLUTION FIX: an earlier slice (S9.4) called resolve() EAGERLY
// for every word at sentence-build time (VoxSystem::build_sentence), which
// decoded audio for words that might never be reached. This was reverted:
// build_sentence() now only PARSES the word list (storing each word's
// "<dir>/<word>" path on VoxWord::path, s_vox.c:500) and never calls
// resolve() at all; VoxSystem::load_word() (called from bind_channel() for
// word 0, and from next_word() for every later word — i.e. exactly at
// word-ADVANCE time, matching the real S_LoadSound call inside VOX_LoadWord,
// s_vox.c:153) is the ONLY call site left. A word whose `audio` is already
// non-null (constructed directly, bypassing build_sentence — several tests
// do this) is left alone: load_word() only resolves when `audio == nullptr`.
// This restores the two-phase shape S9.4's own doc comment called for
// ("mirroring VOX_LoadSound's eager per-word S_FindName loop" for
// STRUCTURE, "lazily at word-advance time like the real S_LoadSound call"
// for DECODE) with one simplification: legacy's S_FindName (name lookup,
// not decode) really IS eager in legacy (s_vox.c:507, inside the same
// per-word build loop that also collects volume/pitch/start/end/
// timecompress); this port defers BOTH the name lookup and the decode to
// load_word() time, since resolve() collapses them into one call (see
// above) and splitting IVoxAudioResolver into a separate find()/load() pair
// was judged not worth the added interface surface for this slice — see the
// S9.6 uncertainty list.
//
// @thread-safety: implementations must tolerate being called from whichever
// thread VoxSystem::build_sentence()/free_word() run on (see the file-header
// note — T_AudioDecoder, unasserted this slice).
// ---------------------------------------------------------------------------
class IVoxAudioResolver
{
public:
    IVoxAudioResolver() noexcept                                = default;
    virtual ~IVoxAudioResolver()                                = default;
    IVoxAudioResolver( const IVoxAudioResolver & )               = delete;
    IVoxAudioResolver &operator=( const IVoxAudioResolver & )    = delete;

    // Resolve `path` ("<dir>/<word>", no extension — legacy never appends
    // one either, s_vox.c:500) to decoded audio. Returns nullptr on failure
    // (VOX_LoadWord's early-return path — the sentence terminates at this
    // word, s_vox.c:150-156). `in_cache` mirrors S_FindName's out-param /
    // FL_VOXWORD_IN_CACHE (s_vox.c:506-509): true means the AudioData is a
    // shared, already-registered resource VoxSystem must NOT release; false
    // means VoxSystem now owns it and must release() it via free_word.
    [[nodiscard]] virtual const AudioData *resolve( std::string_view path, bool &in_cache ) noexcept = 0;

    // Release a non-cached AudioData previously returned by resolve()
    // (VOX_FreeWord's `FS_FreeSound(word->sfx->cache)`, s_vox.c:185).
    virtual void release( const AudioData *data ) noexcept = 0;
};

// ---------------------------------------------------------------------------
// ImmediateSentenceSlot — the s_sentenceImmediateName single-slot handoff
// (s_load.c:32-33,312-345). DOCUMENTED QUIRK, preserved not fixed: a second
// '!'-prefixed S_RegisterSound call before the first is consumed OVERWRITES
// the first — the legacy SENTENCE_INDEX(-99999) handle has no per-call
// identity at all (every '!'-sentence registration returns the SAME
// sentinel; S_GetSfxByHandle(SENTENCE_INDEX) always re-derives from
// WHATEVER this slot currently holds). Bounded to
// xash::limits::sound_vox_immediate_name_max (256, matching legacy's
// `string s_sentenceImmediateName` == MAX_STRING) via truncation, matching
// `Q_strncpy(dst, src, sizeof(dst))`'s always-truncate-never-overflow
// contract. R9.3 flags this as a live P-3 door concern (race/collision
// hazard), not just style — S9.6's real S_RegisterSound wiring should
// prefer an explicit return/parameter over this slot where the call site
// allows; this class exists so the quirk's overwrite semantics are testable
// in isolation until then.
// ---------------------------------------------------------------------------
class ImmediateSentenceSlot
{
public:
    // Store `name` — TRUNCATES to sound_vox_immediate_name_max-1 chars
    // (Q_strncpy always null-terminates) and unconditionally OVERWRITES
    // any prior, possibly-still-unconsumed value (the documented quirk).
    void set( std::string_view name ) noexcept;

    // The slot's current value (possibly stale/overwritten — by design).
    [[nodiscard]] const std::string &value() const noexcept { return name_; }

private:
    std::string name_;
};

// ---------------------------------------------------------------------------
// IVoxTimeLeftQuery — SND_GetChannelTimeLeft's sentence branch
// (s_main.c:291-320), S9.6. Kept separate from IVoxWordAdvance (rather than
// adding a new pure virtual there) so channel allocation (T_Main, S9.6) does
// not force every IVoxWordAdvance implementer/test-double (including
// test_sound_mixer.cpp's mock, S9.3) to grow a method it does not need.
// VoxSystem is the only production implementer (multiple inheritance below).
// ---------------------------------------------------------------------------
class IVoxTimeLeftQuery
{
public:
    IVoxTimeLeftQuery() noexcept                              = default;
    virtual ~IVoxTimeLeftQuery()                               = default;
    IVoxTimeLeftQuery( const IVoxTimeLeftQuery & )             = delete;
    IVoxTimeLeftQuery &operator=( const IVoxTimeLeftQuery & )  = delete;

    // Sum of the CURRENT word's remaining samples (forced_end - sample,
    // s_main.c:299) plus every not-yet-reached word's full/partial
    // (word.end percent) sample count. Like legacy, this FORCES a load of
    // every remaining word for the estimate (S_LoadSound, s_main.c:311-313
    // — a self-flagged legacy wart, "TODO: this function needs to be
    // removed after whole sound subsystem rewrite", s_main.c:281): the
    // S9.6 parity audit showed skipping lookahead resolution under-reports
    // a live sentence's time-left and flips SND_PickDynamicChannel's
    // eviction choice in saturated scenes. Resolution uses the channel's
    // bound resolver with load_word()'s exact bookkeeping (audio+in_cache
    // cached on the word, so the later real advance skips its own
    // resolve); the sum stops at the first word that FAILS to resolve
    // (s_main.c:309-313's breaks). Non-const for exactly that caching
    // side effect. Returns 0 if `chan` has no bound sentence or is
    // already SENTENCE_FINISHED.
    [[nodiscard]] virtual int time_left( const MixChannel &chan ) noexcept = 0;
};

// ---------------------------------------------------------------------------
// VoxSystem — the sentence table + word-list builder + the IVoxWordAdvance
// implementation driving a bound channel's word list via the S9.3 hook
// contract (Mixer::set_vox_advance), plus the IVoxTimeLeftQuery channel-
// allocation seam (S9.6).
// ---------------------------------------------------------------------------
class VoxSystem final : public IVoxWordAdvance, public IVoxTimeLeftQuery
{
public:
    VoxSystem() noexcept  = default;
    ~VoxSystem() override = default;

    VoxSystem( const VoxSystem & )            = delete;
    VoxSystem &operator=( const VoxSystem & ) = delete;

    // VOX_ReadSentenceFile (s_vox.c:570-583), minus the FS_LoadFile call:
    // the caller supplies the already-read bytes (mirrors how S9.2's
    // IAudioCodec::decode() takes a caller-owned span instead of doing its
    // own filesystem I/O — no direct fs dependency here either). Clears the
    // existing table first (VOX_Shutdown, called both explicitly by
    // VOX_ReadSentenceFile and via clear() below) then parses. Entries
    // beyond xash::limits::sound_vox_sentence_table_max are silently
    // dropped (the legacy cap, no error).
    void load_sentence_file( std::span<const std::byte> data );

    // VOX_Shutdown (s_vox.c:590-598). Legacy frees entries but does NOT null
    // the now-dangling `rgpszrawsentence[]` pointers (a documented
    // stale-pointer hazard "until next reload") — this class stores owned
    // std::strings in a std::vector, so clear() leaves no dangling state by
    // construction; the hazard class simply does not exist here (RAII, not
    // a reproduced quirk — there is nothing behaviourally equivalent to pin).
    void clear() noexcept { sentences_.clear(); }

    [[nodiscard]] std::size_t sentence_count() const noexcept { return sentences_.size(); }

    // VOX_LookupString (s_vox.c:257-298) over this instance's own table.
    [[nodiscard]] std::optional<std::string_view> lookup( std::string_view name ) const noexcept
    {
        return vox_lookup_string( sentences_, name );
    }

    // VOX_LoadSound (s_vox.c:444-521), MINUS the channel-binding tail
    // (`ch->words = ...; ch->word_index = 0; VOX_LoadWord(ch)` — see
    // bind_channel) AND minus any audio resolution (S9.6 lazy-resolution
    // fix — see IVoxAudioResolver's doc comment above): every parsed word
    // gets its "<dir>/<word>" path recorded on VoxWord::path but `audio`
    // stays nullptr until load_word() resolves it at word-advance time.
    // Returns nullopt on any of VOX_LoadSound's early-return error paths
    // (unknown sentence name / directory-prefix overflow / sentence text >=
    // sound_vox_sentence_text_max) — exactly like legacy, which never
    // signals failure to ITS caller either; it just leaves ch->words
    // untouched (== not a VOX channel).
    [[nodiscard]] std::optional<VoxSentence> build_sentence( std::string_view name ) const;

    // S9.6 wiring point: bind a freshly-built sentence's word list to a mix
    // channel — the VOX_LoadSound tail (`ch->words = ...; ch->word_index =
    // 0; VOX_LoadWord(ch)`, s_vox.c:515-520) plus its own "free any existing
    // words from a previous sentence on this channel" pre-step
    // (s_vox.c:457-462). `resolver` is retained (borrowed, @lifetime: caller
    // outlives every unbind_channel()/rebind) for later free_word() release()
    // calls. Sets chan.is_sentence = true and loads word 0.
    void bind_channel( MixChannel &chan, VoxSentence sentence, IVoxAudioResolver *resolver = nullptr );

    // Release a channel's sentence state (S_FreeChannel's VOX-side
    // counterpart — S9.6 calls this when a VOX channel is freed). No-op if
    // `chan` was never bound. Sets chan.is_sentence = false.
    void unbind_channel( MixChannel &chan ) noexcept;

    // IVoxWordAdvance (mixer.hpp) — the word-swap hook vox_mix_channel_to_
    // buffer calls whenever the current word finishes (VOX_FreeWord +
    // word_index++ + VOX_LoadWord, s_mix.c:294-302, MINUS the `chan->sfx =
    // ...` bookkeeping line, which has no MixChannel-side equivalent — see
    // vox.cpp). Fails safe (sets FL_CHAN_SENTENCE_FINISHED, returns false)
    // if `chan` has no bound sentence, which should not happen: the Mixer
    // only calls this for chan.is_sentence channels routed through
    // vox_mix_channel_to_buffer with a non-null vox_ hook.
    [[nodiscard]] bool next_word( MixChannel &chan ) noexcept override;

    // IVoxTimeLeftQuery (S9.6) — see the interface doc comment above.
    [[nodiscard]] int time_left( const MixChannel &chan ) noexcept override;

    // VOX_SetChanVol (s_vox.c:190-204), S9.6 wiring point: scales
    // chan.leftvol/rightvol by the CURRENT bound word's volume percent
    // (no-op at 100, the default, or if `chan` has no bound sentence / is
    // SENTENCE_FINISHED — matching `!ch->words || FL_CHAN_SENTENCE_FINISHED`
    // s_vox.c:194). Called from SND_Spatialize (channel_alloc.cpp) AFTER the
    // distance/pan computation, exactly like legacy's own call site
    // (s_main.c:574,607).
    void apply_word_volume( MixChannel &chan ) const noexcept;

private:
    // Per-channel driver state (channel_t::words + channel_t::word_index,
    // keyed by MixChannel identity — S9.6 owns the real channel_t<->
    // MixChannel bridge; this slice's Mixer calls next_word() with the SAME
    // shared IVoxWordAdvance instance for every sentence channel, so the
    // channel's ADDRESS is the only identity available to key on).
    struct ChannelState
    {
        VoxSentence        sentence;
        std::size_t        word_index = 0;
        IVoxAudioResolver  *resolver  = nullptr; // @lifetime: caller (outlives the binding)
    };

    // VOX_LoadWord (s_vox.c:141-168) over one bound channel's current word.
    // Sets FL_CHAN_SENTENCE_FINISHED first, unconditionally; clears it only
    // on full success (audio resolved). On success rebinds chan.source,
    // trims start/end into chan.sample/forced_end, and caches the word's
    // timecompress/pitch onto chan.timecompress/chan.vox_pitch (the same
    // "cache the per-word value on MixChannel" pattern S9.3 already
    // established for timecompress). Returns true iff a word was loaded.
    [[nodiscard]] bool load_word( MixChannel &chan, ChannelState &state ) noexcept;

    // VOX_FreeWord (s_vox.c:170-188) over one bound channel's current word:
    // vox_free_word_fields() first (unconditional), then the cache-ownership
    // release gated on `!in_cache` (matching `!FBitSet(word->flags,
    // FL_VOXWORD_IN_CACHE)`).
    void free_word( MixChannel &chan, ChannelState &state ) noexcept;

    std::vector<VoxSentenceEntry>              sentences_; // rgpszrawsentence[]/cszrawsentences
    std::unordered_map<const MixChannel *, ChannelState> bound_;
};

} // namespace xash::sound
