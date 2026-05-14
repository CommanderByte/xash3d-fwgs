// xash3dpp — IFileSystem009 compatibility shim  (optional)
// Legacy reference: filesystem/VFileSystem009.cpp
//
// This file includes the legacy VFileSystem009.h (from filesystem/) to obtain
// the interface declaration, but no legacy types ever appear in the rewrite
// public headers.

#include <xash3dpp/private/filesystem/vfs009/vfs009.hpp>

// Pull in the legacy interface definition only inside this translation unit.
// Suppress clang/GCC pedantic warnings that the legacy header may trigger.
// (All other rewrite TUs never see this header.)
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
// TODO: #include "../../../../filesystem/VFileSystem009.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace xash::filesystem::vfs009 {

// ---------------------------------------------------------------------------
// Shim adapter — implements IFileSystem009 by delegating to Filesystem.
// ---------------------------------------------------------------------------

// TODO: class Vfs009Adapter final : public IFileSystem009 { ... };

void* create_vfs009_interface(Filesystem& fs) {
    // TODO: return new Vfs009Adapter{fs};
    (void)fs;
    return nullptr;
}

} // namespace xash::filesystem::vfs009
