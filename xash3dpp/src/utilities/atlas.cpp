// xash3dpp — texture atlas packer implementation
// Legacy reference: public/atlas.c
//
// Strip-based bin-packer: each column tracks how many vertical pixels are
// used; allocation scans for the widest w-wide gap that fits h pixels.

#include <xash3dpp/utilities/atlas.hpp>
#include <algorithm>

namespace xash::utilities {

Atlas::Atlas( int size ) noexcept
    : m_size( std::min( size, ATLAS_MAX_SIZE ) )
{
}

void Atlas::clear() noexcept
{
    m_allocated.fill( 0 );
    m_max_height = 0;
}

std::optional<Atlas::Block> Atlas::alloc( int w, int h ) noexcept
{
    if( w <= 0 || h <= 0 || w > m_size || h > m_size )
        return std::nullopt;

    int best_y = m_size;
    int best_x = -1;
    const int last_x = m_size - w;

    for( int x = 0; x <= last_x; ++x )
    {
        int y = 0;
        int i = 0;

        for( ; i < w; ++i )
        {
            y = std::max( y, m_allocated[x + i] );
            if( y >= best_y )
                break;
        }

        if( i == w && y + h <= m_size )
        {
            best_y = y;
            best_x = x;
        }
    }

    if( best_x < 0 )
        return std::nullopt;

    const int new_height = best_y + h;
    for( int i = 0; i < w; ++i )
        m_allocated[best_x + i] = new_height;

    m_max_height = std::max( m_max_height, new_height );
    return Block{ best_x, best_y };
}

} // namespace xash::utilities
