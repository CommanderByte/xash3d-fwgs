// xash3dpp — platform (POSIX) — system console I/O
// Legacy reference: engine/platform/posix/con_posix.c  (Posix_Input)

#if defined(_WIN32)
#  error "This file is POSIX-only"
#endif

#include <xash3dpp/platform/console.hpp>

#include "../detail/assert_main.hpp"

#include <sys/select.h>
#include <unistd.h>   // STDIN_FILENO, STDOUT_FILENO, read, write

#include <cstring>    // std::memcpy, std::memmove

namespace xash::platform::console {

// ---------------------------------------------------------------------------
// write
// ---------------------------------------------------------------------------

void write( std::string_view text ) noexcept
{
    if( text.empty() ) return;
    const char *ptr  = text.data();
    std::size_t left = text.size();
    while( left > 0 )
    {
        ssize_t n;
        do { n = ::write( STDOUT_FILENO, ptr, left ); } while( n < 0 && errno == EINTR );
        if( n <= 0 ) break;
        ptr  += n;
        left -= static_cast<std::size_t>( n );
    }
}

// ---------------------------------------------------------------------------
// read_line — non-blocking line accumulator
// ---------------------------------------------------------------------------

std::string_view read_line() noexcept
{
    detail::assert_main_thread( "console::read_line" );
    static char       accum[1024];
    static std::size_t accum_len = 0;
    static char       result[1024];

    // Poll stdin with zero timeout — do not block.
    {
        fd_set fds;
        FD_ZERO( &fds );
        FD_SET( STDIN_FILENO, &fds );
        struct timeval tv{};
        if( ::select( STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv ) > 0 )
        {
            std::size_t space = sizeof( accum ) - accum_len - 1;
            ssize_t n;
            do { n = ::read( STDIN_FILENO, accum + accum_len, space ); }
            while( n < 0 && errno == EINTR );
            if( n > 0 ) accum_len += static_cast<std::size_t>( n );
        }
    }

    // Return the first complete line if one is available.
    for( std::size_t i = 0; i < accum_len; ++i )
    {
        if( accum[i] == '\n' )
        {
            std::size_t line_len = i;
            if( line_len > 0 && accum[line_len - 1] == '\r' ) --line_len;
            std::memcpy( result, accum, line_len );
            result[line_len] = '\0';
            std::memmove( accum, accum + i + 1, accum_len - i - 1 );
            accum_len -= ( i + 1 );
            return std::string_view{ result, line_len };
        }
    }
    return {};
}

} // namespace xash::platform::console
