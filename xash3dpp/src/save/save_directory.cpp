// xash3dpp — save-directory management (Chunk 8, slice S8.5).
// See save_directory.hpp for the legacy citations.

#include <xash3dpp/private/save/save_directory.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/utilities/string.hpp>

namespace xash::save {

namespace
{
// %02d-shaped zero-pad (values >= 100 print in full, matching printf).
[[nodiscard]] std::string pad2( int n ) noexcept
{
    std::string s = std::to_string( n );
    if ( s.size() < 2 )
        s.insert( 0, 2 - s.size(), '0' );
    return s;
}

[[nodiscard]] std::string slot_path( std::string_view dir, std::string_view stem, int n,
                                     std::string_view ext ) noexcept
{
    std::string p( dir );
    p += stem;
    if ( n > 0 )
        p += pad2( n );
    p += ext;
    return p;
}
} // namespace

// ---------------------------------------------------------------------------
// Stem predicates
// ---------------------------------------------------------------------------

bool is_quicksave_stem( std::string_view save_name ) noexcept
{
    return ::xash::utilities::ci_equal( save_name, "quick" );
}

bool is_autosave_stem( std::string_view save_name ) noexcept
{
    return ::xash::utilities::ci_equal( save_name, "autosave" );
}

// ---------------------------------------------------------------------------
// directory_count
// ---------------------------------------------------------------------------

std::size_t directory_count( ::xash::filesystem::Filesystem &fs, std::string_view pattern ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    return fs.search( pattern, /*case_insensitive*/ true, /*gamedironly*/ true ).files.size();
}

// ---------------------------------------------------------------------------
// clear_save_dir
// ---------------------------------------------------------------------------

void clear_save_dir( ::xash::filesystem::Filesystem &fs, std::string_view dir ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    const std::string pattern = std::string( dir ) + "*.HL?";
    const auto        found   = fs.search( pattern, true, true );
    for ( const auto &f : found.files )
        (void)fs.remove( f ); // best-effort delete, matching legacy FS_Delete (void, unchecked)
}

// ---------------------------------------------------------------------------
// age_save_list
// ---------------------------------------------------------------------------

void age_save_list( ::xash::filesystem::Filesystem &fs, std::string_view stem, int count,
                    std::string_view dir ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( count <= 0 )
        return;

    // Delete the oldest slot (e.g. quick05.sav/.bmp) — sv_save.c:598-604.
    (void)fs.remove( slot_path( dir, stem, count, ".sav" ) );
    (void)fs.remove( slot_path( dir, stem, count, ".bmp" ) );

    // Scroll the remaining slots down (sv_save.c:611-631): oldName is the
    // unnumbered stem when count==1, else the (count-1) slot; newName is
    // always the `count` slot.  FS_Rename silently no-ops on a missing
    // source (Filesystem::rename mirrors this — a not-found `from` is
    // skipped, not an error), matching legacy's tolerance for a not-yet-full
    // rotation.
    for ( int c = count; c > 0; --c )
    {
        const int old_n = ( c == 1 ) ? 0 : ( c - 1 );
        (void)fs.rename( slot_path( dir, stem, old_n, ".sav" ), slot_path( dir, stem, c, ".sav" ) );
        (void)fs.rename( slot_path( dir, stem, old_n, ".bmp" ), slot_path( dir, stem, c, ".bmp" ) );
    }
}

// ---------------------------------------------------------------------------
// select_latest_save / latest_save
// ---------------------------------------------------------------------------

std::optional<std::string_view> select_latest_save( std::span<const SaveFileTimeEntry> entries ) noexcept
{
    std::optional<std::string_view>                 best;
    std::optional<std::filesystem::file_time_type>   best_time;

    for ( const auto &e : entries )
    {
        if ( !e.time )
            continue; // FS_FileTime <= 0 ("found a match?" guard, sv_save.c:2277)
        // SV_CompareFileTime(newest, ft) < 0 -> newest is strictly older -> replace.
        if ( !best_time || *e.time > *best_time )
        {
            best      = e.name;
            best_time = e.time;
        }
    }
    return best;
}

std::optional<std::string> latest_save( ::xash::filesystem::Filesystem &fs, std::string_view dir ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    const std::string pattern = std::string( dir ) + "*.sav";
    const auto        found   = fs.search( pattern, true, true );
    if ( found.files.empty() )
        return std::nullopt;

    std::vector<SaveFileTimeEntry> entries;
    entries.reserve( found.files.size() );
    for ( const auto &name : found.files )
        entries.push_back( SaveFileTimeEntry{ name, fs.file_time( name ) } );

    const auto best = select_latest_save( entries );
    if ( !best )
        return std::nullopt;
    return std::string( *best );
}

} // namespace xash::save
