#pragma once
// xash3dpp — shared helpers for sorted archive-entry tables  (PAK, ZIP, …)
// Legacy reference: filesystem/pak.c (FS_FindFile_PAK, FS_Search_PAK),
//                   filesystem/zip.c (FS_FindFile_ZIP, FS_Search_ZIP)
//
// Requirements on Entry type T:
//   • T::name  is a std::string
//   • Entries must be pre-sorted case-insensitively ascending (use CiNameLess)

#include <xash3dpp/utilities/string.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace xash::filesystem {

// ---------------------------------------------------------------------------
// CiNameLess — sort comparator for Entry vectors whose key is .name
// ---------------------------------------------------------------------------

template<typename T>
struct CiNameLess
{
    bool operator()( const T& a, const T& b ) const noexcept
    {
        return xash::utilities::ci_less( a.name, b.name );
    }
};

// ---------------------------------------------------------------------------
// ci_find_by_name — binary search by case-insensitive .name
//
// Returns a const pointer into 'entries', or nullptr if not found.
// Mirrors legacy FS_FindFile_PAK / FS_FindFile_ZIP.
// ---------------------------------------------------------------------------

template<typename T>
[[nodiscard]] const T* ci_find_by_name( const std::vector<T>& entries,
                           std::string_view      name ) noexcept
{
    // Bounded compare (utilities ci_compare): 'name' may be a slice of a
    // larger buffer with no NUL at data()+size() — never hand it to a
    // C-string compare (HB-1 / M-7). Ordering matches CiNameLess.
    auto it = std::lower_bound( entries.begin(), entries.end(), name,
        []( const T& e, std::string_view n ) {
            return xash::utilities::ci_compare( e.name, n ) < 0;
        } );

    if ( it == entries.end() ) return nullptr;
    if ( xash::utilities::ci_compare( it->name, name ) != 0 ) return nullptr;

    return &*it;
}

// ---------------------------------------------------------------------------
// archive_search_by_name — match pattern against all entries and their
// directory-prefix paths; deduplicates before appending.
//
// Mirrors legacy FS_Search_PAK / FS_Search_ZIP.
// ---------------------------------------------------------------------------

template<typename T>
[[nodiscard]] std::vector<std::string> archive_search_by_name( const std::vector<T>& entries,
                                                   std::string_view      pattern )
{
    using xash::utilities::match_pattern;
    std::vector<std::string> results;  // @pre-reserved: cold local dedup accumulator on the search path; bounded by matching entries, no pre-sizing (reserve N/A)

    for ( const auto& e : entries ) {
        std::string temp = e.name;
        while ( !temp.empty() ) {
            if ( match_pattern( temp, pattern, /*case_insensitive=*/true ) ) {
                if ( std::find( results.begin(), results.end(), temp )
                         == results.end() )
                    results.push_back( temp );
            }
            auto slash = temp.rfind( '/' );
            if ( slash == std::string::npos ) slash = temp.rfind( '\\' );
            if ( slash == std::string::npos ) break;
            temp.resize( slash );
        }
    }

    return results;
}

} // namespace xash::filesystem
