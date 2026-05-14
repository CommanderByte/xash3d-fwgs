#pragma once
// xash3dpp — PK3DIR (uncompressed PK3 directory) backend  (internal)
// A pk3dir is a plain directory that acts like a ZIP archive in the search path.
// Legacy reference: filesystem/filesystem.c (FS_AddPK3Directory)

#include <xash3dpp/private/filesystem/backends/dir_backend.hpp>

namespace xash::filesystem::backends {

// PK3DIR is semantically identical to a plain directory but gets different
// default flags (no exec permission).  It reuses DirBackend internally.

class Pk3DirBackend final : public ISearchBackend {
public:
    Pk3DirBackend(std::string_view root_path, SearchPathFlags flags);

    static std::unique_ptr<ISearchBackend>
        Create(std::string_view path, SearchPathFlags flags);

    std::string Info() const override;

    std::unique_ptr<File> OpenFile(std::string_view path,
                                   std::string_view mode) override;

    std::optional<std::filesystem::file_time_type>
        FileTime(std::string_view path) override;

    std::optional<std::string> FindFile(std::string_view path) override;

    std::vector<std::string> Search(std::string_view pattern,
                                    bool case_insensitive) override;

    std::vector<std::byte> LoadFile(std::string_view path) override;

private:
    DirBackend inner_;
};

} // namespace xash::filesystem::backends
