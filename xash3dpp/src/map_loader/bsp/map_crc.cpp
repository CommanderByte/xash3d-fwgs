// xash3dpp — map checksum (see header for the wire-frozen quirks)
// Legacy reference: engine/common/mod_bmodel.c:4093-4165, public/crclib.c.

#include <xash3dpp/private/map_loader/bsp/map_crc.hpp>

#include <xash3dpp/utilities/hash.hpp>

namespace xash::map_loader::bsp {

std::uint32_t map_checksum_multiplayer( std::span<const std::byte> file,
                                        const dheader_t &header ) noexcept
{
    // utilities::crc32 is the crclib port (reflected zlib polynomial); the
    // wire-frozen quirk is only that crc32_final's invert never happens.
    ::xash::utilities::Crc32 crc;
    ::xash::utilities::crc32_init( crc ); // 0xFFFFFFFF

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

        ::xash::utilities::crc32_update( crc, file.data() + ofs, len );
    }

    // NO final xor-invert — CRC32_MapFile never calls CRC32_Final.
    return crc;
}

} // namespace xash::map_loader::bsp
