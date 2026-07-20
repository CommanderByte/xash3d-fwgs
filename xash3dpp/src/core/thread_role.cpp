// xash3dpp — thread role registry implementation
// Design: see include/xash3dpp/core/thread_role.hpp.
//
// Implementation notes:
//   • A single thread_local variable stores the role for each thread.
//     Default-initialised to ThreadRole::Unknown.
//   • Mismatches in assert_thread_role() use XASH_FATAL — logging the actual
//     vs expected role names before aborting.

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/core/assert.hpp>
#include <xash3dpp/core/log.hpp>

namespace xash::core {

namespace {

// Per-thread role.  Zero-initialised on each thread's first reference; that
// initial value maps to ThreadRole::Unknown (enum value 0).
thread_local ThreadRole tls_role{ ThreadRole::Unknown };

} // anonymous namespace

// ---------------------------------------------------------------------------

// compliance-allow(thread-assert): the role-registry primitive itself —
// mutates only the caller's own thread_local, and by definition runs before
// any role exists to assert
void register_thread_role( ThreadRole role ) noexcept
{
    tls_role = role;
}

ThreadRole current_thread_role() noexcept
{
    return tls_role;
}

const char *thread_role_name( ThreadRole role ) noexcept
{
    switch( role )
    {
    case ThreadRole::Unknown:       return "Unknown";
    case ThreadRole::Main:          return "Main";
    case ThreadRole::AudioCallback: return "AudioCallback";
    case ThreadRole::AudioDecoder:  return "AudioDecoder";
    case ThreadRole::Worker:        return "Worker";
    case ThreadRole::Render:        return "Render";
    case ThreadRole::NetIO:         return "NetIO";
    }
    return "?";
}

void assert_thread_role( ThreadRole expected ) noexcept
{
    const ThreadRole actual = tls_role;
    if( actual == expected ) [[likely]]
        return;

    // Log the mismatch with both role names before XASH_FATAL aborts.
    logf( LogLevel::Fatal, "thread_role",
          "thread role mismatch: expected %s, got %s",
          thread_role_name( expected ),
          thread_role_name( actual ) );
    XASH_FATAL( actual == expected,
                "thread_role: thread called subsystem in the wrong role" );
}

} // namespace xash::core
