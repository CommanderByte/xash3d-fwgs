#pragma once
// xash3dpp — platform console I/O contract
// Legacy reference: engine/platform/win32/con_win.c  (Wcon_Input, Wcon_WinPrint)
//                   engine/platform/posix/con_posix.c (Posix_Input)
//
// The "console" here is the system-level developer console:
//   • Win32:   the conhost window that xash3d creates on startup
//   • POSIX:   stdin / stdout when the engine is run in a terminal
//   • Android: no-op (no terminal; logcat is the output channel)
//
// Porting contract (mandatory, see docs/boundaries/platform-boundary.md):
//   Every platform must provide implementations of all functions below.
//   Platforms without a console concept return empty strings / do nothing.
//
// Threading: all functions may be called from the main thread only unless
//            noted otherwise.

#include <string_view>

namespace xash::platform::console {

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

// Write a UTF-8 line to the system console.
// The implementation may append a newline if |text| does not end with one.
// Called from the engine log sink; must never block.
void write( std::string_view text ) noexcept;

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

// Poll for a complete line of input from the system console.
// Returns the line text (without the trailing newline) if one is available,
// or an empty string_view if no input is ready.
//
// The returned view is valid only until the next call to read_line() or until
// the calling frame ends.  Callers must copy if persistence is needed.
//
// Must not block.  Fire-and-forget platforms return {} unconditionally.
std::string_view read_line() noexcept;

} // namespace xash::platform::console
