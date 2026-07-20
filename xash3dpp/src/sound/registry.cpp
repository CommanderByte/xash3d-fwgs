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

const AudioData *SfxRegistry::load_sfx( SfxHandle handle ) noexcept
{
    if( handle == k_sentence_handle || handle < 0 || static_cast<std::size_t>( handle ) >= slots_.size() )
        return nullptr;

    SfxSlot &slot = slots_[static_cast<std::size_t>( handle )];
    if( slot.cache.has_value() )
        return &*slot.cache; // "see if still in memory" (s_load.c:110-111)

    std::optional<AudioData> decoded = loader_ != nullptr ? loader_->load( slot.name ) : std::nullopt;
    slot.cache = decoded.has_value() ? std::move( *decoded ) : make_default_sound(); // S_CreateDefaultSound fallback (s_load.c:129)
    return &*slot.cache;
}

const AudioData *SfxRegistry::resolve( std::string_view path, bool &in_cache ) noexcept
{
    // S_FindName (eager NAME lookup — s_vox.c:507).
    const SfxHandle handle = find_name( path );
    if( handle == k_invalid_sound_handle )
    {
        in_cache = false;
        return nullptr; // !word->sfx (s_vox.c:150-151)
    }

    // in_cache mirrors FL_VOXWORD_IN_CACHE: was this sfx ALREADY decoded
    // before this call (shared with some other channel/registration)?
    in_cache = slots_[static_cast<std::size_t>( handle )].cache.has_value();

    return load_sfx( handle ); // S_LoadSound (lazy decode, s_vox.c:153)
}

void SfxRegistry::release( const AudioData *data ) noexcept
{
    // FS_FreeSound(word->sfx->cache); word->sfx->cache = NULL (s_vox.c:185-186).
    for( SfxSlot &slot : slots_ )
    {
        if( slot.cache.has_value() && &*slot.cache == data )
        {
            slot.cache.reset();
            return;
        }
    }
}

} // namespace xash::sound
