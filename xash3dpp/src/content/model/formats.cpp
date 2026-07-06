// xash3dpp — sprite + alias model header parsers
// Legacy reference: engine/common/mod_sprite.c (Mod_LoadSpriteModel /
//   Mod_SwapSprite), engine/common/mod_alias.c (Mod_LoadAliasModel).
//
// On-disk sprite headers differ by version (dsprite_q1_t 36 bytes vs
// dsprite_hl_t 40 bytes, engine/sprite.h) so the fields are read into a
// normalised SpriteInfo rather than a fixed-offset view. Parsed with
// bounds-checked read_le (H-3/H-4).

#include <xash3dpp/content/formats.hpp>

#include <xash3dpp/utilities/swap.hpp>

#include <bit>
#include <utility>

namespace xash::content {

Result<SpriteModel> parse_sprite( std::span<const std::byte> file )
{
    namespace u = ::xash::utilities;

    if( file.size() < 8 )
        return std::unexpected( LoadError::Truncated );
    if( u::read_le<std::int32_t>( file.data() ) != k_sprite_ident )
        return std::unexpected( LoadError::BadMagic );

    const auto version = u::read_le<std::int32_t>( file.data() + 4 );

    auto i32 = [&]( std::size_t off ) -> std::int32_t {
        return ( off + 4 <= file.size() ) ? u::read_le<std::int32_t>( file.data() + off ) : 0;
    };
    auto f32 = [&]( std::size_t off ) -> float {
        return ( off + 4 <= file.size() )
                   ? std::bit_cast<float>( u::read_le<std::uint32_t>( file.data() + off ) )
                   : 0.0f;
    };

    SpriteInfo info;
    info.version = version;

    if( version == 1 )
    {
        // dsprite_q1_t: type, boundingradius(float), bounds[2], numframes, ...
        if( file.size() < 36 )
            return std::unexpected( LoadError::Truncated );
        info.type            = i32( 8 );
        info.tex_format      = 0;
        info.bounding_radius = f32( 12 );
        info.max_width       = i32( 16 );
        info.max_height      = i32( 20 );
        info.num_frames      = i32( 24 );
    }
    else if( version == 2 || version == 32 )
    {
        // dsprite_hl_t: type, texFormat, boundingradius(int), bounds[2], numframes, ...
        if( file.size() < 40 )
            return std::unexpected( LoadError::Truncated );
        info.type            = i32( 8 );
        info.tex_format      = i32( 12 );
        info.bounding_radius = static_cast<float>( i32( 16 ) );
        info.max_width       = i32( 20 );
        info.max_height      = i32( 24 );
        info.num_frames      = i32( 28 );
    }
    else
    {
        return std::unexpected( LoadError::BadVersion );
    }

    std::vector<std::byte> data( file.begin(), file.end() );
    return SpriteModel{ info, std::move( data ) };
}

Result<AliasModel> parse_alias( std::span<const std::byte> file )
{
    namespace u = ::xash::utilities;

    if( file.size() < 8 )
        return std::unexpected( LoadError::Truncated );
    if( u::read_le<std::int32_t>( file.data() ) != k_alias_ident )
        return std::unexpected( LoadError::BadMagic );
    if( u::read_le<std::int32_t>( file.data() + 4 ) != k_alias_version )
        return std::unexpected( LoadError::BadVersion );

    std::vector<std::byte> data( file.begin(), file.end() );
    return AliasModel{ std::move( data ) };
}

} // namespace xash::content
