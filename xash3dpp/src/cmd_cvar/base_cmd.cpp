// xash3dpp — cmd_cvar: shared registry types and hash map (PRIVATE)
// Legacy reference: engine/common/base_cmd.c, engine/common/base_cmd.h
//
// CmdHashMap<V> is defined as a header-only template in:
//   include/xash3dpp/private/cmd_cvar/cmd_hash_map.hpp
//
// Internal Command and AliasDef structs are defined in:
//   include/xash3dpp/private/cmd_cvar/registry_types.hpp
//
// This translation unit exists to:
//   1. Verify both private headers compile in isolation.
//   2. Carry any future non-template helpers for the shared registry.

#include <xash3dpp/private/cmd_cvar/cmd_hash_map.hpp>
#include <xash3dpp/private/cmd_cvar/registry_types.hpp>
#include <xash3dpp/cmd_cvar/cvar.hpp>

#include <type_traits>

namespace xash::cmd_cvar {

static_assert(limits::alias_name_max >= 2,
    "alias_name_max must be at least 2 (one char + NUL)");

static_assert(std::is_trivially_destructible_v<Command>,
    "Command must be trivially destructible so bulk pool-free is safe");

static_assert(std::is_trivially_destructible_v<AliasDef>,
    "AliasDef must be trivially destructible so bulk pool-free is safe");

} // namespace xash::cmd_cvar
