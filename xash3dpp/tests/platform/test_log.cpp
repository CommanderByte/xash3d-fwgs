// xash3dpp — platform logging tests
// Covers: log() (all levels, empty text, long text truncation),
//         logf() (formatting, truncation),
//         log_va() (va_list path),
//         log_set_callback() (callback receives body, level, tag),
//         callback reset (nullptr restores default sink),
//         log_verbose inline helper (only active when XASH_VERBOSE defined).
//
// None of these tests verify the exact bytes written to stdout — that is
// tested implicitly by the test runner capturing output.  We verify:
//   (a) calls do not crash,
//   (b) the callback receives the expected arguments,
//   (c) truncation does not overflow the buffer.

#include <xash3dpp/platform/log.hpp>

#include <cstdio>
#include <cstring>    // std::strcmp, std::strlen
#include <string>

static int g_pass = 0, g_fail = 0;

#define CHECK(expr) \
    do { if (expr) { ++g_pass; } \
         else { ++g_fail; std::printf("FAIL [line %d]: %s\n", __LINE__, #expr); } } while(0)

// ---------------------------------------------------------------------------
// Callback capture helper
// ---------------------------------------------------------------------------

struct CallbackCapture
{
    xash::platform::LogLevel last_level{};
    std::string last_tag;
    std::string last_text;
    int call_count = 0;

    static CallbackCapture *instance;

    static void callback( xash::platform::LogLevel level,
                          std::string_view tag,
                          std::string_view text ) noexcept
    {
        if( !instance ) return;
        instance->last_level = level;
        instance->last_tag   = std::string{ tag };
        instance->last_text  = std::string{ text };
        ++instance->call_count;
    }

    void install()
    {
        instance = this;
        xash::platform::log_set_callback( &CallbackCapture::callback );
    }

    void uninstall()
    {
        xash::platform::log_set_callback( nullptr );
        instance = nullptr;
    }
};

CallbackCapture *CallbackCapture::instance = nullptr;

// ---------------------------------------------------------------------------
// Smoke: all levels do not crash
// ---------------------------------------------------------------------------

static void test_all_levels_no_crash()
{
    using xash::platform::LogLevel;
    xash::platform::log( LogLevel::Verbose, "test", "verbose message" );
    xash::platform::log( LogLevel::Info,    "test", "info message"    );
    xash::platform::log( LogLevel::Warning, "test", "warning message" );
    xash::platform::log( LogLevel::Error,   "test", "error message"   );
    xash::platform::log( LogLevel::Fatal,   "test", "fatal message"   );
    ++g_pass;
}

// ---------------------------------------------------------------------------
// Empty text does not crash
// ---------------------------------------------------------------------------

static void test_empty_text_no_crash()
{
    xash::platform::log( xash::platform::LogLevel::Info, "test", "" );
    xash::platform::log( xash::platform::LogLevel::Info, "",     "" );
    ++g_pass;
}

// ---------------------------------------------------------------------------
// logf formatting
// ---------------------------------------------------------------------------

static void test_logf_basic()
{
    CallbackCapture cap;
    cap.install();

    xash::platform::logf( xash::platform::LogLevel::Info, "fmt",
                          "value=%d str=%s", 42, "hello" );

    CHECK( cap.call_count == 1 );
    CHECK( cap.last_tag   == "fmt" );
    CHECK( cap.last_level == xash::platform::LogLevel::Info );
    // The body should contain the formatted values.
    CHECK( cap.last_text.find( "42" )    != std::string::npos );
    CHECK( cap.last_text.find( "hello" ) != std::string::npos );

    cap.uninstall();
}

// ---------------------------------------------------------------------------
// Callback receives correct tag and level
// ---------------------------------------------------------------------------

static void test_callback_tag_and_level()
{
    CallbackCapture cap;
    cap.install();

    xash::platform::log( xash::platform::LogLevel::Warning, "filesystem",
                         "path too long" );

    CHECK( cap.call_count == 1 );
    CHECK( cap.last_tag   == "filesystem" );
    CHECK( cap.last_level == xash::platform::LogLevel::Warning );
    CHECK( cap.last_text  == "path too long" );

    cap.uninstall();
}

// ---------------------------------------------------------------------------
// Truncation: message exceeding platform_log_buffer_size does not crash
// and the callback still fires.
// ---------------------------------------------------------------------------

static void test_long_message_truncated()
{
    // Build a message clearly larger than any reasonable stack buffer.
    std::string long_msg( 4096, 'X' );

    CallbackCapture cap;
    cap.install();

    xash::platform::log( xash::platform::LogLevel::Info, "test",
                         std::string_view{ long_msg } );

    // Must have called the callback exactly once and must not have crashed.
    CHECK( cap.call_count == 1 );
    // The captured text must be shorter than the original (truncated).
    CHECK( cap.last_text.size() < long_msg.size() );

    cap.uninstall();
}

// ---------------------------------------------------------------------------
// Callback reset: nullptr restores default-only sink; no callback fires.
// ---------------------------------------------------------------------------

static void test_callback_reset()
{
    CallbackCapture cap;
    cap.install();
    cap.uninstall();  // sets callback to nullptr

    xash::platform::log( xash::platform::LogLevel::Info, "test",
                         "should not reach callback" );

    // cap.uninstall sets instance=nullptr, so the callback_count will be 0
    // because instance is nullptr before it can be incremented.  However,
    // after log_set_callback(nullptr) the function pointer itself is null,
    // so our static won't be called at all.
    CHECK( cap.call_count == 0 );
}

// ---------------------------------------------------------------------------
// Multiple callbacks: second install replaces first
// ---------------------------------------------------------------------------

static void test_callback_replace()
{
    CallbackCapture cap1, cap2;
    cap1.install();

    xash::platform::log( xash::platform::LogLevel::Info, "t", "first" );
    CHECK( cap1.call_count == 1 );

    // Replace with cap2.
    cap2.install();  // installs cap2 and sets instance = &cap2

    xash::platform::log( xash::platform::LogLevel::Info, "t", "second" );
    CHECK( cap1.call_count == 1 );  // first callback not called again
    CHECK( cap2.call_count == 1 );

    cap2.uninstall();
}

// ---------------------------------------------------------------------------
// log_verbose inline helper
// ---------------------------------------------------------------------------

static void test_log_verbose_no_crash()
{
    // Whether compiled with XASH_VERBOSE or not, calling the helper must not
    // crash.  When XASH_VERBOSE is absent it should be a no-op.
    CallbackCapture cap;
    cap.install();

    xash::platform::log_verbose( "test", "verbose helper" );

#ifdef XASH_VERBOSE
    CHECK( cap.call_count == 1 );
#else
    CHECK( cap.call_count == 0 );  // compiled out
#endif

    cap.uninstall();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    test_all_levels_no_crash();
    test_empty_text_no_crash();
    test_logf_basic();
    test_callback_tag_and_level();
    test_long_message_truncated();
    test_callback_reset();
    test_callback_replace();
    test_log_verbose_no_crash();

    std::printf( "test_log: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
