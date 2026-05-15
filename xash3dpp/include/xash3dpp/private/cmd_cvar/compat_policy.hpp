#pragma once
// xash3dpp — ICompatPolicy: GoldSrc compatibility quirks interface (PRIVATE)
//
// This header is NOT part of the cmd_cvar public API.  Only the host layer
// (which chooses between GoldSrcCompatPolicy and NullCompatPolicy at link time)
// and the two compat TUs themselves should include it.
//
// Core cmd_cvar files forward-declare ICompatPolicy via context.hpp and
// never include this header directly.
//
// Implementations:
//   src/cmd_cvar/compat_goldsrc.cpp — enabled when XASH_GOLDSRC_COMPAT=1
//   src/cmd_cvar/compat_null.cpp    — enabled when XASH_GOLDSRC_COMPAT=0
//
// See boundary doc D12 for the full isolation rationale.

#include <cstdint>

namespace xash::cmd_cvar {

struct ICompatPolicy {
    // Return a replacement cvar name if this name should be silently redirected.
    // Example: "gl_widescreen_yfov" → "r_adjust_fov" (HL25 quirk).
    // Return nullptr if no redirect applies.
    [[nodiscard]] virtual const char *redirect_cvar_name(const char *name) const noexcept = 0;

    // Return true if this command name is in the GoldSrc HL-mod exemption table
    // (ricochet / dod cl_filterstuffcmd bypass).
    [[nodiscard]] virtual bool is_filterable_exempt(const char *cmd_name) const noexcept = 0;

    // Return true if the cmd_overridable flag should be applied to the named command.
    // The GoldSrc implementation uses the CMD_OVERRIDABLE list from engine code.
    [[nodiscard]] virtual bool is_overridable_command(const char *cmd_name) const noexcept = 0;

protected:
    ICompatPolicy()          = default;
    virtual ~ICompatPolicy() = default;
};

} // namespace xash::cmd_cvar
