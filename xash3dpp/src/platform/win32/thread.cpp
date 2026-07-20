// xash3dpp — platform (Win32) — thread spawn primitive (Q-24)
// Design brief: docs/design/thread-spawn-and-inbox-brief.md §3.3
//
// No legacy reference — the first thread ever spawned in the xash3dpp tree.

#ifndef _WIN32
#  error "This file is Win32-only"
#endif

#include <xash3dpp/platform/thread.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/core/log.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstddef>  // std::size_t
#include <cstring>  // std::strlen, std::memcpy

namespace xash::platform {

namespace {

// ---------------------------------------------------------------------------
// SetThreadDescription — resolved dynamically.
//
// SetThreadDescription is a Windows 10 1607+ API. The project does not pin
// a minimum WINVER/_WIN32_WINNT, and no existing platform TU calls a
// version-gated kernel32 export unconditionally, so this follows the
// codebase's own dynamic-loading idiom (open_library/get_symbol in
// win32/sys.cpp) rather than risking a run-time failure — or a compile-time
// failure under an older Windows SDK — from calling the API directly.
// Resolved once via a C++11 magic-static, the same pattern get_time() uses
// for its QPC ClockInit in sys.cpp.
// ---------------------------------------------------------------------------

using SetThreadDescriptionFn = HRESULT ( WINAPI * )( HANDLE, PCWSTR );

[[nodiscard]] SetThreadDescriptionFn resolve_set_thread_description() noexcept
{
    static const SetThreadDescriptionFn s_fn = []() noexcept -> SetThreadDescriptionFn {
        HMODULE kernel32 = GetModuleHandleW( L"kernel32.dll" );
        if( !kernel32 )
            return nullptr;
        return reinterpret_cast<SetThreadDescriptionFn>( // SAFETY: GetProcAddress -> typed WINAPI function pointer; standard dynamic-resolution idiom for a post-Win7 API that may not exist on the running OS
            GetProcAddress( kernel32, "SetThreadDescription" ) );
    }();
    return s_fn;
}

// Copied into std::thread's own internal invoker storage when spawn_thread()
// constructs the std::thread below — the caller's |name|/|user| pointers do
// not need to outlive spawn_thread() itself (name is copied by value here;
// user remains a borrowed pointer per the header's @lifetime contract).
struct ThreadStartCtx
{
    ::xash::core::ThreadRole role;
    ThreadPriority           prio;
    ThreadFn                 fn;
    void                     *user;
    char                     name[::xash::limits::platform_thread_name_max];
};

void apply_name( const char *name ) noexcept
{
    if( !name || !name[0] )
        return;

    auto set_desc = resolve_set_thread_description();
    if( !set_desc )
        return; // pre-Win10 — no-op, matches is_debugger_present()'s "unsupported platform" degrade (sys.cpp)

    wchar_t wname[::xash::limits::platform_thread_name_max];
    int len = MultiByteToWideChar( CP_UTF8, 0, name, -1, wname,
                                    static_cast<int>( sizeof( wname ) / sizeof( wname[0] ) ) );
    if( len <= 0 )
        return;
    set_desc( GetCurrentThread(), wname );
}

void apply_priority( ThreadPriority prio ) noexcept
{
    switch( prio )
    {
    case ThreadPriority::Normal:
        break; // THREAD_PRIORITY_NORMAL is the default — no call needed
    case ThreadPriority::High:
        SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_HIGHEST );
        break;
    case ThreadPriority::Realtime:
        // XASH3DPP-STUB(chunk12): real-time scheduling lands with the SDL
        // audio device chunk (T_AudioCallback) — until then, log and run at
        // Normal so callers never see silent priority loss.
        ::xash::core::log( ::xash::core::LogLevel::Warning, "platform",
                            "spawn_thread: ThreadPriority::Realtime requested but not yet implemented — running at Normal" );
        break;
    }
}

void thread_trampoline( ThreadStartCtx ctx ) noexcept
{
    ::xash::core::register_thread_role( ctx.role ); // FIRST action — thread_role.hpp contract
    apply_name( ctx.name );
    apply_priority( ctx.prio );
    ctx.fn( ctx.user );
}

} // namespace

// compliance-allow(thread-assert): spawn_thread is callable from any thread
// by design (see thread.hpp @thread-safety) — a Main assert would be false
// precision on a primitive every off-main consumer (G-1/G-3/NetIO/Worker)
// needs to call, including from a non-Main thread spawning a further thread.
JoinHandle spawn_thread( ::xash::core::ThreadRole role, const char *name, ThreadPriority prio,
                          ThreadFn fn, void *user ) noexcept
{
    ThreadStartCtx ctx{ role, prio, fn, user, {} };
    if( name )
    {
        std::size_t n = std::strlen( name );
        if( n >= sizeof( ctx.name ) )
            n = sizeof( ctx.name ) - 1;
        std::memcpy( ctx.name, name, n );
        ctx.name[n] = '\0';
    }
    return JoinHandle{ std::thread{ thread_trampoline, ctx } };
}

} // namespace xash::platform
