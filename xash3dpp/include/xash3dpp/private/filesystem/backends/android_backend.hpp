#pragma once
// xash3dpp — Android AAsset search backend  (internal)
// Compiled only when XASH_ANDROID is defined.
// Legacy reference: filesystem/android.c

#include <xash3dpp/private/filesystem/i_search_backend.hpp>
#include <xash3dpp/platform/os_io.hpp>
#include <xash3dpp/filesystem/search_path_flags.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace xash::filesystem::backends {

#if defined(XASH_ANDROID)

class AndroidBackend final : public ISearchBackend {
public:
    AndroidBackend(xash::memory::PoolHandle pool,
                   std::string_view base_path, SearchPathFlags flags,
                   bool engine_package);

    static std::unique_ptr<ISearchBackend>
        create(xash::memory::PoolHandle pool,
               std::string_view path, SearchPathFlags flags);

    std::string info() const override;

    std::unique_ptr<File> open_file(std::string_view path,
                                   std::string_view mode) override;

    std::optional<std::filesystem::file_time_type>
        file_time(std::string_view path) override;

    std::optional<std::string> find_file(std::string_view path) override;

    std::vector<std::string> search(std::string_view pattern,
                                    bool case_insensitive) override;

    std::vector<std::byte> load_file(std::string_view path) override;

private:
    std::string                    base_path_;
    SearchPathFlags                flags_;
    ::xash::platform::AssetManagerHandle*  mgr_ = nullptr; // @lifetime: platform JNI glue (g_handles) — process-lifetime, borrowed
};

#endif // XASH_ANDROID

} // namespace xash::filesystem::backends
