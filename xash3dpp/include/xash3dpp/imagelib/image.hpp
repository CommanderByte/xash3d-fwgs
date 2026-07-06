#pragma once
// xash3dpp — imagelib decoded-image value type (OQ-1 internal Image)
// Legacy reference: common/com_image.h rgbdata_t; engine/common/imagelib/img_main.c
//
// A codec decodes a file into an Image (owns its pixel bytes + optional palette).
// The renderer seam (Chunk 13) adapts Image -> rgbdata_t; content never freezes
// on the legacy struct. Bulk pixel data is a std::vector, matching the
// map_loader precedent for loaded binary data (raw malloc/new stay forbidden).
//
// @thread-safety: a plain value type — no shared state; safe to move between
// threads. Decoding that produces one is main-thread today (see ImageDecoder).

#include <xash3dpp/imagelib/pixel_format.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace xash::imagelib {

// ---------------------------------------------------------------------------
// Palette — up to 256 RGBA entries (the GoldSrc/Quake 8-bit colour table).
// ---------------------------------------------------------------------------

class Palette
{
public:
    Palette() = default;

    [[nodiscard]] Rgba  operator[]( std::size_t i ) const noexcept { return entries_[i & 0xFF]; }
    [[nodiscard]] Rgba &operator[]( std::size_t i ) noexcept       { return entries_[i & 0xFF]; }

    [[nodiscard]] std::span<const Rgba> entries() const noexcept { return entries_; }
    [[nodiscard]] bool has_alpha() const noexcept { return has_alpha_; }
    void set_has_alpha( bool v ) noexcept { has_alpha_ = v; }

private:
    std::array<Rgba, 256> entries_ {};
    bool                  has_alpha_ = false;  // legacy PF_INDEXED_32 vs _24
};

// ---------------------------------------------------------------------------
// Image — a decoded (or kept-compressed) image with its owned pixel bytes.
// ---------------------------------------------------------------------------

class Image
{
public:
    Image() = default;

    Image( std::uint16_t w, std::uint16_t h, PixelFormat fmt,
           std::vector<std::byte> pixels, ImageFlags flags = ImageFlags::None ) noexcept
        : width_( w ), height_( h ), format_( fmt ), flags_( flags ),
          pixels_( std::move( pixels ) )
    {
    }

    // ---- dimensions / format --------------------------------------------
    [[nodiscard]] std::uint16_t width()  const noexcept { return width_; }
    [[nodiscard]] std::uint16_t height() const noexcept { return height_; }
    [[nodiscard]] std::uint16_t depth()  const noexcept { return depth_; }
    [[nodiscard]] std::uint8_t  mip_count() const noexcept { return mip_count_; }
    [[nodiscard]] PixelFormat   format() const noexcept { return format_; }
    [[nodiscard]] ImageFlags    flags()  const noexcept { return flags_; }
    [[nodiscard]] bool has( ImageFlags f ) const noexcept { return has_flag( flags_, f ); }

    // ---- pixel data ------------------------------------------------------
    [[nodiscard]] std::span<const std::byte> pixels() const noexcept { return pixels_; }
    [[nodiscard]] std::span<std::byte>       pixels_mut() noexcept   { return pixels_; }
    [[nodiscard]] bool empty() const noexcept { return pixels_.empty(); }

    [[nodiscard]] const std::optional<Palette> &palette() const noexcept { return palette_; }
    [[nodiscard]] Rgba fog_params() const noexcept { return fog_params_; }

    // ---- builders (codecs fill the optional bits post-construction) ------
    void set_depth( std::uint16_t d ) noexcept       { depth_ = d; }
    void set_mip_count( std::uint8_t n ) noexcept    { mip_count_ = n; }
    void set_flags( ImageFlags f ) noexcept          { flags_ = f; }
    void add_flags( ImageFlags f ) noexcept          { flags_ |= f; }
    void set_palette( Palette p ) noexcept           { palette_ = std::move( p ); }
    void set_fog_params( Rgba p ) noexcept           { fog_params_ = p; }

private:
    std::uint16_t          width_     = 0;
    std::uint16_t          height_    = 0;
    std::uint16_t          depth_     = 1;
    std::uint8_t           mip_count_ = 1;
    PixelFormat            format_    = PixelFormat::Unknown;
    ImageFlags             flags_     = ImageFlags::None;
    std::vector<std::byte> pixels_;              // @pre-reserved: decoded level_bytes() (assign at decode; cold path)
    std::optional<Palette> palette_;             // present iff format_ == Indexed8
    Rgba                   fog_params_ {};        // water fog colour+density / decal reflectivity
};

} // namespace xash::imagelib
