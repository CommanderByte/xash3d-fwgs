// xash3dpp — Android AAsset search backend  (internal)
// Compiled only when XASH_ANDROID is defined.
// Legacy reference: filesystem/android.c

#if defined(XASH_ANDROID)

#include <xash3dpp/private/filesystem/backends/android_backend.hpp>
#include <xash3dpp/platform/os_io.hpp>
#include <xash3dpp/private/filesystem/os_file_factory.hpp>
#include <xash3dpp/utilities/path.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <strings.h>  // strncasecmp
#include <cstdio>      // SEEK_END, SEEK_SET

namespace xash::filesystem::backends {

using ::xash::platform::OsFd;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

// open the asset at full_path into an anonymous fd and return the fd plus its
// length (obtained by seeking to end).  Both are 0/invalid on failure.
struct AssetFd { OsFd fd; FsOffset length; };

AssetFd open_asset_fd(::xash::platform::AssetManagerHandle* mgr,
                      std::string_view full_path) noexcept {
    OsFd fd = ::xash::platform::open_asset(mgr, full_path);
    if (!fd.valid()) return {OsFd{}, 0};

    const FsOffset len = ::xash::platform::seek(fd, 0, SEEK_END);
    ::xash::platform::seek(fd, 0, SEEK_SET);
    if (len < 0) return {OsFd{}, 0};

    return {std::move(fd), len};
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AndroidBackend::AndroidBackend(xash::memory::PoolHandle pool,
                                std::string_view base_path,
                                SearchPathFlags  flags,
                                bool             engine_package)
    : ISearchBackend{pool},
      base_path_{base_path},
      flags_{flags},
      mgr_{::xash::platform::get_asset_manager(engine_package)}
{}

std::unique_ptr<ISearchBackend>
AndroidBackend::create(xash::memory::PoolHandle pool,
                       std::string_view path, SearchPathFlags flags) {
    return std::unique_ptr<ISearchBackend>{
        xash::memory::pool_new<AndroidBackend>( pool, pool, path, flags,
                                                /*engine_package=*/false ) };
}

// ---------------------------------------------------------------------------
// info
// ---------------------------------------------------------------------------

std::string AndroidBackend::info() const {
    return "android-assets://" + base_path_;
}

// ---------------------------------------------------------------------------
// open_file
// ---------------------------------------------------------------------------

std::unique_ptr<File> AndroidBackend::open_file(std::string_view path,
                                                std::string_view /*mode*/)
{
    if (!mgr_) return nullptr;

    const std::string full = xash::utilities::path_join(base_path_, path);
    auto [fd, len] = open_asset_fd(mgr_, full);
    if (!fd.valid()) return nullptr;

    return create_os_file(pool_, std::move(fd), len);
}

// ---------------------------------------------------------------------------
// file_time — Android assets carry no mtime.
// ---------------------------------------------------------------------------

std::optional<std::filesystem::file_time_type>
AndroidBackend::file_time(std::string_view /*path*/) {
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// find_file — case-insensitive resolution via directory listing.
// ---------------------------------------------------------------------------

std::optional<std::string> AndroidBackend::find_file(std::string_view path) {
    if (!mgr_) return std::nullopt;

    // Split path into optional directory prefix and filename.
    const std::size_t slash = path.rfind('/');
    const std::string_view dir_sv  = (slash != std::string_view::npos)
                                       ? path.substr(0, slash)
                                       : std::string_view{};
    const std::string_view name_sv = (slash != std::string_view::npos)
                                       ? path.substr(slash + 1)
                                       : path;

    const std::string lookup = xash::utilities::path_join(base_path_, dir_sv);
    const auto entries = ::xash::platform::list_assets(mgr_, lookup);

    for (const auto& entry : entries) {
        if (entry.size() == name_sv.size() &&
                ::strncasecmp(entry.c_str(), name_sv.data(), name_sv.size()) == 0) {
            // Reconstruct relative path using the canonical (packaged) casing.
            return xash::utilities::path_join(dir_sv, entry);
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// search — glob matching over the directory listing.
// ---------------------------------------------------------------------------

std::vector<std::string> AndroidBackend::search(std::string_view pattern,
                                                  bool             case_insensitive)
{
    if (!mgr_) return {};

    // Extract the directory prefix from the pattern.
    const std::size_t slash = pattern.rfind('/');
    const std::string_view dir_sv  = (slash != std::string_view::npos)
                                       ? pattern.substr(0, slash)
                                       : std::string_view{};
    const std::string_view glob_sv = (slash != std::string_view::npos)
                                       ? pattern.substr(slash + 1)
                                       : pattern;

    const std::string lookup = xash::utilities::path_join(base_path_, dir_sv);
    const auto entries = ::xash::platform::list_assets(mgr_, lookup);

    std::vector<std::string> result;
    for (const auto& entry : entries) {
        if (match_pattern(entry, glob_sv, case_insensitive)) {
            result.push_back(xash::utilities::path_join(dir_sv, entry));
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// load_file
// ---------------------------------------------------------------------------

// compliance-allow(thread-assert): ISearchBackend is immutable after
// construction (backend-interface.md); load_file is a const-correct read
// invoked under the facade's shared_lock — any-thread by contract.
std::vector<std::byte> AndroidBackend::load_file(std::string_view path) {
    if (!mgr_) return {};

    const std::string full = xash::utilities::path_join(base_path_, path);
    auto [fd, len] = open_asset_fd(mgr_, full);
    if (!fd.valid() || len == 0) return {};

    std::vector<std::byte> buf(static_cast<std::size_t>(len));
    const std::int64_t n = ::xash::platform::read(fd, buf.data(), buf.size());
    if (n != len) buf.clear();
    return buf;
}

} // namespace xash::filesystem::backends

#endif // XASH_ANDROID
