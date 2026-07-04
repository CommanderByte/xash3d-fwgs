#pragma once
// xash3dpp tests — in-memory synthetic BSP builder.
// Follows the tests/filesystem write_pak/write_zip/write_wad precedent: no
// committed binary fixtures; every test synthesizes the byte image it needs.
// Layout produced: dheader_t [+ dextrahdr_t when BSP30ext] + lump payloads
// in index order, each 4-byte aligned (matching real compiler output).
// Unset lumps get fileofs = 0 (the legacy "unused lump" marker).

#include <xash3dpp/private/map_loader/bsp/disk_format.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace test_bsp {

namespace bsp = ::xash::map_loader::bsp;

class TestBspBuilder
{
public:
    explicit TestBspBuilder( std::int32_t version = bsp::k_hlbsp_version )
        : version_( version )
    {
    }

    void set_lump_bytes( int lump, const void *data, std::size_t len )
    {
        auto &slot = lumps_[static_cast<std::size_t>( lump )];
        slot.emplace( len );
        if ( len > 0 )
            std::memcpy( slot->data(), data, len );
    }

    template <typename T>
    void set_lump_records( int lump, const std::vector<T> &records )
    {
        static_assert( std::is_trivially_copyable_v<T> );
        set_lump_bytes( lump, records.data(), records.size() * sizeof( T ));
    }

    void set_entities( std::string_view text )
    {
        set_lump_bytes( bsp::k_lump_entities, text.data(), text.size() );
    }

    // Appends a dextrahdr_t after the standard header (BSP30ext).  The id
    // and version are overridable so tests can pin the legacy nuance that
    // only the ID gates the extended-clipnode guess.
    void enable_bsp30ext( std::int32_t id = bsp::k_extra_header_id,
                          std::int32_t version = bsp::k_extra_version )
    {
        bsp30ext_ = true;
        ext_id_   = id;
        ext_ver_  = version;
    }

    // Swaps the ENTITIES and PLANES directory entries in the emitted header,
    // emulating HL: Blue Shift maps.
    void swap_entities_planes() { swap01_ = true; }

    [[nodiscard]] std::vector<std::byte> build() const
    {
        bsp::dheader_t hdr{};
        hdr.version = version_;

        std::size_t cursor = sizeof( bsp::dheader_t );
        if ( bsp30ext_ )
            cursor += sizeof( bsp::dextrahdr_t );

        // First pass: assign offsets.
        std::array<bsp::dlump_t, bsp::k_header_lumps> dirs{};
        for ( int i = 0; i < bsp::k_header_lumps; ++i )
        {
            const auto &slot = lumps_[static_cast<std::size_t>( i )];
            if ( !slot.has_value() )
                continue; // fileofs 0 == unused
            cursor = align4( cursor );
            dirs[static_cast<std::size_t>( i )].fileofs =
                static_cast<std::int32_t>( cursor );
            dirs[static_cast<std::size_t>( i )].filelen =
                static_cast<std::int32_t>( slot->size() );
            cursor += slot->size();
        }

        for ( int i = 0; i < bsp::k_header_lumps; ++i )
            hdr.lumps[i] = dirs[static_cast<std::size_t>( i )];
        if ( swap01_ )
        {
            const bsp::dlump_t tmp        = hdr.lumps[bsp::k_lump_entities];
            hdr.lumps[bsp::k_lump_entities] = hdr.lumps[bsp::k_lump_planes];
            hdr.lumps[bsp::k_lump_planes]   = tmp;
        }

        // Second pass: emit bytes.
        std::vector<std::byte> out( cursor, std::byte{ 0 } );
        std::memcpy( out.data(), &hdr, sizeof hdr );
        if ( bsp30ext_ )
        {
            bsp::dextrahdr_t ext{};
            ext.id      = ext_id_;
            ext.version = ext_ver_;
            std::memcpy( out.data() + sizeof( bsp::dheader_t ), &ext, sizeof ext );
        }
        for ( int i = 0; i < bsp::k_header_lumps; ++i )
        {
            const auto &slot = lumps_[static_cast<std::size_t>( i )];
            const auto &dir  = dirs[static_cast<std::size_t>( i )];
            if ( !slot.has_value() || slot->empty() )
                continue;
            std::memcpy( out.data() + dir.fileofs, slot->data(), slot->size() );
        }
        return out;
    }

private:
    [[nodiscard]] static std::size_t align4( std::size_t v )
    {
        return ( v + 3u ) & ~std::size_t{ 3 };
    }

    std::int32_t version_;
    bool         bsp30ext_ = false;
    std::int32_t ext_id_   = 0;
    std::int32_t ext_ver_  = 0;
    bool         swap01_   = false;
    std::array<std::optional<std::vector<std::byte>>, bsp::k_header_lumps> lumps_{};
};

// Worldspawn entity text with a quoted classname — satisfies both the
// Blue-Shift probe and the 32-byte entities mincount.
inline constexpr std::string_view k_worldspawn_entities =
    "{\n\"classname\" \"worldspawn\"\n\"message\" \"test\"\n}\n";

// Worldspawn text used by the minimal-world fixture (wad + message keys).
inline constexpr std::string_view k_minimal_world_entities =
    "{\n"
    "\"classname\" \"worldspawn\"\n"
    "\"wad\" \"\\half-life\\valve\\halflife.wad;decals.wad\"\n"
    "\"message\" \"Test Map\"\n"
    "}\n";

// ---------------------------------------------------------------------------
// Minimal valid world fixture (hand-derived, one splitting node):
//   planes:  p0 { normal +X, dist 128, type PLANE_X }   → signbits 0
//            p1 { normal -Y, dist -64, type PLANE_Y }   → signbits 0b010
//   faces:   2 zeroed records (contents unread pre-C6; the count feeds
//            marksurface validation)
//   marks:   { 0, 1 }
//   leafs:   0 solid (visofs -1, ambient {1,2,3,4}) / 1 empty (visofs 0) /
//            2 water (visofs -1)
//   node 0:  plane 0, children { leaf1, leaf2 } = { -2, -3 }
//   model 0: bounds ±64, headnode { 0, -1, -1, -1 }, visleafs 2, numfaces 2
//   visdata: 4 arbitrary bytes
// ---------------------------------------------------------------------------

inline TestBspBuilder make_minimal_world( bool bsp2 = false )
{
    TestBspBuilder b{ bsp2 ? bsp::k_qbsp2_version : bsp::k_hlbsp_version };

    b.set_entities( k_minimal_world_entities );

    const std::vector<bsp::dplane_t> planes = {
        { {  1.0f,  0.0f, 0.0f },  128.0f, bsp::k_plane_x },
        { {  0.0f, -1.0f, 0.0f },  -64.0f, bsp::k_plane_y },
    };
    b.set_lump_records( bsp::k_lump_planes, planes );

    if ( bsp2 )
    {
        const std::vector<bsp::dface32_t> faces( 2 );
        b.set_lump_records( bsp::k_lump_faces, faces );
        const std::vector<bsp::dmarkface32_t> marks = { 0, 1 };
        b.set_lump_records( bsp::k_lump_marksurfaces, marks );

        std::vector<bsp::dleaf32_t> leafs( 3 );
        leafs[0] = { -2, -1, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, { 1, 2, 3, 4 } };
        leafs[1] = { -1,  0, { 0, 0, 0 }, { 64, 64, 64 }, 0, 2, {} };
        leafs[2] = { -3, -1, { -64, -64, -64 }, { 0, 0, 0 }, 0, 0, {} };
        b.set_lump_records( bsp::k_lump_leafs, leafs );

        std::vector<bsp::dnode32_t> nodes( 1 );
        nodes[0] = { 0, { -2, -3 }, { -128, -128, -128 }, { 128, 128, 128 }, 0, 2 };
        b.set_lump_records( bsp::k_lump_nodes, nodes );
    }
    else
    {
        const std::vector<bsp::dface_t> faces( 2 );
        b.set_lump_records( bsp::k_lump_faces, faces );
        const std::vector<bsp::dmarkface_t> marks = { 0, 1 };
        b.set_lump_records( bsp::k_lump_marksurfaces, marks );

        std::vector<bsp::dleaf_t> leafs( 3 );
        leafs[0] = { -2, -1, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, { 1, 2, 3, 4 } };
        leafs[1] = { -1,  0, { 0, 0, 0 }, { 64, 64, 64 }, 0, 2, {} };
        leafs[2] = { -3, -1, { -64, -64, -64 }, { 0, 0, 0 }, 0, 0, {} };
        b.set_lump_records( bsp::k_lump_leafs, leafs );

        std::vector<bsp::dnode_t> nodes( 1 );
        nodes[0] = { 0, { -2, -3 }, { -128, -128, -128 }, { 128, 128, 128 }, 0, 2 };
        b.set_lump_records( bsp::k_lump_nodes, nodes );
    }

    std::vector<bsp::dmodel_t> models( 1 );
    models[0] = { { -64.0f, -64.0f, -64.0f },
                  {  64.0f,  64.0f,  64.0f },
                  {   0.0f,   0.0f,   0.0f },
                  { 0, -1, -1, -1 },
                  /*visleafs=*/2, /*firstface=*/0, /*numfaces=*/2 };
    b.set_lump_records( bsp::k_lump_models, models );

    const unsigned char vis[4] = { 0x03, 0x00, 0x01, 0xFF };
    b.set_lump_bytes( bsp::k_lump_visibility, vis, sizeof vis );

    return b;
}

} // namespace test_bsp
