#pragma once
// xash3dpp — constexpr archive-type registry  (internal)
// Legacy reference: filesystem/filesystem.c  (g_archives[])
//
// Mount order within a game directory: PAK → PK3 → pk3dir → WAD → plain dir.
// Earlier entries shadow later ones (first-match wins).

#include <xash3dpp/filesystem/search_path_flags.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/private/filesystem/i_search_backend.hpp>

#include <array>
#include <memory>
#include <string_view>

namespace xash::filesystem {

using BackendFactory =
    std::unique_ptr<ISearchBackend>(*)(xash::memory::PoolHandle pool,
                                       std::string_view         path,
                                       SearchPathFlags          flags);

struct ArchiveType {
    std::string_view extension;      // e.g. "pak", "pk3", "pk3dir", "wad"
    SearchPathFlags  default_flags;
    bool             mounts_wads;    // auto-mount .wad files found inside
    bool             allow_exec;     // may serve native library files
    BackendFactory   factory;
};

// Forward-declare the factory functions so this header stays self-contained.
namespace backends {
[[nodiscard]] std::unique_ptr<ISearchBackend> create_pak   (xash::memory::PoolHandle, std::string_view, SearchPathFlags);
[[nodiscard]] std::unique_ptr<ISearchBackend> create_zip   (xash::memory::PoolHandle, std::string_view, SearchPathFlags);
[[nodiscard]] std::unique_ptr<ISearchBackend> create_pk3dir(xash::memory::PoolHandle, std::string_view, SearchPathFlags);
[[nodiscard]] std::unique_ptr<ISearchBackend> create_wad   (xash::memory::PoolHandle, std::string_view, SearchPathFlags);
} // namespace backends

inline constexpr std::array<ArchiveType, 4> k_archive_types = {{
    { "pak",    SearchPathFlags::Exec, true,  true,  &backends::create_pak    },
    { "pk3",    SearchPathFlags::None, true,  false, &backends::create_zip    },
    { "pk3dir", SearchPathFlags::None, true,  false, &backends::create_pk3dir },
    { "wad",    SearchPathFlags::None, false, false, &backends::create_wad    },
}};

} // namespace xash::filesystem
