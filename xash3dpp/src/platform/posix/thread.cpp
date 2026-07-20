// xash3dpp — platform (POSIX: Linux, macOS, FreeBSD, Android) — thread spawn
// primitive (Q-24)
// Design brief: docs/design/thread-spawn-and-inbox-brief.md §3.3
//
// No legacy reference — the first thread ever spawned in the xash3dpp tree
// (net_ws.c's `create_thread` macro is the only legacy pthread_create call,
// and it has no naming/priority/role concept to port).

#if defined(_WIN32)
#  error "This file is POSIX-only"
#endif

#include <xash3dpp/platform/thread.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/core/log.hpp>

#include <pthread.h>
#include <sched.h>

#include <cstddef>  // std::size_t
#include <cstring>  // std::strlen, std::memcpy

namespace xash::platform {

namespace {

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

#if defined(__linux__) || defined(__ANDROID__)
    // glibc/bionic pthread_setname_np hard-caps at 16 bytes INCLUDING the
    // null terminator (15 visible characters) — truncate rather than fail.
    // The full (untruncated) name still lives in ctx.name for any future
    // logging use; only the OS-visible debugger name is shortened here.
    char buf[16];
    std::size_t n = std::strlen( name );
    if( n >= sizeof( buf ) )
        n = sizeof( buf ) - 1;
    std::memcpy( buf, name, n );
    buf[n] = '\0';
    pthread_setname_np( pthread_self(), buf );
#else
    // TODO: macOS's pthread_setname_np(const char*) takes no pthread_t
    // (self-only) and FreeBSD/NetBSD/OpenBSD each have their own
    // differently-signed variant (e.g. FreeBSD's pthread_set_name_np).
    // Mirrors the existing is_debugger_present() gap (sys.cpp), which
    // likewise leaves macOS/BSD unimplemented rather than guessing at an
    // unverified API.
    (void)name;
#endif
}

void apply_priority( ThreadPriority prio ) noexcept
{
    switch( prio )
    {
    case ThreadPriority::Normal:
        break; // default SCHED_OTHER scheduling — no call needed
    case ThreadPriority::High:
    {
        // Best-effort: raise to SCHED_RR at a modest priority. POSIX
        // processes typically lack CAP_SYS_NICE / root to change scheduling
        // policy, so a failure here is silently tolerated at the OS level
        // (a Warning is logged instead of failing spawn_thread) — there is
        // no portable POSIX equivalent of Win32's THREAD_PRIORITY_HIGHEST,
        // and legacy has no precedent (net_ws.c's create_thread macro never
        // touches scheduling).
        int min_prio = sched_get_priority_min( SCHED_RR );
        int max_prio = sched_get_priority_max( SCHED_RR );
        if( min_prio >= 0 && max_prio >= 0 )
        {
            sched_param sp{};
            sp.sched_priority = ( min_prio + max_prio ) / 2;
            if( pthread_setschedparam( pthread_self(), SCHED_RR, &sp ) != 0 )
            {
                ::xash::core::log( ::xash::core::LogLevel::Warning, "platform",
                                    "spawn_thread: failed to raise priority to High (SCHED_RR) — insufficient privilege; continuing at Normal" );
            }
        }
        break;
    }
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
