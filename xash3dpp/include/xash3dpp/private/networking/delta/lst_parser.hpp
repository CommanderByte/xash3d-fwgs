#pragma once
// xash3dpp — delta.lst script parser (Layer 4)
// Legacy reference: engine/common/net_encode.c Delta_InitFields /
// Delta_ParseTable / Delta_ParseField.
//
// Grammar per section:
//   <struct_name> <encodeDll: none|gamedll|clientdll> [<encodeFunc>]
//   {
//       DEFINE_DELTA( <field>, <FLAGS|...>, <bits>, <multiplier> )
//       DEFINE_DELTA_POST( <field>, <FLAGS|...>, <bits>, <mult>, <post_mult> )
//   }
//
// Error policy (Q-5): structural errors (unknown struct, missing '{') were
// legacy Sys_Error — here they log at Error and fail the whole parse.
// Per-field syntax errors log and skip the field, exactly like the legacy
// Con_DPrintf paths.

#include <string_view>

#include <xash3dpp/networking/delta.hpp>

namespace xash::networking::delta {

// Parse a complete delta.lst script into `impl`'s tables.  Does NOT clear
// existing tables and does NOT apply the movevars fallback — the DeltaTables
// lifecycle owns both.  Returns false after logging on structural failure.
[[nodiscard]] bool parse_delta_lst( std::string_view script,
                                    DeltaTables::Impl &impl ) noexcept;

} // namespace xash::networking::delta
