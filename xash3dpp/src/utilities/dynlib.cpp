// xash3dpp — DLL export table helpers
// Legacy reference: public/dllhelpers.c

#include <xash3dpp/utilities/dynlib.hpp>

namespace xash::utilities {

void clear_exports( std::span<const ExportEntry> table ) noexcept
{
    for( const auto &e : table )
        *e.slot = nullptr;
}

bool validate_exports( std::span<const ExportEntry> table ) noexcept
{
    for( const auto &e : table )
        if( *e.slot == nullptr )
            return false;
    return true;
}

} // namespace xash::utilities
