#pragma once
// xash3dpp — synthetic studiohdr byte-image builder for content tests.
//
// Assembles a valid studiohdr_t (plus bone / bone-controller / anim chunks) in
// memory, following the tests/README "no committed binary fixtures" rule — every
// byte is synthesized here. Offsets match engine/studio.h (see studio.cpp). The
// builder appends chunks past the 244-byte header and patches the count/index
// fields, returning each chunk's byte offset so tests can construct the typed
// sub-views (BoneView / AnimView / ...).

#include <xash3dpp/content/studio.hpp>
#include <xash3dpp/utilities/swap.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace xash::content::test {

// Encode a studio anim RLE span header word: low byte = valid, high byte = total.
[[nodiscard]] inline std::int16_t anim_num( std::uint8_t valid, std::uint8_t total ) noexcept
{
    return static_cast<std::int16_t>( static_cast<std::uint16_t>( valid )
        | ( static_cast<std::uint16_t>( total ) << 8 ) );
}

class StudioBuilder
{
public:
    StudioBuilder()
    {
        buf_.resize( k_studio_header_size, std::byte{ 0 } );
        put_i32( 0, k_studio_ident );
        put_i32( 4, k_studio_version );
    }

    // --- header field setters (by studiohdr byte offset) -------------------
    StudioBuilder &header_i32( std::size_t off, std::int32_t v ) { put_i32( off, v ); return *this; }

    // --- chunk appenders (return the chunk's byte offset) ------------------

    // mstudiobone_t (112 B).
    std::size_t add_bone( std::int32_t parent, const std::array<std::int32_t, 6> &ctrl,
                          const std::array<float, 6> &value, const std::array<float, 6> &scale )
    {
        const std::size_t off = grow( k_studio_bone_stride );
        put_i32( off + 32, parent );
        for( int i = 0; i < 6; ++i ) put_i32( off + 40 + 4 * i, ctrl[static_cast<std::size_t>( i )] );
        for( int i = 0; i < 6; ++i ) put_f32( off + 64 + 4 * i, value[static_cast<std::size_t>( i )] );
        for( int i = 0; i < 6; ++i ) put_f32( off + 88 + 4 * i, scale[static_cast<std::size_t>( i )] );
        return off;
    }

    // mstudiobonecontroller_t (24 B).
    std::size_t add_bonecontroller( std::int32_t bone, std::int32_t type, float start, float end, std::int32_t index )
    {
        const std::size_t off = grow( k_studio_bonectrl_stride );
        put_i32( off + 0, bone );
        put_i32( off + 4, type );
        put_f32( off + 8, start );
        put_f32( off + 12, end );
        put_i32( off + 20, index );
        return off;
    }

    // mstudioanim_t (12 B offset[6]) + the channel RLE streams. channels[i] is
    // the list of int16 animvalue words for channel i (empty => offset 0 =>
    // the bone default). Streams are laid out contiguously after the header.
    std::size_t add_anim( const std::array<std::vector<std::int16_t>, 6> &channels )
    {
        const std::size_t base = grow( k_studio_anim_stride );
        for( int i = 0; i < 6; ++i )
        {
            const auto &ch = channels[static_cast<std::size_t>( i )];
            if( ch.empty() )
            {
                put_u16( base + 2 * static_cast<std::size_t>( i ), 0 );
                continue;
            }
            const std::size_t stream = grow( ch.size() * 2 );
            put_u16( base + 2 * static_cast<std::size_t>( i ),
                     static_cast<std::uint16_t>( stream - base ) );
            for( std::size_t k = 0; k < ch.size(); ++k )
                put_i16( stream + 2 * k, ch[k] );
        }
        return base;
    }

    // Append a contiguous block of mstudioanim_t (one per entry — the real .mdl
    // layout is `numblends * numbones` anims back-to-back), then their RLE
    // streams. Returns the block base offset (= a sequence's animindex). Each
    // entry's channel offsets are relative to that entry's own 12-byte header.
    std::size_t add_anim_block( const std::vector<std::array<std::vector<std::int16_t>, 6>> &anims )
    {
        const std::size_t base = grow( anims.size() * k_studio_anim_stride );
        for( std::size_t a = 0; a < anims.size(); ++a )
        {
            const std::size_t hdr = base + a * k_studio_anim_stride;
            for( int i = 0; i < 6; ++i )
            {
                const auto &ch = anims[a][static_cast<std::size_t>( i )];
                if( ch.empty() )
                {
                    put_u16( hdr + 2 * static_cast<std::size_t>( i ), 0 );
                    continue;
                }
                const std::size_t stream = grow( ch.size() * 2 );
                put_u16( hdr + 2 * static_cast<std::size_t>( i ),
                         static_cast<std::uint16_t>( stream - hdr ) );
                for( std::size_t k = 0; k < ch.size(); ++k )
                    put_i16( stream + 2 * k, ch[k] );
            }
        }
        return base;
    }

    // mstudioseqdesc_t (176 B) — only the fields the bone solver reads.
    std::size_t add_seqdesc( std::int32_t numframes, std::int32_t motiontype, std::int32_t motionbone,
                             std::int32_t numblends, std::int32_t animindex, std::int32_t seqgroup )
    {
        const std::size_t off = grow( k_studio_seqdesc_stride );
        put_i32( off + 56, numframes );
        put_i32( off + 68, motiontype );
        put_i32( off + 72, motionbone );
        put_i32( off + 120, numblends );
        put_i32( off + 124, animindex );
        put_i32( off + 156, seqgroup );
        return off;
    }

    // Stamp the header length to the final size and return the bytes.
    [[nodiscard]] const std::vector<std::byte> &bytes()
    {
        put_i32( 72, static_cast<std::int32_t>( buf_.size() ) );
        return buf_;
    }

private:
    std::size_t grow( std::size_t n )
    {
        const std::size_t off = buf_.size();
        buf_.resize( off + n, std::byte{ 0 } );
        return off;
    }
    void put_i32( std::size_t off, std::int32_t v ) { ::xash::utilities::write_le<std::int32_t>( buf_.data() + off, v ); }
    void put_u16( std::size_t off, std::uint16_t v ) { ::xash::utilities::write_le<std::uint16_t>( buf_.data() + off, v ); }
    void put_i16( std::size_t off, std::int16_t v ) { ::xash::utilities::write_le<std::int16_t>( buf_.data() + off, v ); }
    void put_f32( std::size_t off, float v ) { ::xash::utilities::write_le<std::uint32_t>( buf_.data() + off, std::bit_cast<std::uint32_t>( v ) ); }

    std::vector<std::byte> buf_;
};

} // namespace xash::content::test
