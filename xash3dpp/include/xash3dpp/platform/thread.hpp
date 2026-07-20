#pragma once
// xash3dpp — platform thread-spawn primitive (Q-24 OS-boilerplate half)
// Design brief: docs/design/thread-spawn-and-inbox-brief.md §3.3
// Legacy reference: none directly applicable — this is the FIRST thread ever
// spawned in the xash3dpp tree. The only legacy analogue is net_ws.c's
// `create_thread` macro (`!pthread_create(&t, NULL, pfn, NULL)`), which has
// no naming, priority, or role concept to port.
//
// Implementations:
//   src/platform/win32/thread.cpp — Windows (SetThreadDescription, resolved
//                                   dynamically since it is a Windows 10+
//                                   API; SetThreadPriority)
//   src/platform/posix/thread.cpp — Linux / macOS / FreeBSD / Android
//                                   (pthread_setname_np, sched_setscheduler)
//
// Design notes:
//   • Free function + RAII join handle. No init/shutdown.
//   • JoinHandle wraps std::thread, NOT std::jthread: the brief's contract
//     is plain join-on-destruction with no cooperative-cancellation
//     (stop_token) semantics. std::jthread's extra machinery (an internal
//     stop_state allocation plus an implicit stop-request on destruction)
//     would add state and behaviour this primitive never uses. std::thread
//     wrapped in an explicit join()-on-destroy RAII type gives exactly the
//     contract the brief asks for, nothing more (P-3 narrowest-state
//     spirit) — see JoinHandle below.
//   • Entry point is the C-idiom `void (*fn)(void *user)` — matches
//     cmd_cvar::CommandCtxFn (command.hpp) rather than a templated
//     `Fn &&fn` forwarding-reference. A template would make spawn_thread a
//     header-only function instantiated per call site (more object code,
//     an implicit closure type per lambda, and a harder ABI story for a
//     primitive every off-main consumer in the tree — G-1, G-3, NetIO,
//     the future worker pool — will call). The fn+user shape is already the
//     project's established idiom for a capture-less callback plus
//     borrowed context (P-3 exception class; see INP-OQ-1's
//     cmd_add(name, CommandCtxFn, void *user, ...) in
//     decisions-architecture.md).
//   • register_thread_role(role) runs as the FIRST action on the new
//     thread, before naming/priority/fn — see thread_role.hpp: subsystem
//     entry points assert this role, so it must be visible before any
//     subsystem code (including the caller's fn) can run.
//   • |name| is copied into a small fixed-size buffer (limits::
//     platform_thread_name_max) at spawn_thread() call time, then copied
//     again into std::thread's own internal invoker storage — the caller's
//     string does NOT need to outlive the call. |user| is NOT copied: it is
//     a borrowed pointer that must outlive the spawned thread (see the
//     @lifetime note on spawn_thread below), mirroring CommandCtxFn's
//     user-data contract.
//
// @thread-safety: spawn_thread is callable from any thread. The returned
// JoinHandle is a move-only, single-owner RAII type — concurrent join() /
// destruction of the SAME JoinHandle from multiple threads is not supported
// (same confinement rule as std::thread itself).

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/limits.hpp>

#include <thread>

namespace xash::platform {

// ---------------------------------------------------------------------------
// ThreadPriority
// ---------------------------------------------------------------------------

enum class ThreadPriority
{
    Normal,    // Default OS scheduling — no priority call is made.
    High,      // Win32: SetThreadPriority(THREAD_PRIORITY_HIGHEST).
               // POSIX: best-effort SCHED_RR bump; silently falls back to
               // Normal if the process lacks the privilege (CAP_SYS_NICE /
               // root) needed to change scheduling policy — POSIX has no
               // portable equivalent of Win32's priority knob, and legacy
               // has no precedent (net_ws.c's create_thread macro never
               // touches scheduling).
    Realtime,  // XASH3DPP-STUB(chunk12): logs a Warning and runs the thread
               // at Normal priority instead. Real-time scheduling lands
               // with the SDL audio device chunk (T_AudioCallback), per the
               // design brief.
};

// ---------------------------------------------------------------------------
// Thread entry point
// ---------------------------------------------------------------------------

// C-idiom entry point: |user| is passed through unchanged. See the header
// comment above for why this shape was chosen over a templated Fn&&.
using ThreadFn = void ( * )( void *user ) noexcept;

// ---------------------------------------------------------------------------
// JoinHandle — join-on-destruction RAII handle to a spawned thread.
// ---------------------------------------------------------------------------
//
// Movable, non-copyable — exclusive ownership, exactly like std::thread
// itself (moving over a still-joinable *this would otherwise call
// std::terminate per the std::thread contract; operator=(JoinHandle&&)
// joins *this first to avoid that).
//
// @thread-safety: a single JoinHandle instance is confined to its owner —
// concurrent join() calls (explicit or via destructor) on the SAME instance
// from multiple threads are not supported.
class JoinHandle
{
public:
    JoinHandle() noexcept = default;
    ~JoinHandle() noexcept { join(); }

    JoinHandle( const JoinHandle & )            = delete;
    JoinHandle &operator=( const JoinHandle & ) = delete;

    JoinHandle( JoinHandle &&other ) noexcept : thread_{ std::move( other.thread_ ) } {}
    JoinHandle &operator=( JoinHandle &&other ) noexcept
    {
        if( this != &other )
        {
            join(); // finish our own thread first — avoids std::terminate() from std::thread::operator= on a still-joinable target
            thread_ = std::move( other.thread_ );
        }
        return *this;
    }

    // Block until the thread finishes. No-op if not joinable (already
    // joined, default-constructed, or moved-from).
    void join() noexcept
    {
        if( thread_.joinable() )
            thread_.join();
    }

    // True if this handle owns a thread that has not yet been joined.
    [[nodiscard]] bool joinable() const noexcept { return thread_.joinable(); }

private:
    friend JoinHandle spawn_thread( ::xash::core::ThreadRole role, const char *name,
                                     ThreadPriority prio, ThreadFn fn, void *user ) noexcept;

    explicit JoinHandle( std::thread &&t ) noexcept : thread_{ std::move( t ) } {}

    std::thread thread_;
};

// ---------------------------------------------------------------------------
// spawn_thread
// ---------------------------------------------------------------------------

// Spawn a new OS thread running fn(user). Before fn runs, the new thread:
//   1. registers |role| via core::register_thread_role() — the FIRST action
//      on the new thread (thread_role.hpp: subsystem entry points assert
//      this role, so it must be visible before anything else runs);
//   2. applies a debugger-visible name derived from |name| (Win32:
//      SetThreadDescription, resolved dynamically because it is a Windows
//      10+ API; POSIX: pthread_setname_np on Linux/Android, truncated to
//      that platform's 16-byte limit — see win32/thread.cpp and
//      posix/thread.cpp for the exact per-OS behaviour and gaps);
//   3. applies |prio| (see ThreadPriority).
//
// |name| is copied into an internal fixed-size buffer synchronously, before
// this function returns — the caller's string does not need to outlive the
// call; it is truncated to limits::platform_thread_name_max - 1 bytes. A
// null |name| is treated as "no name" (naming step is skipped).
//
// @lifetime: |user| is a borrowed pointer — it must remain valid until the
// spawned thread has finished running fn (i.e. until the returned
// JoinHandle is joined, explicitly or via destruction). Not copied.
//
// Thread creation failure (OS resource exhaustion) throws std::system_error
// from the underlying std::thread constructor; under this project's
// no-exceptions build (/EHs-c- / -fno-exceptions) an exception escaping this
// noexcept function terminates the process — the same fate as any other
// unrecoverable allocation failure elsewhere in the engine.
[[nodiscard]] JoinHandle spawn_thread( ::xash::core::ThreadRole role, const char *name,
                                        ThreadPriority prio, ThreadFn fn, void *user ) noexcept;

} // namespace xash::platform
