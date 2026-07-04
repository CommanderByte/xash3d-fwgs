// xash3dpp — PVS queries
// Legacy reference: engine/common/mod_bmodel.c (see header).

#include <xash3dpp/map_loader/pvs.hpp>

#include <cstring>

namespace xash::map_loader {

void decompress_pvs( std::span<const std::byte> in, std::size_t visbytes,
                     std::span<std::byte> out ) noexcept
{
    std::size_t dst = 0;
    const std::size_t limit = visbytes < out.size() ? visbytes : out.size();

    if ( in.empty() )
    {
        // Legacy NULL input: no vis info == all visible.
        std::memset( out.data(), 0xFF, limit );
        return;
    }

    std::size_t src = 0;
    while ( dst < limit )
    {
        if ( src >= in.size() )
        {
            // Hardening: legacy walks past the compressed buffer here.
            std::memset( out.data() + dst, 0, limit - dst );
            return;
        }

        const auto b = static_cast<unsigned char>( in[src] );
        if ( b != 0 )
        {
            out[dst++] = in[src++];
            continue;
        }

        // zero byte + run length of zero bytes (clamped to the output end)
        std::size_t run = src + 1 < in.size()
            ? static_cast<unsigned char>( in[src + 1] )
            : 0;
        if ( run > limit - dst )
            run = limit - dst;
        std::memset( out.data() + dst, 0, run );
        src += 2;
        dst += run;
    }
}

} // namespace xash::map_loader
