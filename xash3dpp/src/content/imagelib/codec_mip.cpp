// xash3dpp — imagelib MIP (miptex) codec
// Legacy reference: engine/common/imagelib/img_wad.c  Image_LoadMIP.
//   Two on-disk variants share the mip_t header (name[16] + w + h + offsets[4]):
//     • Half-Life 1.0.0.1 mip: 4 mip levels + an embedded 256-colour palette.
//     • Quake1 1.01 mip: 4 mip levels, no palette (uses the built-in Q1 table).
//   The embedded/built-in palette + the LUMP_* rendermode drive an index->RGBA
//   expansion (palette.hpp), and the texture name selects the quirks:
//     '{'          -> masked decal (index 255 transparent; OneBitAlpha)
//     '!'/"water*" -> HL water: fog colour+density parsed into fog_params
//     "sky" + 2:1  -> QuakeSky
//     '~'/'+N~'    -> WAD3 luma layer (gated by r_allow_wad3_luma, off by default)
//   plus Quake-vs-HL palette classification (QuakePal + luma) and texgamma for
//   HL-palette mips. See PARITY NOTES in the report for renderer-side deferrals.
//
// Codecs are stateless (boundary H-1) and every source read is bounds-checked
// (modernization H-4): a truncated/hostile lump yields ImageError, never a fault.
// Output is always PF_RGBA_32 (Rgba8), matching the sibling WAD codec.

#include <xash3dpp/private/imagelib/codec.hpp>
#include <xash3dpp/private/imagelib/palette.hpp>

#include <xash3dpp/utilities/swap.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace xash::imagelib {
namespace {

// ---- frozen mip_t format constants (common/wadfile.h) ---------------------
constexpr std::size_t k_mip_header  = 40;       // name[16] + w + h + offsets[4]
constexpr std::size_t k_mip_name    = 16;       // mip_t.name field width
constexpr std::size_t k_max_dim     = 8192;     // Image_ValidSize (IMAGE_MAXWIDTH/HEIGHT)
constexpr std::size_t k_palette_rgb = 256 * 3;  // embedded palette payload

// Runtime seams that are off by default in the legacy engine. Kept as explicit
// compile-time constants so a later renderer/config pass can inject the real
// cvar/force-flag state (see PARITY NOTES):
//   r_allow_wad3_luma "0" — the '~'/'+N~' HL-palette luma path is disabled.
//   IL_LOAD_DECAL          — set only on decal/menu loads, so '{' is masked here.
constexpr bool k_allow_wad3_luma = false;
constexpr bool k_load_decal      = false;

[[nodiscard]] std::uint8_t byte_at( std::span<const std::byte> s, std::size_t i ) noexcept
{
    return std::to_integer<std::uint8_t>( s[i] );
}

[[nodiscard]] char name_ch( std::string_view s, std::size_t i ) noexcept
{
    return i < s.size() ? s[i] : '\0';
}

[[nodiscard]] bool is_ascii_digit( char c ) noexcept { return c >= '0' && c <= '9'; }

// Case-insensitive prefix test (legacy Q_strnicmp) over ASCII.
[[nodiscard]] bool istarts_with( std::string_view s, std::string_view prefix ) noexcept
{
    if( s.size() < prefix.size() )
        return false;
    for( std::size_t i = 0; i < prefix.size(); ++i )
    {
        char a = s[i], b = prefix[i];
        if( a >= 'A' && a <= 'Z' ) a = static_cast<char>( a - 'A' + 'a' );
        if( b >= 'A' && b <= 'Z' ) b = static_cast<char>( b - 'A' + 'a' );
        if( a != b )
            return false;
    }
    return true;
}

// ---- decode ---------------------------------------------------------------
// Parity with Image_LoadMIP. `name` is the file/texture name (the '{' masked
// quirk keys off it, as legacy does via Q_strrchr(name,'{')); the sky/water/luma
// quirks key off the embedded mip_t.name, exactly as legacy reads mip.name.
[[nodiscard]] Result<Image> decode_mip( std::string_view name, std::span<const std::byte> file )
{
    if( file.size() < k_mip_header )
        return std::unexpected( ImageError::Truncated );

    const auto width   = ::xash::utilities::read_le<std::uint32_t>( file.data() + 16 );
    const auto height  = ::xash::utilities::read_le<std::uint32_t>( file.data() + 20 );
    const auto offset0 = ::xash::utilities::read_le<std::uint32_t>( file.data() + 24 );

    if( width == 0 || height == 0 )
        return std::unexpected( ImageError::BadHeader );
    if( width > k_max_dim || height > k_max_dim )
        return std::unexpected( ImageError::TooLarge );

    const std::size_t pixels   = static_cast<std::size_t>( width ) * height;
    const std::size_t mipchain = ( pixels * 85 ) >> 6;  // all four mip levels
    const std::size_t px_off   = offset0;

    // Bounds-check the mip-level-0 pixel run (H-4).
    if( px_off > file.size() || file.size() - px_off < pixels )
        return std::unexpected( ImageError::Truncated );
    const std::span<const std::byte> indices = file.subspan( px_off, pixels );

    // Embedded mip name (NUL-terminated within 16 bytes) for the mip.name quirks.
    std::array<char, k_mip_name + 1> namebuf {};
    for( std::size_t i = 0; i < k_mip_name; ++i )
    {
        const std::uint8_t c = byte_at( file, i );
        if( c == 0 )
            break;
        namebuf[i] = static_cast<char>( c );
    }
    const std::string_view ename{ namebuf.data() };
    const char             c0 = name_ch( ename, 0 );

    // '{' masked quirk keys off the file name (legacy Q_strrchr(name,'{')).
    const bool masked_name = name.find( '{' ) != std::string_view::npos;

    // Legacy chooses the variant by available size (base = sizeof(mip_t) = 40).
    const std::size_t need_hl = k_mip_header + mipchain + 2 + k_palette_rgb;
    const std::size_t need_q1 = k_mip_header + mipchain;

    Palette    pal;
    ImageFlags flags   = ImageFlags::None;
    Rgba       fog     {};
    bool       has_fog = false;

    if( file.size() >= need_hl )
    {
        // ---- Half-Life mip with embedded palette ---------------------------
        std::array<std::uint8_t, k_palette_rgb> raw_pal_store {};
        bool have_pal = false;

        const std::size_t count_off = px_off + mipchain;
        if( count_off <= file.size() && file.size() - count_off >= 2 + k_palette_rgb )
        {
            const auto numcolors = ::xash::utilities::read_le<std::uint16_t>( file.data() + count_off );
            if( numcolors == 256 )  // else corrupted lump -> fall back to built-in HL
            {
                const std::size_t rgb_off = count_off + 2;
                for( std::size_t i = 0; i < k_palette_rgb; ++i )
                    raw_pal_store[i] = byte_at( file, rgb_off + i );
                have_pal = true;
            }
        }
        const std::span<const std::uint8_t> raw_pal = raw_pal_store;

        const PaletteClass cls  = have_pal ? classify_palette( raw_pal ) : PaletteClass::Invalid;
        Rendermode         mode = Rendermode::Normal;

        if( masked_name )
        {
            // Masked unless decal-load mode AND the palette terminator isn't the
            // classic blue key (0,0,255) — legacy img_wad.c:466.
            const bool term_blue = have_pal && raw_pal[765] == 0 && raw_pal[766] == 0 && raw_pal[767] == 255;
            if( !k_load_decal || term_blue )
            {
                mode = Rendermode::Masked;
                flags |= ImageFlags::OneBitAlpha | ImageFlags::HasAlpha;
            }
            else
            {
                mode = Rendermode::Gradient;
                flags |= ImageFlags::ColorIndex | ImageFlags::HasAlpha;
            }
        }
        else if( cls == PaletteClass::Quake1 )
        {
            // A Quake texture converted to an HL mip with its palette left as Q1:
            // fullbright pixels become a luma layer; skip texgamma (over-darkens).
            if( c0 != '*' && c0 != '!' && detect_luma( indices, /*exclude_255*/ false ) )
                flags |= ImageFlags::HasLuma;
            flags |= ImageFlags::QuakePal;
            mode = Rendermode::Normal;
        }
        else
        {
            // Genuine HL (or custom) palette — apply texgamma.
            mode = Rendermode::TexGamma;
            if( k_allow_wad3_luma && have_pal
                && ( c0 == '~'
                     || ( c0 == '+' && is_ascii_digit( name_ch( ename, 1 ) ) && name_ch( ename, 2 ) == '~' ) ) )
                flags |= ImageFlags::HasLuma;
        }

        // Build the working palette. A NULL/corrupt embedded palette falls back
        // to the built-in HL table with no rendermode (legacy Image_GetPaletteLMP
        // (NULL,...) -> Image_GetPaletteHL, which uses LUMP_NORMAL).
        pal = have_pal ? build_palette( raw_pal, mode ) : halflife_palette();

        // HL mips: index 255 is the transparency key — force its alpha to 0
        // (legacy image.d_currentpal[255] &= 0xFFFFFF).
        {
            Rgba key = pal[255];
            key.a    = 0;
            pal[255] = key;
        }

        // Refuse luma if no dark palette entry exists to strip fullbright toward
        // (legacy Image_FindBestBlack == -1 clears IMAGE_HAS_LUMA).
        if( mode == Rendermode::TexGamma && has_flag( flags, ImageFlags::HasLuma )
            && find_best_black( pal ) == -1 )
            flags &= ~ImageFlags::HasLuma;

        // fog_params (legacy guards this on a non-NULL embedded palette).
        if( have_pal )
        {
            if( c0 == '!' || istarts_with( ename, "water" ) )
            {
                // HL water: fog colour = palette entry 3, density = entry 4's red.
                fog     = Rgba{ raw_pal[3 * 3 + 0], raw_pal[3 * 3 + 1], raw_pal[3 * 3 + 2], raw_pal[4 * 3 + 0] };
                has_fog = true;
            }
            else if( mode == Rendermode::Gradient )
            {
                // Gradient decal: reflectivity = index-255 colour + its average.
                const std::uint8_t dr = raw_pal[255 * 3 + 0], dg = raw_pal[255 * 3 + 1], db = raw_pal[255 * 3 + 2];
                fog     = Rgba{ dr, dg, db, static_cast<std::uint8_t>( ( static_cast<int>( dr ) + dg + db ) / 3 ) };
                has_fog = true;
            }
            else
            {
                // General texture reflectivity: average palette colour.
                std::uint32_t sr = 0, sg = 0, sb = 0;
                for( std::size_t i = 0; i < 256; ++i )
                {
                    sr += raw_pal[i * 3 + 0];
                    sg += raw_pal[i * 3 + 1];
                    sb += raw_pal[i * 3 + 2];
                }
                fog     = Rgba{ static_cast<std::uint8_t>( sr / 256 ), static_cast<std::uint8_t>( sg / 256 ),
                                static_cast<std::uint8_t>( sb / 256 ), 0 };
                has_fog = true;
            }
        }
    }
    else if( file.size() >= need_q1 )
    {
        // ---- Quake1 mip without palette (built-in Q1 table) ----------------
        pal = quake_palette();  // index 255 already fully transparent
        flags |= ImageFlags::QuakePal;

        // Luma: skip liquids ('*'/'!'); index 255 does not count as fullbright.
        if( c0 != '*' && c0 != '!' && detect_luma( indices, /*exclude_255*/ true ) )
            flags |= ImageFlags::HasLuma;

        // Arcane-Dimensions transparent textures: '{' + any index-255 pixel sets
        // HAS_ALPHA (legacy deliberately leaves ONEBIT_ALPHA clear here).
        if( masked_name )
        {
            for( const std::byte b : indices )
            {
                if( std::to_integer<std::uint8_t>( b ) == 255 )
                {
                    flags |= ImageFlags::HasAlpha;
                    break;
                }
            }
        }
        // pal == NULL in legacy -> no fog_params for Quake mips.
    }
    else
    {
        return std::unexpected( ImageError::UnknownFormat );
    }

    // Quake-sky: embedded name starts "sky" (case-sensitive) and is 2:1.
    if( ename.starts_with( "sky" ) && width == height * 2 )
        flags |= ImageFlags::QuakeSky;

    // HAS_COLOR mirrors Image_Copy8bitRGBA's palette scan.
    if( palette_has_color( pal ) )
        flags |= ImageFlags::HasColor;

    std::vector<std::byte> rgba( pixels * 4 );
    expand_indexed_rgba( indices, pal, rgba );

    Image img( static_cast<std::uint16_t>( width ), static_cast<std::uint16_t>( height ),
               PixelFormat::Rgba8, std::move( rgba ), flags );
    if( has_fog )
        img.set_fog_params( fog );
    return img;
}

// ---- the registered codec -------------------------------------------------
class MipCodec final : public IImageCodec
{
public:
    [[nodiscard]] bool handles( std::string_view ext ) const noexcept override { return ext == "mip"; }
    [[nodiscard]] Result<Image> decode( std::string_view name, std::span<const std::byte> file ) const override
    {
        return decode_mip( name, file );
    }
};

} // namespace

const IImageCodec &mip_codec() noexcept
{
    static const MipCodec codec;
    return codec;
}

} // namespace xash::imagelib
