#pragma once
// xash3dpp — thread role registry and assertions
//
// Library: xash3dpp_core.
//
// Each thread that crosses a subsystem boundary registers a ThreadRole on
// startup.  Subsystem entry points can then assert their thread-safety
// contract at runtime with assert_thread_role(...).
//
// Design notes (threading-model.md):
//   • Free-function API; no init/shutdown.  Storage is thread_local.
//   • register_thread_role() is the only mutator — must be called from the
//     thread it describes, exactly once, before that thread calls into any
//     subsystem.  The main thread is captured implicitly on first query: if
//     a thread reads current_thread_role() / assert_thread_role() without
//     having registered, it is treated as ThreadRole::Main only if
//     register_thread_role(Main) has been called on it; otherwise the role
//     is Unknown.
//   • assert_thread_role() is a hard runtime check that fires via XASH_FATAL
//     on mismatch — it logs the actual vs expected role and aborts.
//   • The legacy assert_main_thread() helper in
//     <xash3dpp/private/platform/assert_main.hpp> is a thin wrapper kept for
//     existing platform code; new code SHOULD use assert_thread_role.
//
// Typical usage (worker thread startup):
//   void worker_thread_main()
//   {
//       xash::core::register_thread_role(xash::core::ThreadRole::Worker);
//       // ... work loop ...
//   }
//
// Typical usage (subsystem entry point):
//   void networking::poll()
//   {
//       xash::core::assert_thread_role(xash::core::ThreadRole::NetIO);
//       // ... safe to touch NetIO-only state ...
//   }
//
// @thread-safety: thread-local storage; each thread has its own role. register_thread_role()
// must be called exactly once from the target thread; current_thread_role() / assert_thread_role()
// are safe from any thread.

namespace xash::core {

// ---------------------------------------------------------------------------
// ThreadRole
// ---------------------------------------------------------------------------
//
// Roles known to the engine.  Additional roles may be added at the end (the
// numeric value of existing entries must NOT change — debug logs may include
// the integer value).
enum class ThreadRole
{
    Unknown,        // default for any thread that has not registered yet
    Main,           // the main / host thread (event pump, frame loop)
    AudioCallback,  // OS-driven audio buffer-fill callback (real-time)
    AudioDecoder,   // background audio stream decoder
    Worker,         // generic worker-pool thread
    Render,         // dedicated render thread (PLANNED — Chunk 10)
    NetIO,          // networking I/O thread (PLANNED — Chunk 2)
};

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

// Record the calling thread's role.  Must be called from the thread itself
// before that thread invokes any subsystem entry point that uses
// assert_thread_role().  Idempotent only if called with the same role.
void register_thread_role( ThreadRole role ) noexcept;

// Return the role registered for the calling thread.  Returns ThreadRole::Unknown
// if register_thread_role() has never been called on this thread.
[[nodiscard]] ThreadRole current_thread_role() noexcept;

// Abort (via XASH_FATAL) if the calling thread's role is not |expected|.
// No-op in NDEBUG / release builds for ThreadRole::Worker checks?  No — the
// check fires in every build.  Thread role mismatches are programmer bugs
// that we want to surface immediately, the same as XASH_FATAL.
void assert_thread_role( ThreadRole expected ) noexcept;

// Human-readable name for a ThreadRole, suitable for diagnostic messages.
// Returns a non-null, NUL-terminated string with static storage duration.
[[nodiscard]] const char *thread_role_name( ThreadRole role ) noexcept;

} // namespace xash::core
