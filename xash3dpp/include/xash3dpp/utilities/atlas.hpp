#pragma once
// xash3dpp — 2D texture atlas packer (strip-based)
// Legacy reference: public/atlas.h + public/atlas.c
//
// ATLAS_MAX_SIZE is kept at 1024 to match the legacy struct layout.
// Changing it is a breaking change for anything serialising atlas coordinates.

#include <array>
#include <cstdint>
#include <optional>

namespace xash::utilities {

static constexpr int ATLAS_MAX_SIZE = 1024;

// Not thread-safe: one owner thread only.  If an Atlas instance is shared
// across threads, the caller must provide external synchronisation.
class Atlas
{
public:
    explicit Atlas( int size ) noexcept;

    // Reset the atlas to empty.
    void clear() noexcept;

    struct Block { int x, y; };

    // Allocate a w×h rectangle.  Returns coordinates on success, nullopt if full.
    std::optional<Block> alloc( int w, int h ) noexcept;

    int size() const noexcept { return m_size; }
    int max_height() const noexcept { return m_max_height; }

private:
    std::array<int, ATLAS_MAX_SIZE> m_allocated{};
    int m_size{};
    int m_max_height{};
};

} // namespace xash::utilities
