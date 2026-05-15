// xash3dpp — platform (Win32) — system console I/O
// Legacy reference: engine/platform/win32/con_win.c  (Wcon_WinPrint, Wcon_Input)

#ifndef _WIN32
#  error "This file is Win32-only"
#endif

#include <xash3dpp/platform/console.hpp>

#include "../detail/assert_main.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>

#include <cstring>   // std::memmove, std::memcpy

namespace xash::platform::console {

// ---------------------------------------------------------------------------
// write
// ---------------------------------------------------------------------------

void write( std::string_view text ) noexcept
{
    if( text.empty() ) return;
    HANDLE h = GetStdHandle( STD_OUTPUT_HANDLE );
    if( h == INVALID_HANDLE_VALUE || h == nullptr ) return;

    DWORD written;
    WriteFile( h, text.data(), static_cast<DWORD>( text.size() ),
               &written, nullptr );
}

// ---------------------------------------------------------------------------
// read_line — non-blocking line accumulator
// ---------------------------------------------------------------------------

std::string_view read_line() noexcept
{
    detail::assert_main_thread( "console::read_line" );
    static char   accum[1024];
    static DWORD  accum_len = 0;
    static char   result[1024];

    HANDLE h = GetStdHandle( STD_INPUT_HANDLE );
    if( h != INVALID_HANDLE_VALUE && h != nullptr )
    {
        DWORD file_type = GetFileType( h );

        if( file_type == FILE_TYPE_CHAR )
        {
            // Interactive console: poll for keyboard events then read a line.
            // We only read when Enter is already in the event queue so that
            // ReadConsoleA (line-input mode) returns immediately.
            bool enter_pending = false;
            DWORD n_events;
            if( GetNumberOfConsoleInputEvents( h, &n_events ) && n_events > 0 )
            {
                static INPUT_RECORD evbuf[64];
                DWORD n_peeked;
                if( PeekConsoleInputA( h, evbuf, 64, &n_peeked ) )
                {
                    for( DWORD i = 0; i < n_peeked; ++i )
                    {
                        if( evbuf[i].EventType == KEY_EVENT &&
                            evbuf[i].Event.KeyEvent.bKeyDown &&
                            evbuf[i].Event.KeyEvent.wVirtualKeyCode == VK_RETURN )
                        {
                            enter_pending = true;
                            break;
                        }
                    }
                }
            }
            if( enter_pending )
            {
                DWORD n;
                DWORD space = static_cast<DWORD>( sizeof( accum ) - accum_len - 1 );
                if( ReadConsoleA( h, accum + accum_len, space, &n, nullptr ) )
                    accum_len += n;
            }
        }
        else if( file_type == FILE_TYPE_PIPE || file_type == FILE_TYPE_DISK )
        {
            DWORD avail;
            if( PeekNamedPipe( h, nullptr, 0, nullptr, &avail, nullptr ) && avail > 0 )
            {
                DWORD n;
                DWORD space = static_cast<DWORD>( sizeof( accum ) - accum_len - 1 );
                if( ReadFile( h, accum + accum_len, ( avail < space ? avail : space ),
                              &n, nullptr ) )
                    accum_len += n;
            }
        }
    }

    // Return the first complete line if one is available.
    for( DWORD i = 0; i < accum_len; ++i )
    {
        if( accum[i] == '\n' )
        {
            DWORD line_len = i;
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
