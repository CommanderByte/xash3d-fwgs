#pragma once
// xash3dpp — 2D texture atlas packer (strip-based)
// Legacy reference: public/atlas.h + public/atlas.c
//
// ATLAS_MAX_SIZE mirrors limits::atlas_max_size (ABI-constrained: matches the legacy
// struct layout; changing it is a breaking serialisation change).
//
// @thread-safety: thread-agnostic value type — confine each Atlas instance
// to its owner's thread.

#include <xash3dpp/limits.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace xash::utilities {

static constexpr int ATLAS_MAX_SIZE = static_cast<int>(xash::limits::atlas_max_size);

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
    [[nodiscard]] std::optional<Block> alloc( int w, int h ) noexcept;

    [[nodiscard]] int size() const noexcept { return m_size; }
    [[nodiscard]] int max_height() const noexcept { return m_max_height; }

private:
    std::array<int, ATLAS_MAX_SIZE> m_allocated{};
    int m_size{};
    int m_max_height{};
};

} // namespace xash::utilities
