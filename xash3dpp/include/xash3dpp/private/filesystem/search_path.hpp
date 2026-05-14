#pragma once
// xash3dpp — search path entry  (internal)
// Legacy reference: filesystem/filesystem_internal.h  (searchpath_t)

#include <xash3dpp/filesystem/search_path_flags.hpp>
#include <xash3dpp/private/filesystem/i_search_backend.hpp>

#include <memory>
#include <string>

namespace xash::filesystem {

struct SearchPath {
    std::unique_ptr<ISearchBackend> backend;
    std::string                     source_path;  // disk path that was mounted
    SearchPathFlags                 flags;
};

} // namespace xash::filesystem
