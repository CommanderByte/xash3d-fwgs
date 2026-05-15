// xash3dpp — platform OS I/O tests
// Covers: open_file (round-trip read/write), open_memfd, seek/tell, flush
//         (no crash), file_size, file_time, list_directory, make_directory,
//         rename_file, delete_file, is_case_insensitive.
//
// Platform-specific invariants:
//   Win32  — is_case_insensitive always returns true (NTFS default)
//   macOS  — is_case_insensitive always returns true (HFS+/APFS default)
//   Linux  — either value is accepted (depends on kernel and filesystem)
//
// open_memfd is tested as an optional feature: the call may return an invalid
// fd on platforms that do not support it (e.g. macOS without memfd_create).

#include <xash3dpp/platform/os_io.hpp>   // includes <filesystem> transitively

#include <cstdio>    // SEEK_SET, SEEK_END
#include <cstring>   // strlen, strcmp, strncmp
#include <filesystem>

namespace fs = std::filesystem;

static int g_pass = 0, g_fail = 0;

#define CHECK(expr) \
    do { if (expr) { ++g_pass; } \
         else { ++g_fail; std::printf("FAIL [line %d]: %s\n", __LINE__, #expr); } } while(0)

// ---------------------------------------------------------------------------
// Test-directory helpers
// ---------------------------------------------------------------------------

static fs::path g_testdir;

/// Returns the absolute path for a file |name| inside the test directory.
static std::string T(const char *name)
{
    return (g_testdir / name).string();
}

/// Creates |path| and writes |content| to it.  Returns true on success.
static bool write_file(const std::string &path, const char *content)
{
    using M = xash::platform::OpenMode;
    auto fd = xash::platform::open_file(path, M::WriteOnly | M::Create | M::Truncate);
    if (!fd.valid()) return false;
    const auto len = static_cast<std::size_t>(std::strlen(content));
    return xash::platform::write(fd, content, len) == static_cast<std::int64_t>(len);
}

// ---------------------------------------------------------------------------
// open_file: write then read
// ---------------------------------------------------------------------------

static void test_write_read_roundtrip()
{
    const char *content = "Hello, xash3dpp!";
    const std::size_t clen = std::strlen(content);

    CHECK(write_file(T("rw.tmp"), content));

    auto fd = xash::platform::open_file(T("rw.tmp"), xash::platform::OpenMode::ReadOnly);
    CHECK(fd.valid());

    char buf[64]{};
    auto n = xash::platform::read(fd, buf, sizeof(buf) - 1);
    CHECK(n == static_cast<std::int64_t>(clen));
    CHECK(std::strcmp(buf, content) == 0);
}

// ---------------------------------------------------------------------------
// seek / tell
// ---------------------------------------------------------------------------

static void test_seek_tell()
{
    const char *content = "ABCDEFGHIJ";  // 10 bytes
    const std::int64_t clen = static_cast<std::int64_t>(std::strlen(content));

    CHECK(write_file(T("seek.tmp"), content));

    auto fd = xash::platform::open_file(T("seek.tmp"), xash::platform::OpenMode::ReadOnly);
    CHECK(fd.valid());

    // seek to offset 4, read 3 bytes — expect "EFG"
    CHECK(xash::platform::seek(fd, 4, SEEK_SET) == 4);
    CHECK(xash::platform::tell(fd) == 4);

    char buf[4]{};
    CHECK(xash::platform::read(fd, buf, 3) == 3);
    CHECK(buf[0] == 'E' && buf[1] == 'F' && buf[2] == 'G');

    // seek to end — position should equal file length
    CHECK(xash::platform::seek(fd, 0, SEEK_END) == clen);
}

// ---------------------------------------------------------------------------
// open_memfd (optional feature — skip gracefully if unsupported)
// ---------------------------------------------------------------------------

static void test_memfd_roundtrip()
{
    auto fd = xash::platform::open_memfd("xash_test");
    if (!fd.valid()) {
        // Not supported on this platform/kernel — acceptable.
        ++g_pass;
        return;
    }

    const char *data = "memfd_data_42";
    const std::size_t len = std::strlen(data);

    CHECK(xash::platform::write(fd, data, len) == static_cast<std::int64_t>(len));
    CHECK(xash::platform::seek(fd, 0, SEEK_SET) == 0);

    char buf[64]{};
    CHECK(xash::platform::read(fd, buf, len) == static_cast<std::int64_t>(len));
    CHECK(std::strncmp(buf, data, len) == 0);
}

// ---------------------------------------------------------------------------
// flush — smoke: must not crash with a valid fd
// ---------------------------------------------------------------------------

static void test_flush_no_crash()
{
    auto fd = xash::platform::open_file(T("flush.tmp"),
                 xash::platform::OpenMode::WriteOnly |
                 xash::platform::OpenMode::Create   |
                 xash::platform::OpenMode::Truncate);
    CHECK(fd.valid());
    xash::platform::write(fd, "x", 1);
    xash::platform::flush(fd);
    ++g_pass;
}

// ---------------------------------------------------------------------------
// file_size
// ---------------------------------------------------------------------------

static void test_file_size()
{
    const char *content = "12345";
    CHECK(write_file(T("size.tmp"), content));

    auto sz = xash::platform::file_size(T("size.tmp"));
    CHECK(sz.has_value());
    CHECK(sz.value() == static_cast<std::int64_t>(std::strlen(content)));

    // Non-existent file must return nullopt.
    CHECK(!xash::platform::file_size(T("__no_such_file__.tmp")).has_value());
}

// ---------------------------------------------------------------------------
// file_time
// ---------------------------------------------------------------------------

static void test_file_time()
{
    CHECK(write_file(T("time.tmp"), "t"));

    auto ft = xash::platform::file_time(T("time.tmp"));
    CHECK(ft.has_value());

    // Non-existent file must return nullopt.
    CHECK(!xash::platform::file_time(T("__no_such_file__.tmp")).has_value());
}

// ---------------------------------------------------------------------------
// list_directory, make_directory, rename_file, delete_file
// ---------------------------------------------------------------------------

static void test_directory_ops()
{
    // Create a fresh sub-directory.
    const std::string subdir = (g_testdir / "listdir").string();
    CHECK(xash::platform::make_directory(subdir));

    // make_directory must be idempotent (directory already exists → true).
    CHECK(xash::platform::make_directory(subdir));

    // Create a file inside it.
    const std::string file_a = (g_testdir / "listdir" / "file_a.tmp").string();
    CHECK(write_file(file_a, "a"));

    // list_directory returns entry names (no path prefix).
    auto entries = xash::platform::list_directory(subdir);
    bool found_a = false;
    for (const auto &e : entries)
        if (e == "file_a.tmp") { found_a = true; break; }
    CHECK(found_a);

    // rename_file
    const std::string file_b = (g_testdir / "listdir" / "file_b.tmp").string();
    CHECK(xash::platform::rename_file(file_a, file_b));

    entries = xash::platform::list_directory(subdir);
    bool found_b = false;
    for (const auto &e : entries)
        if (e == "file_b.tmp") { found_b = true; break; }
    CHECK(found_b);

    // delete_file removes the file.
    CHECK(xash::platform::delete_file(file_b));
    CHECK(!xash::platform::file_size(file_b).has_value());
}

// ---------------------------------------------------------------------------
// is_case_insensitive
// ---------------------------------------------------------------------------

static void test_is_case_insensitive()
{
    const std::string path = g_testdir.string();

#ifdef _WIN32
    // NTFS is always case-insensitive.
    CHECK(xash::platform::is_case_insensitive(path));
#elif defined(__APPLE__)
    // HFS+ / APFS default is case-insensitive.
    CHECK(xash::platform::is_case_insensitive(path));
#else
    // Linux: result depends on the filesystem and kernel version — accept either.
    (void)xash::platform::is_case_insensitive(path);
    ++g_pass;
#endif
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    // Create an isolated directory for all test artefacts.
    g_testdir = fs::temp_directory_path() / "xash3dpp_test_os_io";
    fs::create_directories(g_testdir);

    test_write_read_roundtrip();
    test_seek_tell();
    test_memfd_roundtrip();
    test_flush_no_crash();
    test_file_size();
    test_file_time();
    test_directory_ops();
    test_is_case_insensitive();

    // Remove all test artefacts.
    std::error_code ec;
    fs::remove_all(g_testdir, ec);

    std::printf("os_io: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
