// xash3dpp — unit tests for core::ThreadRole
//
// Verifies:
//   • Default role on a fresh thread is Unknown.
//   • register_thread_role updates the calling thread's role.
//   • Each thread has independent storage (thread_local).
//   • thread_role_name returns the expected literal for each enum value.

#include <xash3dpp/core/thread_role.hpp>

#include <atomic>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

int g_failures = 0;

#define CHECK( expr ) do { \
    if( !( expr ) ) { \
        std::fprintf( stderr, "FAIL  %s:%d  %s\n", __FILE__, __LINE__, #expr ); \
        ++g_failures; \
    } \
} while( 0 )

void test_default_role_is_unknown()
{
    // On the main thread of the test process the role starts as Unknown;
    // we have not called register_thread_role yet.
    CHECK( xash::core::current_thread_role() == xash::core::ThreadRole::Unknown );
}

void test_register_updates_role()
{
    using xash::core::ThreadRole;
    xash::core::register_thread_role( ThreadRole::Main );
    CHECK( xash::core::current_thread_role() == ThreadRole::Main );

    // Idempotent re-registration to a new role.
    xash::core::register_thread_role( ThreadRole::Worker );
    CHECK( xash::core::current_thread_role() == ThreadRole::Worker );

    // Restore Main for the rest of the test process.
    xash::core::register_thread_role( ThreadRole::Main );
}

void test_thread_local_isolation()
{
    using xash::core::ThreadRole;
    std::atomic<bool>       saw_unknown_before{ false };
    std::atomic<ThreadRole> after_register{ ThreadRole::Unknown };

    std::thread worker( [&]() {
        // Fresh thread — role must default to Unknown regardless of what the
        // main thread has registered for itself.
        saw_unknown_before.store(
            xash::core::current_thread_role() == ThreadRole::Unknown );

        xash::core::register_thread_role( ThreadRole::AudioDecoder );
        after_register.store( xash::core::current_thread_role() );
    } );
    worker.join();

    CHECK( saw_unknown_before.load() );
    CHECK( after_register.load() == ThreadRole::AudioDecoder );

    // Main thread's role is unaffected by the worker's registration.
    CHECK( xash::core::current_thread_role() == ThreadRole::Main );
}

void test_role_names()
{
    using xash::core::ThreadRole;
    using xash::core::thread_role_name;

    CHECK( std::strcmp( thread_role_name( ThreadRole::Unknown ),       "Unknown"       ) == 0 );
    CHECK( std::strcmp( thread_role_name( ThreadRole::Main ),          "Main"          ) == 0 );
    CHECK( std::strcmp( thread_role_name( ThreadRole::AudioCallback ), "AudioCallback" ) == 0 );
    CHECK( std::strcmp( thread_role_name( ThreadRole::AudioDecoder ),  "AudioDecoder"  ) == 0 );
    CHECK( std::strcmp( thread_role_name( ThreadRole::Worker ),        "Worker"        ) == 0 );
    CHECK( std::strcmp( thread_role_name( ThreadRole::Render ),        "Render"        ) == 0 );
    CHECK( std::strcmp( thread_role_name( ThreadRole::NetIO ),         "NetIO"         ) == 0 );
}

void test_assert_thread_role_matches()
{
    using xash::core::ThreadRole;
    // Currently registered as Main — assert_thread_role(Main) must not abort.
    xash::core::assert_thread_role( ThreadRole::Main );
}

} // namespace

int main()
{
    test_default_role_is_unknown();
    test_register_updates_role();
    test_thread_local_isolation();
    test_role_names();
    test_assert_thread_role_matches();

    if( g_failures != 0 )
    {
        std::fprintf( stderr, "test_thread_role: %d failure(s)\n", g_failures );
        return EXIT_FAILURE;
    }
    std::fputs( "test_thread_role: OK\n", stderr );
    return EXIT_SUCCESS;
}
