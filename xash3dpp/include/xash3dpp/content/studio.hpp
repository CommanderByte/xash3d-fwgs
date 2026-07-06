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
