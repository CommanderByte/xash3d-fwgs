// xash3dpp — `.HL3` entity patch (Chunk 8, slice S8.5).
// See entity_patch.hpp for the derived layout + sv_save.c cites.

#include <xash3dpp/private/save/entity_patch.hpp>

#include <xash3dpp/private/save/format.hpp> // k_default_save_directory

#include <xash3dpp/abi/eiface.hpp> // k_fenttable_removed
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>

#include <string>

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
} // namespace

std::vector<std::byte> write_entity_patch( const EntityTable &table ) noexcept
{
    std::vector<std::byte> out;

    std::int32_t removed = 0;
    for ( std::size_t i = 0; i < table.count(); ++i )
        if ( ( static_cast<unsigned>( table.row( i ).flags ) & ::xash::abi::k_fenttable_removed ) != 0 )
            ++removed;

    out.reserve( sizeof( std::int32_t ) * ( static_cast<std::size_t>( removed ) + 1 ) );
    append_i32_le( out, removed );
    for ( std::size_t i = 0; i < table.count(); ++i )
        if ( ( static_cast<unsigned>( table.row( i ).flags ) & ::xash::abi::k_fenttable_removed ) != 0 )
            append_i32_le( out, static_cast<std::int32_t>( i ) );

    return out;
}

Result<std::vector<std::int32_t>> read_entity_patch( std::span<const std::byte> image ) noexcept
{
    if ( image.size() < sizeof( std::int32_t ) )
        return std::unexpected( SaveError::TruncatedBlock );

    const std::int32_t count = read_i32_le( image.subspan( 0, 4 ) );
    if ( count < 0 )
        return std::unexpected( SaveError::CorruptHeader );

    const std::size_t need = sizeof( std::int32_t ) * ( static_cast<std::size_t>( count ) + 1 );
    if ( need > image.size() )
        return std::unexpected( SaveError::TruncatedBlock );

    std::vector<std::int32_t> indices;
    indices.reserve( static_cast<std::size_t>( count ) );
    for ( std::int32_t i = 0; i < count; ++i )
        indices.push_back( read_i32_le( image.subspan( 4 + static_cast<std::size_t>( i ) * 4, 4 ) ) );

    return indices;
}

Result<void> apply_entity_patch( EntityTable &table, std::span<const std::int32_t> indices ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    for ( const std::int32_t idx : indices )
    {
        if ( idx < 0 || static_cast<std::size_t>( idx ) >= table.count() )
            return std::unexpected( SaveError::CorruptHeader );
        // Plain assignment (NOT SetBits) — the exact legacy quirk
        // (sv_save.c:1069), clobbering any other flag bits already set.
        table.row( static_cast<std::size_t>( idx ) ).flags =
            static_cast<int>( ::xash::abi::k_fenttable_removed );
    }
    return {};
}

Result<void>
write_hl3_file( ::xash::filesystem::Filesystem &fs, std::string_view level,
                const EntityTable &table ) noexcept
{
    const std::vector<std::byte> image = write_entity_patch( table );
    const std::string path = std::string( k_default_save_directory ) + std::string( level ) + ".HL3";
    if ( !fs.write_file( path, image ) )
        return std::unexpected( SaveError::IoError );
    return {};
}

Result<std::vector<std::int32_t>>
load_hl3_file( ::xash::filesystem::Filesystem &fs, std::string_view level ) noexcept
{
    const std::string path = std::string( k_default_save_directory ) + std::string( level ) + ".HL3";
    if ( !fs.file_exists( path ) )
        return std::vector<std::int32_t>{}; // EntityPatchRead's silent no-op (sv_save.c:1062-1063)

    const std::vector<std::byte> image = fs.load_file( path );
    return read_entity_patch( image );
}

} // namespace xash::save
