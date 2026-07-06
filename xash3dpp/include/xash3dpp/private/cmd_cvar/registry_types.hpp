#pragma once
// xash3dpp — internal Command and AliasDef registry types (PRIVATE)
//
// These structs are opaque to all code outside xash3dpp_cmd_cvar.
// Public consumers see only CommandDesc (a copyable snapshot) and CommandFn.
//
// Command
//   Full internal record for a registered command.
//   'name' and 'desc' are pool-owned NUL-terminated strings.
//   'abi_next' forms the legacy linked list returned by Cmd_GetList equivalents.
//
// AliasDef
//   Alias record: maps a name to an expansion string.
//   'name' is an inline fixed-size buffer (no extra pool alloc needed).
//   'value' is a pool-owned copy of the alias expansion.

#include <xash3dpp/cmd_cvar/command.hpp>
#include <xash3dpp/limits.hpp>

#include <cstdint>

namespace xash::cmd_cvar {

// ---------------------------------------------------------------------------
// Command — full internal command record
// ---------------------------------------------------------------------------
struct Command {
    char         *name;        // pool-owned  @lifetime: impl-pool (pool_dup'd; freed on cmd_unlink/shutdown)
    char         *desc;        // pool-owned; may be nullptr  @lifetime: impl-pool (pool_dup'd; freed on cmd_unlink/shutdown)
    CommandFn     fn;
    std::uint32_t flags;       // CommandFlags bitmask
    std::uint32_t owner_flags; // mirrors CvarFlags domain for unlink matching
    Command      *abi_next;    // ABI linked-list for Cmd_GetList  @lifetime: registry (list link; nodes owned by impl-pool)
};

// ---------------------------------------------------------------------------
// AliasDef — command alias record
// ---------------------------------------------------------------------------
struct AliasDef {
    char        name[::xash::limits::alias_name_max]; // NUL-terminated; inlined to avoid extra pool alloc
    char       *value;                        // pool-owned expansion string  @lifetime: impl-pool (pool_dup'd; freed on unalias/shutdown)
    AliasDef   *abi_next;                      // @lifetime: registry (list link; nodes owned by impl-pool)
};

} // namespace xash::cmd_cvar
