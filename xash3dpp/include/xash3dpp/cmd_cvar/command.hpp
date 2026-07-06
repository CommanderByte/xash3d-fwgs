#pragma once
// xash3dpp — command public types
// Legacy reference: engine/common/cmd.h
//
// @thread-safety: value types only (POD descriptors + flag enums); no shared
// state. CommandDesc is a copyable snapshot; its pointers borrow registry
// memory (see @lifetime notes) and are only valid on the thread that owns the
// CmdCvarContext.

#include <cstdint>

namespace xash::cmd_cvar {

// Function pointer type for command callbacks.
// Layout-identical to the legacy xcommand_t typedef.
using CommandFn = void (*)();

// ---------------------------------------------------------------------------
// CommandFlags
// ---------------------------------------------------------------------------

enum CommandFlags : std::uint32_t {
    FCMD_EXTDLL      = 1u << 0,  // registered by the server DLL
    FCMD_CLIENTDLL   = 1u << 1,  // registered by the client DLL
    FCMD_GAMEUIDLL   = 1u << 2,  // registered by the menu DLL
    FCMD_PRIVILEGED  = 1u << 3,  // executes only in a trusted context
    FCMD_OVERRIDABLE = 1u << 4,  // game DLL may silently replace this command
};

// ---------------------------------------------------------------------------
// CommandDesc — copyable read-only descriptor for autocomplete / scripting
// ---------------------------------------------------------------------------

struct CommandDesc {
    const char   *name;  // @lifetime: registry (borrowed; valid until the command is removed)
    const char   *desc;  // @lifetime: registry (borrowed; valid until the command is removed)
    std::uint32_t flags;
    // ParamSpec span will be added here when the scripting layer is designed.
};

} // namespace xash::cmd_cvar
