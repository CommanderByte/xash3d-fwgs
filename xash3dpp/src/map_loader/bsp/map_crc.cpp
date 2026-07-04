// xash3dpp — map checksum (see header for the wire-frozen quirks)
// Legacy reference: engine/common/mod_bmodel.c:4093-4165, public/crclib.c.

#include <xash3dpp/private/map_loader/bsp/map_crc.hpp>

#include <array>

namespace xash::map_loader::bsp {

namespace {

// Standard reflected CRC-32 table (poly 0xEDB88320) — identical to the
// crc32table in public/crclib.c (first entries 0x00000000, 0x77073096, …).
consteval std::array<std::uint32_t, 256> make_crc_table()
{
    std::array<std::uint32_t, 256> t{};
    for ( std::uint32_t i = 0; i < 256; ++i )
    {
        std::uint32_t c = i;
        for ( int k = 0; k < 8; ++k )
            c = ( c & 1u ) ? 0xEDB88320u ^ ( c >> 1 ) : c >> 1;
        t[i] = c;
    }
    return t;
}

constexpr auto k_crc_table = make_crc_table();

void crc_process( std::uint32_t &crc, std::span<const std::byte> data ) noexcept
{
    for ( const std::byte b : data )
        crc = k_crc_table[( crc ^ static_cast<std::uint8_t>( b )) & 0xFFu] ^ ( crc >> 8 );
}

} // namespace

std::uint32_t map_checksum_multiplayer( std::span<const std::byte> file,
                                        const dheader_t &header ) noexcept
{
    std::uint32_t crc = 0xFFFFFFFFu; // CRC32_Init

    // Lumps PLANES..MODELS in index order; ENTITIES (index 0) excluded.
    for ( int i = k_lump_planes; i < k_header_lumps; ++i )
    {
        const dlump_t &l = header.lumps[i];
        if ( l.fileofs < 0 || l.filelen <= 0 )
            continue; // legacy FS_Seek/while(lumplen>0) skip these

        auto ofs = static_cast<std::size_t>( l.fileofs );
        if ( ofs >= file.size() )
            continue; // legacy hits EOF immediately

        std::size_t len = static_cast<std::size_t>( l.filelen );
        if ( len > file.size() - ofs )
            len = file.size() - ofs; // legacy stops at EOF

        crc_process( crc, file.subspan( ofs, len ));
    }

    // NO final xor-invert — CRC32_MapFile never calls CRC32_Final.
    return crc;
}

} // namespace xash::map_loader::bsp
