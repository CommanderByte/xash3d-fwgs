// xash3dpp — platform::spawn_thread / JoinHandle tests (Q-24 OS-boilerplate
// half; HB-4 design brief, docs/design/thread-spawn-and-inbox-brief.md §3.3)
//
// Covers:
//   • role registration is visible inside the spawned thread, before fn runs
//   • debugger-visible name round-trip (best-effort, OS-queryable platforms
//     only — skips gracefully where the query API is unavailable)
//   • join-on-destruction (scope exit blocks until the thread finishes)
//   • explicit join()
//   • High priority spawn succeeds (even where the OS bump is best-effort)
//   • Realtime priority logs a warning and the thread still runs (stub)
//   • the user pointer is delivered unchanged to fn

#include <xash3dpp/platform/thread.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/core/log.hpp>

#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <string_view>
#include <thread>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#elif defined(__linux__) || defined(__ANDROID__)
#  include <pthread.h>
#endif

#include "../test_helpers.hpp"

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Role registration
// ---------------------------------------------------------------------------

static void test_role_visible_in_thread()
{
    using xash::core::ThreadRole;
    std::atomic<bool> role_matched{ false };

    auto handle = xash::platform::spawn_thread(
        ThreadRole::Worker, "role-test", xash::platform::ThreadPriority::Normal,
        []( void *user ) noexcept {
            auto *flag = static_cast<std::atomic<bool> *>( user );
            // register_thread_role(Worker) already ran as the FIRST action
            // inside spawn_thread's trampoline, before this fn — assert it
            // the same way a real subsystem entry point would.
            xash::core::assert_thread_role( ThreadRole::Worker );
            flag->store( xash::core::current_thread_role() == ThreadRole::Worker );
        },
        &role_matched );
    handle.join();

    CHECK( role_matched.load() );
    // Main thread's own role is unaffected by the worker's registration.
    CHECK( xash::core::current_thread_role() == ThreadRole::Main );
}

// ---------------------------------------------------------------------------
// Thread naming — best-effort round trip, OS-queryable platforms only.
// ---------------------------------------------------------------------------

#if defined(_WIN32)
using GetThreadDescriptionFn = HRESULT ( WINAPI * )( HANDLE, PWSTR * );
#endif

namespace {
struct NameCheckCtx
{
    std::atomic<bool> ran{ false };
    std::atomic<bool> matched{ false };
};
} // namespace

static void test_name_round_trip()
{
    using xash::core::ThreadRole;
    NameCheckCtx ctx;

    auto handle = xash::platform::spawn_thread(
        ThreadRole::Worker, "xash-name-test", xash::platform::ThreadPriority::Normal,
        []( void *user ) noexcept {
            auto *c = static_cast<NameCheckCtx *>( user );
#if defined(_WIN32)
            // GetThreadDescription is Win10+ — resolve dynamically exactly
            // like win32/thread.cpp resolves SetThreadDescription, so this
            // test degrades gracefully (skip, don't fail) on an older OS.
            HMODULE kernel32 = GetModuleHandleW( L"kernel32.dll" );
            auto get_desc = kernel32
                ? reinterpret_cast<GetThreadDescriptionFn>( // SAFETY: same dynamic-resolution idiom as win32/thread.cpp
                      GetProcAddress( kernel32, "GetThreadDescription" ) )
                : nullptr;
            if( get_desc )
            {
                PWSTR desc = nullptr;
                if( SUCCEEDED( get_desc( GetCurrentThread(), &desc ) ) && desc )
                {
                    char buf[64] = {};
                    int n = WideCharToMultiByte( CP_UTF8, 0, desc, -1, buf,
                                                  static_cast<int>( sizeof( buf ) ), nullptr, nullptr );
                    LocalFree( desc );
                    c->matched.store( n > 0 && std::strcmp( buf, "xash-name-test" ) == 0 );
                }
                else
                {
                    c->matched.store( true ); // API present but returned nothing usable — not a spawn_thread bug
                }
            }
            else
            {
                c->matched.store( true ); // pre-Win10 — GetThreadDescription unavailable; best-effort skip
            }
#elif defined(__linux__) || defined(__ANDROID__)
            char buf[16] = {};
            if( pthread_getname_np( pthread_self(), buf, sizeof( buf ) ) == 0 )
                // glibc/bionic truncate to 15 chars + null (see posix/thread.cpp).
                c->matched.store( std::strncmp( buf, "xash-name-test", sizeof( buf ) - 1 ) == 0 );
            else
                c->matched.store( true ); // best-effort — a query-path error is not a spawn_thread bug
#else
            // macOS/BSD: naming itself is TODO in posix/thread.cpp (mirrors
            // is_debugger_present's existing macOS/BSD TODO) — nothing to
            // verify here.
            c->matched.store( true );
#endif
            c->ran.store( true );
        },
        &ctx );
    handle.join();

    CHECK( ctx.ran.load() );
    CHECK( ctx.matched.load() );
}

// ---------------------------------------------------------------------------
// Join-on-destruction
// ---------------------------------------------------------------------------

static void test_join_on_destruction()
{
    std::atomic<bool> ran{ false };
    {
        auto handle = xash::platform::spawn_thread(
            xash::core::ThreadRole::Worker, "join-dtor-test", xash::platform::ThreadPriority::Normal,
            []( void *user ) noexcept {
                // A brief delay: if JoinHandle's destructor were a no-op
                // instead of actually joining, the CHECK below would very
                // likely observe `ran == false` (racing this sleep).
                std::this_thread::sleep_for( std::chrono::milliseconds( 30 ) );
                static_cast<std::atomic<bool> *>( user )->store( true );
            },
            &ran );
        // handle destructs here — must block until the thread above sets `ran`.
    }
    CHECK( ran.load() );
}

// ---------------------------------------------------------------------------
// Explicit join()
// ---------------------------------------------------------------------------

static void test_explicit_join()
{
    std::atomic<bool> ran{ false };
    auto handle = xash::platform::spawn_thread(
        xash::core::ThreadRole::Worker, "explicit-join-test", xash::platform::ThreadPriority::Normal,
        []( void *user ) noexcept { static_cast<std::atomic<bool> *>( user )->store( true ); },
        &ran );

    CHECK( handle.joinable() );
    handle.join();
    CHECK( !handle.joinable() );
    CHECK( ran.load() );

    // Calling join() again must be a safe no-op.
    handle.join();
}

// ---------------------------------------------------------------------------
// Priority — High
// ---------------------------------------------------------------------------

static void test_high_priority_spawn_succeeds()
{
    std::atomic<bool> ran{ false };
    auto handle = xash::platform::spawn_thread(
        xash::core::ThreadRole::Worker, "high-prio-test", xash::platform::ThreadPriority::High,
        []( void *user ) noexcept { static_cast<std::atomic<bool> *>( user )->store( true ); },
        &ran );
    handle.join();

    // High is best-effort on POSIX (may silently stay at Normal without
    // CAP_SYS_NICE) — the only universal guarantee is that the thread still
    // runs to completion.
    CHECK( ran.load() );
}

// ---------------------------------------------------------------------------
// Priority — Realtime (stub: logs + runs at Normal)
// ---------------------------------------------------------------------------

static std::atomic<bool> g_saw_realtime_warning{ false };

static void realtime_log_callback( xash::core::LogLevel level, std::string_view tag,
                                    std::string_view text ) noexcept
{
    if( level == xash::core::LogLevel::Warning && tag == "platform" &&
        text.find( "Realtime" ) != std::string_view::npos )
        g_saw_realtime_warning.store( true );
}

static void test_realtime_logs_and_runs()
{
    g_saw_realtime_warning.store( false );
    xash::core::log_set_callback( realtime_log_callback );

    std::atomic<bool> ran{ false };
    auto handle = xash::platform::spawn_thread(
        xash::core::ThreadRole::Worker, "realtime-test", xash::platform::ThreadPriority::Realtime,
        []( void *user ) noexcept { static_cast<std::atomic<bool> *>( user )->store( true ); },
        &ran );
    handle.join();

    xash::core::log_set_callback( nullptr ); // restore the default sink before other tests run

    CHECK( ran.load() );                    // the thread still executed fn despite the stub
    CHECK( g_saw_realtime_warning.load() ); // XASH3DPP-STUB(chunk12) logged its warning
}

// ---------------------------------------------------------------------------
// User pointer delivery
// ---------------------------------------------------------------------------

namespace {
struct Payload
{
    int                value;
    std::atomic<bool>  observed{ false };
};
} // namespace

static void test_user_pointer_delivered()
{
    Payload payload{ 42, {} };

    auto handle = xash::platform::spawn_thread(
        xash::core::ThreadRole::Worker, "user-ptr-test", xash::platform::ThreadPriority::Normal,
        []( void *user ) noexcept {
            auto *p = static_cast<Payload *>( user );
            p->observed.store( p->value == 42 );
        },
        &payload );
    handle.join();

    CHECK( payload.observed.load() );
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );

    RUN_TEST( test_role_visible_in_thread );
    RUN_TEST( test_name_round_trip );
    RUN_TEST( test_join_on_destruction );
    RUN_TEST( test_explicit_join );
    RUN_TEST( test_high_priority_spawn_succeeds );
    RUN_TEST( test_realtime_logs_and_runs );
    RUN_TEST( test_user_pointer_delivered );

    std::printf( "thread: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
