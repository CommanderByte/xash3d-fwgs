// xash3dpp — studio model header parse + typed view
// Legacy reference: engine/studio.h (studiohdr_t), engine/common/mod_studio.c
//   (Mod_LoadStudioModel -> R_StudioLoadHeader -> Mod_SwapStudioModel).
//
// The studiohdr_t is a flat header of int32 / vec3 fields; StudioView reads them
// by frozen offset through bounds-checked read_le (H-3/H-4). Field offsets are
// the ABI layout and must not move (studio.h).

#include <xash3dpp/content/studio.hpp>

#include <xash3dpp/utilities/swap.hpp>

#include <bit>
#include <cstring>
#include <utility>

namespace xash::content {

namespace {

// studiohdr_t field byte offsets (engine/studio.h layout, little-endian).
constexpr std::size_t kOffIdent      = 0;
constexpr std::size_t kOffVersion    = 4;
constexpr std::size_t kOffName       = 8;    // char[64]
constexpr std::size_t kOffLength     = 72;
constexpr std::size_t kOffEye        = 76;   // vec3
constexpr std::size_t kOffHullMin    = 88;   // vec3 (min)
constexpr std::size_t kOffHullMax    = 100;  // vec3 (max)
constexpr std::size_t kOffClipMin    = 112;  // vec3 (bbmin)
constexpr std::size_t kOffClipMax    = 124;  // vec3 (bbmax)
constexpr std::size_t kOffFlags      = 136;
constexpr std::size_t kOffNumBones   = 140;
constexpr std::size_t kOffBoneIndex  = 144;
constexpr std::size_t kOffNumHitbox  = 156;
constexpr std::size_t kOffHitboxIndex = 160;
constexpr std::size_t kOffNumSeq     = 164;
constexpr std::size_t kOffSeqIndex   = 168;
constexpr std::size_t kOffNumSeqGrp  = 172;
constexpr std::size_t kOffNumTextures = 180;
constexpr std::size_t kOffNumBodyparts = 204;
constexpr std::size_t kOffNumAttach  = 212;
constexpr std::size_t kOffAttachIndex = 216;

} // namespace

// ---------------------------------------------------------------------------
// StudioView
// ---------------------------------------------------------------------------

std::int32_t StudioView::i32( std::size_t off ) const noexcept
{
    if( off + sizeof( std::int32_t ) > data_.size() )
        return 0;
    return ::xash::utilities::read_le<std::int32_t>( data_.data() + off );
}

::xash::utilities::Vec3 StudioView::vec3( std::size_t off ) const noexcept
{
    auto f = [&]( std::size_t o ) -> float {
        if( o + sizeof( std::uint32_t ) > data_.size() )
            return 0.0f;
        return std::bit_cast<float>( ::xash::utilities::read_le<std::uint32_t>( data_.data() + o ) );
    };
    return ::xash::utilities::Vec3{ f( off ), f( off + 4 ), f( off + 8 ) };
}

std::string_view StudioView::name() const noexcept
{
    if( data_.size() < kOffName + 64 )
        return {};
    const char *p = reinterpret_cast<const char *>( data_.data() + kOffName );
    std::size_t n = 0;
    while( n < 64 && p[n] != '\0' )
        ++n;
    return std::string_view{ p, n };
}

std::int32_t StudioView::length() const noexcept           { return i32( kOffLength ); }
std::int32_t StudioView::flags() const noexcept            { return i32( kOffFlags ); }
std::int32_t StudioView::num_bones() const noexcept        { return i32( kOffNumBones ); }
std::int32_t StudioView::bone_index() const noexcept       { return i32( kOffBoneIndex ); }
std::int32_t StudioView::num_hitboxes() const noexcept     { return i32( kOffNumHitbox ); }
std::int32_t StudioView::hitbox_index() const noexcept     { return i32( kOffHitboxIndex ); }
std::int32_t StudioView::num_seq() const noexcept          { return i32( kOffNumSeq ); }
std::int32_t StudioView::seq_index() const noexcept        { return i32( kOffSeqIndex ); }
std::int32_t StudioView::num_seqgroups() const noexcept    { return i32( kOffNumSeqGrp ); }
std::int32_t StudioView::num_textures() const noexcept     { return i32( kOffNumTextures ); }
std::int32_t StudioView::num_bodyparts() const noexcept    { return i32( kOffNumBodyparts ); }
std::int32_t StudioView::num_attachments() const noexcept  { return i32( kOffNumAttach ); }
std::int32_t StudioView::attachment_index() const noexcept { return i32( kOffAttachIndex ); }

::xash::utilities::Vec3 StudioView::eye_position() const noexcept { return vec3( kOffEye ); }
::xash::utilities::Vec3 StudioView::hull_min() const noexcept    { return vec3( kOffHullMin ); }
::xash::utilities::Vec3 StudioView::hull_max() const noexcept    { return vec3( kOffHullMax ); }
::xash::utilities::Vec3 StudioView::clip_min() const noexcept    { return vec3( kOffClipMin ); }
::xash::utilities::Vec3 StudioView::clip_max() const noexcept    { return vec3( kOffClipMax ); }

// ---------------------------------------------------------------------------
// parse_studio
// ---------------------------------------------------------------------------

Result<StudioModel> parse_studio( std::span<const std::byte> file )
{
    if( file.size() < k_studio_header_size )
        return std::unexpected( LoadError::Truncated );

    const auto ident = ::xash::utilities::read_le<std::int32_t>( file.data() + kOffIdent );
    if( ident != k_studio_ident )
        return std::unexpected( LoadError::BadMagic );

    const auto version = ::xash::utilities::read_le<std::int32_t>( file.data() + kOffVersion );
    if( version != k_studio_version )
        return std::unexpected( LoadError::BadVersion );

    // Copy `length` bytes (legacy stores exactly that), clamped to the file.
    const auto length = ::xash::utilities::read_le<std::int32_t>( file.data() + kOffLength );
    std::size_t take = file.size();
    if( length >= static_cast<std::int32_t>( k_studio_header_size )
        && static_cast<std::size_t>( length ) <= file.size() )
        take = static_cast<std::size_t>( length );

    std::vector<std::byte> data( file.begin(), file.begin() + static_cast<std::ptrdiff_t>( take ) );
    return StudioModel{ std::move( data ) };
}

} // namespace xash::content
