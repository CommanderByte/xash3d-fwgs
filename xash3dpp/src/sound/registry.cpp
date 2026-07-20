// xash3dpp — sfx registry implementation (Chunk 9, slice S9.6).
// Legacy reference: engine/client/sound/s_load.c (see registry.hpp for the
// per-function line map). Legacy C engine is REFERENCE-ONLY.

#include <xash3dpp/private/sound/registry.hpp>

#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/private/sound/codec.hpp>
#include <xash3dpp/utilities/path.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <utility>

namespace xash::sound {

// ---------------------------------------------------------------------------
// make_default_sound — S_CreateDefaultSound (s_load.c:80-96).
// ---------------------------------------------------------------------------
AudioData make_default_sound() noexcept
{
    AudioData data;
    data.rate     = ::xash::limits::sound_dma_speed; // SOUND_DMA_SPEED
    data.width    = 2;
    data.channels = 1;
    data.samples  = ::xash::limits::sound_dma_speed;
    data.type     = AudioFormatType::Pcm;
    data.flags    = AudioFlags::None;
    data.buffer.assign( static_cast<std::size_t>( data.samples ) * data.width * data.channels, std::byte{ 0 } );
    return data;
}

// ---------------------------------------------------------------------------
// FilesystemAudioLoader — a SIMPLIFIED single-path resolution (see
// registry.hpp file-header deviations vs. soundlib's fuller FS_LoadSound).
// ---------------------------------------------------------------------------
std::optional<AudioData> FilesystemAudioLoader::load( std::string_view name ) noexcept
{
    if( fs_ == nullptr || name.empty() )
        return std::nullopt;

    // Extract + lowercase the extension (codec.hpp's `handles(ext)`/
    // `find_audio_codec` contract expects lowercase, no leading dot).
    std::string_view ext_view = ::xash::utilities::file_extension( name ); // ".ext" or "" (COM_FileExtension)
    if( !ext_view.empty() && ext_view.front() == '.' )
        ext_view.remove_prefix( 1 );
    std::string ext = ::xash::utilities::to_lower( ext_view );

    std::string path( name );
    const IAudioCodec *codec = ext.empty() ? nullptr : find_audio_codec( ext );

    if( codec == nullptr )
    {
        // No recognised extension: try appending ".wav" (this slice's only
        // linked codec) — mirrors the OBSERVABLE outcome of legacy's
        // load_game[] table probe for the common no-extension precache-name
        // case, without soundlib's full two-path/DEFAULT_SOUNDPATH search.
        path = std::string( name ) + ".wav";
        ext  = "wav";
        codec = find_audio_codec( ext );
    }

    if( codec == nullptr )
        return std::nullopt;

    const std::vector<std::byte> bytes = fs_->load_file( path );
    if( bytes.empty() )
        return std::nullopt;

    Result<AudioData> decoded = codec->decode( path, std::span<const std::byte>( bytes.data(), bytes.size() ) );
    if( !decoded.has_value() )
        return std::nullopt;

    return std::move( *decoded );
}

// ---------------------------------------------------------------------------
// SfxRegistry
// ---------------------------------------------------------------------------

SfxHandle SfxRegistry::register_sound( std::string_view name ) noexcept
{
    if( name.empty() )
        return k_invalid_sound_handle; // COM_StringEmptyOrNULL guard (s_load.c:316)

    if( test_sound_char( name, '!' ) )
    {
        // "this is a sentence" — store the FULL name (with '!') into the
        // single immediate slot and return the sentinel handle. No table
        // touch, no decode (s_load.c:319-323).
        immediate_.set( name );
        return k_sentence_handle;
    }

    // "some stupid mappers used leading '/' or '\' in path" (s_load.c:326-327)
    // — strip up to two leading slashes.
    std::string_view n = name;
    if( !n.empty() && ( n.front() == '/' || n.front() == '\\' ) )
        n.remove_prefix( 1 );
    if( !n.empty() && ( n.front() == '/' || n.front() == '\\' ) )
        n.remove_prefix( 1 );

    // register_sound() NEVER decodes here — see registry.hpp's file-header
    // deviation note (legacy's `if( !s_registering ) S_LoadSound( sfx );`
    // is not reproduced; this slice is always lazy).
    return find_name( n );
}

SfxHandle SfxRegistry::find_name( std::string_view name ) noexcept
{
    if( name.empty() )
        return k_invalid_sound_handle;
    if( name.size() >= ::xash::abi::k_max_qpath ) // sfx->name[64] (s_load.c:158)
        return k_invalid_sound_handle;

    // COM_FixSlashes — normalise '\\' to '/' (s_load.c:162).
    std::string fixed( name );
    for( char &c : fixed )
        if( c == '\\' )
            c = '/';

    if( auto it = by_name_.find( fixed ); it != by_name_.end() )
        return it->second; // "see if already loaded" (s_load.c:166-179); servercount bump not modelled (see file header)

    if( slots_.size() >= ::xash::limits::sound_max_sfx )
        return k_invalid_sound_handle; // MAX_SFX exhausted (s_load.c:187-188)

    const SfxHandle handle = static_cast<SfxHandle>( slots_.size() );
    slots_.push_back( SfxSlot{ fixed, std::nullopt } );
    by_name_.emplace( std::move( fixed ), handle );
    return handle;
}

const SfxSlot *SfxRegistry::get( SfxHandle handle ) noexcept
{
    SfxHandle real = handle;
    if( handle == k_sentence_handle )
        real = find_name( immediate_.value() ); // re-derive via the immediate slot (s_load.c:344-345)

    if( real < 0 || static_cast<std::size_t>( real ) >= slots_.size() )
        return nullptr;
    return &slots_[static_cast<std::size_t>( real )];
}

// The ALL-IN-ONE form (cached -> load -> install), for callers that hold no
// external lock. A caller holding one must use cached()/install() directly so
// the decode runs unlocked — see registry.hpp's CONC-9 protocol note.
const AudioData *SfxRegistry::load_sfx( SfxHandle handle ) noexcept
{
    if( const AudioData *hit = cached( handle ); hit != nullptr )
        return hit; // "see if still in memory" (s_load.c:110-111)

    if( handle == k_sentence_handle || handle < 0 || static_cast<std::size_t>( handle ) >= slots_.size() )
        return nullptr;

    std::optional<AudioData> decoded =
        loader_ != nullptr ? loader_->load( slots_[static_cast<std::size_t>( handle )].name ) : std::nullopt;
    return install( handle, std::move( decoded ) ); // S_CreateDefaultSound fallback on nullopt (s_load.c:129)
}

const AudioData *SfxRegistry::cached( SfxHandle handle ) const noexcept
{
    if( handle == k_sentence_handle || handle < 0 || static_cast<std::size_t>( handle ) >= slots_.size() )
        return nullptr;

    const SfxSlot &slot = slots_[static_cast<std::size_t>( handle )];
    return slot.cache.has_value() ? &*slot.cache : nullptr; // "see if still in memory" (s_load.c:110-111)
}

const AudioData *SfxRegistry::install( SfxHandle handle, std::optional<AudioData> decoded ) noexcept
{
    if( handle == k_sentence_handle || handle < 0 || static_cast<std::size_t>( handle ) >= slots_.size() )
        return nullptr;

    SfxSlot &slot = slots_[static_cast<std::size_t>( handle )];

    // DOUBLE-CHECKED install (CONC-9): another thread may have decoded and
    // installed the same slot while this caller was decoding with the mutex
    // released.  The loser simply drops its own decode on the floor and uses
    // the winner's pointer — cheap, and it is what keeps SfxSlot::cache's
    // address-stability invariant intact (an installed entry is never
    // replaced, so no address ever changes).
    if( slot.cache.has_value() )
        return &*slot.cache;

    slot.cache = decoded.has_value() ? std::move( *decoded ) : make_default_sound(); // S_CreateDefaultSound (s_load.c:129)
    return &*slot.cache;
}

SfxRegistry::ResolveLookup SfxRegistry::lookup( std::string_view path ) noexcept
{
    ResolveLookup out;

    // S_FindName (eager NAME lookup — s_vox.c:507).
    out.handle = find_name( path );
    if( out.handle == k_invalid_sound_handle )
        return out; // !word->sfx (s_vox.c:150-151)

    // in_cache mirrors FL_VOXWORD_IN_CACHE: was this sfx ALREADY decoded
    // before this call (shared with some other channel/registration)?
    const SfxSlot &slot = slots_[static_cast<std::size_t>( out.handle )];
    out.in_cache        = slot.cache.has_value();
    out.cached          = out.in_cache ? &*slot.cache : nullptr;
    out.loader          = loader_;
    out.name            = slot.name;
    return out;
}

const AudioData *SfxRegistry::resolve( std::string_view path, bool &in_cache ) noexcept
{
    const ResolveLookup lu = lookup( path );
    in_cache               = lu.in_cache;
    if( lu.handle == k_invalid_sound_handle )
        return nullptr; // !word->sfx (s_vox.c:150-151)

    return load_sfx( lu.handle ); // S_LoadSound (lazy decode, s_vox.c:153)
}

void SfxRegistry::release( const AudioData * ) noexcept
{
    // DELIBERATE NO-OP — a DOCUMENTED DEVIATION from legacy, not an omission.
    //
    // Legacy: VOX_FreeWord does `FS_FreeSound( word->sfx->cache );
    //         word->sfx->cache = NULL;` (s_vox.c:185-186) to bound memory.
    // Rewrite: decoded audio is RETAINED for this registry's lifetime.
    //
    // WHY (gate findings F-1/F-2 parity, CONC-1 concurrency — all confirmed):
    // an AudioData address handed out by load_sfx()/resolve()/install() is
    // borrowed by MixChannel::source, by every bound VOX word, AND by
    // AudioCommand::source pointers still in flight on the MPSC.  Word
    // retirement runs on T_AudioDecoder, so destroying the cache entry here
    // would free audio that another LIVE channel — or a queued command T_Main
    // has already submitted — still points at: a cross-thread use-after-free.
    // Retaining makes the address-stability invariant documented on
    // SfxSlot::cache unconditional, which is what the borrow design needs.
    //
    // COST: retention is bounded by MAX_SFX (limits::sound_max_sfx) distinct
    // sounds — exactly the capacity this registry is already sized and
    // pre-reserved for.  Buying the memory back would cost either refcounting
    // every borrow (channels, words, in-flight commands) or a per-block
    // re-resolve through registry_mutex_ on the decoder's hot paint path;
    // neither is worth it at this bound.
    //
    // The SEAM stays: VoxSystem::free_word() still calls release() for a
    // !FL_VOXWORD_IN_CACHE word, so the ownership statement at the call site
    // remains truthful and a future refcounted policy has a place to land.
    // vox_free_word_fields()' unconditional channel-field zeroing (the pinned
    // s_vox.c:171-173 quirk) is untouched by this and MUST stay as pinned.
}

} // namespace xash::sound
