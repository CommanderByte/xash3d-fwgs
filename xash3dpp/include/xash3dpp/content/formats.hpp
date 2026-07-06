#pragma once
// xash3dpp — sprite + alias model formats (the non-studio, non-brush loaders)
// Legacy reference: engine/common/mod_sprite.c (dsprite_q1_t/dsprite_hl_t),
//   mod_alias.c (Quake MDL). Sprite frame textures and the alias mesh are the
//   renderer's job; these loaders parse the header and own the file bytes.
//
// @thread-safety: plain value types — no shared state.

#include <xash3dpp/content/errors.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace xash::content {

// ---------------------------------------------------------------------------
// Sprite ("IDSP") — versions 1 (Quake), 2 (Half-Life), 32 (truecolor).
// ---------------------------------------------------------------------------

inline constexpr std::int32_t k_sprite_ident = ( 'P' << 24 ) | ( 'S' << 16 ) | ( 'D' << 8 ) | 'I';

struct SpriteInfo
{
    std::int32_t version    = 0;
    std::int32_t type       = 0;   // angletype (camera align)
    std::int32_t tex_format = 0;   // drawtype (0 on Quake sprites)
    std::int32_t num_frames = 0;
    std::int32_t max_width  = 0;    // header bounds[0]
    std::int32_t max_height = 0;    // header bounds[1]
    float        bounding_radius = 0.0f;
};

class SpriteModel
{
public:
    SpriteModel() = default;
    SpriteModel( SpriteInfo info, std::vector<std::byte> data ) noexcept
        : info_( info ), data_( std::move( data ) ) {}

    [[nodiscard]] const SpriteInfo&           info() const noexcept  { return info_; }
    [[nodiscard]] std::span<const std::byte>  bytes() const noexcept { return data_; }

private:
    SpriteInfo             info_ {};
    std::vector<std::byte> data_;   // @pre-reserved: file length (assign at parse; cold path)
};

[[nodiscard]] Result<SpriteModel> parse_sprite( std::span<const std::byte> file );

// ---------------------------------------------------------------------------
// Alias ("IDPO") — Quake MDL v6. Parse-minimal (render-only; boundary OQ-7):
// validate the header and own the bytes; the mesh/skins are the renderer's.
// ---------------------------------------------------------------------------

inline constexpr std::int32_t k_alias_ident   = ( 'O' << 24 ) | ( 'P' << 16 ) | ( 'D' << 8 ) | 'I';
inline constexpr std::int32_t k_alias_version = 6;

class AliasModel
{
public:
    AliasModel() = default;
    explicit AliasModel( std::vector<std::byte> data ) noexcept : data_( std::move( data ) ) {}

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return data_; }
    [[nodiscard]] bool                       empty() const noexcept { return data_.empty(); }

private:
    std::vector<std::byte> data_;   // @pre-reserved: file length (assign at parse; cold path)
};

[[nodiscard]] Result<AliasModel> parse_alias( std::span<const std::byte> file );

} // namespace xash::content
