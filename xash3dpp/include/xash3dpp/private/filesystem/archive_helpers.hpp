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
    using xash::utilities::strnicmp;
    auto it = std::lower_bound( entries.begin(), entries.end(), name,
        []( const T& e, std::string_view n ) {
            const std::size_t len =
                ( e.name.size() > n.size() ? e.name.size() : n.size() ) + 1;
            return strnicmp( e.name.c_str(), n.data(), len ) < 0;
        } );

    if ( it == entries.end() ) return nullptr;

    const std::size_t len =
        ( it->name.size() > name.size() ? it->name.size() : name.size() ) + 1;
    if ( strnicmp( it->name.c_str(), name.data(), len ) != 0 ) return nullptr;

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
    std::vector<std::string> results;

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
