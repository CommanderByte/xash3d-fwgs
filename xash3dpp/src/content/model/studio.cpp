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
constexpr std::size_t kOffNumBoneCtrl  = 148;
constexpr std::size_t kOffBoneCtrlIndex = 152;
constexpr std::size_t kOffNumHitbox  = 156;
constexpr std::size_t kOffHitboxIndex = 160;
constexpr std::size_t kOffNumSeq     = 164;
constexpr std::size_t kOffSeqIndex   = 168;
constexpr std::size_t kOffNumSeqGrp  = 172;
constexpr std::size_t kOffNumTextures = 180;
constexpr std::size_t kOffNumBodyparts = 204;
constexpr std::size_t kOffNumAttach  = 212;
constexpr std::size_t kOffAttachIndex = 216;

// mstudiobone_t field offsets (within a 112-byte bone chunk).
constexpr std::size_t kBoneParent = 32;
constexpr std::size_t kBoneCtrl   = 40;  // int32 bonecontroller[6]
constexpr std::size_t kBoneValue  = 64;  // vec_t value[6]
constexpr std::size_t kBoneScale  = 88;  // vec_t scale[6]

// mstudiobonecontroller_t field offsets (within a 24-byte chunk).
constexpr std::size_t kBcType  = 4;
constexpr std::size_t kBcStart = 8;
constexpr std::size_t kBcEnd   = 12;
constexpr std::size_t kBcIndex = 20;

// mstudioseqdesc_t field offsets (within a 176-byte chunk).
constexpr std::size_t kSeqNumFrames  = 56;
constexpr std::size_t kSeqMotionType = 68;
constexpr std::size_t kSeqMotionBone = 72;
constexpr std::size_t kSeqNumBlends  = 120;
constexpr std::size_t kSeqAnimIndex  = 124;
constexpr std::size_t kSeqSeqGroup   = 156;

// mstudioattachment_t field offsets (within an 88-byte chunk).
constexpr std::size_t kAttBone = 36;
constexpr std::size_t kAttOrg  = 40; // vec3

// mstudiobbox_t field offsets (within a 32-byte chunk).
constexpr std::size_t kHbBone  = 0;
constexpr std::size_t kHbGroup = 4;
constexpr std::size_t kHbMin   = 8;  // vec3
constexpr std::size_t kHbMax   = 20; // vec3

// Bounds-checked little-endian reads over the studiohdr byte image; an
// out-of-range offset returns 0 (untrusted files never fault a reader).
[[nodiscard]] std::int32_t rd_i32( std::span<const std::byte> d, std::size_t off ) noexcept
{
    if( off + sizeof( std::int32_t ) > d.size() ) return 0;
    return ::xash::utilities::read_le<std::int32_t>( d.data() + off );
}
[[nodiscard]] std::uint16_t rd_u16( std::span<const std::byte> d, std::size_t off ) noexcept
{
    if( off + sizeof( std::uint16_t ) > d.size() ) return 0;
    return ::xash::utilities::read_le<std::uint16_t>( d.data() + off );
}
[[nodiscard]] std::int16_t rd_i16( std::span<const std::byte> d, std::size_t off ) noexcept
{
    if( off + sizeof( std::int16_t ) > d.size() ) return 0;
    return ::xash::utilities::read_le<std::int16_t>( d.data() + off );
}
[[nodiscard]] std::uint8_t rd_u8( std::span<const std::byte> d, std::size_t off ) noexcept
{
    if( off + sizeof( std::uint8_t ) > d.size() ) return 0;
    return static_cast<std::uint8_t>( d[off] );
}
[[nodiscard]] float rd_f32( std::span<const std::byte> d, std::size_t off ) noexcept
{
    if( off + sizeof( std::uint32_t ) > d.size() ) return 0.0f;
    return std::bit_cast<float>( ::xash::utilities::read_le<std::uint32_t>( d.data() + off ) );
}

} // namespace

// ---------------------------------------------------------------------------
// Sub-views (typed offset cursors — the G-2 door)
// ---------------------------------------------------------------------------

std::int32_t BoneView::parent() const noexcept              { return rd_i32( data_, off_ + kBoneParent ); }
std::int32_t BoneView::bonecontroller( int c ) const noexcept { return rd_i32( data_, off_ + kBoneCtrl + 4 * static_cast<std::size_t>( c ) ); }
float        BoneView::value( int c ) const noexcept        { return rd_f32( data_, off_ + kBoneValue + 4 * static_cast<std::size_t>( c ) ); }
float        BoneView::scale( int c ) const noexcept        { return rd_f32( data_, off_ + kBoneScale + 4 * static_cast<std::size_t>( c ) ); }

std::uint8_t AnimValueCursor::valid() const noexcept        { return rd_u8( data_, off_ ); }
std::uint8_t AnimValueCursor::total() const noexcept        { return rd_u8( data_, off_ + 1 ); }
std::int16_t AnimValueCursor::value( std::size_t word ) const noexcept { return rd_i16( data_, off_ + 2 * word ); }

std::uint16_t AnimView::channel_offset( int c ) const noexcept { return rd_u16( data_, off_ + 2 * static_cast<std::size_t>( c ) ); }

std::int32_t BoneControllerView::type() const noexcept  { return rd_i32( data_, off_ + kBcType ); }
std::int32_t BoneControllerView::index() const noexcept { return rd_i32( data_, off_ + kBcIndex ); }
float        BoneControllerView::start() const noexcept { return rd_f32( data_, off_ + kBcStart ); } // compliance-allow(thread-assert): read-only value query over caller-owned bytes, not a mutator
float        BoneControllerView::end() const noexcept   { return rd_f32( data_, off_ + kBcEnd ); }

std::int32_t SeqDescView::numframes() const noexcept  { return rd_i32( data_, off_ + kSeqNumFrames ); }
std::int32_t SeqDescView::motiontype() const noexcept { return rd_i32( data_, off_ + kSeqMotionType ); }
std::int32_t SeqDescView::motionbone() const noexcept { return rd_i32( data_, off_ + kSeqMotionBone ); }
std::int32_t SeqDescView::numblends() const noexcept  { return rd_i32( data_, off_ + kSeqNumBlends ); }
std::int32_t SeqDescView::animindex() const noexcept  { return rd_i32( data_, off_ + kSeqAnimIndex ); }
std::int32_t SeqDescView::seqgroup() const noexcept   { return rd_i32( data_, off_ + kSeqSeqGroup ); }

std::int32_t AttachmentView::bone() const noexcept { return rd_i32( data_, off_ + kAttBone ); }
::xash::utilities::Vec3 AttachmentView::org() const noexcept
{
    return { rd_f32( data_, off_ + kAttOrg ), rd_f32( data_, off_ + kAttOrg + 4 ), rd_f32( data_, off_ + kAttOrg + 8 ) };
}

std::int32_t HitboxView::bone() const noexcept  { return rd_i32( data_, off_ + kHbBone ); }
std::int32_t HitboxView::group() const noexcept { return rd_i32( data_, off_ + kHbGroup ); }
::xash::utilities::Vec3 HitboxView::bbmin() const noexcept
{
    return { rd_f32( data_, off_ + kHbMin ), rd_f32( data_, off_ + kHbMin + 4 ), rd_f32( data_, off_ + kHbMin + 8 ) };
}
::xash::utilities::Vec3 HitboxView::bbmax() const noexcept
{
    return { rd_f32( data_, off_ + kHbMax ), rd_f32( data_, off_ + kHbMax + 4 ), rd_f32( data_, off_ + kHbMax + 8 ) };
}

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
std::int32_t StudioView::num_bonecontrollers() const noexcept  { return i32( kOffNumBoneCtrl ); }
std::int32_t StudioView::bonecontroller_index() const noexcept { return i32( kOffBoneCtrlIndex ); }
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

BoneView StudioView::bone( int i ) const noexcept
{
    return BoneView{ data_, static_cast<std::size_t>( bone_index() )
        + k_studio_bone_stride * static_cast<std::size_t>( i ) };
}

BoneControllerView StudioView::bonecontroller( int j ) const noexcept
{
    return BoneControllerView{ data_, static_cast<std::size_t>( bonecontroller_index() )
        + k_studio_bonectrl_stride * static_cast<std::size_t>( j ) };
}

SeqDescView StudioView::seqdesc( int i ) const noexcept
{
    return SeqDescView{ data_, static_cast<std::size_t>( seq_index() )
        + k_studio_seqdesc_stride * static_cast<std::size_t>( i ) };
}

AttachmentView StudioView::attachment( int i ) const noexcept
{
    return AttachmentView{ data_, static_cast<std::size_t>( attachment_index() )
        + k_studio_attachment_stride * static_cast<std::size_t>( i ) };
}

HitboxView StudioView::hitbox( int i ) const noexcept
{
    return HitboxView{ data_, static_cast<std::size_t>( hitbox_index() )
        + k_studio_hitbox_stride * static_cast<std::size_t>( i ) };
}

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
