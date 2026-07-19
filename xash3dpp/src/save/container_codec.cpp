// xash3dpp — `.sav` container codec (Chunk 8, slice S8.5).
// See container_codec.hpp for the derived layout + sv_save.c cites.

#include <xash3dpp/private/save/container_codec.hpp>

#include <xash3dpp/private/save/descriptor_codec.hpp>
#include <xash3dpp/private/save/save_directory.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/limits.hpp>

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

[[nodiscard]] std::int32_t read_i32_le( std::span<const std::byte> b ) noexcept
{
    const auto u = static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[0] ) ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[1] ) ) << 8 ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[2] ) ) << 16 ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[3] ) ) << 24 );
    return static_cast<std::int32_t>( u );
}

// Zero-fill then copy the truncated string — deterministic canonicalization
// (save-boundary.md "Zero-filled FIELD_CHARACTER tails" deviation), matching
// level_state_writer.cpp's copy_fixed.
void copy_fixed( char *dst, std::size_t n, std::string_view src ) noexcept
{
    std::memset( dst, 0, n );
    const std::size_t count = ( src.size() < n ) ? src.size() : ( n - 1 );
    if ( count > 0 )
        std::memcpy( dst, src.data(), count );
}
} // namespace

// ---------------------------------------------------------------------------
// parse_five_int_preamble
// ---------------------------------------------------------------------------

Result<FiveIntPreamble>
parse_five_int_preamble( std::span<const std::byte> image, std::int32_t expected_magic,
                         std::int32_t expected_version ) noexcept
{
    if ( image.size() < k_five_int_preamble_bytes )
        return std::unexpected( SaveError::TruncatedBlock );

    const std::int32_t id      = read_i32_le( image.subspan( 0, 4 ) );
    const std::int32_t version = read_i32_le( image.subspan( 4, 4 ) );
    if ( id != expected_magic )
        return std::unexpected( SaveError::BadMagic );
    if ( version != expected_version )
        return std::unexpected( SaveError::VersionMismatch );

    FiveIntPreamble p;
    p.size        = read_i32_le( image.subspan( 8, 4 ) );
    p.token_count = read_i32_le( image.subspan( 12, 4 ) );
    p.token_size  = read_i32_le( image.subspan( 16, 4 ) );

    if ( p.size < 0 || p.token_count < 0 || p.token_size < 0 )
        return std::unexpected( SaveError::CorruptHeader );
    if ( static_cast<std::size_t>( p.token_count ) > ::xash::limits::save_hash_strings ||
         static_cast<std::size_t>( p.token_size ) > ::xash::limits::save_heap_size ||
         static_cast<std::size_t>( p.size ) > ::xash::limits::save_heap_size )
        return std::unexpected( SaveError::CorruptHeader );

    const std::size_t need = k_five_int_preamble_bytes +
                             static_cast<std::size_t>( p.token_size ) +
                             static_cast<std::size_t>( p.size );
    if ( need > image.size() )
        return std::unexpected( SaveError::TruncatedBlock );

    return p;
}

// ---------------------------------------------------------------------------
// write_sav_container
// ---------------------------------------------------------------------------

Result<void>
write_sav_container( const SavContainerParams &params, ISaveGlobalState *global_state,
                     std::span<const EmbeddedFile> embedded_files, SaveBuffer &buf,
                     std::vector<std::byte> &out ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    buf.reset(); // SaveClear (sv_save.c:742-754) — reusable working buffer

    SaveBufferSink sink( buf );
    TokenTable    &tokens = buf.tokens();

    GameHeader header{};
    copy_fixed( header.map_name, sizeof( header.map_name ), params.map_name );
    copy_fixed( header.comment, sizeof( header.comment ), params.comment );
    header.map_count = static_cast<std::int32_t>( embedded_files.size() );

    if ( auto r = write_descriptor_block( sink, tokens, "GameHeader", &header,
                                          k_game_header_desc, 0.0f );
         !r )
        return r;

    if ( global_state != nullptr )
    {
        if ( auto r = global_state->save_global_state( sink, tokens ); !r )
            return r;
    }

    const std::size_t data_size = buf.size(); // sv_save.c: pSaveData->size

    const std::size_t token_size = tokens.flattened_size();
    std::vector<std::byte> token_blob( token_size );
    if ( auto r = tokens.flatten( token_blob ); !r )
        return std::unexpected( r.error() );

    out.clear();
    std::size_t embedded_total = 0;
    for ( const auto &f : embedded_files )
        embedded_total += ::xash::limits::save_container_name_field + sizeof( std::int32_t ) + f.data.size();
    out.reserve( k_five_int_preamble_bytes + token_size + data_size + embedded_total );

    append_i32_le( out, k_savegame_magic );
    append_i32_le( out, k_savegame_version );
    append_i32_le( out, static_cast<std::int32_t>( data_size ) );
    append_i32_le( out, static_cast<std::int32_t>( tokens.token_count() ) );
    append_i32_le( out, static_cast<std::int32_t>( token_size ) );
    out.insert( out.end(), token_blob.begin(), token_blob.end() );
    const std::span<const std::byte> data_reg = buf.data();
    out.insert( out.end(), data_reg.begin(), data_reg.end() );

    // DirectoryCopy-shaped embedded records (sv_save.c:647-670).
    for ( const auto &f : embedded_files )
    {
        std::vector<char> name( ::xash::limits::save_container_name_field );
        copy_fixed( name.data(), name.size(), f.name );
        for ( char c : name )
            out.push_back( static_cast<std::byte>( c ) );
        append_i32_le( out, static_cast<std::int32_t>( f.data.size() ) );
        out.insert( out.end(), f.data.begin(), f.data.end() );
    }

    return {};
}

// ---------------------------------------------------------------------------
// read_sav_container
// ---------------------------------------------------------------------------

Result<void>
read_sav_container( std::span<const std::byte> image, IRestoreGlobalState *global_state,
                    SaveBuffer &buf, SavContainerResult &out ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    auto pre = parse_five_int_preamble( image, k_savegame_magic, k_savegame_version );
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

    out.header = GameHeader{};
    if ( auto r = read_descriptor_block( data, offset, tokens, "GameHeader", &out.header,
                                         k_game_header_desc, 0.0f );
         !r )
        return std::unexpected( r.error() );

    if ( global_state != nullptr )
    {
        if ( auto r = global_state->restore_global_state( data, offset, tokens ); !r )
            return std::unexpected( r.error() );
    }

    // DirectoryExtract (sv_save.c:679-706): extension-blind extraction of
    // header.map_count records from the REMAINDER of the on-disk image
    // (past the preamble+tokens+data region parsed above) — NOT from `data`
    // (the SaveBuffer working copy), since the embedded records are never
    // loaded into the SAVERESTOREDATA buffer in legacy either.
    out.records.clear();
    if ( out.header.map_count > 0 )
        out.records.reserve( static_cast<std::size_t>( out.header.map_count ) );

    std::size_t cursor = pre->data_offset() + static_cast<std::size_t>( pre->size );
    for ( std::int32_t i = 0; i < out.header.map_count; ++i )
    {
        const std::size_t k_name_field = ::xash::limits::save_container_name_field;
        if ( cursor + k_name_field + sizeof( std::int32_t ) > image.size() )
            return std::unexpected( SaveError::TruncatedBlock );

        const std::span<const std::byte> name_bytes = image.subspan( cursor, k_name_field );
        cursor += k_name_field;
        const std::int32_t file_size = read_i32_le( image.subspan( cursor, 4 ) );
        cursor += 4;
        if ( file_size < 0 || cursor + static_cast<std::size_t>( file_size ) > image.size() )
            return std::unexpected( SaveError::TruncatedBlock );

        ExtractedRecord rec;
        // The name field is zero-padded; the C-string ends at the first NUL
        // (COM_FileWithoutPath produced it, never containing embedded NULs).
        std::size_t len = 0;
        while ( len < name_bytes.size() && name_bytes[len] != std::byte{ 0 } )
            ++len;
        rec.name.assign( reinterpret_cast<const char *>( name_bytes.data() ), len );
        // Own a copy (see ExtractedRecord doc): the caller-visible record must
        // survive the transient on-disk image the load_sav_file wrapper frees.
        const std::span<const std::byte> rec_bytes =
            image.subspan( cursor, static_cast<std::size_t>( file_size ) );
        rec.data.assign( rec_bytes.begin(), rec_bytes.end() );
        cursor += static_cast<std::size_t>( file_size );

        out.records.push_back( std::move( rec ) );
    }

    return {};
}

// ---------------------------------------------------------------------------
// SAV-OQ-1 door helper
// ---------------------------------------------------------------------------

Result<HlxSideBlockHeader> parse_hlx_header( std::span<const std::byte> data ) noexcept
{
    if ( data.size() < sizeof( HlxSideBlockHeader ) )
        return std::unexpected( SaveError::TruncatedBlock );

    HlxSideBlockHeader hdr;
    hdr.magic   = read_i32_le( data.subspan( 0, 4 ) );
    hdr.version = read_i32_le( data.subspan( 4, 4 ) );
    hdr.size    = read_i32_le( data.subspan( 8, 4 ) );

    if ( hdr.magic != k_hlx_side_block_magic )
        return std::unexpected( SaveError::BadMagic );
    if ( hdr.size < 0 ||
         sizeof( HlxSideBlockHeader ) + static_cast<std::size_t>( hdr.size ) > data.size() )
        return std::unexpected( SaveError::TruncatedBlock );

    return hdr;
}

// ---------------------------------------------------------------------------
// File-backed wrappers
// ---------------------------------------------------------------------------

Result<void>
write_sav_file( ::xash::filesystem::Filesystem &fs, std::string_view save_name,
                const SavContainerParams &params, ISaveGlobalState *global_state,
                std::span<const EmbeddedFile> embedded_files, SaveBuffer &buf ) noexcept
{
    std::vector<std::byte> image;
    if ( auto r = write_sav_container( params, global_state, embedded_files, buf, image ); !r )
        return r;

    const std::string path = std::string( k_default_save_directory ) + std::string( save_name ) + ".sav";
    if ( !fs.write_file( path, image ) )
        return std::unexpected( SaveError::IoError );
    return {};
}

Result<std::optional<SavContainerResult>>
load_sav_file( ::xash::filesystem::Filesystem &fs, std::string_view save_name,
              IRestoreGlobalState *global_state, SaveBuffer &buf ) noexcept
{
    const std::string path = std::string( k_default_save_directory ) + std::string( save_name ) + ".sav";
    if ( !fs.file_exists( path ) )
        return std::optional<SavContainerResult>{}; // mirrors SV_LoadGame's silent FS_FileExists guard

    const std::vector<std::byte> image = fs.load_file( path );

    SavContainerResult result;
    if ( auto r = read_sav_container( image, global_state, buf, result ); !r )
        return std::unexpected( r.error() );

    return std::optional<SavContainerResult>{ std::move( result ) };
}

} // namespace xash::save
