#pragma once
// xash3dpp — studio model header parse + typed view (O-4)
// Legacy reference: engine/studio.h (studiohdr_t, frozen ABI), mod_studio.c
//   (Mod_LoadStudioModel / R_StudioLoadHeader header validation).
//
// The frozen studiohdr_t byte image is what the game DLL walks via
// pfnGetModelPtr (raw void*). StudioView is the server/engine-side typed read
// surface over that same image — the EntityView-over-entvars_t analog (P-4);
// raw offset access stays confined here (G-2 door).
//
// @thread-safety: StudioView is a non-owning value over caller-owned bytes;
// StudioModel owns its bytes. Both are plain values — safe to move.

#include <xash3dpp/content/errors.hpp>
#include <xash3dpp/utilities/math.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace xash::content {

// "IDST" little-endian; studio format version 10 (STUDIO_VERSION).
inline constexpr std::int32_t k_studio_ident   = ( 'T' << 24 ) | ( 'S' << 16 ) | ( 'D' << 8 ) | 'I';
inline constexpr std::int32_t k_studio_version = 10;
// Size of the frozen studiohdr_t (through transitionindex).
inline constexpr std::size_t  k_studio_header_size = 244;

// Byte strides of the studiohdr sub-chunks (engine/studio.h, frozen ABI).
inline constexpr std::size_t k_studio_bone_stride       = 112; // mstudiobone_t
inline constexpr std::size_t k_studio_bonectrl_stride   = 24;  // mstudiobonecontroller_t
inline constexpr std::size_t k_studio_anim_stride       = 12;  // mstudioanim_t (uint16 offset[6])
inline constexpr std::size_t k_studio_seqdesc_stride    = 176; // mstudioseqdesc_t

// mstudiobonecontroller_t motion-type flags (engine/studio.h). The low bits are
// the axis/rotation type (masked by k_studio_types); k_studio_rloop marks a
// 0..360 wrapping controller. k_studio_mouth is the reserved mouth slot index.
inline constexpr std::int32_t k_studio_x     = 0x0001;
inline constexpr std::int32_t k_studio_y     = 0x0002;
inline constexpr std::int32_t k_studio_z     = 0x0004;
inline constexpr std::int32_t k_studio_xr    = 0x0008;
inline constexpr std::int32_t k_studio_yr    = 0x0010;
inline constexpr std::int32_t k_studio_zr    = 0x0020;
inline constexpr std::int32_t k_studio_types = 0x7FFF;
inline constexpr std::int32_t k_studio_rloop = 0x8000;
inline constexpr int          k_studio_mouth = 4;

// ---------------------------------------------------------------------------
// Typed offset sub-views over the studiohdr byte image (the G-2 door — raw
// offset access stays confined to these read surfaces). Each is a non-owning
// {span, base-offset} cursor; out-of-range reads return 0 (untrusted files).
// ---------------------------------------------------------------------------

// mstudiobone_t (112 B): parent@32, bonecontroller[6]@40, value[6]@64,
// scale[6]@88. value[0..2] = position, value[3..5] = rotation (radians);
// scale[] = per-channel RLE decompression scale; bonecontroller[i] = the
// controller slot driving channel i, or -1.
class BoneView
{
public:
    BoneView() = default;
    BoneView( std::span<const std::byte> data, std::size_t off ) noexcept : data_( data ), off_( off ) {}

    [[nodiscard]] std::int32_t parent() const noexcept;
    [[nodiscard]] std::int32_t bonecontroller( int channel ) const noexcept; // channel 0..5
    [[nodiscard]] float        value( int channel ) const noexcept;
    [[nodiscard]] float        scale( int channel ) const noexcept;

private:
    std::span<const std::byte> data_;
    std::size_t                off_ = 0;
};

// mstudioanimvalue_t (2 B union {num{valid,total}, int16 value}). A cursor into
// an RLE animation stream; reads are word-indexed (2 bytes) from `off`.
class AnimValueCursor
{
public:
    AnimValueCursor() = default;
    AnimValueCursor( std::span<const std::byte> data, std::size_t off ) noexcept : data_( data ), off_( off ) {}

    [[nodiscard]] std::uint8_t valid() const noexcept;                 // num.valid (low byte)
    [[nodiscard]] std::uint8_t total() const noexcept;                 // num.total (high byte)
    [[nodiscard]] std::int16_t value( std::size_t word ) const noexcept; // int16 at off + 2*word
    [[nodiscard]] AnimValueCursor advance( std::size_t words ) const noexcept
    {
        return { data_, off_ + 2 * words };
    }

private:
    std::span<const std::byte> data_;
    std::size_t                off_ = 0;
};

// mstudioanim_t (12 B): uint16 offset[6] — byte offsets from the mstudioanim_t
// to each channel's RLE stream (0 = channel absent, use the bone default).
class AnimView
{
public:
    AnimView() = default;
    AnimView( std::span<const std::byte> data, std::size_t off ) noexcept : data_( data ), off_( off ) {}

    [[nodiscard]] std::uint16_t channel_offset( int channel ) const noexcept; // offset[channel]
    [[nodiscard]] bool          has_channel( int channel ) const noexcept { return channel_offset( channel ) != 0; }
    [[nodiscard]] AnimValueCursor channel( int channel ) const noexcept
    {
        return { data_, off_ + channel_offset( channel ) };
    }

private:
    std::span<const std::byte> data_;
    std::size_t                off_ = 0;
};

// mstudiobonecontroller_t (24 B): bone@0, type@4, start@8, end@12, index@20.
class BoneControllerView
{
public:
    BoneControllerView() = default;
    BoneControllerView( std::span<const std::byte> data, std::size_t off ) noexcept : data_( data ), off_( off ) {}

    [[nodiscard]] std::int32_t type() const noexcept;
    [[nodiscard]] std::int32_t index() const noexcept;
    [[nodiscard]] float        start() const noexcept;
    [[nodiscard]] float        end() const noexcept;

private:
    std::span<const std::byte> data_;
    std::size_t                off_ = 0;
};

// mstudioseqdesc_t (176 B): the bone solver reads numframes@56, motiontype@68,
// motionbone@72, numblends@120, animindex@124, seqgroup@156. animindex locates
// the mstudioanim_t array (numblends * numbones anims) for the sequence.
class SeqDescView
{
public:
    SeqDescView() = default;
    SeqDescView( std::span<const std::byte> data, std::size_t off ) noexcept : data_( data ), off_( off ) {}

    [[nodiscard]] std::int32_t numframes() const noexcept;
    [[nodiscard]] std::int32_t motiontype() const noexcept;
    [[nodiscard]] std::int32_t motionbone() const noexcept;
    [[nodiscard]] std::int32_t numblends() const noexcept;
    [[nodiscard]] std::int32_t animindex() const noexcept;
    [[nodiscard]] std::int32_t seqgroup() const noexcept;

private:
    std::span<const std::byte> data_;
    std::size_t                off_ = 0;
};

// ---------------------------------------------------------------------------
// StudioView — typed, bounds-safe read surface over a studiohdr_t byte image.
// Out-of-range reads return 0 / empty rather than faulting (untrusted files).
// ---------------------------------------------------------------------------

class StudioView
{
public:
    StudioView() = default;
    explicit StudioView( std::span<const std::byte> data ) noexcept : data_( data ) {}

    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] std::int32_t     length() const noexcept;
    [[nodiscard]] std::int32_t     flags() const noexcept;

    [[nodiscard]] std::int32_t num_bones() const noexcept;
    [[nodiscard]] std::int32_t bone_index() const noexcept;
    [[nodiscard]] std::int32_t num_bonecontrollers() const noexcept;
    [[nodiscard]] std::int32_t bonecontroller_index() const noexcept;
    [[nodiscard]] std::int32_t num_hitboxes() const noexcept;
    [[nodiscard]] std::int32_t hitbox_index() const noexcept;
    [[nodiscard]] std::int32_t num_seq() const noexcept;
    [[nodiscard]] std::int32_t seq_index() const noexcept;
    [[nodiscard]] std::int32_t num_seqgroups() const noexcept;
    [[nodiscard]] std::int32_t num_textures() const noexcept;
    [[nodiscard]] std::int32_t num_bodyparts() const noexcept;
    [[nodiscard]] std::int32_t num_attachments() const noexcept;
    [[nodiscard]] std::int32_t attachment_index() const noexcept;

    [[nodiscard]] ::xash::utilities::Vec3 eye_position() const noexcept;
    [[nodiscard]] ::xash::utilities::Vec3 hull_min() const noexcept;   // movement hull
    [[nodiscard]] ::xash::utilities::Vec3 hull_max() const noexcept;
    [[nodiscard]] ::xash::utilities::Vec3 clip_min() const noexcept;   // clipping bbox
    [[nodiscard]] ::xash::utilities::Vec3 clip_max() const noexcept;

    // Sub-view accessors — index into the bone / bone-controller arrays. The
    // caller bounds the index against num_bones() / num_bonecontrollers(); an
    // out-of-range view simply reads zeros.
    [[nodiscard]] BoneView           bone( int i ) const noexcept;
    [[nodiscard]] BoneControllerView bonecontroller( int j ) const noexcept;
    [[nodiscard]] SeqDescView        seqdesc( int i ) const noexcept;

    [[nodiscard]] std::span<const std::byte> data() const noexcept { return data_; }
    [[nodiscard]] bool valid() const noexcept { return data_.size() >= k_studio_header_size; }

private:
    [[nodiscard]] std::int32_t    i32( std::size_t off ) const noexcept;
    [[nodiscard]] ::xash::utilities::Vec3 vec3( std::size_t off ) const noexcept;

    std::span<const std::byte> data_;
};

// ---------------------------------------------------------------------------
// StudioModel — owns a studiohdr byte image; view() interprets it.
// ---------------------------------------------------------------------------

class StudioModel
{
public:
    StudioModel() = default;
    explicit StudioModel( std::vector<std::byte> data ) noexcept : data_( std::move( data ) ) {}

    [[nodiscard]] StudioView                 view() const noexcept { return StudioView{ data_ }; }
    [[nodiscard]] std::span<const std::byte>  bytes() const noexcept { return data_; }
    [[nodiscard]] bool                        empty() const noexcept { return data_.empty(); }

private:
    std::vector<std::byte> data_;   // @pre-reserved: studiohdr length (assign at parse; cold path)
};

// Parse + validate a studio (.mdl) file header (legacy Mod_LoadStudioModel).
// Copies `length` bytes (clamped to the file) into the returned StudioModel.
[[nodiscard]] Result<StudioModel> parse_studio( std::span<const std::byte> file );

} // namespace xash::content
