// xash3dpp — SV_GetSaveComment port + SaveBuildComment port (Chunk 8, slice
// S8.5).  See save_comment.hpp for the derived layout + sv_save.c cites.

#include <xash3dpp/private/save/save_comment.hpp>

#include <xash3dpp/private/save/field_sink.hpp>
#include <xash3dpp/private/save/token_table.hpp>

#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <chrono>
#include <cmath>

namespace xash::save {

namespace
{
// The `.sav` root preamble this parser hand-decodes (sv_save.c:2302-2488) —
// SAME 20-byte shape as the container's FiveIntPreamble, but this file stays
// independent of container_codec.hpp on purpose (save-boundary.md "Preserved
// quirk": SV_GetSaveComment must remain a standalone hand-parse).
constexpr std::size_t k_preamble_bytes = 20;

[[nodiscard]] std::int32_t read_i32_le( std::span<const std::byte> b ) noexcept
{
    const auto u = static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[0] ) ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[1] ) ) << 8 ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[2] ) ) << 16 ) |
                   ( static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( b[3] ) ) << 24 );
    return static_cast<std::int32_t>( u );
}

// Q_strncpy(dest, pData, size) over a raw (not-necessarily-NUL-terminated)
// field payload: cap at MAX_STRING, stop at the first embedded NUL.
[[nodiscard]] std::string decode_cstring_field( std::span<const std::byte> payload ) noexcept
{
    const std::size_t cap = payload.size() < k_comment_max_string ? payload.size() : k_comment_max_string;
    std::size_t       len = 0;
    while ( len < cap && payload[len] != std::byte{ 0 } )
        ++len;
    std::string s( len, '\0' );
    for ( std::size_t i = 0; i < len; ++i )
        s[i] = static_cast<char>( std::to_integer<unsigned char>( payload[i] ) );
    return s;
}

// strlcpy-shaped cap: truncate to at most `max_len` characters.
[[nodiscard]] std::string cap( std::string s, std::size_t max_len ) noexcept
{
    if ( s.size() > max_len )
        s.resize( max_len );
    return s;
}

[[nodiscard]] std::string pad2( int n ) noexcept
{
    std::string s = std::to_string( n );
    if ( s.size() < 2 )
        s.insert( 0, 2 - s.size(), '0' );
    return s;
}

// Reentrant localtime wrapper (PINNED path per save-boundary.md: filesystem
// FileTime -> clock_cast<system_clock> -> localtime_s/_r -> C-locale month
// formatting).  Implemented locally: save is the only subsystem needing this,
// and both platform APIs are trivial one-liners with an incompatible
// argument order (MSVC localtime_s(tm*,time_t*) vs POSIX localtime_r(time_t*,
// tm*)), not worth a shared platform-layer seam for one call site.
[[nodiscard]] std::tm localtime_reentrant( std::time_t t ) noexcept
{
    std::tm out{};
#if defined( _WIN32 )
    localtime_s( &out, &t );
#else
    localtime_r( &t, &out );
#endif
    return out;
}

// C-locale month abbreviations — NOT strftime("%b", ...), which is process-
// locale-dependent and would break byte-for-byte pinning under a non-C/en
// locale.  Matches the fixed English abbreviations glibc/msvcrt use under the
// "C" locale.
constexpr const char *k_month_abbrev[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};
} // namespace

// ---------------------------------------------------------------------------
// save_comment
// ---------------------------------------------------------------------------

Result<CommentInfo>
save_comment( std::span<const std::byte> image, std::string_view save_name,
             std::optional<std::time_t> file_time_utc, IMapValidityChecker *map_checker ) noexcept
{
    CommentInfo info;

    if ( image.size() < 4 )
        return std::unexpected( SaveError::TruncatedBlock );
    const std::int32_t id = read_i32_le( image.subspan( 0, 4 ) );
    if ( id != k_savegame_magic )
        return std::unexpected( SaveError::BadMagic ); // legacy "<corrupted>"

    if ( image.size() < 8 )
        return std::unexpected( SaveError::TruncatedBlock );
    const std::int32_t tag = read_i32_le( image.subspan( 4, 4 ) );
    if ( tag == 0x0065 )
    {
        info.status = SaveCommentStatus::OldVersionUnsupported;
        return info;
    }
    if ( tag < k_savegame_version )
    {
        info.status = SaveCommentStatus::OldVersion;
        return info;
    }
    if ( tag > k_savegame_version )
    {
        info.status = SaveCommentStatus::InvalidVersion;
        return info;
    }

    if ( image.size() < k_preamble_bytes )
        return std::unexpected( SaveError::TruncatedBlock );
    const std::int32_t data_size   = read_i32_le( image.subspan( 8, 4 ) );
    const std::int32_t token_count = read_i32_le( image.subspan( 12, 4 ) );
    const std::int32_t token_size  = read_i32_le( image.subspan( 16, 4 ) );

    // "<corrupted hashtable>" sanity check (sv_save.c:2358-2370).
    if ( token_count < 0 || static_cast<std::size_t>( token_count ) > ::xash::limits::save_hash_strings )
        return std::unexpected( SaveError::CorruptHeader );
    if ( token_size < 0 || static_cast<std::size_t>( token_size ) > ::xash::limits::save_heap_size )
        return std::unexpected( SaveError::CorruptHeader );
    if ( data_size < 0 || static_cast<std::size_t>( data_size ) > ::xash::limits::save_heap_size )
        return std::unexpected( SaveError::CorruptHeader );

    const std::size_t need = k_preamble_bytes + static_cast<std::size_t>( token_size ) +
                             static_cast<std::size_t>( data_size );
    if ( need > image.size() )
        return std::unexpected( SaveError::TruncatedBlock );

    // BuildHashTable equivalent.  A tokenSize==0 image (legacy's NULL
    // pTokenList crash trigger) is bounds-safe here — see the "Deviations"
    // doc comment in save_comment.hpp.
    TokenTable tokens( static_cast<std::size_t>( token_count ) );
    const std::span<const std::byte> token_blob =
        image.subspan( k_preamble_bytes, static_cast<std::size_t>( token_size ) );
    if ( auto r = tokens.rebuild( token_blob ); !r )
        return std::unexpected( r.error() );

    const std::span<const std::byte> data =
        image.subspan( k_preamble_bytes + static_cast<std::size_t>( token_size ),
                       static_cast<std::size_t>( data_size ) );
    std::size_t offset = 0;

    auto hdr = read_block_header( data, offset, tokens, "GameHeader" );
    if ( !hdr )
    {
        if ( hdr.error() == SaveError::CorruptHeader )
        {
            info.status = SaveCommentStatus::MissingGameHeader;
            return info;
        }
        return std::unexpected( hdr.error() ); // TruncatedBlock/BadFieldRecord — genuine corruption
    }
    if ( *hdr < 0 )
        return std::unexpected( SaveError::BadFieldRecord );

    std::string map_name;
    std::string description;
    for ( std::int32_t i = 0; i < *hdr; ++i )
    {
        auto rec = next_field_record( data, offset );
        if ( !rec )
            return std::unexpected( rec.error() );

        const std::string_view name = tokens.token_at( rec->token_idx );
        if ( ::xash::utilities::ci_equal( name, "comment" ) )
            description = decode_cstring_field( rec->payload );
        else if ( ::xash::utilities::ci_equal( name, "mapName" ) )
            map_name = decode_cstring_field( rec->payload );
        // else: unrecognized field name -> skip (offset already advanced).
    }

    if ( map_name.empty() )
    {
        info.status = SaveCommentStatus::UnknownVersion; // "<unknown version>"
        return info;
    }
    info.map_name = map_name;

    if ( map_checker != nullptr )
    {
        const MapValidity v = map_checker->check( map_name );
        if ( v == MapValidity::InvalidVersion )
        {
            info.status = SaveCommentStatus::MapInvalidVersion;
            return info;
        }
        if ( v == MapValidity::Missing )
        {
            info.status = SaveCommentStatus::MapMissing;
            return info;
        }
    }

    // Timestamp (PINNED path — see save_comment.hpp / localtime_reentrant doc).
    if ( file_time_utc )
    {
        const std::tm tm = localtime_reentrant( *file_time_utc );
        const int      month_idx = ( tm.tm_mon >= 0 && tm.tm_mon < 12 ) ? tm.tm_mon : 0;
        // "%b%d %Y" — NO space between month abbrev and day (legacy literal
        // format string, sv_save.c:2477).  strftime's "%d" is a ZERO-PADDED
        // 2-digit day (01-31) — CORRECTED 2026-07-19 (S8.5 parity audit: was
        // std::to_string(tm_mday), producing "Jan5" instead of "Jan05" for a
        // single-digit day).
        info.date_string = cap( std::string( k_month_abbrev[month_idx] ) +
                                    pad2( tm.tm_mday ) + " " +
                                    std::to_string( tm.tm_year + 1900 ),
                                k_comment_time_size - 1 );
        // "%H:%M" — zero-padded.
        info.time_string = cap( pad2( tm.tm_hour ) + ":" + pad2( tm.tm_min ),
                                k_comment_time_size - 1 );
    }

    // Quick/autosave PREFIX classification (Q_strstr substring, comment-time
    // — a DIFFERENT predicate than save_directory.hpp's rotation-time
    // Q_stricmp exact-stem check; save-boundary.md Quirks).
    std::string prefixed;
    if ( save_name.find( "quick" ) != std::string_view::npos )
        prefixed = "[quick]" + description;
    else if ( save_name.find( "autosave" ) != std::string_view::npos )
        prefixed = "[autosave]" + description;
    else
        prefixed = description;
    info.description = cap( std::move( prefixed ), k_comment_field_size - 1 );

    info.description_ext = ( description.size() > k_comment_field_size )
                               ? cap( description.substr( k_comment_field_size ), k_comment_field_size - 1 )
                               : std::string{};

    info.status = SaveCommentStatus::Ok;
    return info;
}

Result<CommentInfo>
save_comment_file( ::xash::filesystem::Filesystem &fs, std::string_view path,
                   IMapValidityChecker *map_checker ) noexcept
{
    if ( !fs.file_exists( path ) )
    {
        CommentInfo info;
        info.status = SaveCommentStatus::NotFound; // "just not exist - clear comment"
        return info;
    }

    const std::vector<std::byte> image = fs.load_file( path );

    std::optional<std::time_t> file_time_utc;
    if ( const auto ft = fs.file_time( path ); ft.has_value() )
    {
        const auto sys_tp = std::chrono::clock_cast<std::chrono::system_clock>( *ft );
        file_time_utc      = std::chrono::system_clock::to_time_t( sys_tp );
    }

    return save_comment( image, path, file_time_utc, map_checker );
}

// ---------------------------------------------------------------------------
// gTitleComments (sv_save.c:229-304) — ported verbatim, ordering preserved.
// ---------------------------------------------------------------------------

const std::array<TitleComment, 66> k_title_comments = { {
    { "T0A0", "#T0A0TITLE" },
    { "C0A0", "#C0A0TITLE" },
    { "C1A0", "#C0A1TITLE" },
    { "C1A1", "#C1A1TITLE" },
    { "C1A2", "#C1A2TITLE" },
    { "C1A3", "#C1A3TITLE" },
    { "C1A4", "#C1A4TITLE" },
    { "C2A1", "#C2A1TITLE" },
    { "C2A2", "#C2A2TITLE" },
    { "C2A3", "#C2A3TITLE" },
    { "C2A4D", "#C2A4TITLE2" },
    { "C2A4E", "#C2A4TITLE2" },
    { "C2A4F", "#C2A4TITLE2" },
    { "C2A4G", "#C2A4TITLE2" },
    { "C2A4", "#C2A4TITLE1" },
    { "C2A5", "#C2A5TITLE" },
    { "C3A1", "#C3A1TITLE" },
    { "C3A2", "#C3A2TITLE" },
    { "C4A1A", "#C4A1ATITLE" },
    { "C4A1B", "#C4A1ATITLE" },
    { "C4A1C", "#C4A1ATITLE" },
    { "C4A1D", "#C4A1ATITLE" },
    { "C4A1E", "#C4A1ATITLE" },
    { "C4A1", "#C4A1TITLE" },
    { "C4A2", "#C4A2TITLE" },
    { "C4A3", "#C4A3TITLE" },
    { "C5A1", "#C5TITLE" },
    { "OFBOOT", "#OF_BOOT0TITLE" },
    { "OF0A", "#OF1A1TITLE" },
    { "OF1A1", "#OF1A3TITLE" },
    { "OF1A2", "#OF1A3TITLE" },
    { "OF1A3", "#OF1A3TITLE" },
    { "OF1A4", "#OF1A3TITLE" },
    { "OF1A", "#OF1A5TITLE" },
    { "OF2A1", "#OF2A1TITLE" },
    { "OF2A2", "#OF2A1TITLE" },
    { "OF2A3", "#OF2A1TITLE" },
    { "OF2A", "#OF2A4TITLE" },
    { "OF3A1", "#OF3A1TITLE" },
    { "OF3A2", "#OF3A1TITLE" },
    { "OF3A", "#OF3A3TITLE" },
    { "OF4A1", "#OF4A1TITLE" },
    { "OF4A2", "#OF4A1TITLE" },
    { "OF4A3", "#OF4A1TITLE" },
    { "OF4A", "#OF4A4TITLE" },
    { "OF5A", "#OF5A1TITLE" },
    { "OF6A1", "#OF6A1TITLE" },
    { "OF6A2", "#OF6A1TITLE" },
    { "OF6A3", "#OF6A1TITLE" },
    { "OF6A4b", "#OF6A4TITLE" },
    { "OF6A4", "#OF6A4TITLE" },
    { "OF6A5", "#OF6A4TITLE" },
    { "OF6A", "#OF6A4TITLE" },
    { "OF7A", "#OF7A0TITLE" },
    { "ba_tram", "#BA_TRAMTITLE" },
    { "ba_security", "#BA_SECURITYTITLE" },
    { "ba_main", "#BA_SECURITYTITLE" },
    { "ba_elevator", "#BA_SECURITYTITLE" },
    { "ba_canal", "#BA_CANALSTITLE" },
    { "ba_yard", "#BA_YARDTITLE" },
    { "ba_xen", "#BA_XENTITLE" },
    { "ba_hazard", "#BA_HAZARD" },
    { "ba_power", "#BA_POWERTITLE" },
    { "ba_teleport1", "#BA_POWERTITLE" },
    { "ba_teleport", "#BA_TELEPORTTITLE" },
    { "ba_outro", "#BA_OUTRO" },
} };

// ---------------------------------------------------------------------------
// build_save_comment (SaveBuildComment, sv_save.c:306-357)
// ---------------------------------------------------------------------------

std::string
build_save_comment( std::string_view map_name, std::string_view world_message,
                    std::string_view dll_comment, float sv_time_seconds ) noexcept
{
    std::string_view name = dll_comment;

    if ( name.empty() )
    {
        for ( const auto &row : k_title_comments )
        {
            if ( map_name.size() >= row.mapname.size() &&
                ::xash::utilities::ci_equal( map_name.substr( 0, row.mapname.size() ), row.mapname ) )
            {
                name = row.titlename;
                break;
            }
        }
        if ( name.empty() )
            name = !world_message.empty() ? world_message : map_name;
    }

    // "%-64.64s" — left-justified, truncated to exactly 64 chars.
    std::string padded( name.substr( 0, name.size() < 64 ? name.size() : 64 ) );
    if ( padded.size() < 64 )
        padded.append( 64 - padded.size(), ' ' );

    // "%02d:%02d" — both zero-padded (sv_save.c:357); legacy computes both
    // in double precision (sv.time / 60.0, fmod(sv.time, 60.0)).
    const int minutes = static_cast<int>( static_cast<double>( sv_time_seconds ) / 60.0 );
    const int seconds = static_cast<int>( std::fmod( static_cast<double>( sv_time_seconds ), 60.0 ) );

    return padded + " " + pad2( minutes ) + ":" + pad2( seconds );
}

} // namespace xash::save
