// xash3dpp — `.HL2` client-state block (Chunk 8, slice S8.5).
// See client_state.hpp for the derived layout + sv_save.c cites.

#include <xash3dpp/private/save/client_state.hpp>

#include <xash3dpp/private/save/container_codec.hpp> // FiveIntPreamble / parse_five_int_preamble (shared shape)
#include <xash3dpp/private/save/descriptor_codec.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>

#include <array>
#include <cstring>

namespace xash::save {

namespace
{
void append_i32_le( std::vector<std::byte> &out, std::int32_t v ) noexcept
{
    const auto u = static_cast<std::uint32_t>( v );
    out.push_back( static_cast<std::byte>( u & 0xFFu ) );
    out.push_back( static_cast<std::byte>( ( u >> 8 ) & 0xFFu ) );
    out.push_back( static_cast<std::byte>( ( u >> 16 ) & 0xFFu ) );
    out.push_back( static_cast<std::byte>( ( u >> 24 ) & 0xFFu ) );
}

void copy_fixed( char *dst, std::size_t n, std::string_view src ) noexcept
{
    std::memset( dst, 0, n );
    const std::size_t count = ( src.size() < n ) ? src.size() : ( n - 1 );
    if ( count > 0 )
        std::memcpy( dst, src.data(), count );
}
} // namespace

// ---------------------------------------------------------------------------
// write_client_state
// ---------------------------------------------------------------------------

Result<void>
write_client_state( const ClientStateParams &params, SaveBuffer &buf,
                    std::vector<std::byte> &out ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    buf.reset(); // SaveClear (sv_save.c:742-754)

    SaveBufferSink sink( buf );
    TokenTable    &tokens = buf.tokens();

    // --- Header scalars (sv_save.c:1194-1230) ---
    SaveClient header{};
    header.entity_count = static_cast<std::int32_t>( params.static_entities.size() );

    std::span<const SaveDecalEntry> decals{};
    if ( params.decal_provider != nullptr )
    {
        decals             = params.decal_provider->decals(); // always captured, even on a changelevel
        header.decal_count = static_cast<std::int32_t>( decals.size() );
    }

    std::span<const SaveSoundEntry> sounds{};
    if ( params.include_transient_audio ) // "sounds won't going across transition" (sv_save.c:1212)
    {
        if ( params.sound_provider != nullptr )
        {
            sounds              = params.sound_provider->dynamic_sounds();
            header.sound_count  = static_cast<std::int32_t>( sounds.size() );
        }
        if ( params.music_provider != nullptr )
        {
            const MusicState music = params.music_provider->music_state();
            copy_fixed( header.intro_track, sizeof( header.intro_track ), music.intro_track );
            copy_fixed( header.main_track, sizeof( header.main_track ), music.main_track );
            header.track_position = music.track_position;
        }
    }

    header.viewentity = params.view_entity;
    header.wateralpha = params.wateralpha;
    header.wateramp   = params.wateramp;

    if ( auto r = write_descriptor_block( sink, tokens, "ClientHeader", &header,
                                          k_save_client_desc, 0.0f );
         !r )
        return r;

    // --- Decals (sv_save.c:1233-1244).  Landmark offset rebasing
    //     (FDECAL_USE_LANDMARK) is deferred to a future adjacent-transfer
    //     slice (S8.6), matching level_state_loader.hpp's precedent for
    //     CreateEntityTransitionList — decals round-trip as-is here. ---
    for ( const auto &d : decals )
    {
        if ( auto r = write_descriptor_block( sink, tokens, "DECALLIST", &d,
                                              k_decal_entry_desc, 0.0f );
             !r )
            return r;
    }

    // --- Static entities (sv_save.c:1250-1252; NOT capability-gated).
    //     `messagenum` (FIELD_MODELNAME) is a companion-TEXT field — the
    //     wire value is `entry.model_name`, not the in-struct handle (D2,
    //     descriptor_codec.hpp FieldTextBinding).  A local mutable copy is
    //     bound because FieldTextBinding always carries a non-const
    //     `std::string*` (shared shape with the read side, which needs to
    //     write through it); write only ever reads through the pointer. ---
    for ( const auto &e : params.static_entities )
    {
        std::string model_name_copy = e.model_name;
        const std::array<FieldTextBinding, 1> bindings = {
            FieldTextBinding{ "messagenum", &model_name_copy },
        };
        if ( auto r = write_descriptor_block( sink, tokens, "STATICENTITY", &e.state,
                                              k_static_entry_desc, 0.0f, nullptr, nullptr,
                                              bindings );
             !r )
            return r;
    }

    // --- Sounds (sv_save.c:1255-1256). ---
    for ( const auto &s : sounds )
    {
        if ( auto r = write_descriptor_block( sink, tokens, "SOUNDLIST", &s,
                                              k_sound_entry_desc, 0.0f );
             !r )
            return r;
    }

    const std::size_t data_size = buf.size();

    const std::size_t token_size = tokens.flattened_size();
    std::vector<std::byte> token_blob( token_size );
    if ( auto r = tokens.flatten( token_blob ); !r )
        return std::unexpected( r.error() );

    out.clear();
    out.reserve( k_five_int_preamble_bytes + token_size + data_size );
    append_i32_le( out, k_savegame_magic );          // sv_save.c:1266
    append_i32_le( out, k_client_savegame_version );  // sv_save.c:1265
    append_i32_le( out, static_cast<std::int32_t>( data_size ) );
    append_i32_le( out, static_cast<std::int32_t>( tokens.token_count() ) );
    append_i32_le( out, static_cast<std::int32_t>( token_size ) );
    out.insert( out.end(), token_blob.begin(), token_blob.end() );
    const std::span<const std::byte> data_reg = buf.data();
    out.insert( out.end(), data_reg.begin(), data_reg.end() );

    return {};
}

// ---------------------------------------------------------------------------
// read_client_state
// ---------------------------------------------------------------------------

Result<void>
read_client_state( std::span<const std::byte> image, SaveBuffer &buf, LoadedClientState &out ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    auto pre = parse_five_int_preamble( image, k_savegame_magic, k_client_savegame_version );
    if ( !pre )
        return std::unexpected( pre.error() );

    TokenTable tokens( static_cast<std::size_t>( pre->token_count ) );
    const std::span<const std::byte> token_blob =
        image.subspan( pre->token_offset(), static_cast<std::size_t>( pre->token_size ) );
    if ( auto r = tokens.rebuild( token_blob ); !r )
        return std::unexpected( r.error() );

    const std::span<const std::byte> payload =
        image.subspan( pre->data_offset(), static_cast<std::size_t>( pre->size ) );
    if ( auto r = buf.load_from( payload ); !r )
        return std::unexpected( r.error() );

    const std::span<const std::byte> data = buf.data();
    std::size_t                      offset = 0;

    out.header = SaveClient{};
    if ( auto r = read_descriptor_block( data, offset, tokens, "ClientHeader", &out.header,
                                         k_save_client_desc, 0.0f );
         !r )
        return std::unexpected( r.error() );

    // Grow-as-you-go (no upfront reserve on an attacker-controlled count) —
    // a corrupt/huge count fails naturally via TruncatedBlock once `data` is
    // exhausted, matching the "reject-gracefully, don't over-allocate" stance.
    out.decals.clear();
    for ( std::int32_t i = 0; i < out.header.decal_count; ++i )
    {
        SaveDecalEntry d{};
        if ( auto r = read_descriptor_block( data, offset, tokens, "DECALLIST", &d,
                                             k_decal_entry_desc, 0.0f );
             !r )
            return std::unexpected( r.error() );
        out.decals.push_back( d );
    }

    out.static_entities.clear();
    for ( std::int32_t i = 0; i < out.header.entity_count; ++i )
    {
        StaticEntityEntry entry{};
        const std::array<FieldTextBinding, 1> bindings = {
            FieldTextBinding{ "messagenum", &entry.model_name },
        };
        if ( auto r = read_descriptor_block( data, offset, tokens, "STATICENTITY", &entry.state,
                                             k_static_entry_desc, 0.0f, bindings );
             !r )
            return std::unexpected( r.error() );
        out.static_entities.push_back( std::move( entry ) );
    }

    out.sounds.clear();
    for ( std::int32_t i = 0; i < out.header.sound_count; ++i )
    {
        SaveSoundEntry s{};
        if ( auto r = read_descriptor_block( data, offset, tokens, "SOUNDLIST", &s,
                                             k_sound_entry_desc, 0.0f );
             !r )
            return std::unexpected( r.error() );
        out.sounds.push_back( s );
    }

    return {};
}

// ---------------------------------------------------------------------------
// File-backed wrappers
// ---------------------------------------------------------------------------

Result<void>
write_hl2_file( ::xash::filesystem::Filesystem &fs, std::string_view level,
                const ClientStateParams &params, SaveBuffer &buf ) noexcept
{
    std::vector<std::byte> image;
    if ( auto r = write_client_state( params, buf, image ); !r )
        return r;

    const std::string path = std::string( k_default_save_directory ) + std::string( level ) + ".HL2";
    if ( !fs.write_file( path, image ) )
        return std::unexpected( SaveError::IoError );
    return {};
}

Result<std::optional<LoadedClientState>>
load_hl2_file( ::xash::filesystem::Filesystem &fs, std::string_view level, SaveBuffer &buf ) noexcept
{
    const std::string path = std::string( k_default_save_directory ) + std::string( level ) + ".HL2";
    if ( !fs.file_exists( path ) )
        return std::optional<LoadedClientState>{};

    const std::vector<std::byte> image = fs.load_file( path );

    LoadedClientState result;
    if ( auto r = read_client_state( image, buf, result ); !r )
        return std::unexpected( r.error() );

    return std::optional<LoadedClientState>{ std::move( result ) };
}

} // namespace xash::save
