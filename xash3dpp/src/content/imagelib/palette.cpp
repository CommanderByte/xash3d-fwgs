// xash3dpp — imagelib GoldSrc/Quake 8-bit palette machinery
// Legacy reference: engine/common/imagelib/img_utils.c
//   palette_q1[768]/palette_hl[768], Image_ComparePalette, Image_SetPalette,
//   Image_GetPaletteQ1/HL, Image_Copy8bitRGBA, Image_FindBestBlack.
//
// The palette tables below are transcribed byte-for-byte from img_utils.c (the
// two tables differ only in the fullbright band around index 244, where HL swaps
// three colours for particle green). Parity is exact: build_palette reproduces
// each LUMP_* rendermode, and the texgamma LUT reproduces BuildGammaTable with
// the legacy default cvars (gamma 2.5, texgamma 2.0) — see PARITY NOTES.

#include <xash3dpp/private/imagelib/palette.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace xash::imagelib {
namespace {

// ---- the two built-in 256*RGB tables (verbatim from img_utils.c) -----------
constexpr std::array<std::uint8_t, 768> k_palette_q1 = {
    0, 0, 0, 15, 15, 15, 31, 31, 31, 47, 47, 47, 63, 63, 63, 75, 75, 75, 91, 91, 91, 107, 107, 107,
    123, 123, 123, 139, 139, 139, 155, 155, 155, 171, 171, 171, 187, 187, 187, 203, 203, 203, 219, 219, 219, 235, 235, 235,
    15, 11, 7, 23, 15, 11, 31, 23, 11, 39, 27, 15, 47, 35, 19, 55, 43, 23, 63, 47, 23, 75, 55, 27,
    83, 59, 27, 91, 67, 31, 99, 75, 31, 107, 83, 31, 115, 87, 31, 123, 95, 35, 131, 103, 35, 143, 111, 35,
    11, 11, 15, 19, 19, 27, 27, 27, 39, 39, 39, 51, 47, 47, 63, 55, 55, 75, 63, 63, 87, 71, 71, 103,
    79, 79, 115, 91, 91, 127, 99, 99, 139, 107, 107, 151, 115, 115, 163, 123, 123, 175, 131, 131, 187, 139, 139, 203,
    0, 0, 0, 7, 7, 0, 11, 11, 0, 19, 19, 0, 27, 27, 0, 35, 35, 0, 43, 43, 7, 47, 47, 7,
    55, 55, 7, 63, 63, 7, 71, 71, 7, 75, 75, 11, 83, 83, 11, 91, 91, 11, 99, 99, 11, 107, 107, 15,
    7, 0, 0, 15, 0, 0, 23, 0, 0, 31, 0, 0, 39, 0, 0, 47, 0, 0, 55, 0, 0, 63, 0, 0,
    71, 0, 0, 79, 0, 0, 87, 0, 0, 95, 0, 0, 103, 0, 0, 111, 0, 0, 119, 0, 0, 127, 0, 0,
    19, 19, 0, 27, 27, 0, 35, 35, 0, 47, 43, 0, 55, 47, 0, 67, 55, 0, 75, 59, 7, 87, 67, 7,
    95, 71, 7, 107, 75, 11, 119, 83, 15, 131, 87, 19, 139, 91, 19, 151, 95, 27, 163, 99, 31, 175, 103, 35,
    35, 19, 7, 47, 23, 11, 59, 31, 15, 75, 35, 19, 87, 43, 23, 99, 47, 31, 115, 55, 35, 127, 59, 43,
    143, 67, 51, 159, 79, 51, 175, 99, 47, 191, 119, 47, 207, 143, 43, 223, 171, 39, 239, 203, 31, 255, 243, 27,
    11, 7, 0, 27, 19, 0, 43, 35, 15, 55, 43, 19, 71, 51, 27, 83, 55, 35, 99, 63, 43, 111, 71, 51,
    127, 83, 63, 139, 95, 71, 155, 107, 83, 167, 123, 95, 183, 135, 107, 195, 147, 123, 211, 163, 139, 227, 179, 151,
    171, 139, 163, 159, 127, 151, 147, 115, 135, 139, 103, 123, 127, 91, 111, 119, 83, 99, 107, 75, 87, 95, 63, 75,
    87, 55, 67, 75, 47, 55, 67, 39, 47, 55, 31, 35, 43, 23, 27, 35, 19, 19, 23, 11, 11, 15, 7, 7,
    187, 115, 159, 175, 107, 143, 163, 95, 131, 151, 87, 119, 139, 79, 107, 127, 75, 95, 115, 67, 83, 107, 59, 75,
    95, 51, 63, 83, 43, 55, 71, 35, 43, 59, 31, 35, 47, 23, 27, 35, 19, 19, 23, 11, 11, 15, 7, 7,
    219, 195, 187, 203, 179, 167, 191, 163, 155, 175, 151, 139, 163, 135, 123, 151, 123, 111, 135, 111, 95, 123, 99, 83,
    107, 87, 71, 95, 75, 59, 83, 63, 51, 67, 51, 39, 55, 43, 31, 39, 31, 23, 27, 19, 15, 15, 11, 7,
    111, 131, 123, 103, 123, 111, 95, 115, 103, 87, 107, 95, 79, 99, 87, 71, 91, 79, 63, 83, 71, 55, 75, 63,
    47, 67, 55, 43, 59, 47, 35, 51, 39, 31, 43, 31, 23, 35, 23, 15, 27, 19, 11, 19, 11, 7, 11, 7,
    255, 243, 27, 239, 223, 23, 219, 203, 19, 203, 183, 15, 187, 167, 15, 171, 151, 11, 155, 131, 7, 139, 115, 7,
    123, 99, 7, 107, 83, 0, 91, 71, 0, 75, 55, 0, 59, 43, 0, 43, 31, 0, 27, 15, 0, 11, 7, 0,
    0, 0, 255, 11, 11, 239, 19, 19, 223, 27, 27, 207, 35, 35, 191, 43, 43, 175, 47, 47, 159, 47, 47, 143,
    47, 47, 127, 47, 47, 111, 47, 47, 95, 43, 43, 79, 35, 35, 63, 27, 27, 47, 19, 19, 31, 11, 11, 15,
    43, 0, 0, 59, 0, 0, 75, 7, 0, 95, 7, 0, 111, 15, 0, 127, 23, 7, 147, 31, 7, 163, 39, 11,
    183, 51, 15, 195, 75, 27, 207, 99, 43, 219, 127, 59, 227, 151, 79, 231, 171, 95, 239, 191, 119, 247, 211, 139,
    167, 123, 59, 183, 155, 55, 199, 195, 55, 231, 227, 87, 127, 191, 255, 171, 231, 255, 215, 255, 255, 103, 0, 0,
    139, 0, 0, 179, 0, 0, 215, 0, 0, 255, 0, 0, 255, 243, 147, 255, 247, 199, 255, 255, 255, 159, 91, 83
};

// Used only for particle colours (differs from Q1 at entries 244..246).
constexpr std::array<std::uint8_t, 768> k_palette_hl = {
    0, 0, 0, 15, 15, 15, 31, 31, 31, 47, 47, 47, 63, 63, 63, 75, 75, 75, 91, 91, 91, 107, 107, 107,
    123, 123, 123, 139, 139, 139, 155, 155, 155, 171, 171, 171, 187, 187, 187, 203, 203, 203, 219, 219, 219, 235, 235, 235,
    15, 11, 7, 23, 15, 11, 31, 23, 11, 39, 27, 15, 47, 35, 19, 55, 43, 23, 63, 47, 23, 75, 55, 27,
    83, 59, 27, 91, 67, 31, 99, 75, 31, 107, 83, 31, 115, 87, 31, 123, 95, 35, 131, 103, 35, 143, 111, 35,
    11, 11, 15, 19, 19, 27, 27, 27, 39, 39, 39, 51, 47, 47, 63, 55, 55, 75, 63, 63, 87, 71, 71, 103,
    79, 79, 115, 91, 91, 127, 99, 99, 139, 107, 107, 151, 115, 115, 163, 123, 123, 175, 131, 131, 187, 139, 139, 203,
    0, 0, 0, 7, 7, 0, 11, 11, 0, 19, 19, 0, 27, 27, 0, 35, 35, 0, 43, 43, 7, 47, 47, 7,
    55, 55, 7, 63, 63, 7, 71, 71, 7, 75, 75, 11, 83, 83, 11, 91, 91, 11, 99, 99, 11, 107, 107, 15,
    7, 0, 0, 15, 0, 0, 23, 0, 0, 31, 0, 0, 39, 0, 0, 47, 0, 0, 55, 0, 0, 63, 0, 0,
    71, 0, 0, 79, 0, 0, 87, 0, 0, 95, 0, 0, 103, 0, 0, 111, 0, 0, 119, 0, 0, 127, 0, 0,
    19, 19, 0, 27, 27, 0, 35, 35, 0, 47, 43, 0, 55, 47, 0, 67, 55, 0, 75, 59, 7, 87, 67, 7,
    95, 71, 7, 107, 75, 11, 119, 83, 15, 131, 87, 19, 139, 91, 19, 151, 95, 27, 163, 99, 31, 175, 103, 35,
    35, 19, 7, 47, 23, 11, 59, 31, 15, 75, 35, 19, 87, 43, 23, 99, 47, 31, 115, 55, 35, 127, 59, 43,
    143, 67, 51, 159, 79, 51, 175, 99, 47, 191, 119, 47, 207, 143, 43, 223, 171, 39, 239, 203, 31, 255, 243, 27,
    11, 7, 0, 27, 19, 0, 43, 35, 15, 55, 43, 19, 71, 51, 27, 83, 55, 35, 99, 63, 43, 111, 71, 51,
    127, 83, 63, 139, 95, 71, 155, 107, 83, 167, 123, 95, 183, 135, 107, 195, 147, 123, 211, 163, 139, 227, 179, 151,
    171, 139, 163, 159, 127, 151, 147, 115, 135, 139, 103, 123, 127, 91, 111, 119, 83, 99, 107, 75, 87, 95, 63, 75,
    87, 55, 67, 75, 47, 55, 67, 39, 47, 55, 31, 35, 43, 23, 27, 35, 19, 19, 23, 11, 11, 15, 7, 7,
    187, 115, 159, 175, 107, 143, 163, 95, 131, 151, 87, 119, 139, 79, 107, 127, 75, 95, 115, 67, 83, 107, 59, 75,
    95, 51, 63, 83, 43, 55, 71, 35, 43, 59, 31, 35, 47, 23, 27, 35, 19, 19, 23, 11, 11, 15, 7, 7,
    219, 195, 187, 203, 179, 167, 191, 163, 155, 175, 151, 139, 163, 135, 123, 151, 123, 111, 135, 111, 95, 123, 99, 83,
    107, 87, 71, 95, 75, 59, 83, 63, 51, 67, 51, 39, 55, 43, 31, 39, 31, 23, 27, 19, 15, 15, 11, 7,
    111, 131, 123, 103, 123, 111, 95, 115, 103, 87, 107, 95, 79, 99, 87, 71, 91, 79, 63, 83, 71, 55, 75, 63,
    47, 67, 55, 43, 59, 47, 35, 51, 39, 31, 43, 31, 23, 35, 23, 15, 27, 19, 11, 19, 11, 7, 11, 7,
    255, 243, 27, 239, 223, 23, 219, 203, 19, 203, 183, 15, 187, 167, 15, 171, 151, 11, 155, 131, 7, 139, 115, 7,
    123, 99, 7, 107, 83, 0, 91, 71, 0, 75, 55, 0, 59, 43, 0, 43, 31, 0, 27, 15, 0, 11, 7, 0,
    0, 0, 255, 11, 11, 239, 19, 19, 223, 27, 27, 207, 35, 35, 191, 43, 43, 175, 47, 47, 159, 47, 47, 143,
    47, 47, 127, 47, 47, 111, 47, 47, 95, 43, 43, 79, 35, 35, 63, 27, 27, 47, 19, 19, 31, 11, 11, 15,
    43, 0, 0, 59, 0, 0, 75, 7, 0, 95, 7, 0, 111, 15, 0, 127, 23, 7, 147, 31, 7, 163, 39, 11,
    183, 51, 15, 195, 75, 27, 207, 99, 43, 219, 127, 59, 227, 151, 79, 231, 171, 95, 239, 191, 119, 247, 211, 139,
    167, 123, 59, 183, 155, 55, 199, 195, 55, 231, 227, 87, 0, 255, 0, 171, 231, 255, 215, 255, 255, 103, 0, 0,
    139, 0, 0, 179, 0, 0, 215, 0, 0, 255, 0, 0, 255, 243, 147, 255, 247, 199, 255, 255, 255, 159, 91, 83
};

// texgamma LUT — BuildGammaTable (engine/client/gamma.c) with the legacy default
// cvars gamma 2.5 and texgamma 2.0: g1 = 1/gamma = 0.4, g2 = g1 * texgamma = 0.8,
// texgammatable[i] = clamp( (int)( (float)pow(i/255, g2) * 255.0f ), 0, 255 ).
// Built once on first use (std::pow is not constexpr). See PARITY NOTES: runtime
// gamma config should later be injected instead of these frozen defaults.
[[nodiscard]] const std::array<std::uint8_t, 256> &texgamma_table() noexcept
{
    static const std::array<std::uint8_t, 256> table = [] {
        std::array<std::uint8_t, 256> t {};
        constexpr double g2 = ( 1.0 / 2.5 ) * 2.0;  // = 0.8
        for( int i = 0; i < 256; ++i )
        {
            const float d   = static_cast<float>( std::pow( i / 255.0, g2 ) );
            const int   inf = static_cast<int>( d * 255.0f );  // legacy truncation
            t[static_cast<std::size_t>( i )] =
                static_cast<std::uint8_t>( inf < 0 ? 0 : ( inf > 255 ? 255 : inf ) );
        }
        return t;
    }();
    return table;
}

// Bounds-clamped read of a raw-palette byte (short palettes read as 0).
[[nodiscard]] std::uint8_t pal_byte( std::span<const std::uint8_t> pal, std::size_t i ) noexcept
{
    return i < pal.size() ? pal[i] : std::uint8_t{ 0 };
}

} // namespace

// ---------------------------------------------------------------------------
// Built-in tables
// ---------------------------------------------------------------------------

std::span<const std::uint8_t> quake_palette_rgb() noexcept { return k_palette_q1; }
std::span<const std::uint8_t> halflife_palette_rgb() noexcept { return k_palette_hl; }

// ---------------------------------------------------------------------------
// classify_palette — Image_ComparePalette (first 765 bytes; last colour free).
// ---------------------------------------------------------------------------

PaletteClass classify_palette( std::span<const std::uint8_t> pal ) noexcept
{
    constexpr std::size_t k_compare = 765;
    if( pal.size() < k_compare )
        return PaletteClass::Invalid;

    bool is_q1 = true, is_hl = true;
    for( std::size_t i = 0; i < k_compare; ++i )
    {
        if( pal[i] != k_palette_q1[i] ) is_q1 = false;
        if( pal[i] != k_palette_hl[i] ) is_hl = false;
    }
    if( is_q1 ) return PaletteClass::Quake1;
    if( is_hl ) return PaletteClass::HalfLife;
    return PaletteClass::Custom;
}

// ---------------------------------------------------------------------------
// build_palette — Image_SetPalette rendermodes.
// ---------------------------------------------------------------------------

Palette build_palette( std::span<const std::uint8_t> pal, Rendermode mode ) noexcept
{
    Palette out;

    switch( mode )
    {
    case Rendermode::Normal:
        for( std::size_t i = 0; i < 256; ++i )
            out[i] = Rgba{ pal_byte( pal, i * 3 + 0 ), pal_byte( pal, i * 3 + 1 ),
                           pal_byte( pal, i * 3 + 2 ), 0xFF };
        break;

    case Rendermode::TexGamma:
    {
        const std::array<std::uint8_t, 256> &tg = texgamma_table();
        for( std::size_t i = 0; i < 256; ++i )
            out[i] = Rgba{ tg[pal_byte( pal, i * 3 + 0 )], tg[pal_byte( pal, i * 3 + 1 )],
                           tg[pal_byte( pal, i * 3 + 2 )], 0xFF };
        break;
    }

    case Rendermode::Gradient:
    {
        // Front colour is the index-255 entry; alpha ramps with the index.
        const std::uint8_t fr = pal_byte( pal, 765 );
        const std::uint8_t fg = pal_byte( pal, 766 );
        const std::uint8_t fb = pal_byte( pal, 767 );
        for( std::size_t i = 0; i < 256; ++i )
            out[i] = Rgba{ fr, fg, fb, static_cast<std::uint8_t>( i ) };
        break;
    }

    case Rendermode::Masked:
        for( std::size_t i = 0; i < 255; ++i )
            out[i] = Rgba{ pal_byte( pal, i * 3 + 0 ), pal_byte( pal, i * 3 + 1 ),
                           pal_byte( pal, i * 3 + 2 ), 0xFF };
        out[255] = Rgba{ 0, 0, 0, 0 };  // transparent key
        out.set_has_alpha( true );
        break;
    }

    return out;
}

Palette quake_palette() noexcept
{
    Palette pal = build_palette( k_palette_q1, Rendermode::Normal );
    pal[255] = Rgba{ 0, 0, 0, 0 };  // Image_GetPaletteQ1: d_8toQ1table[255] = 0
    pal.set_has_alpha( true );
    return pal;
}

Palette halflife_palette() noexcept
{
    return build_palette( k_palette_hl, Rendermode::Normal );
}

// ---------------------------------------------------------------------------
// Pixel-scan helpers
// ---------------------------------------------------------------------------

bool detect_luma( std::span<const std::byte> indices, bool exclude_255 ) noexcept
{
    for( const std::byte b : indices )
    {
        const std::uint8_t idx = std::to_integer<std::uint8_t>( b );
        if( idx > k_fullbright_index && ( !exclude_255 || idx != 255 ) )
            return true;
    }
    return false;
}

int find_best_black( const Palette &pal ) noexcept
{
    int min_color  = 32;   // packed-RGB threshold (legacy: color < 32)
    int best_black = -1;
    for( int i = 0; i < 256; ++i )
    {
        const Rgba c = pal[static_cast<std::size_t>( i )];
        // Packed RGB in the legacy little-endian layout: (b<<16)|(g<<8)|r.
        const int color = ( static_cast<int>( c.b ) << 16 )
                        | ( static_cast<int>( c.g ) << 8 )
                        |   static_cast<int>( c.r );
        if( color < min_color )
        {
            min_color  = color;
            best_black = i;
        }
    }
    return best_black;
}

bool palette_has_color( const Palette &pal ) noexcept
{
    for( std::size_t i = 0; i < 256; ++i )
    {
        const Rgba c = pal[i];
        if( c.r != c.g || c.g != c.b )
            return true;
    }
    return false;
}

void expand_indexed_rgba( std::span<const std::byte> indices, const Palette &pal,
                          std::span<std::byte> out ) noexcept
{
    const std::size_t n = indices.size() < out.size() / 4 ? indices.size() : out.size() / 4;
    for( std::size_t i = 0; i < n; ++i )
    {
        const std::uint8_t idx = std::to_integer<std::uint8_t>( indices[i] );
        const Rgba         c   = pal[idx];
        out[i * 4 + 0] = std::byte{ c.r };
        out[i * 4 + 1] = std::byte{ c.g };
        out[i * 4 + 2] = std::byte{ c.b };
        out[i * 4 + 3] = std::byte{ c.a };
    }
}

} // namespace xash::imagelib
