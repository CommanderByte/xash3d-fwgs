// xash3dpp — PK3DIR backend  (internal)
// Delegates entirely to DirBackend.

#include <xash3dpp/private/filesystem/backends/pk3dir_backend.hpp>

namespace xash::filesystem::backends {

Pk3DirBackend::Pk3DirBackend(xash::memory::PoolHandle pool,
                             std::string_view root_path, SearchPathFlags flags)
    : ISearchBackend{pool}, inner_{pool, root_path, flags}
{}

std::unique_ptr<ISearchBackend>
Pk3DirBackend::Create(xash::memory::PoolHandle pool,
                      std::string_view path, SearchPathFlags flags) {
    return std::unique_ptr<ISearchBackend>{
        xash::memory::pool_new<Pk3DirBackend>( pool, pool, path, flags ) };
}

std::unique_ptr<ISearchBackend>
create_pk3dir(xash::memory::PoolHandle pool,
              std::string_view path, SearchPathFlags flags) {
    return Pk3DirBackend::Create(pool, path, flags);
}

std::string Pk3DirBackend::Info() const {
    return inner_.Info() + " (pk3dir)";
}

std::unique_ptr<File> Pk3DirBackend::OpenFile(std::string_view path,
                                               std::string_view mode) {
    return inner_.OpenFile(path, mode);
}

std::optional<std::filesystem::file_time_type>
Pk3DirBackend::file_time(std::string_view path) {
    return inner_.file_time(path);
}

std::optional<std::string> Pk3DirBackend::FindFile(std::string_view path) {
    return inner_.FindFile(path);
}

std::vector<std::string> Pk3DirBackend::search(std::string_view pattern,
                                                bool case_insensitive) {
    return inner_.search(pattern, case_insensitive);
}

std::vector<std::byte> Pk3DirBackend::load_file(std::string_view path) {
    return inner_.load_file(path);
}

void Pk3DirBackend::InvalidateDirectory(std::string_view subdir) noexcept {
    inner_.InvalidateDirectory(subdir);
}

} // namespace xash::filesystem::backends
