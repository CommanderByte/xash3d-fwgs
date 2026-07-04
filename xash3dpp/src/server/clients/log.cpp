// xash3dpp — HL-standard server log (Chunk 6 S9 satellite)
// Legacy reference: engine/server/sv_log.c — Log_Printf (:101), the
// "MM/DD/YYYY - HH:MM:SS: " timestamp prefix and the net_log-even-when-
// inactive UDP quirk (:125-126).
// Deep dive: docs/legacy-survey/deep-dive-server-clients.md §6 (sv_log.c).
//
// File rotation + the UDP `log %s` OOB send are filesystem/networking seams
// (the host wires them); the parity-critical part owned here is the exact
// line format and the active/net_log gating.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/clients.hpp>

#include <xash3dpp/utilities/string.hpp>

#include <ctime>

namespace xash::server {

void log_printf( ClientMachinery &cm, const char *text, char *out,
                 std::size_t out_size ) noexcept
{
    ( void )cm; // net_log UDP dispatch is a host seam (see header)

    if ( out == nullptr || out_size == 0 )
        return;

    if ( text == nullptr )
        text = "";

    // legacy strftime( "%m/%d/%Y - %H:%M:%S" ) then Q_snprintf "%s: %s".
    char        stamp[64] = {};
    std::time_t now       = std::time( nullptr );
    std::tm    *tb        = std::localtime( &now );
    if ( tb != nullptr )
        std::strftime( stamp, sizeof( stamp ), "%m/%d/%Y - %H:%M:%S", tb );

    ::xash::utilities::snprintf( out, out_size, "%s: %s", stamp, text );

    // XASH3DPP-STUB(S8-seam): when cm.log.net_log, legacy sends the line as an
    // OOB `log %s` datagram to cm.log.net_address even if !cm.log.active
    // (sv_log.c:125-126) — the host owns the UDP socket; wire it in the frame
    // loop.  File/console echo gates on cm.log.active && (MP || singleplayer).
}

} // namespace xash::server
