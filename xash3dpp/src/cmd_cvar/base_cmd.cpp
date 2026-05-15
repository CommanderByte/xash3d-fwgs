// xash3dpp — cmd_cvar: shared hash map for commands, cvars, and aliases
// Legacy reference: engine/common/base_cmd.c, engine/common/base_cmd.h
//
// Existing subsystems used:
//   xash3dpp_utilities — utilities::stricmp (case-insensitive name lookup)
//   xash3dpp_memory    — pool-backed node allocation

#include <xash3dpp/cmd_cvar/cvar.hpp>
#include <xash3dpp/cmd_cvar/command.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/utilities/string.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace xash::cmd_cvar {

// TODO: implement hash map keyed on case-insensitive cvar/command name.
//
// Design constraints (from boundary doc):
//   • Key type: const char* (NUL-terminated, case-insensitive compare)
//   • Fixed bucket count: limits::cvar_hash_buckets
//   • Separate chaining via singly-linked nodes
//   • Nodes allocated from a PoolHandle passed at construction
//   • find()   — O(1) average; returns void* or typed pointer
//   • insert() — adds a new node; caller guarantees the name is unique
//   • remove() — unlinks and frees the node; pointer returned for caller cleanup
//
// The single map is shared by commands, cvars, and aliases (legacy behaviour).

} // namespace xash::cmd_cvar
