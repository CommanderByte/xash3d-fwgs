#pragma once
// xash3dpp — dynamic library export table helpers
// Legacy reference: public/crtlib.h (dllfunc_t) + public/dllhelpers.c
//
// Used by plugin loaders (filesystem, renderer, game DLL) to resolve and
// validate their export tables after dlopen/LoadLibrary.
//
// @thread-safety: pure functions over caller-owned tables — safe from any thread.

#include <cstddef>
#include <span>
#include <string_view>

namespace xash::utilities {

struct ExportEntry
{
    std::string_view name;   // symbol name string
    void       **slot;   // pointer to the function-pointer slot to fill
};

// Zero-fill all function-pointer slots in the table.
void clear_exports( std::span<const ExportEntry> table ) noexcept;

// Return true only if every slot in the table is non-null.
[[nodiscard]] bool validate_exports( std::span<const ExportEntry> table ) noexcept;

} // namespace xash::utilities
