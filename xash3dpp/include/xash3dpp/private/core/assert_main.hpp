#pragma once
// Private utility — xash3dpp_core implementation detail.
// Not part of the public xash3dpp_core API.
//
// Legacy wrapper for "main-thread only" debug enforcement.  New code SHOULD
// use xash::core::assert_thread_role(ThreadRole::Main) from
// <xash3dpp/core/thread_role.hpp> instead — this header is kept because
// platform/ uses it from sys.cpp / crash.cpp / console.cpp and there is no
// reason to migrate those call sites yet.
//
// Provides two functions:
//
//   capture_main_thread()    — record the calling thread as the main thread.
//                              Called once from get_time() via a magic-static.
//   assert_main_thread(loc)  — abort (via XASH_FATAL) if the current thread is
//                              not the main thread.  loc is a string literal
//                              naming the function being protected, used in the
//                              message.
//
// Threading safety of this header itself:
//   capture_main_thread() writes the static ID once, from inside a magic-static
//   initialiser in get_time() — guaranteed to run exactly once before any other
//   thread can have called get_time().  Subsequent reads in assert_main_thread()
//   are therefore safe (happens-before via the magic-static guarantee).

#include <xash3dpp/core/assert.hpp>
#include <thread>

namespace xash::core::detail {

inline std::thread::id &main_thread_id_ref() noexcept
{
    static std::thread::id id;
    return id;
}

// Record the current thread as the main thread.
// Must be called exactly once, from the main thread, before any worker threads
// are spawned.  Called automatically by get_time() on first use.
inline void capture_main_thread() noexcept
{
    main_thread_id_ref() = std::this_thread::get_id();
}

// Assert that the current thread is the main thread.
// No-op until capture_main_thread() has been called (safe to call early).
inline void assert_main_thread( [[maybe_unused]] const char *location ) noexcept
{
    const std::thread::id &id = main_thread_id_ref();
    // Skip the check if the main thread has not been captured yet — this can
    // happen if a platform function is called before get_time() (unusual but
    // allowed at startup before threads are spawned).
    XASH_FATAL( id == std::thread::id{} || id == std::this_thread::get_id(),
                "platform main-thread-only function called from a worker thread" );
}

} // namespace xash::core::detail
