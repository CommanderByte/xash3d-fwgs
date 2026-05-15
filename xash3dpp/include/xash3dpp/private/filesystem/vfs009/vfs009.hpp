#pragma once
// xash3dpp — IFileSystem009 compatibility shim  (optional)
// Legacy reference: filesystem/VFileSystem009.cpp, filesystem/VFileSystem009.h
//
// Compiled only when the CMake option XASH_VFS009_SHIM is ON (default ON).
// Returns a heap-allocated object that implements the Valve IFileSystem009
// interface by delegating to a xash::filesystem::Filesystem reference.
// The returned pointer is typed as void* to avoid including the legacy
// VFileSystem009.h in any rewrite header.

#include <xash3dpp/filesystem/filesystem.hpp>

namespace xash::filesystem::vfs009 {

// Returns an IFileSystem009 * (as void*) that wraps `fs`.
// The caller owns the returned object and is responsible for deleting it
// (cast to the appropriate type before deletion).
// Returns nullptr if the shim is not available (should never happen when
// the TU is compiled in).
[[nodiscard]] void* create_vfs009_interface(Filesystem& fs);

} // namespace xash::filesystem::vfs009
